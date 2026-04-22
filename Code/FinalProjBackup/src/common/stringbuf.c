#include "stringbuf.h"

#include "darray.h"

#include <string.h>

string_buf CreateStringBuf() {
    return DArrayCreate(u8);
}

void StringBufAppendChar(string_buf sb, u8 c) {
    DArrayPush(sb, c);
}

void StringBufAppend(string_buf sb, const char *str) {
    StringBufAppendLen(sb, str, strlen(str));
}

void StringBufAppendLen(string_buf sb, const char *str, u32 strLen) {
    for (int i = 0; i < strLen; i++) {
        u8 c = str[i];
        StringBufAppendChar(sb, c);
    }
}

void StringBufAppendNewLn(string_buf sb, const char *str) {
    StringBufAppendLen(sb, str, strlen(sb));
    StringBufAppendChar(sb, '\n');
}

void StringBufAppendNullTerminator(string_buf sb) {
    StringBufAppendChar(sb, '\0');
}

u64 StringBufGetLen(string_buf sb) {
    return DArrayLength(sb);
}

u64 StringBufGetCap(string_buf sb) {
    return DArrayCapacity(sb);
}