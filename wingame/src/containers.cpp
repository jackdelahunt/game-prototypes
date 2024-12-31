#ifndef CPP_CONTAINERS
#define CPP_CONTAINERS

#include "common.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct Allocator;

template<typename T>
struct Slice;

template <typename T>
Slice<T> alloc(Allocator *allocator, i64 amount);

template<typename T>
struct Slice {
    T *data;
    i64 length;

    T& operator[] (i64 index) {
        assert(index < this->length);
        return this->data[index];
    }
};

template <typename T>
T *at_ptr(Slice<T> *slice, i64 index) {
    assert(index >= 0);
    assert(index < slice->length);

    return &slice->data[index];
}

template <typename... Ts>
Slice<char> fmt_string(Allocator *allocator, Slice<char> format, Ts... args) {
    // TODO: this just triple the length of the format, dont know what
    // else to do here...
    Slice<char> buffer = alloc<char>(allocator, format.length * 3);
    snprintf(buffer.data, buffer.length, format.data, args...);

    return buffer;
}

// -1 to not include null byte
#define STR(s) \
Slice<char>{(char *) s, sizeof(s) - 1 }

struct Allocator {
    Slice<u8> memory;
    i64 used;
};

void init(Allocator *allocator, i64 byte_count) {
    *allocator = {
        .memory = Slice<u8>{
            .data = (u8 *) malloc(byte_count),
            .length = byte_count,
        },
        .used = 0,
    };

    assert(allocator->memory.data);
}

template <typename T>
Slice<T> alloc(Allocator *allocator, i64 amount) {
    u8 *current = &allocator->memory.data[allocator->used];

    allocator->used += sizeof(T) * amount;
    assert(allocator->used < allocator->memory.length);

    return Slice<T> {.data = (T*) current, .length = amount};
}

void reset(Allocator *allocator) {
    allocator->used = 0;
}

void deinit(Allocator *allocator) {
    assert(allocator->memory.data);
    assert(allocator->memory.length > 0);

    free(allocator->memory.data);
    *allocator = {};
}

#endif
