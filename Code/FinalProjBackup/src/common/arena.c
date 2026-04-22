#include "arena.h"

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

arena_t *globalArena = 0;

arena_t *CreateArena(u8 *mem, u64 memSize) {
    arena_t *arena = (arena_t *)malloc(sizeof(arena_t));

    arena->mem = mem;
    arena->capacity = memSize;
    arena->pos = 0;

    memset(arena->mem, 0, memSize);

    return arena;
}

u8 *PushBytes(arena_t *arena, u64 bytes) {
    if (bytes + arena->pos >= arena->capacity) {
        printf("out of memory\n");
        return NULL;
    }

    u8 *mem = arena->mem + arena->pos;
    arena->pos += bytes;

    return mem;
}
