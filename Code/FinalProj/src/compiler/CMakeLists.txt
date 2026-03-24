add_executable(compiler main.c lexer.c ast.c parser.c semantics.c astEval.c)

target_link_libraries(compiler common)