#ifndef _ARENA_H
#define _ARENA_H

#include "common.h"

typedef struct arena_t {
    u8 *mem;
    u64 capacity;
    u64 pos;
} arena_t;

arena_t *CreateArena(u8 *mem, u64 memSize);

u8 *PushBytes(arena_t *arena, u64 bytes);

#define PushStruct(arena, type) (type *)PushBytes(arena, sizeof(type))
#define PushArray(arena, type, count) (type *)PushBytes(arena, sizeof(type) * count)

extern arena_t *globalArena;

#endif