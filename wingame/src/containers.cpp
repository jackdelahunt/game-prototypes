#ifndef CPP_CONTAINERS
#define CPP_CONTAINERS

#include "common.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct Allocator;

template<typename T>
struct Slice;

template <typename T> internal
Slice<T> alloc(Allocator *allocator, i64 amount);

template<typename T>
struct Slice {
    T *data;
    i64 len;

    T& operator[] (i64 index) {
        assert(index < this->len);
        return this->data[index];
    }
};

template <typename T> internal
Slice<T> slice(Slice<T> s, i64 start, i64 end) {
    assert(start >= 0 && end > 0);
    assert(end > start);
    assert(start < s.len && end <= s.len);

    return Slice<T> {.data = s.data + start, .len = end - start};
}

template <typename... Ts> internal
Slice<char> fmt_string(Allocator *allocator, Slice<char> format, Ts... args) {
    // TODO: this just triple the length of the format, dont know what
    // else to do here...
    Slice<char> buffer = alloc<char>(allocator, format.len * 3);
    i64 write_count = snprintf(buffer.data, buffer.len, format.data, args...);
    buffer.len = write_count;
    return buffer;
}

// -1 to not include null byte
#define STR(s) \
Slice<char>{(char *) s, sizeof(s) - 1 }

struct Allocator {
    Slice<u8> memory;
    i64 used;
};

internal
void init(Allocator *allocator, i64 byte_count) {
    *allocator = {
        .memory = Slice<u8>{
            .data = (u8 *) malloc(byte_count),
            .len = byte_count,
        },
        .used = 0,
    };

    assert(allocator->memory.data);
}

template <typename T> internal
Slice<T> alloc(Allocator *allocator, i64 amount) {
    u8 *current = &allocator->memory.data[allocator->used];

    allocator->used += sizeof(T) * amount;
    assert(allocator->used < allocator->memory.len);

    return Slice<T> {.data = (T*) current, .len = amount};
}

internal
void reset(Allocator *allocator) {
    allocator->used = 0;
}

internal
void deinit(Allocator *allocator) {
    assert(allocator->memory.data);
    assert(allocator->memory.len > 0);

    free(allocator->memory.data);
    *allocator = {};
}

#endif
