#ifndef COMMON_CPP
#define COMMON_CPP

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

template <typename T>
struct Slice { // TODO: do safety checks in slices
    T *ptr;
    i64 len;

    Slice() {}

    Slice(T *data, i64 len) { // C++ sucks
        this->ptr = data;
        this->len = len;
    }

    Slice(const char *c_string) {
        this->ptr = (T *) c_string;
        this->len = strlen(c_string);
    }

    T& operator[](i64 index) {
        return this->ptr[index];
    }

    Slice<T> slice(i64 start, i64 end) {
        return Slice<T>(this->ptr + start, end - start);
    }

    const char *c() {
        return (const char *) this->ptr;
    }
};

typedef Slice<u8> string;

template <typename T>
Slice<T> make_slice(T *data, i64 len) {
    return Slice<T>(data, len);
}

template <typename T>
Slice<T> mem_alloc(i64 len) {
    T *ptr = (T *) malloc(len * sizeof(T));
    return make_slice(ptr, len);
}

template <typename T>
void mem_free(Slice<T> slice) {
    free(slice.ptr);
}

template <typename T, i64 N>
struct Array {
    T data[N];
    i64 size = N;
    i64 len;

    T& operator[](i64 index) {
        return this->data[index];
    }
};

template <typename T, i64 N>
void append(Array<T, N> *array, T value) {
    assert(array->len < N);

    array->data[array->len] = value;
    array->len += 1;
}

template <typename T, i64 N>
T* push(Array<T, N> *array) {
    assert(array->len < N);

    T *ptr = &array->data[array->len];
    array->len++;
    return ptr;
}

template <typename T, i64 N>
void reset(Array<T, N> *array) {
    array->len = 0;
}

template <typename T, i64 N>
void swap_remove(Array<T, N> *array, i64 index) {
    assert(index < array->len);

    array->data[index] = array->data[array->len - 1];
    array->len -= 1;
}

Slice<u8> read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == nullptr) {
        return {};
    }

    fseek(file, 0, SEEK_END);
    i64 file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    u8 *data = (u8 *) malloc(file_size + 1);
    fread(data, file_size, 1, file);
    fclose(file);
    
    data[file_size] = 0; // null terminate

    return make_slice(data, file_size);
}

// 0 -> 1
f32 rand_f32() {
    return (f32) rand() / (f32) RAND_MAX;
}

// -1 -> 1
f32 rand_f32_negative() {
    return (rand_f32() * 2.0f) - 1.0f;
}

#endif
