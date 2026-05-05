#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stdbool.h>

#define FAT32_MAX_FILES 16
#define FAT32_NAME_LEN  13  /* "XXXXXXXX.XXX\0" */

typedef struct {
    char     name[FAT32_NAME_LEN];
    uint32_t size;
    uint32_t start_cluster;
} fat32_file_t;

extern const char *fat32_err;

bool fat32_init(void);
int  fat32_find_files(const char *ext, fat32_file_t *files, int max);
uint32_t fat32_read_file(const fat32_file_t *f, uint8_t *buf, uint32_t max_bytes);

/* Streaming read: fn is called per sector; data pointer is only valid during the callback. */
typedef bool (*fat32_write_fn)(uint32_t offset, const uint8_t *data,
                               uint32_t len, void *ctx);
uint32_t fat32_read_file_stream(const fat32_file_t *f,
                                fat32_write_fn fn, void *ctx);

#endif /* FAT32_H */
