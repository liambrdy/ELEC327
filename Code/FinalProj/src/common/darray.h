#ifndef _DARRAY_H
#define _DARRAY_H

#include "common.h"

enum {
    DARRAY_CAPACITY,
    DARRAY_LENGTH,
    DARRAY_STRIDE,
    DARRAY_FIELD_LENGTH
};

void *_DArrayCreate(u64 capacity, u64 stride);
void _DArrayDestroy(void *array);

u64 _DArrayFieldGet(void *array, u64 field);
void _DArrayFieldSet(void *array, u64 field, u64 value);

void *_DArrayResize(void *array);

void *_DArrayPush(void *array, const void *valuePtr);
void _DArrayPop(void *array, void *dest);

void *_DArrayPopAt(void *array, u64 index, void *dest);
void *_DArrayInsertAt(void *array, u64 index, void *valuePtr);

#define DARRAY_DEFAULT_CAPACITY 1
#define DARRAY_RESIZE_FACTOR 2

#define DArrayCreate(type) \
    _DArrayCreate(DARRAY_DEFAULT_CAPACITY, sizeof(type))

#define DArrayReserve(type, capacity) \
    _DArrayCreate(capacity, sizeof(type))

#define DArrayDestroy(array) \
    _DArrayDestroy(array)

#define DArrayPush(array, value)           \
    {                                      \
        typeof(value) temp = value;        \
        array = _DArrayPush(array, &temp); \
    }

#define DArrayPop(array, dest) \
    _DArrayPop(array, dest)

#define DArrayInsertAt(array, index, value) \
    {\
        typeof(value) temp = value; \
        array = _DArrayInsertAt(array, index, &temp); \
    }

#define DArrayPopAt(array, index, dest) \
    _DArrayPopAt(array, index, dest)

#define DArrayClear(array) \
    _DArrayFieldSet(array, DARRAY_LENGTH, 0)

#define DArrayCapacity(array) \
    _DArrayFieldGet(array, DARRAY_CAPACITY)

#define DArrayLength(array) \
    _DArrayFieldGet(array, DARRAY_LENGTH)

#define DArrayStride(array) \
    _DArrayFieldGet(array, DARRAY_STRIDE)

#define DArrayLengthSet(array, length) \
    _DArrayFieldSet(array, DARRAY_LENGTH, length)

#endif