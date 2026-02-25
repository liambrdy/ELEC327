#ifndef _HASHTABLE_H
#define _HASHTABLE_H

#include "common.h"

#include <stdbool.h>

typedef struct hash_item_t {
    u8 *key;
    void *data;

    struct hash_item_t *prevItem;
    struct hash_item_t *nextItem;
} hash_item_t;

#define BUCKET_COUNT 256

typedef struct hash_table_t {
    hash_item_t *data[BUCKET_COUNT];

    u32 dataSize;
} hash_table_t;

hash_table_t *CreateHashTable(u32 dataSize);

void HashInsert(hash_table_t *table, u8 *key, void *data);
bool HashContains(hash_table_t *table, u8 *key);

void *HashGet(hash_table_t *table, u8 *key);

#endif