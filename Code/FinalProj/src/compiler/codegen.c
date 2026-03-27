#include "codegen.h"

#include "stringbuf.h"

void Writex86_64Header(string_buf sb) {
    StringBufAppendNewLn(sb, "section .text");
    StringBufAppendNewLn(sb, "\tglobal _start");
}

string_buf CodeGenx86_64(ast_node_t *node) {
    string_buf sb = CreateStringBuf();

    StringBufAppendNullTerminator(sb);
    return sb;
}