#include "file.h"

#include <stdio.h>

#include "arena.h"

loaded_file_t LoadFile(u8 *path) {
    loaded_file_t t = {0};
    t.success = false;

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("failed to load file: %s\n", path);
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