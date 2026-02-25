#include "hashtable.h"

#include <stdlib.h>
#include <string.h>

hash_table_t *CreateHashTable(u32 dataSize) {
    hash_table_t *table = (hash_table_t *)malloc(sizeof(hash_table_t));
    memset(table, 0, sizeof(hash_table_t));
    
    table->dataSize = dataSize;

    // for (int i = 0; i < ARRAY_LEN(table->data); i++) {
    //     table->data[i].prevItem = table->data[i].nextItem = table->data + i;
    // }

    return table;
}

static u32 Hash(u8 *key) {
    unsigned long hash = 5381;
    int c;

    while (c = *key++)
        hash = ((hash << 5) + hash) + c;

    return hash;
}

void HashInsert(hash_table_t *table, u8 *key, void *data) {
    u32 bucket = Hash(key) % BUCKET_COUNT;
    
    hash_item_t *current = table->data + bucket;
    while (current->nextItem != NULL) {
        current = current->nextItem;
    }

    hash_item_t *newItem = (hash_item_t *)malloc(sizeof(hash_item_t));
    strcpy(newItem->key, key);
    memcpy(newItem->data, data, table->dataSize);

    current->nextItem = newItem;
}

bool HashContains(hash_table_t *table, u8 *key) {
    u32 bucket = Hash(key) % BUCKET_COUNT;

    hash_item_t *current = table->data + bucket;
    while (current->nextItem != NULL) {
        if (strcmp(key, current->key) == 0) {
            return true;
        }

        current = current->nextItem;
    }

    return false;
}

void *HashGet(hash_table_t *table, u8 *key) {
    u32 bucket = Hash(key) % BUCKET_COUNT;

    hash_item_t *current = table->data + bucket;
    while (current->nextItem != NULL) {
        if (strcmp(key, current->key) == 0) {
            return current->data;
        }

        current = current->nextItem;
    }

    return NULL;
}