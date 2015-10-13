add_flang_library(flangFrontend
  ASTConsumers.cpp
  CompilerInstance.cpp
  FrontendAction.cpp
  TextDiagnosticPrinter.cpp
  TextDiagnosticBuffer.cpp
  VerifyDiagnosticConsumer.cpp
  )

target_link_libraries(flangFrontend
  flangAST
  flangBasic
  flangLex
  flangSema
  LLVMSupport
  )
