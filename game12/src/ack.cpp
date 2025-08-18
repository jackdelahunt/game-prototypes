#ifndef ACK_CPP
#define ACK_CPP

#include <cstring>
#include <mutex>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>

#define BREAKPOINT __debugbreak()
#define ASSERT(x) if (!(x)) __debugbreak();
#define BIT_SET(a, b) ((a & b) != 0)
#define SET_BIT(a, b) (a) |= (b)
#define UNSET_BIT(a, b) (a) &= ~(b) 
#define SCOPE }switch(0){default:

#define KB(x) ((x) * 1024)
#define MB(x) ((x) * 1024 * 1024)
#define GB(x) ((x) * 1024 * 1024 * 1024)

#define RESET_ASCII_CODE         "\033[0m"

// Regular colors
#define BLACK_ASCII_CODE         "\033[30m"
#define RED_ASCII_CODE           "\033[31m"
#define GREEN_ASCII_CODE         "\033[32m"
#define YELLOW_ASCII_CODE        "\033[33m"
#define BLUE_ASCII_CODE          "\033[34m"
#define MAGENTA_ASCII_CODE       "\033[35m"
#define CYAN_ASCII_CODE          "\033[36m"
#define WHITE_ASCII_CODE         "\033[37m"

// Bright colors
#define BRIGHT_BLACK_ASCII_CODE  "\033[90m"
#define BRIGHT_RED_ASCII_CODE    "\033[91m"
#define BRIGHT_GREEN_ASCII_CODE  "\033[92m"
#define BRIGHT_YELLOW_ASCII_CODE "\033[93m"
#define BRIGHT_BLUE_ASCII_CODE   "\033[94m"
#define BRIGHT_MAGENTA_ASCII_CODE "\033[95m"
#define BRIGHT_CYAN_ASCII_CODE   "\033[96m"
#define BRIGHT_WHITE_ASCII_CODE  "\033[97m"

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

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::steady_clock::time_point;

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

// @arena
struct Arena {
    i64 end;
    Slice<u8> bytes;
};

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

struct LogOptions {
    const char *thread_name;
    const char *thread_colour;
};

// @timer
struct Timer {
    std::chrono::steady_clock::time_point start_time;
    std::chrono::milliseconds time_limit; 
};

// @sampler
#define SAMPLER_SIZE 100
struct Sampler {
    f32         samples[SAMPLER_SIZE];
    TimePoint   times[SAMPLER_SIZE];
};

// @atomicsnapshot
template <typename T>
struct AtomicSnapshot {
    T buffers[2];
    std::atomic<T*> read_ptr;
    T* write_ptr;
};

// @file
struct File {
    str path;
    FILE *handle;
};

template <typename T>   Slice<T> slice_create(T *data, i64 len);
template <typename T>   Slice<T> slice_create_malloc(i64 len);
template <typename T>   void slice_free(Slice<T> slice);
template <typename T>   Slice<u8> slice_to_bytes(Slice<T> slice);
template <typename T>   Slice<T> slice_from_bytes(Slice<u8> slice);
template <typename T>   T *bytes_to_ptr(Slice<u8> slice);
template <typename T>   Slice<u8> bytes_from_ptr(T *ptr);
template <typename T>   void slice_copy(Slice<T> dst, Slice<T> src);
template <typename T>   void slice_copy_raw_ptr(Slice<T> slice, void *ptr);

template <typename T>   FixedArray<T> fixed_array_create(i64 size);
template <typename T>   void append(FixedArray<T> *array, T value);
template <typename T>   T* push(FixedArray<T> *array);
template <typename T>   void reset(FixedArray<T> *array);
template <typename T>   void swap_remove(FixedArray<T> *array, i64 index);

template <typename T, i64 N>    StackArray<T, N> stack_array_create();
template <typename T, i64 N>    void append(StackArray<T, N> *array, T value);
template <typename T, i64 N>    T* push(StackArray<T, N> *array);
template <typename T, i64 N>    void reset(StackArray<T, N> *array);
template <typename T, i64 N>    void swap_remove(StackArray<T, N> *array, i64 index);

Arena arena_create(i64 size);
void arena_destroy(Arena *arena);
void arena_reset(Arena *arena);
template <typename T>   T *arena_alloc(Arena *arena);
template <typename T>   Slice<T> arena_alloc_many(Arena *arena, i64 size);
template <typename T>   Slice<T> arena_realloc(Arena *arena, Slice<T> old_slice, i64 new_size);
template <typename T>   DynamicArray<T> dynamic_array_create(Arena *arena, i64 capacity); 
template <typename T>   void dynamic_array_maybe_grow(DynamicArray<T> *array, i64 required_slots); 
template <typename T>   void append(DynamicArray<T> *array, T value); 
template <typename T>   void append_many(DynamicArray<T> *array, Slice<T> values); 
template <typename T>   Slice<T> push_many(DynamicArray<T> *array, i64 count); 

template<typename... Args>  str fmt(Arena *arena, str format, Args... args);
template<typename T>        void fmt_arg(DynamicArray<u8> *bytes, str format, i64 &index, T arg); 
template<typename T>        void fmt_value(DynamicArray<u8> *bytes, T value);
template<>                  void fmt_value(DynamicArray<u8> *bytes, bool value);
template<>                  void fmt_value(DynamicArray<u8> *bytes, str value);

void log(str s);
void log_set_thread_options(LogOptions options);
void log_thread_name();
template<typename... Args>  void logf(str format, Args... args);

Timer timer_create_ms(i64 milliseconds); 
bool timer_is_complete_reset(Timer *timer); 
bool timer_is_complete(Timer *timer, f32 *delta_time); 

Sampler sampler_create(); 
void sampler_append(Sampler *sampler, f32 sample); 
f32 sampler_average(Sampler *sampler); 
f32 sampler_seconds_per_sample(Sampler *sampler); 
f32 sampler_samples_per_second(Sampler *sampler); 

template <typename T>   void atomic_snapshot_init(AtomicSnapshot<T> *snapshot); 
template <typename T>   T *atomic_snapshot_write(AtomicSnapshot<T> *snapshot); 
template <typename T>   T *atomic_snapshot_read(AtomicSnapshot<T> *snapshot); 
template <typename T>   void atomic_snapshot_swap(AtomicSnapshot<T> *snapshot);

f32 rand_f32();
f32 rand_f32_negative();
i64 rand_i64();
i64 rand_i64(i64 min, i64 max);

str read_entire_file(str path);
File new_file(str path); 
bool create_file(File *file); 
Slice<u8> read_entire_file(File *file); 
bool write_file(File *file, Slice<u8> bytes); 
void close_file(File *file); 
str read_entire_file(str path); 

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
T *bytes_to_ptr(Slice<u8> slice) {
    ASSERT(slice.len == sizeof(T));

    return (T *) slice.ptr;
}

template <typename T>
Slice<u8> bytes_from_ptr(T *ptr) {
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

template <typename T>
FixedArray<T> fixed_array_create(i64 size) {
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

template <typename T, i64 N>
StackArray<T, N> stack_array_create() {
    auto sa = StackArray<T, N> {
        .data = {},
        .size = N,
        .len = 0
    };

    // all zeros
    memset(sa.data, 0, N);

    return sa;
}

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

FMT_VALUE_IMPL_PRIMITIVE(void *, "%p")
FMT_VALUE_IMPL_PRIMITIVE(const char *, "%s")
FMT_VALUE_IMPL_PRIMITIVE(char *, "%s")

FMT_VALUE_IMPL_PRIMITIVE(i64, "%lld")
FMT_VALUE_IMPL_PRIMITIVE(i32, "%d")
FMT_VALUE_IMPL_PRIMITIVE(i16, "%hd")
FMT_VALUE_IMPL_PRIMITIVE(i8, "%hhd")

FMT_VALUE_IMPL_PRIMITIVE(u64, "%llu")
FMT_VALUE_IMPL_PRIMITIVE(u32, "%u")
FMT_VALUE_IMPL_PRIMITIVE(u16, "%hu")
FMT_VALUE_IMPL_PRIMITIVE(u8, "%hhu")

FMT_VALUE_IMPL_PRIMITIVE(f32, "%f")
FMT_VALUE_IMPL_PRIMITIVE(f64, "%f")

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

template<>
void fmt_value(DynamicArray<u8> *bytes, bool value) {
    if (value) {
        append_many<u8>(bytes, "true");
    }
    else {
        append_many<u8>(bytes, "false");
    }
}

template<> 
void fmt_value(DynamicArray<u8> *bytes, str value) {
    append_many(bytes, value);
}

// @log
thread_local LogOptions tl_options = LogOptions {
    .thread_name = NULL,
    .thread_colour = NULL,
};

std::mutex log_mutex;

void log(str s) {
    log_mutex.lock();

    if (tl_options.thread_colour != NULL) {
        printf("%s", tl_options.thread_colour);
    }

    log_thread_name();

    printf(RESET_ASCII_CODE);

    fwrite(s.ptr, 1, s.len, stdout);
    fwrite("\n", 1, 1, stdout);

    log_mutex.unlock();
}

void log_set_thread_options(LogOptions options) {
    tl_options = options;
}

void log_thread_name() {
    const char *name = "?";

    if (tl_options.thread_name != NULL) {
        name = tl_options.thread_name;
    }

    printf("[%s] ", name); 
}

template<typename... Args>
void logf(str format, Args... args) {
    Arena scratch = arena_create(KB(1));

    str s = fmt(&scratch, format, args...);
    log(s);

    arena_destroy(&scratch);
}

Timer timer_create_ms(i64 milliseconds) {
    return Timer {
        .start_time = std::chrono::steady_clock::now(),
        .time_limit = std::chrono::milliseconds(milliseconds)
    };
}

bool timer_is_complete_reset(Timer *timer) {
    auto now = std::chrono::steady_clock::now();

    if (now - timer->start_time >= timer->time_limit) {
        timer->start_time = now;
        return true; 
    }

    return false;
}

bool timer_is_complete(Timer *timer, f32 *delta_time) {
    auto now = std::chrono::steady_clock::now();
    auto duration = now - timer->start_time;


    if (duration >= timer->time_limit) {
        timer->start_time = now;
        *delta_time = std::chrono::duration<f32>(duration).count();
        return true; 
    }

    *delta_time = 0;
    return false;
}

Sampler sampler_create() {
    return Sampler {};
}

void sampler_append(Sampler *sampler, f32 sample) {
    TimePoint time = Clock::now();

    // shift all samples back one space dropping the first 
    for (i64 i = 1; i < SAMPLER_SIZE; i++) {
        sampler->samples[i - 1] = sampler->samples[i];
        sampler->times[i - 1] = sampler->times[i];
    }

    // set last same to new one
    sampler->samples[SAMPLER_SIZE - 1] = sample;
    sampler->times[SAMPLER_SIZE - 1] = time;
}

f32 sampler_average(Sampler *sampler) {
    f32 total = 0;

    for (i64 i = 0; i < SAMPLER_SIZE; i++) {
        total += sampler->samples[i];
    }

    return total / f32(SAMPLER_SIZE);
}

f32 sampler_seconds_per_sample(Sampler *sampler) {
    TimePoint start = sampler->times[0];
    TimePoint end = sampler->times[SAMPLER_SIZE - 1];

    f32 delta_time = std::chrono::duration<f32>(end - start).count();
    return delta_time / f32(SAMPLER_SIZE);
}

f32 sampler_samples_per_second(Sampler *sampler) {
    return 1.0f / sampler_seconds_per_sample(sampler);
}

template <typename T>
void atomic_snapshot_init(AtomicSnapshot<T> *snapshot) {
    snapshot->buffers[0] = T{};
    snapshot->buffers[1] = T{};
    snapshot->read_ptr.store(&snapshot->buffers[0], std::memory_order_relaxed);
    snapshot->write_ptr = &snapshot->buffers[1];
}

template <typename T>
T *atomic_snapshot_write(AtomicSnapshot<T> *snapshot) {
    return snapshot->write_ptr;
}

template <typename T>
T *atomic_snapshot_read(AtomicSnapshot<T> *snapshot) {
    return snapshot->read_ptr.load(std::memory_order_acquire);
}

template <typename T>
void atomic_snapshot_swap(AtomicSnapshot<T> *snapshot) {
    T* old_read = snapshot->read_ptr.exchange(snapshot->write_ptr, std::memory_order_acq_rel);
    snapshot->write_ptr = old_read;
}

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

// min -> max - 1
i64 rand_i64(i64 min, i64 max) {
    return min + (i64)(rand() % (max - min));
}

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
