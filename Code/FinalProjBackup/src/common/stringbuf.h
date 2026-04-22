#ifndef _STRING_BUF_H
#define _STRING_BUF_H

#include "common.h"

typedef u8 *string_buf;

string_buf CreateStringBuf();
void StringBufAppendChar(string_buf sb, u8 c);
void StringBufAppend(string_buf sb, const char *str);
void StringBufAppendLen(string_buf sb, const char *str, u32 strLen);
void StringBufAppendNewLn(string_buf sb, const char *str);
void StringBufAppendNullTerminator(string_buf sb);

u64 StringBufGetLen(string_buf sb);
u64 StringBufGetCap(string_buf sb);

#endif