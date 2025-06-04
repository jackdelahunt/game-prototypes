#ifndef ACK_CPP
#define ACK_CPP

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// cursed c++ headers to get saving working
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>

#define ASSERT(x) if (!(x)) __debugbreak();
#define BIT_SET(a, b) (a) & (b)
#define SCOPE }switch(0){default:

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

    Slice() {
        this->ptr = NULL;
        this->len = 0;
    }

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

    T* begin() {
        return ptr;
    }

    T* end() {
        return ptr + len;
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
    i64 bytes = len * sizeof(T);

    T *ptr = (T *) malloc(bytes);
    memset(ptr, 0, bytes);

    return make_slice(ptr, len);
}

template <typename T>
void mem_free(Slice<T> slice) {
    free(slice.ptr);
}

template <typename T>
struct FixedArray {
    Slice<T> slice;
    i64 len;

    T& operator[](i64 index) {
        return this->slice[index];
    }

    T* begin() {
        return &slice[0];
    }

    T* end() {
        return &slice[len];
    }
};

template <typename T>
FixedArray<T> new_fixed_array(i64 size) {
    return FixedArray<T> {
        .slice = mem_alloc<T>(size),
        .len = 0
    };
}

template <typename T>
void append(FixedArray<T> *array, T value) {
    assert(array->len < array->slice.len);

    array->slice[array->len] = value;
    array->len += 1;
}

template <typename T>
T* push(FixedArray<T> *array) {
    assert(array->len < array->slice.len);

    T *ptr = &array->slice[array->len];
    array->len++;
    return ptr;
}

template <typename T>
void reset(FixedArray<T> *array) {
    array->len = 0;
}

template <typename T>
void swap_remove(FixedArray<T> *array, i64 index) {
    assert(index < array->len);

    array->slice[index] = array->slice[array->len - 1];
    array->len -= 1;
}

template <typename T, i64 N>
struct StackArray {
    T data[N];
    i64 size = N;
    i64 len;

    T& operator[](i64 index) {
        return this->data[index];
    }

    T* begin() {
        return data;
    }

    T* end() {
        return data + len;
    }
};

template <typename T, i64 N>
void append(StackArray<T, N> *array, T value) {
    assert(array->len < N);

    array->data[array->len] = value;
    array->len += 1;
}

template <typename T, i64 N>
T* push(StackArray<T, N> *array) {
    assert(array->len < N);

    T *ptr = &array->data[array->len];
    array->len++;
    return ptr;
}

template <typename T, i64 N>
void reset(StackArray<T, N> *array) {
    array->len = 0;
}

template <typename T, i64 N>
void swap_remove(StackArray<T, N> *array, i64 index) {
    assert(index < array->len);

    array->data[index] = array->data[array->len - 1];
    array->len -= 1;
}

f32 rand_f32();
f32 rand_f32_negative();
i64 rand_i64();
string read_file(string path);
bool file_exists(const char *path);
bool copy_file(const char *path, const char *new_path);
std::string read_entire_file(const char *path);

// 0 -> 1
f32 rand_f32() {
    return (f32) rand() / (f32) RAND_MAX;
}

// -1 -> 1
f32 rand_f32_negative() {
    return (rand_f32() * 2.0f) - 1.0f;
}

// 0 -> RAND_MAX
i64 rand_i64() {
    return (i64) rand();
}

string read_file(string path) {
    FILE *file = fopen(path.c(), "rb");
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

bool file_exists(const char *path) {
    return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

bool copy_file(const char *path, const char *new_path) {
    if (!file_exists(path)) {
        return false;
    }

    std::ifstream src(path, std::ios::binary);
    std::ofstream dst(new_path, std::ios::binary);

    if (!src || !dst) return false;

    dst << src.rdbuf();

    return src && dst;
}

std::string read_entire_file(const char *path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return ""; // Could also throw or handle error differently
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

#endif
