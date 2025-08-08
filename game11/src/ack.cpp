#ifndef ACK_CPP
#define ACK_CPP

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BREAKPOINT __debugbreak()
#define ASSERT(x) if (!(x)) __debugbreak();
#define BIT_SET(a, b) ((a & b) != 0)
#define SET_BIT(a, b) (a) |= (b)
#define UNSET_BIT(a, b) (a) &= ~(b) 
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

// @slice
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
        ASSERT(index < this->len);

        return this->ptr[index];
    }

    // slice from a range
    Slice<T> slice(i64 start, i64 end) {
        return Slice<T>(this->ptr + start, end - start);
    }

    // slice from a starting point until the end
    Slice<T> slice_from(i64 start) {
        return Slice<T>(this->ptr + start, this->len - start);
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

typedef Slice<u8> str;

template <typename T>
Slice<T> slice_create(T *data, i64 len) {
    return Slice<T>(data, len);
}

template <typename T>
Slice<T> slice_create_malloc(i64 len) {
    i64 bytes = len * sizeof(T);

    T *ptr = (T *) malloc(bytes);
    memset(ptr, 0, bytes);

    return slice_create(ptr, len);
}

template <typename T>
void slice_free(Slice<T> slice) {
    free(slice.ptr);
}

template <typename T>
Slice<u8> slice_to_bytes(Slice<T> slice) {
    return slice_create((u8 *) slice.ptr, sizeof(T) * slice.len);
}

template <typename T>
Slice<T> slice_from_bytes(Slice<u8> slice) {
    return slice_create((T *) slice.ptr, slice.len / sizeof(T));
}

template <typename T>
T *slice_to_ptr(Slice<u8> slice) {
    ASSERT(slice.len == sizeof(T));

    return (T *) slice.ptr;
}

template <typename T>
Slice<u8> slice_from_ptr(T *ptr) {
    return slice_create((u8 *) ptr, sizeof(T));
}

template <typename T>
void slice_copy(Slice<T> dst, Slice<T> src) {
    ASSERT(dst.ptr && src.ptr);
    ASSERT(src.len <= dst.len);

    for (i64 i = 0; i < src.len; i++) {
        dst[i] = src[i];
    }
}

// copy memory from ptr into slice for the entire
// length of the slice, assumes data at ptr is valid
// for the length of the slice and the slice has
// valid memory to copy to
template <typename T>
void slice_copy_raw_ptr(Slice<T> slice, void *ptr) {
    ASSERT(slice.ptr && ptr);

    for (i64 i = 0; i < slice.len; i++) {
        slice.ptr[i] = ((T *) ptr)[i];
    }
}

// @fixedarray
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
        .slice = slice_create_malloc<T>(size),
        .len = 0
    };
}

template <typename T>
void append(FixedArray<T> *array, T value) {
    ASSERT(array->len < array->slice.len);

    array->slice[array->len] = value;
    array->len += 1;
}

template <typename T>
T* push(FixedArray<T> *array) {
    ASSERT(array->len < array->slice.len);

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
    ASSERT(index < array->len);

    array->slice[index] = array->slice[array->len - 1];
    array->len -= 1;
}

// @stackarray
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
    ASSERT(array->len < N);

    array->data[array->len] = value;
    array->len += 1;
}

template <typename T, i64 N>
T* push(StackArray<T, N> *array) {
    ASSERT(array->len < N);

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
    ASSERT(index < array->len);

    array->data[index] = array->data[array->len - 1];
    array->len -= 1;
}

// @arena
struct Arena {
    i64 end;
    Slice<u8> bytes;
};

Arena arena_create(i64 size) {
    return Arena {
        .end = 0,
        .bytes = slice_create_malloc<u8>(size)
    };
}

void arena_destroy(Arena *arena) {
    slice_free(arena->bytes);
    *arena = {};
}

void arena_reset(Arena *arena) {
    arena->end = 0;
}

template <typename T>
T *arena_alloc(Arena *arena) {
    const i64 SIZE = sizeof(T);

    ASSERT(arena->end + SIZE <= arena->bytes.len);

    T *ptr = (T *) &arena->bytes[arena->end];
    arena->end += SIZE;

    return ptr;
}

template <typename T>
Slice<T> arena_alloc_many(Arena *arena, i64 size) {
    i64 byte_count = sizeof(T) * size;

    ASSERT(arena->end + byte_count <= arena->bytes.len);

    Slice<u8> bytes = arena->bytes.slice(arena->end, arena->end + byte_count);
    arena->end += byte_count;

    return slice_create((T *) bytes.ptr, size);
}

template <typename T>
Slice<T> arena_realloc(Arena *arena, Slice<T> old_slice, i64 new_size) {
    // right now I just always reallocate, could check if this was the
    // last allocation and just extend the length of the slice - 08/08/25

    Slice<T> new_slice = arena_alloc_many<T>(arena, new_size);
    slice_copy(new_slice, old_slice);
    return new_slice;
}

// @dynamicarray
template <typename T>
struct DynamicArray {
    Arena *arena;
    Slice<T> slice;
    i64 len;
    i64 capacity;

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
DynamicArray<T> dynamic_array_create(Arena *arena, i64 capacity) {
    return DynamicArray<T> {
        .arena = arena,
        .slice = arena_alloc_many<T>(arena, capacity),
        .len = 0,
        .capacity = capacity
    };
}


template <typename T>
void dynamic_array_maybe_grow(DynamicArray<T> *array, i64 required_slots) {
    i64 capacity_needed = array->len + required_slots;
    i64 new_capacity = capacity_needed * 2;

    if (capacity_needed > array->capacity) {
        printf("dyn array grew, len is %lld capacity was %lld now %lld\n", array->len, array->capacity, new_capacity);
        array->slice = arena_realloc(array->arena, array->slice, new_capacity);
        array->capacity = new_capacity;
    }
}

template <typename T>
void append(DynamicArray<T> *array, T value) {
    dynamic_array_maybe_grow(array, 1);

    array->slice[array->len] = value;
    array->len += 1;
}

template <typename T>
void append_many(DynamicArray<T> *array, Slice<T> values) {
    if (values.len <= 0) {
        return;
    }

    Slice<T> sub_slice = push_many(array, values.len);
    slice_copy(sub_slice, values);
}

// appends 'count' number of items to the array and then returns a slice
// to the new items from the end of the array. The slice returned is 'count'
// number to items long
template <typename T>
Slice<T> push_many(DynamicArray<T> *array, i64 count) {
    dynamic_array_maybe_grow(array, count);

    Slice<T> s = array->slice.slice(array->len, array->len + count);
    array->len += count;

    return s;
}

// @format
// get bytes required to format the value, +1 when reserving space
// because snprintf needs that for the null terminator even thoug                           
// it doesn't report it the return value, nice one C! 
// 
// This is the same for all basic type that are supported in printf
// it was rude of me not to use a macro to generate a template here
// - 08/08/25
#define FMT_VALUE_IMPL_PRIMITIVE(TYPE, FORMAT)                                                  \
template<>                                                                                      \
void fmt_value(DynamicArray<u8> *bytes, TYPE value) {                                           \
    i64 required_bytes = snprintf(NULL, 0, FORMAT, value);                                      \
    Slice<u8> reserved_space = push_many(bytes, required_bytes + 1);                            \
    i64 written = snprintf((char *) reserved_space.ptr, reserved_space.len, FORMAT, value);     \
    ASSERT(written == required_bytes);                                                          \
}

template<typename T>
void fmt_value(DynamicArray<u8> *bytes, T value);

FMT_VALUE_IMPL_PRIMITIVE(const char *, "%s")
FMT_VALUE_IMPL_PRIMITIVE(i64, "%lld")
FMT_VALUE_IMPL_PRIMITIVE(i32, "%d")
FMT_VALUE_IMPL_PRIMITIVE(i16, "%hd")
FMT_VALUE_IMPL_PRIMITIVE(i8, "%hhd")
FMT_VALUE_IMPL_PRIMITIVE(u64, "%llu")
FMT_VALUE_IMPL_PRIMITIVE(u32, "%u")
FMT_VALUE_IMPL_PRIMITIVE(u16, "%hu")
FMT_VALUE_IMPL_PRIMITIVE(u8, "%hhu")

template<>
void fmt_value(DynamicArray<u8> *bytes, bool value) {
    if (value) {
        append_many<u8>(bytes, "true");
    }
    else {
        append_many<u8>(bytes, "false");
    }
}

template<typename T>
void fmt_arg(DynamicArray<u8> *bytes, str format, i64 &index, T arg) {
    while(index < format.len) {
        u8 byte = format[index];

        if (byte == '{' && index + 1 < format.len && format[index + 1] == '}') {
            fmt_value(bytes, arg);
            index += 2;
            break;
        }
        else {
            append(bytes, byte);
            index++;
        }
    }
}

template<typename... Args>
str fmt(Arena *arena, str format, Args... args) {
    i64 index = 0;
    DynamicArray<u8> bytes = dynamic_array_create<u8>(arena, format.len * 2);

    // unfolds as seperate statements for each arg 
    (fmt_arg(&bytes, format, index, args), ...);

    // write remaining bytes to buffer as there are no more args
    // to search for a {} pair in the format string
    if (index < format.len) {
        while(index < format.len) {
            append(&bytes, format[index]);
            index++;
        }
    }

    return bytes.slice.slice(0, bytes.len);
}

// @log
void logln(str s) {
    fwrite(s.ptr, 1, s.len, stdout);
    fwrite("\n", 1, 1, stdout);
}

f32 rand_f32();
f32 rand_f32_negative();
i64 rand_i64();
str read_entire_file(str path);

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

struct File {
    str path;
    FILE *handle;
};

File new_file(str path) {
    return File {
        .path = path,
        .handle = NULL,
    };
}

bool create_file(File *file) {
    file->handle = fopen(file->path.c(), "wb");
    if (file->handle == NULL) {
        return false;
    }

    return true;
}

Slice<u8> read_entire_file(File *file) {
    file->handle = fopen(file->path.c(), "rb");
    if (file->handle == NULL) {
        return {};
    }

    fseek(file->handle, 0, SEEK_END);
    i64 file_size = ftell(file->handle);
    fseek(file->handle, 0, SEEK_SET);

    
    Slice<u8> bytes = slice_create_malloc<u8>(file_size);
    fread(bytes.ptr, file_size, 1, file->handle);
    fclose(file->handle);

    file->handle = NULL;
    
    return bytes;
}

bool write_file(File *file, Slice<u8> bytes) {
    ASSERT(file->handle != NULL);

    i64 written = fwrite(bytes.ptr, 1, bytes.len, file->handle);

    if (written != bytes.len) {
        return false;
    }

    return true;
}

void close_file(File *file) {
    ASSERT(file->handle != NULL);

    fclose(file->handle);
    file->handle = NULL;
}

str read_entire_file(str path) {
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

    return slice_create(data, file_size);
}

#endif
