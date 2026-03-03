#include "hashtable.h"

#include <stdlib.h>
#include <string.h>

#include "arena.h"

hash_table_t *CreateHashTable(u32 dataSize) {
    hash_table_t *table = PushStruct(globalArena, hash_table_t);
    
    table->dataSize = dataSize;

    return table;
}

static u32 Hash(slice_t *key) {
    unsigned long hash = 5381;

    for (int i = 0; i < key->len; i++) {
        u8 c = key->str[i];
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

void HashInsert(hash_table_t *table, slice_t *key, void *data) {
    u32 bucket = Hash(key) % BUCKET_COUNT;
    
    hash_item_t *current = table->data[bucket];
    while (current != NULL) {
        if (CompareSliceToStr(key, current->key, strlen(current->key))) {
            memcpy(current->data, data, table->dataSize);
            return;
        }

        current = current->nextItem;
    }

    hash_item_t *newItem = (hash_item_t *)malloc(sizeof(hash_item_t));
    newItem->key = SliceToStr(key);

    newItem->data = malloc(table->dataSize);
    memcpy(newItem->data, data, table->dataSize);

    newItem->nextItem = table->data[bucket];
    table->data[bucket] = newItem;
}

void HashInsertStr(hash_table_t *table, u8 *key, void *data) {
    slice_t keySlice = {
        .str = key,
        .len = strlen(key)
    };

    HashInsert(table, &keySlice, data);
}

bool HashContains(hash_table_t *table, slice_t *key) {
    u32 bucket = Hash(key) % BUCKET_COUNT;

    hash_item_t *current = table->data[bucket];
    while (current != NULL) {
        if (CompareSliceToStr(key, current->key, strlen(current->key))) {
            return true;
        }

        current = current->nextItem;
    }

    return false;
}

void *HashGet(hash_table_t *table, slice_t *key) {
    u32 bucket = Hash(key) % BUCKET_COUNT;

    hash_item_t *current = table->data[bucket];    
    while (current != NULL) {
        if (CompareSliceToStr(key, current->key, strlen(current->key))) {
            return current->data;
        }

        current = current->nextItem;
    }

    return NULL;
}

bool HashContainsRet(hash_table_t *table, slice_t *key, void **data) {
    u32 bucket = Hash(key) % BUCKET_COUNT;

    hash_item_t *current = table->data[bucket];    
    while (current != NULL) {
        if (CompareSliceToStr(key, current->key, strlen(current->key))) {
            *data = current->data;
            return true;
        }

        current = current->nextItem;
    }

    return false;
}