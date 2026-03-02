#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>
#include <stdbool.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

#define Kilobytes(kb) (kb * 1024)
#define Megabytes(mb) (Kilobytes(mb) * 1024)
#define Gigabytes(gb) (Megabytes(gb) * 1024)

#endif
