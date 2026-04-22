#ifndef _FILE_H
#define _FILE_H

#include "common.h"

typedef struct loaded_file_t {
    u8 *buffer;
    u32 bufferLen;
    bool success;
} loaded_file_t;

loaded_file_t LoadFile(const u8 *path);
bool WriteFile(const u8 *path, u8 *buf, u32 bufLen);

u8 *GetFileDir(u8 *filepath);

#endif