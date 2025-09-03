#ifndef ACK_CPP
#define ACK_CPP

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>
#include <mutex>

#ifndef WINDOWS
    #error only can build for windows right now sorry!
#endif

#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

#define Log(s)          _log (BrightCyanAsciiCode,  "Log:",   __FILENAME__, __LINE__, s)
#define Logf(f, ...)    _logf(BrightCyanAsciiCode,  "Log:",   __FILENAME__, __LINE__, f, __VA_ARGS__)
#define Info(s)         _log (BrightGreenAsciiCode, "Info:",  __FILENAME__, __LINE__, s)
#define Infof(f, ...)   _logf(BrightGreenAsciiCode, "Info:",  __FILENAME__, __LINE__, f, __VA_ARGS__)
#define Warn(s)         _log (YellowAsciiCode,      "Warn:",  __FILENAME__, __LINE__, s)
#define Warnf(f, ...)   _logf(YellowAsciiCode,      "Warn:",  __FILENAME__, __LINE__, f, __VA_ARGS__)
#define Err(s)          _log (BrightRedAsciiCode,   "Error:", __FILENAME__, __LINE__, s)
#define Errf(f, ...)    _logf(BrightRedAsciiCode,   "Error:", __FILENAME__, __LINE__, f, __VA_ARGS__)
#define Fatal(s)        _log (RedAsciiCode,         "Fatal:", __FILENAME__, __LINE__, s)
#define Fatalf(f, ...)  _logf(RedAsciiCode,         "Fatal:", __FILENAME__, __LINE__, f, __VA_ARGS__)

#ifdef ENABLE_ASSERTS
    #define Breakpoint            __debugbreak()
    #define Unreachable(s)        Fatalf("Unreachable: {}", (s)); Breakpoint
    #define Assert(condition)     if (!(condition)) { Fatalf("Triggered Assert({})", #condition);          Breakpoint; }
    #define Assertf(condition, s) if (!(condition)) { Fatalf("Triggered Assert({}): {}", #condition, (s)); Breakpoint; }
#else
    #define Breakpoint
    #define Unreachable(s)
    #define Assert(condition)
    #define Assertf(condition, s)
#endif

#define BitSet(a, b) ((a & b) != 0)
#define SetBit(a, b) (a) |= (b)
#define UnsetBit(a, b) (a) &= ~(b)

#define MemZero(buffer, size) memset((buffer), 0, (size))

#define Scope }switch(0){default:
#define ScopeBreak break 

// literal in bytes 
#define KB(x) ((x)   * 1024)
#define MB(x) (KB(x) * 1024)
#define GB(x) (MB(x) * 1024)
#define TB(x) (GB(x) * 1024)

// literal in seconds
#define Second(x) (x)
#define Minute(x) ((x)       * 60)
#define Hour(x)   (Minute(x) * 60)
#define Day(x)    (Hour(x)   * 24)

#define ResetAsciiCode         "\033[0m"

#define BlackAsciiCode         "\033[30m"
#define RedAsciiCode           "\033[31m"
#define GreenAsciiCode         "\033[32m"
#define YellowAsciiCode        "\033[33m"
#define BlueAsciiCode          "\033[34m"
#define MagentaAsciiCode       "\033[35m"
#define CyanAsciiCode          "\033[36m"
#define WhiteAsciiCode         "\033[37m"

#define BrightBlackAsciiCode   "\033[90m"
#define BrightRedAsciiCode     "\033[91m"
#define BrightGreenAsciiCode   "\033[92m"
#define BrightYellowAsciiCode  "\033[93m"
#define BrightBlueAsciiCode    "\033[94m"
#define BrightMagentaAsciiCode "\033[95m"
#define BrightCyanAsciiCode    "\033[96m"
#define BrightWhiteAsciiCode   "\033[97m"

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
struct slice { 
    T *ptr;
    i64 len;

    slice();
    slice(T *data, i64 len);
    slice(const char *c_string);

    T& operator[](i64 index);
    T* begin();
    T* end();

    const char *c();
};

typedef slice<u8> string;

// @array
template <typename T, i64 N>
struct array {
    T items[N];
    i64 len = N;

    T& operator[](i64 index);
    T* begin();
    T* end();
};

// @arena
struct Arena {
    i64 end;
    slice<u8> bytes;
};

// @fixedarray
template <typename T>
struct FixedArray {
    slice<T> slice;
    i64 len;

    T& operator[](i64 index);
    T* begin();
    T* end();
};

// @stackarray
template <typename T, i64 N>
struct StackArray {
    T data[N];
    i64 size = N;
    i64 len;

    T& operator[](i64 index);
    T* begin();
    T* end();
};

// @dynamicarray
template <typename T>
struct DynamicArray {
    Arena *arena;
    slice<T> slice;
    i64 len;
    i64 capacity;

    T& operator[](i64 index);
    T* begin();
    T* end();
};

// @timer
struct Timer {
    std::chrono::steady_clock::time_point start_time;
    std::chrono::milliseconds time_limit; 
};

// @stopwatch
struct Stopwatch {
    std::chrono::steady_clock::time_point last_time;
};

// @sampler
#define SAMPLER_SIZE 150
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
    string path;
    FILE *handle;
};

// @slice
template <typename T>   slice<T> slice_create(T *data, i64 len);
template <typename T>   slice<T> slice_range(slice<T> s, i64 start, i64 end);
template <typename T>   slice<T> slice_from(slice<T> s, i64 start);
template <typename T>   slice<T> slice_create_malloc(i64 len);
template <typename T>   void slice_free(slice<T> slice);
template <typename T>   slice<u8> slice_to_bytes(slice<T> slice);
template <typename T>   slice<T> slice_from_bytes(slice<u8> slice);
template <typename T>   T *bytes_to_ptr(slice<u8> slice);
template <typename T>   slice<u8> bytes_from_ptr(T *ptr);
template <typename T>   void slice_copy(slice<T> dst, slice<T> src);
template <typename T>   void slice_copy_raw_ptr(slice<T> slice, void *ptr);
template <typename T>   bool slice_memcmp(slice<T> a, slice<T> b);

// @arena
Arena arena_create(i64 size);
void arena_destroy(Arena *arena);
void arena_reset(Arena *arena);
template <typename T>   T *arena_alloc(Arena *arena);
template <typename T>   slice<T> arena_alloc_many(Arena *arena, i64 size);
template <typename T>   slice<T> arena_realloc(Arena *arena, slice<T> old_slice, i64 new_size);

// @fixedarray
template <typename T>   FixedArray<T> fixed_array_create(i64 size);
template <typename T>   void append(FixedArray<T> *array, T value);
template <typename T>   T* push(FixedArray<T> *array);
template <typename T>   void reset(FixedArray<T> *array);
template <typename T>   void swap_remove(FixedArray<T> *array, i64 index);

// @stackarray
template <typename T, i64 N>    StackArray<T, N> stack_array_create();
template <typename T, i64 N>    void append(StackArray<T, N> *array, T value);
template <typename T, i64 N>    T* push(StackArray<T, N> *array);
template <typename T, i64 N>    void reset(StackArray<T, N> *array);
template <typename T, i64 N>    void swap_remove(StackArray<T, N> *array, i64 index);

// @dynamicarray
template <typename T>   DynamicArray<T> dynamic_array_create(Arena *arena, i64 capacity); 
template <typename T>   void dynamic_array_maybe_grow(DynamicArray<T> *array, i64 required_slots); 
template <typename T>   void append(DynamicArray<T> *array, T value); 
template <typename T>   void append_many(DynamicArray<T> *array, slice<T> values); 
template <typename T>   slice<T> push_many(DynamicArray<T> *array, i64 count);
template <typename T>   slice<T> to_slice(DynamicArray<T> *array); 

// @timer
Timer timer_create_ms(i64 milliseconds); 
bool timer_is_complete_reset(Timer *timer); 
bool timer_is_complete(Timer *timer, f32 *delta_time);

// @stopwatch
Stopwatch stopwatch_create();
f32 stopwatch_get_time(Stopwatch *stopwatch);

// @sampler
Sampler sampler_create(); 
void sampler_append(Sampler *sampler, f32 sample); 
f32 sampler_average(Sampler *sampler); 
f32 sampler_seconds_per_sample(Sampler *sampler); 
f32 sampler_samples_per_second(Sampler *sampler); 

// @atomicsnapshot
template <typename T>   void atomic_snapshot_init(AtomicSnapshot<T> *snapshot); 
template <typename T>   T *atomic_snapshot_write(AtomicSnapshot<T> *snapshot); 
template <typename T>   T *atomic_snapshot_read(AtomicSnapshot<T> *snapshot); 
template <typename T>   void atomic_snapshot_swap(AtomicSnapshot<T> *snapshot);
template <typename T>   void atomic_snapshot_copy_and_swap(AtomicSnapshot<T> *snapshot, T *value);

// @file
string read_entire_file(string path);
File new_file(string path); 
bool create_file(File *file); 
slice<u8> read_entire_file(File *file); 
bool write_file(File *file, slice<u8> bytes); 
void close_file(File *file); 
string read_entire_file(string path); 

// @fmt
template<typename... Args>  string fmt(Arena *arena, string format, Args... args);
template<typename T>        void fmt_arg(DynamicArray<u8> *bytes, string format, i64 &index, T arg); 
template<typename T>        void fmt_value(DynamicArray<u8> *bytes, T value);
template<>                  void fmt_value(DynamicArray<u8> *bytes, i64 value);
template<>                  void fmt_value(DynamicArray<u8> *bytes, i32 value);
template<>                  void fmt_value(DynamicArray<u8> *bytes, i16 value);
template<>                  void fmt_value(DynamicArray<u8> *bytes, i8 value);
template<>                  void fmt_value(DynamicArray<u8> *bytes, u64 value);
template<>                  void fmt_value(DynamicArray<u8> *bytes, u32 value);
template<>                  void fmt_value(DynamicArray<u8> *bytes, u16 value);
template<>                  void fmt_value(DynamicArray<u8> *bytes, u8 value);
template<>                  void fmt_value(DynamicArray<u8> *bytes, f64 value);
template<>                  void fmt_value(DynamicArray<u8> *bytes, f32 value);
template<>                  void fmt_value(DynamicArray<u8> *bytes, bool value);
template<>                  void fmt_value(DynamicArray<u8> *bytes, string value);
template<>                  void fmt_value(DynamicArray<u8> *bytes, void *value);
template<>                  void fmt_value(DynamicArray<u8> *bytes, const char *value);
template<>                  void fmt_value(DynamicArray<u8> *bytes, char *value);

// @log
template<typename... Args>
void _logf(const char *colour, const char *label, const char *file, i32 line, string format, Args... args);
void _log(const char *colour, const char *label, const char *file, i32 line, string s);
void log_set_options(bool print_label, bool print_location);
void log_set_thread_name(const char *name);

// @rand
f32 rand_f32();
f32 rand_f32_negative();
i64 rand_i64();
i64 rand_i64(i64 min, i64 max);

template <typename T>
slice<T>::slice() {
    this->ptr = NULL;
    this->len = 0;
}

template <typename T>
slice<T>::slice(T *data, i64 len) { // C++ sucks
    this->ptr = data;
    this->len = len;
}

template <typename T>
slice<T>::slice(const char *c_string) {
    this->ptr = (T *) c_string;
    this->len = strlen(c_string);
}

template <typename T>
T& slice<T>::operator[](i64 index) {
    Assert(index < this->len);

    return this->ptr[index];
}

template <typename T>
T* slice<T>::begin() {
    return ptr;
}

template <typename T>
T* slice<T>::end() {
    return ptr + len;
}

template <typename T>
const char * slice<T>::c() {
    return (const char *) this->ptr;
}

template <typename T>
slice<T> slice_create(T *data, i64 len) {
    return slice<T>(data, len);
}

template <typename T>
slice<T> slice_range(slice<T> s, i64 start, i64 end) {
    Assert(start >= 0 && start < s.len);
    Assert(end >= 0 && end <= s.len);

    if (start >= end) {
        return slice<T>(NULL, 0);
    }

    return slice<T>(s.ptr + start, end - start);
}

template <typename T>
slice<T> slice_from(slice<T> s, i64 start) {
    return slice<T>(s.ptr + start, s.len - start);
}

template <typename T>
slice<T> slice_create_malloc(i64 len) {
    i64 bytes = len * sizeof(T);

    T *ptr = (T *) malloc(bytes);
    memset(ptr, 0, bytes);

    return slice_create(ptr, len);
}

template <typename T>
void slice_free(slice<T> slice) {
    free(slice.ptr);
}

template <typename T>
slice<u8> slice_to_bytes(slice<T> slice) {
    return slice_create((u8 *) slice.ptr, sizeof(T) * slice.len);
}

template <typename T>
slice<T> slice_from_bytes(slice<u8> slice) {
    return slice_create((T *) slice.ptr, slice.len / sizeof(T));
}

template <typename T>
T *bytes_to_ptr(slice<u8> slice) {
    Assert(slice.len == sizeof(T));

    return (T *) slice.ptr;
}

template <typename T>
slice<u8> bytes_from_ptr(T *ptr) {
    return slice_create((u8 *) ptr, sizeof(T));
}

template <typename T>
void slice_copy(slice<T> dst, slice<T> src) {
    Assert(dst.ptr && src.ptr);
    Assert(src.len <= dst.len);

    for (i64 i = 0; i < src.len; i++) {
        dst[i] = src[i];
    }
}

// copy memory from ptr into slice for the entire
// length of the slice, assumes data at ptr is valid
// for the length of the slice and the slice has
// valid memory to copy to
template <typename T>
void slice_copy_raw_ptr(slice<T> slice, void *ptr) {
    Assert(slice.ptr && ptr);

    for (i64 i = 0; i < slice.len; i++) {
        slice.ptr[i] = ((T *) ptr)[i];
    }
}

template <typename T>
bool slice_memcmp(slice<T> a, slice<T> b) {
    if (a.len != b.len) {
        return false;
    }

    return memcmp(a.ptr, b.ptr, a.len) == 0;
}

template <typename T, i64 N>
T& array<T, N>::operator[](i64 index) {
    Assert(index >= 0 && index < N);

    return this->items[index];
}

template <typename T, i64 N>
T* array<T, N>::begin() {
    return this->items;
}

template <typename T, i64 N>
T* array<T, N>::end() {
    return this->items + N;
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

    Assert(arena->end + SIZE <= arena->bytes.len);

    T *ptr = (T *) &arena->bytes[arena->end];
    arena->end += SIZE;

    return ptr;
}

template <typename T>
slice<T> arena_alloc_many(Arena *arena, i64 size) {
    i64 byte_count = sizeof(T) * size;

    Assertf(arena->end + byte_count <= arena->bytes.len, "Arena ran out of memory");

    slice<u8> bytes = slice_range(arena->bytes, arena->end, arena->end + byte_count);
    arena->end += byte_count;

    return slice_create((T *) bytes.ptr, size);
}

template <typename T>
slice<T> arena_realloc(Arena *arena, slice<T> old_slice, i64 new_size) {
    // right now I just always reallocate, could check if this was the
    // last allocation and just extend the length of the slice - 08/08/25

    slice<T> new_slice = arena_alloc_many<T>(arena, new_size);
    slice_copy(new_slice, old_slice);
    return new_slice;
}

template <typename T>
T& FixedArray<T>::operator[](i64 index) {
    return this->slice[index];
}

template <typename T>
T* FixedArray<T>::begin() {
    return &slice[0];
}

template <typename T>
T* FixedArray<T>::end() {
    return &slice[len];
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
    Assert(array->len < array->slice.len);

    array->slice[array->len] = value;
    array->len += 1;
}

template <typename T>
T* push(FixedArray<T> *array) {
    Assert(array->len < array->slice.len);

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
    Assert(index < array->len);

    array->slice[index] = array->slice[array->len - 1];
    array->len -= 1;
}


template <typename T, i64 N>
T& StackArray<T, N>::operator[](i64 index) {
    return this->data[index];
}

template <typename T, i64 N>
T* StackArray<T, N>::begin() {
    return data;
}

template <typename T, i64 N>
T* StackArray<T, N>::end() {
    return data + len;
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
    Assert(array->len < N);

    array->data[array->len] = value;
    array->len += 1;
}

template <typename T, i64 N>
T* push(StackArray<T, N> *array) {
    Assert(array->len < N);

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
    Assert(index < array->len);

    array->data[index] = array->data[array->len - 1];
    array->len -= 1;
}

template <typename T>
T& DynamicArray<T>::operator[](i64 index) {
    return this->slice[index];
}

template <typename T>
T* DynamicArray<T>::begin() {
    return &slice[0];
}

template <typename T>
T* DynamicArray<T>::end() {
    return &slice[len];
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
void append_many(DynamicArray<T> *array, slice<T> values) {
    if (values.len <= 0) {
        return;
    }

    slice<T> sub_slice = push_many(array, values.len);
    slice_copy(sub_slice, values);
}

// appends 'count' number of items to the array and then returns a slice
// to the new items from the end of the array. The slice returned is 'count'
// number to items long
template <typename T>
slice<T> push_many(DynamicArray<T> *array, i64 count) {
    dynamic_array_maybe_grow(array, count);

    slice<T> s = slice_range(array->slice, array->len, array->len + count);
    array->len += count;

    return s;
}

template <typename T>
slice<T> to_slice(DynamicArray<T> *array) {
    return slice_range(array->slice, 0, array->len);
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

    *delta_time = std::chrono::duration<f32>(duration).count();

    if (duration >= timer->time_limit) {
        timer->start_time = now;
        return true; 
    }

    return false;
}

Stopwatch stopwatch_create() {
    return Stopwatch {
        .last_time = std::chrono::steady_clock::now(),
    };
}

f32 stopwatch_get_time(Stopwatch *stopwatch) {
    auto now = std::chrono::steady_clock::now();
    auto duration = now - stopwatch->last_time;

    stopwatch->last_time = now;

    return std::chrono::duration<f32>(duration).count();
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

template <typename T>   
void atomic_snapshot_copy_and_swap(AtomicSnapshot<T> *snapshot, T *value) {
    T *write_ptr = atomic_snapshot_write(snapshot);
    memcpy(write_ptr, value, sizeof(T));
    atomic_snapshot_swap(snapshot);
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

File new_file(string path) {
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

slice<u8> read_entire_file(File *file) {
    file->handle = fopen(file->path.c(), "rb");
    if (file->handle == NULL) {
        return {};
    }

    fseek(file->handle, 0, SEEK_END);
    i64 file_size = ftell(file->handle);
    fseek(file->handle, 0, SEEK_SET);

    
    slice<u8> bytes = slice_create_malloc<u8>(file_size);
    fread(bytes.ptr, file_size, 1, file->handle);
    fclose(file->handle);

    file->handle = NULL;
    
    return bytes;
}

bool write_file(File *file, slice<u8> bytes) {
    Assert(file->handle != NULL);

    i64 written = fwrite(bytes.ptr, 1, bytes.len, file->handle);

    if (written != bytes.len) {
        return false;
    }

    return true;
}

void close_file(File *file) {
    Assert(file->handle != NULL);

    fclose(file->handle);
    file->handle = NULL;
}

string read_entire_file(string path) {
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

// @fmt
// All primitive types i.e. i32, u16... are first formatted into a fixed buffer using
// snprintf and that is then copied into the string. This is because libc is annoying
// and it requires writing a \0 after snprint. This is just annoying as you need to have
// space for it to write it but also dont want to actually reserve because then you
// just have a random \0 in the middle of your string - 23/08/25
#define FMT_PRIMITIVE_BUFFER_SIZE 512
char g_fmt_primitive_buffer[FMT_PRIMITIVE_BUFFER_SIZE] = {};

template<typename... Args>
string fmt(Arena *arena, string format, Args... args) {
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

    return slice_range(bytes.slice, 0, bytes.len);
}

template<typename T>
void fmt_arg(DynamicArray<u8> *bytes, string format, i64 &index, T arg) {
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
void fmt_value(DynamicArray<u8> *bytes, i64 value) {
    i64 written = snprintf((char *) g_fmt_primitive_buffer, FMT_PRIMITIVE_BUFFER_SIZE, "%lld", value);
    append_many(bytes, slice_create((u8 *) g_fmt_primitive_buffer, written));
    MemZero(g_fmt_primitive_buffer, FMT_PRIMITIVE_BUFFER_SIZE);
}

template<>                                                                                      
void fmt_value(DynamicArray<u8> *bytes, i32 value) {
    i64 written = snprintf((char *) g_fmt_primitive_buffer, FMT_PRIMITIVE_BUFFER_SIZE, "%d", value);
    append_many(bytes, slice_create((u8 *) g_fmt_primitive_buffer, written));
    MemZero(g_fmt_primitive_buffer, FMT_PRIMITIVE_BUFFER_SIZE);
}

template<>                                                                                      
void fmt_value(DynamicArray<u8> *bytes, i16 value) {
    i64 written = snprintf((char *) g_fmt_primitive_buffer, FMT_PRIMITIVE_BUFFER_SIZE, "%hd", value);
    append_many(bytes, slice_create((u8 *) g_fmt_primitive_buffer, written));
    MemZero(g_fmt_primitive_buffer, FMT_PRIMITIVE_BUFFER_SIZE);
}

template<>                                                                                      
void fmt_value(DynamicArray<u8> *bytes, i8 value) {
    i64 written = snprintf((char *) g_fmt_primitive_buffer, FMT_PRIMITIVE_BUFFER_SIZE, "%hhd", value);
    append_many(bytes, slice_create((u8 *) g_fmt_primitive_buffer, written));
    MemZero(g_fmt_primitive_buffer, FMT_PRIMITIVE_BUFFER_SIZE);
}

template<>                                                                                      
void fmt_value(DynamicArray<u8> *bytes, u64 value) {
    i64 written = snprintf((char *) g_fmt_primitive_buffer, FMT_PRIMITIVE_BUFFER_SIZE, "%llu", value);
    append_many(bytes, slice_create((u8 *) g_fmt_primitive_buffer, written));
    MemZero(g_fmt_primitive_buffer, FMT_PRIMITIVE_BUFFER_SIZE);
}

template<>                                                                                      
void fmt_value(DynamicArray<u8> *bytes, u32 value) {
    i64 written = snprintf((char *) g_fmt_primitive_buffer, FMT_PRIMITIVE_BUFFER_SIZE, "%u", value);
    append_many(bytes, slice_create((u8 *) g_fmt_primitive_buffer, written));
    MemZero(g_fmt_primitive_buffer, FMT_PRIMITIVE_BUFFER_SIZE);
}


template<>                                                                                      
void fmt_value(DynamicArray<u8> *bytes, u16 value) {
    i64 written = snprintf((char *) g_fmt_primitive_buffer, FMT_PRIMITIVE_BUFFER_SIZE, "%hu", value);
    append_many(bytes, slice_create((u8 *) g_fmt_primitive_buffer, written));
    MemZero(g_fmt_primitive_buffer, FMT_PRIMITIVE_BUFFER_SIZE);
}

template<>
void fmt_value(DynamicArray<u8> *bytes, u8 value) {
    i64 written = snprintf((char *) g_fmt_primitive_buffer, FMT_PRIMITIVE_BUFFER_SIZE, "%hhu", value);
    append_many(bytes, slice_create((u8 *) g_fmt_primitive_buffer, written));
    MemZero(g_fmt_primitive_buffer, FMT_PRIMITIVE_BUFFER_SIZE);
}

template<>
void fmt_value(DynamicArray<u8> *bytes, f64 value) {
    i64 written = snprintf((char *) g_fmt_primitive_buffer, FMT_PRIMITIVE_BUFFER_SIZE, "%f", value);
    append_many(bytes, slice_create((u8 *) g_fmt_primitive_buffer, written));
    MemZero(g_fmt_primitive_buffer, FMT_PRIMITIVE_BUFFER_SIZE);
}

template<>
void fmt_value(DynamicArray<u8> *bytes, f32 value) {
    i64 written = snprintf((char *) g_fmt_primitive_buffer, FMT_PRIMITIVE_BUFFER_SIZE, "%f", value);
    append_many(bytes, slice_create((u8 *) g_fmt_primitive_buffer, written));
    MemZero(g_fmt_primitive_buffer, FMT_PRIMITIVE_BUFFER_SIZE);
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
void fmt_value(DynamicArray<u8> *bytes, string value) {
    append_many(bytes, value);
}

template<>                  
void fmt_value(DynamicArray<u8> *bytes, void *value) {
    fmt_value(bytes, (u64) value);
}

template<>                  
void fmt_value(DynamicArray<u8> *bytes, const char *value) {
    fmt_value(bytes, string(value));
}

template<>  
void fmt_value(DynamicArray<u8> *bytes, char *value) {
    append_many(bytes, string(value));
}

// @log
std::mutex g_log_mutex;
thread_local const char *tl_thread_name = NULL;
bool g_log_print_label = true;
bool g_log_print_location = true;

template<typename... Args>
void _logf(const char *colour, const char *label, const char *file, i32 line, string format, Args... args) {
    Arena scratch = arena_create(KB(1));

    string s = fmt(&scratch, format, args...);
    _log(colour, label, file, line, s);

    arena_destroy(&scratch);
}

void _log(const char *colour, const char *label, const char *file, i32 line, string s) {
    g_log_mutex.lock();

    printf("%s", colour);

    if (g_log_print_label) {
        printf("%-6s ", label);
    }

    if (g_log_print_location) {
        const char *name = tl_thread_name != NULL ? tl_thread_name : "?";
        printf("[%s] [%s:%d] ", name, file, line);
    }

    // actual message
    fwrite(s.ptr, 1, s.len, stdout);
    fwrite("\n", 1, 1, stdout);

    // reset colour
    printf("%s", ResetAsciiCode);

    g_log_mutex.unlock();
}

void log_set_options(bool print_label, bool print_location) {
    g_log_print_label = print_label;
    g_log_print_location = print_location;
}

void log_set_thread_name(const char *name) {
    tl_thread_name = name;
}

#endif
