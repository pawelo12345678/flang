//===--- CompilerInstance.cpp ---------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "flang/Frontend/CompilerInstance.h"
#include "flang/Basic/AllDiagnostics.h"
#include "flang/Basic/Diagnostic.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Errc.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/raw_ostream.h"
#include <string>
#include <system_error>
#include <memory>
#include <utility>
#include <cassert>

using namespace flang;

// Output Files

void CompilerInstance::addOutputFile(OutputFile &&OutFile) {
  assert(OutFile.OS && "Attempt to add empty stream to output list!");
  OutputFiles.push_back(std::move(OutFile));
}

static std::unique_ptr<llvm::raw_pwrite_stream> createOutputFile(
    StringRef OutputPath, std::error_code &Error, bool Binary,
    bool RemoveFileOnSignal, StringRef InFile, StringRef Extension,
    bool UseTemporary, bool CreateMissingDirectories,
    std::string *ResultPathName, std::string *TempPathName) {
  assert((!CreateMissingDirectories || UseTemporary) &&
         "CreateMissingDirectories is only allowed when using temporary files");

  std::string OutFile, TempFile;
  if (!OutputPath.empty()) {
    OutFile = OutputPath;
  } else if (InFile == "-") {
    OutFile = "-";
  } else if (!Extension.empty()) {
    SmallString<128> Path(InFile);
    llvm::sys::path::replace_extension(Path, Extension);
    OutFile = Path.str();
  } else {
    OutFile = "-";
  }

  std::unique_ptr<llvm::raw_fd_ostream> OS;
  std::string OSFile;

  if (UseTemporary) {
    if (OutFile == "-")
      UseTemporary = false;
    else {
      llvm::sys::fs::file_status Status;
      llvm::sys::fs::status(OutputPath, Status);
      if (llvm::sys::fs::exists(Status)) {
        // Fail early if we can't write to the final destination.
        if (!llvm::sys::fs::can_write(OutputPath)) {
          Error = make_error_code(llvm::errc::operation_not_permitted);
          return nullptr;
        }

        // Don't use a temporary if the output is a special file. This handles
        // things like '-o /dev/null'
        if (!llvm::sys::fs::is_regular_file(Status))
          UseTemporary = false;
      }
    }
  }

  if (UseTemporary) {
    // Create a temporary file.
    SmallString<128> TempPath;
    TempPath = OutFile;
    TempPath += "-%%%%%%%%";
    int fd;
    std::error_code EC =
        llvm::sys::fs::createUniqueFile(TempPath, fd, TempPath);

    if (CreateMissingDirectories &&
        EC == llvm::errc::no_such_file_or_directory) {
      StringRef Parent = llvm::sys::path::parent_path(OutputPath);
      EC = llvm::sys::fs::create_directories(Parent);
      if (!EC) {
        EC = llvm::sys::fs::createUniqueFile(TempPath, fd, TempPath);
      }
    }

    if (!EC) {
      OS.reset(new llvm::raw_fd_ostream(fd, /*shouldClose=*/true));
      OSFile = TempFile = TempPath.str();
    }
    // If we failed to create the temporary, fallback to writing to the file
    // directly. This handles the corner case where we cannot write to the
    // directory, but can write to the file.
  }

  if (!OS) {
    OSFile = OutFile;
    OS.reset(new llvm::raw_fd_ostream(
        OSFile, Error,
        (Binary ? llvm::sys::fs::F_None : llvm::sys::fs::F_Text)));
    if (Error)
      return nullptr;
  }

  // Make sure the out stream file gets removed if we crash.
  if (RemoveFileOnSignal)
    llvm::sys::RemoveFileOnSignal(OSFile);

  if (ResultPathName)
    *ResultPathName = OutFile;
  if (TempPathName)
    *TempPathName = TempFile;

  if (!Binary || OS->supportsSeeking())
    return std::move(OS);

  auto B = llvm::make_unique<llvm::buffer_ostream>(*OS);
  return std::move(B);
}

raw_pwrite_stream *
CompilerInstance::createDefaultOutputFile(bool Binary, StringRef BaseInput,
                                          StringRef Extension) {
  return createOutputFile(getFrontendOpts().OutputFile, Binary,
                          /*RemoveFileOnSignal=*/true, BaseInput, Extension,
                          /*UseTemporary=*/true);
}

raw_pwrite_stream *
CompilerInstance::createOutputFile(StringRef OutputPath, bool Binary,
                                   bool RemoveFileOnSignal, StringRef BaseInput,
                                   StringRef Extension, bool UseTemporary,
                                   bool CreateMissingDirectories) {
  std::string OutputPathName, TempPathName;
  std::error_code EC;
  std::unique_ptr<raw_pwrite_stream> OS = ::createOutputFile(
      OutputPath, EC, Binary, RemoveFileOnSignal, BaseInput, Extension,
      UseTemporary, CreateMissingDirectories, &OutputPathName, &TempPathName);
  if (!OS) {
    getDiagnostics().Report(diag::err_fe_unable_to_open_output) << OutputPath
                                                                << EC.message();
    return nullptr;
  }

  raw_pwrite_stream *Ret = OS.get();
  // Add the output file -- but don't try to remove "-", since this means we are
  // using stdin.
  addOutputFile(OutputFile((OutputPathName != "-") ? OutputPathName : "",
                           TempPathName, std::move(OS)));

  return Ret;
}
