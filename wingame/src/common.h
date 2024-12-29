#pragma once

// project wide macro definitions
// WINDOWS      - compiling for windows
// MACOS        - compiling for macos
// LINUX        - compiling for linux

// DEBUG        - compiling in debug mode

#define internal static

#define DLL_EXPORT __declspec(dllexport)

#define Func(ret, name, ...) ret (* name)(__VA_ARGS__);

typedef char             i8;
typedef short           i16;
typedef int             i32;
typedef int long long   i64;

typedef char unsigned            u8;
typedef short unsigned           u16;
typedef int unsigned             u32;
typedef int long long unsigned   u64;

typedef float  f32;
typedef double f64;
