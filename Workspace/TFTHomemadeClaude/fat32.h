#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stdbool.h>

#define FAT32_MAX_FILES 16
#define FAT32_NAME_LEN  13  /* "XXXXXXXX.XXX\0" */

typedef struct {
    char     name[FAT32_NAME_LEN]; /* null-terminated "name.ext" (lower-case) */
    uint32_t size;                 /* file size in bytes                       */
    uint32_t start_cluster;        /* FAT32 first cluster of this file         */
} fat32_file_t;

/*
 * Parse the MBR (or direct BPB) and cache the FAT32 volume layout.
 * Must be called after sd_init() succeeds.
 * Returns true on success.
 */
bool fat32_init(void);

/*
 * Scan the root directory for files whose 8.3 extension matches ext
 * (upper- or lower-case, e.g. "ROM").
 * Results are stored in files[]; at most max entries are written.
 * Returns the number of entries found.
 */
int fat32_find_files(const char *ext, fat32_file_t *files, int max);

/*
 * Read an entire file into buf, up to max_bytes.
 * Returns the number of bytes actually read (equals f->size if successful).
 */
uint32_t fat32_read_file(const fat32_file_t *f, uint8_t *buf, uint32_t max_bytes);

/*
 * Streaming file read: calls fn(offset, data, len, ctx) for each SD sector
 * (up to 512 bytes each) as it is read.  Stops early if fn returns false.
 * Returns total bytes delivered to fn (equals f->size if fully successful).
 *
 * No large staging buffer is needed — data is handed directly out of the
 * internal 512-byte sector buffer, which is valid only for the duration of
 * the callback.
 */
typedef bool (*fat32_write_fn)(uint32_t offset, const uint8_t *data,
                               uint32_t len, void *ctx);
uint32_t fat32_read_file_stream(const fat32_file_t *f,
                                fat32_write_fn fn, void *ctx);

#endif /* FAT32_H */
