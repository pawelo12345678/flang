add_flang_library(flangLex
  Lexer.cpp
)

add_dependencies(flangLex
  FlangDeclNodes
  FlangDiagnosticParse
  )

target_link_libraries(flangLex
  flangBasic
  LLVMSupport
  )
