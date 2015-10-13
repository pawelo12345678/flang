add_flang_library(flangParse
  ParseDecl.cpp
  ParseSpecStmt.cpp
  ParseExec.cpp
  ParseExpr.cpp
  ParseFormat.cpp
  Parser.cpp
  FixedForm.cpp
)

add_dependencies(flangParse
  FlangDeclNodes
  FlangDiagnosticParse
  )

target_link_libraries(flangParse
  flangAST
  flangBasic
  flangLex
  flangSema
  LLVMSupport
  )
