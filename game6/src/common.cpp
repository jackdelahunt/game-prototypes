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
    T *data;
    i64 len;

    Slice() {}

    Slice(T *data, i64 len) { // C++ sucks
        this->data = data;
        this->len = len;
    }

    Slice(const char *c_string) {
        this->data = (T *) c_string;
        this->len = strlen(c_string);
    }

    T& operator[](i64 index) {
        return this->data[index];
    }

    Slice<T> slice(i64 start, i64 end) {
        return Slice<T>(this->data + start, end - start);
    }

    const char *c() {
        return (const char *) this->data;
    }
};

typedef Slice<u8> string;

template <typename T>
Slice<T> make_slice(T *data, i64 len) {
    return Slice<T>(data, len);
}

Slice<char> read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == nullptr) {
        return {};
    }

    fseek(file, 0, SEEK_END);
    i64 file_size = ftell(file);
    fseek(file, 0, SEEK_SET);  /* same as rewind(f); */
    
    char *data = (char *) malloc(file_size + 1);
    fread(data, file_size, 1, file);
    fclose(file);
    
    data[file_size] = 0; // null terminate

    return make_slice(data, file_size);
}

#endif
