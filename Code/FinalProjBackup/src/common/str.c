#include "str.h"

#include <math.h>
#include <string.h>

#include "arena.h"

static bool IsNum(u8 c) {
    return c >= '0' && c <= '9';
}

static int ToNum(u8 c) {
    return c - '0';
}

bool CompareSlices(slice_t *sliceA, slice_t *sliceB) {
    if (sliceA->len != sliceB->len) 
        return false;

    for (int i = 0; i < sliceA->len; i++) {
        if (sliceA->str[i] != sliceB->str[i])
            return false;
    }

    return true;
}

bool CompareSliceToStr(slice_t *slice, u8 *str, u32 len) {
    slice_t sliceB = {
        .str = str,
        .len = len
    };

    return CompareSlices(slice, &sliceB);
}

int SliceToInt(slice_t *slice) {
    int val = 0;

    for (int i = 0; i < slice->len; i++) {
        u8 c = slice->str[i];
        val += ToNum(c) * (int)pow(10, slice->len - i - 1);
    }

    return val;
}

u8 *SliceToStr(slice_t *slice) {
    u8 *str = PushArray(globalArena, u8, slice->len + 1);
    strncpy(str, slice->str, slice->len);

    str[slice->len] = '\0';

    return str;
}