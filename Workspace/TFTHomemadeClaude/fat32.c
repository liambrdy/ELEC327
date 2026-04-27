#include "fat32.h"
#include "sd.h"
#include <string.h>

/*
 * Minimal read-only FAT32 driver.  Supports 512-byte sectors only.
 * Handles both MBR-partitioned and direct (superfloppyformat) volumes.
 */

/* ---- Single shared 512-byte sector buffer ------------------------------ */

static uint8_t sec_buf[512];

static bool read_sec(uint32_t lba) {
    return sd_read_block(lba, sec_buf);
}

/* Little-endian helpers on sec_buf */
static uint16_t u16le(int off) {
    return (uint16_t)sec_buf[off] | ((uint16_t)sec_buf[off+1] << 8);
}
static uint32_t u32le(int off) {
    return (uint32_t)sec_buf[off]
         | ((uint32_t)sec_buf[off+1] <<  8)
         | ((uint32_t)sec_buf[off+2] << 16)
         | ((uint32_t)sec_buf[off+3] << 24);
}

/* ---- Cached volume geometry -------------------------------------------- */

static uint32_t part_start;      /* LBA of the partition / volume boot sector */
static uint8_t  secs_per_clust;  /* sectors per cluster                        */
static uint32_t fat_start;       /* LBA of the first FAT                       */
static uint32_t data_start;      /* LBA of the first data cluster              */
static uint32_t root_clust;      /* cluster number of the root directory       */

/* ---- Cluster arithmetic ------------------------------------------------- */

static uint32_t clust_to_lba(uint32_t c) {
    return data_start + (uint32_t)(c - 2) * secs_per_clust;
}

/* Follow FAT chain: returns next cluster, or 0 at end / on error. */
static uint32_t fat_next(uint32_t c) {
    uint32_t byte_off = c * 4;
    uint32_t sec  = fat_start + byte_off / 512;
    uint32_t boff = byte_off % 512;
    if (!read_sec(sec)) return 0;
    uint32_t val = u32le((int)boff) & 0x0FFFFFFFu;
    return (val >= 0x0FFFFFF8u) ? 0 : val;
}

/* ---- Public: fat32_init ------------------------------------------------- */

bool fat32_init(void) {
    if (!read_sec(0)) return false;

    /* Signature check */
    if (sec_buf[510] != 0x55 || sec_buf[511] != 0xAA) return false;

    /* Decide: direct FAT32 volume (no MBR) or MBR-partitioned? */
    if (sec_buf[82] == 'F' && sec_buf[83] == 'A' && sec_buf[84] == 'T') {
        /* Sector 0 is already the FAT32 boot sector. */
        part_start = 0;
    } else {
        /* Look at the first valid FAT partition entry in the MBR. */
        part_start = 0;
        for (int p = 0; p < 4; p++) {
            int  off  = 446 + p * 16;
            uint8_t type = sec_buf[off + 4];
            if (type == 0x0B || type == 0x0C || type == 0x0E) {
                uint32_t lba = u32le(off + 8);
                if (lba > 0) { part_start = lba; break; }
            }
        }
        if (part_start == 0) return false;

        if (!read_sec(part_start)) return false;
        if (sec_buf[510] != 0x55 || sec_buf[511] != 0xAA) return false;
    }

    /* Parse the BPB fields we need. */
    uint16_t bytes_per_sec = u16le(11);
    if (bytes_per_sec != 512) return false; /* only 512-byte sectors supported */

    secs_per_clust      = sec_buf[13];
    uint16_t reserved   = u16le(14);
    uint8_t  num_fats   = sec_buf[16];
    uint32_t fat_size   = u32le(36); /* FAT32 sectors-per-FAT (32-bit field) */
    root_clust          = u32le(44); /* FAT32 root directory start cluster   */

    fat_start  = part_start + reserved;
    data_start = fat_start + (uint32_t)num_fats * fat_size;

    return (secs_per_clust > 0 && root_clust >= 2);
}

/* ---- Directory helpers -------------------------------------------------- */

/* True if 8.3 dir entry has the given 1–3 char extension (case-insensitive). */
static bool has_ext(const uint8_t *de, const char *ext) {
    for (int i = 0; i < 3; i++) {
        char ce = ext[i];
        if (ce == '\0') {
            /* Extension ended — remaining ext bytes must all be spaces. */
            for (int j = i; j < 3; j++) if (de[8+j] != ' ') return false;
            return true;
        }
        char cd = (char)de[8 + i];
        /* Case-fold both to upper */
        if (cd >= 'a' && cd <= 'z') cd -= 32;
        if (ce >= 'a' && ce <= 'z') ce -= 32;
        if (cd != ce) return false;
    }
    return true;
}

/* Convert the 8.3 dir entry name to a displayable "name.ext" string. */
static void de_to_name(const uint8_t *de, char *out) {
    int n = 0;
    int nm = 8;
    while (nm > 0 && de[nm-1] == ' ') nm--;
    for (int i = 0; i < nm; i++) {
        char c = (char)de[i];
        if (c >= 'A' && c <= 'Z') c += 32; /* lower-case */
        out[n++] = c;
    }
    int ex = 3;
    while (ex > 0 && de[8+ex-1] == ' ') ex--;
    if (ex > 0) {
        out[n++] = '.';
        for (int i = 0; i < ex; i++) {
            char c = (char)de[8+i];
            if (c >= 'A' && c <= 'Z') c += 32;
            out[n++] = c;
        }
    }
    out[n] = '\0';
}

/* ---- Public: fat32_find_files ------------------------------------------- */

int fat32_find_files(const char *ext, fat32_file_t *files, int max) {
    int count = 0;
    uint32_t clust = root_clust;

    while (clust != 0 && count < max) {
        uint32_t lba = clust_to_lba(clust);

        for (int s = 0; s < secs_per_clust && count < max; s++) {
            if (!read_sec(lba + s)) return count;

            for (int e = 0; e < 16 && count < max; e++) {
                uint8_t *de = &sec_buf[e * 32];

                if (de[0] == 0x00) goto done;  /* end of directory */
                if (de[0] == 0xE5) continue;   /* deleted entry    */
                if (de[11] == 0x0F) continue;  /* LFN entry        */
                if (de[11] & 0x18) continue;   /* dir or vol label */
                if (!has_ext(de, ext)) continue;

                de_to_name(de, files[count].name);
                files[count].size =
                    (uint32_t)de[28] | ((uint32_t)de[29]<<8) |
                    ((uint32_t)de[30]<<16) | ((uint32_t)de[31]<<24);
                uint32_t chi = ((uint32_t)de[21]<<8 | de[20]) << 16;
                uint32_t clo =  (uint32_t)de[27]<<8 | de[26];
                files[count].start_cluster = chi | clo;
                count++;
            }
        }
        clust = fat_next(clust);
    }
done:
    return count;
}

/* ---- Public: fat32_read_file -------------------------------------------- */

uint32_t fat32_read_file(const fat32_file_t *f, uint8_t *buf, uint32_t max_bytes) {
    uint32_t total     = 0;
    uint32_t remaining = (f->size < max_bytes) ? f->size : max_bytes;
    uint32_t clust     = f->start_cluster;

    while (clust != 0 && remaining > 0) {
        uint32_t lba = clust_to_lba(clust);

        for (int s = 0; s < secs_per_clust && remaining > 0; s++) {
            if (!read_sec(lba + s)) return total;
            uint32_t chunk = (remaining < 512) ? remaining : 512;
            memcpy(buf + total, sec_buf, chunk);
            total     += chunk;
            remaining -= chunk;
        }
        clust = fat_next(clust);
    }
    return total;
}

/* ---- Public: fat32_read_file_stream ------------------------------------- */

uint32_t fat32_read_file_stream(const fat32_file_t *f,
                                fat32_write_fn fn, void *ctx) {
    uint32_t total     = 0;
    uint32_t remaining = f->size;
    uint32_t clust     = f->start_cluster;

    while (clust != 0 && remaining > 0) {
        uint32_t lba = clust_to_lba(clust);

        for (int s = 0; s < secs_per_clust && remaining > 0; s++) {
            if (!read_sec(lba + s)) return total;
            uint32_t chunk = (remaining < 512) ? remaining : 512;
            if (!fn(total, sec_buf, chunk, ctx)) return total;
            total     += chunk;
            remaining -= chunk;
        }
        clust = fat_next(clust);
    }
    return total;
}
