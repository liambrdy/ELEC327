add_executable(compiler main.c lexer.c ast.c parser.c semantics.c astEval.c codegen/rom.c)

target_link_libraries(compiler common)
target_include_directories(compiler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})