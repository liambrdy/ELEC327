#include "file.h"

#include <stdio.h>
#include <string.h>
#include <stdio.h>

#include "arena.h"

loaded_file_t LoadFile(u8 *path) {
    loaded_file_t t = {0};
    t.success = false;

    FILE *f = fopen(path, "r");
    if (!f) {
        return t;
    }

    fseek(f, 0, SEEK_END);
    long fileLength = ftell(f);
    rewind(f);

    t.buffer = (u8 *)PushArray(globalArena, u8, fileLength + 1);
    size_t bytesRead = fread(t.buffer, 1, fileLength, f);
    t.buffer[bytesRead] = '\0';

    t.bufferLen = bytesRead + 1;
    t.success = true;

    fclose(f);

    return t;
}

u8 *GetFileDir(u8 *filepath) {
    int pathLen = strlen(filepath);
    u8 *dir = PushArray(globalArena, u8, pathLen);

    memset(dir, 0, pathLen);
    int lastSlashPath = 0;
    for (int i = 0; i < pathLen; i++) {
        if (filepath[i] == '/') lastSlashPath = i;
    }

    strncpy(dir, filepath, lastSlashPath + 1);
    dir[lastSlashPath + 1] = '\0';

    return dir;
}