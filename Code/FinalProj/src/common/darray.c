#include "darray.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void *_DArrayCreate(u64 capacity, u64 stride) {
    u64 headerSize = DARRAY_FIELD_LENGTH * sizeof(u64);
    u64 arraySize = capacity * stride;
    u64 *newArray = (u64 *)malloc(headerSize + arraySize);
    memset(newArray, 0, headerSize + arraySize);
    newArray[DARRAY_CAPACITY] = capacity;
    newArray[DARRAY_LENGTH] = 0;
    newArray[DARRAY_STRIDE] = stride;
    return (void *)(newArray + DARRAY_FIELD_LENGTH);
}

void _DArrayDestroy(void *array) {
    u64 *header = (u64 *)array - DARRAY_FIELD_LENGTH;
    u64 headerSize = DARRAY_FIELD_LENGTH * sizeof(u64);
    u64 totalSize = headerSize + (header[DARRAY_CAPACITY] * header[DARRAY_STRIDE]);
    free(header);
}

u64 _DArrayFieldGet(void *array, u64 field) {
    u64 *header = (u64 *)array - DARRAY_FIELD_LENGTH;
    return header[field];
}

void _DArrayFieldSet(void *array, u64 field, u64 value) {
    u64 *header = (u64 *)array - DARRAY_FIELD_LENGTH;
    header[field] = value;
}

void *_DArrayResize(void *array) {
    u64 length = DArrayLength(array);
    u64 stride = DArrayStride(array);
    void *temp = _DArrayCreate((DARRAY_RESIZE_FACTOR * DArrayCapacity(array)), stride);
    memcpy(temp, array, length * stride);
    
    _DArrayFieldSet(temp, DARRAY_LENGTH, length);
    _DArrayDestroy(array);

    return temp;
}

void *_DArrayPush(void *array, const void *valuePtr) {
    u64 length = DArrayLength(array);
    u64 stride = DArrayStride(array);
    if (length >= DArrayCapacity(array)) {
        array = _DArrayResize(array);
    }

    u64 addr = (u64)array;
    addr += length * stride;
    memcpy((void *)addr, valuePtr, stride);
    _DArrayFieldSet(array, DARRAY_LENGTH, length + 1);

    return array;
}

void _DArrayPop(void *array, void *dest) {
    u64 length = DArrayLength(array);
    u64 stride = DArrayStride(array);
    u64 addr = (u64)array;
    addr += ((length - 1) * stride);
    memcpy(dest, (void *)addr, stride);
    _DArrayFieldSet(array, DARRAY_LENGTH, length - 1);
}

void *_DArrayPopAt(void *array, u64 index, void *dest) {
    u64 length = DArrayLength(array);
    u64 stride = DArrayStride(array);
    if (index >= length) {
        printf("Index out of bounds of this array. Length: %lu, index: %lu", length, index);
        return array;
    }

    u64 addr = (u64)array;
    memcpy(dest, (void *)(addr + (stride * index)), stride);

    if (index != length - 1) {
        memcpy(
            (void *)(addr + (stride * index)),
            (void *)(addr + (stride * (index + 1))),
            stride);
    }

    _DArrayFieldSet(array, DARRAY_LENGTH, length - 1);

    return array;
}

void *_DArrayInsertAt(void *array, u64 index, void *valuePtr) {
    u64 length = DArrayLength(array);
    u64 stride = DArrayStride(array);
    if (index >= length) {
        printf("Index out of bounds of this array. Length: %lu, index: %lu", length, index);
        return array;
    }
    if (length >= DArrayCapacity(array)) {
        array = _DArrayResize(array);
    }

    u64 addr = (u64)array;

    if (index != length - 1) {
        memcpy(
            (void *)(addr + (stride * (index + 1))),
            (void *)(addr + (stride * index)),
            stride);
    }

    memcpy((void *)(addr + (stride * index)), valuePtr, stride);
    _DArrayFieldSet(array, DARRAY_LENGTH, length + 1);

    return array;
}
