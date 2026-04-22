#ifndef _STR_H
#define _STR_H

#include "common.h"

#define SLICE_STR "%.*s"
#define SLICE_ARGS(s) (int)(s).len, (s).str

typedef struct slice_t {
    u8 *str;
    u32 len;
} slice_t;

bool CompareSlices(slice_t *sliceA, slice_t *sliceB);
bool CompareSliceToStr(slice_t *slice, u8 *str, u32 len);

int SliceToInt(slice_t *slice);

u8 *SliceToStr(slice_t *slice);

#endif