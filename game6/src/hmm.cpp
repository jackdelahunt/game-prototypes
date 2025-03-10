/*
  HandmadeMath.h v2.0.0

  This is a single header file with a bunch of useful types and functions for
  games and graphics. Consider it a lightweight alternative to GLM that works
  both C and C++.

  =============================================================================
  CONFIG
  =============================================================================

  By default, all angles in Handmade Math are specified in radians. However, it
  can be configured to use degrees or turns instead. Use one of the following
  defines to specify the default unit for angles:

    #define HANDMADE_MATH_USE_RADIANS
    #define HANDMADE_MATH_USE_DEGREES
    #define HANDMADE_MATH_USE_TURNS

  Regardless of the default angle, you can use the following functions to
  specify an angle in a particular unit:

    HMM_AngleRad(radians)
    HMM_AngleDeg(degrees)
    HMM_AngleTurn(turns)

  The definitions of these functions change depending on the default unit.

  -----------------------------------------------------------------------------

  Handmade Math ships with SSE (SIMD) implementations of several common
  operations. To disable the use of SSE intrinsics, you must define
  HANDMADE_MATH_NO_SSE before including this file:

    #define HANDMADE_MATH_NO_SSE
    #include "HandmadeMath.h"

  -----------------------------------------------------------------------------

  To use Handmade Math without the C runtime library, you must provide your own
  implementations of basic math functions. Otherwise, HandmadeMath.h will use
  the runtime library implementation of these functions.

  Define HANDMADE_MATH_PROVIDE_MATH_FUNCTIONS and provide your own
  implementations of HMM_SINF, HMM_COSF, HMM_TANF, HMM_ACOSF, and HMM_SQRTF
  before including HandmadeMath.h, like so:

    #define HANDMADE_MATH_PROVIDE_MATH_FUNCTIONS
    #define HMM_SINF MySinF
    #define HMM_COSF MyCosF
    #define HMM_TANF MyTanF
    #define HMM_ACOSF MyACosF
    #define HMM_SQRTF MySqrtF
    #include "HandmadeMath.h"
  
  By default, it is assumed that your math functions take radians. To use
  different units, you must define HMM_ANGLE_USER_TO_INTERNAL and
  HMM_ANGLE_INTERNAL_TO_USER. For example, if you want to use degrees in your
  code but your math functions use turns:

    #define HMM_ANGLE_USER_TO_INTERNAL(a) ((a)*HMM_DegToTurn)
    #define HMM_ANGLE_INTERNAL_TO_USER(a) ((a)*HMM_TurnToDeg)

  =============================================================================
  
  LICENSE

  This software is in the public domain. Where that dedication is not
  recognized, you are granted a perpetual, irrevocable license to copy,
  distribute, and modify this file as you see fit.

  =============================================================================

  CREDITS

  Originally written by Zakary Strange.

  Functionality:
   Zakary Strange (strangezak@protonmail.com && @strangezak)
   Matt Mascarenhas (@miblo_)
   Aleph
   FieryDrake (@fierydrake)
   Gingerbill (@TheGingerBill)
   Ben Visness (@bvisness)
   Trinton Bullard (@Peliex_Dev)
   @AntonDan
   Logan Forman (@dev_dwarf)

  Fixes:
   Jeroen van Rijn (@J_vanRijn)
   Kiljacken (@Kiljacken)
   Insofaras (@insofaras)
   Daniel Gibson (@DanielGibson)
*/

#ifndef HANDMADE_MATH_H
#define HANDMADE_MATH_H

// Dummy macros for when test framework is not present.
#ifndef COVERAGE
# define COVERAGE(a, b)
#endif

#ifndef ASSERT_COVERED
# define ASSERT_COVERED(a)
#endif

/* let's figure out if SSE is really available (unless disabled anyway)
   (it isn't on non-x86/x86_64 platforms or even x86 without explicit SSE support)
   => only use "#ifdef HANDMADE_MATH__USE_SSE" to check for SSE support below this block! */
#ifndef HANDMADE_MATH_NO_SSE
# ifdef _MSC_VER /* MSVC supports SSE in amd64 mode or _M_IX86_FP >= 1 (2 means SSE2) */
#  if defined(_M_AMD64) || ( defined(_M_IX86_FP) && _M_IX86_FP >= 1 )
#   define HANDMADE_MATH__USE_SSE 1
#  endif
# else /* not MSVC, probably GCC, clang, icc or something that doesn't support SSE anyway */
#  ifdef __SSE__ /* they #define __SSE__ if it's supported */
#   define HANDMADE_MATH__USE_SSE 1
#  endif /*  __SSE__ */
# endif /* not _MSC_VER */
#endif /* #ifndef HANDMADE_MATH_NO_SSE */

#if (!defined(__cplusplus) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
# define HANDMADE_MATH__USE_C11_GENERICS 1
#endif

#ifdef HANDMADE_MATH__USE_SSE
# include <xmmintrin.h>
#endif

#ifdef _MSC_VER
#pragma warning(disable:4201)
#endif

#if defined(__GNUC__) || defined(__clang__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wfloat-equal"
# if (defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ < 8)) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wmissing-braces"
# endif
# ifdef __clang__
#  pragma GCC diagnostic ignored "-Wgnu-anonymous-struct"
#  pragma GCC diagnostic ignored "-Wmissing-field-initializers"
# endif
#endif

#if defined(__GNUC__) || defined(__clang__)
# define HMM_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
# define HMM_DEPRECATED(msg) __declspec(deprecated(msg))
#else
# define HMM_DEPRECATED(msg)
#endif

#if !defined(HANDMADE_MATH_USE_DEGREES) \
    && !defined(HANDMADE_MATH_USE_TURNS) \
    && !defined(HANDMADE_MATH_USE_RADIANS)
# define HANDMADE_MATH_USE_RADIANS
#endif
    
#define HMM_PI 3.14159265358979323846
#define HMM_PI32 3.14159265359f
#define HMM_DEG180 180.0
#define HMM_DEG18032 180.0f
#define HMM_TURNHALF 0.5
#define HMM_TURNHALF32 0.5f
#define HMM_RadToDeg ((float)(HMM_DEG180/HMM_PI))
#define HMM_RadToTurn ((float)(HMM_TURNHALF/HMM_PI))
#define HMM_DegToRad ((float)(HMM_PI/HMM_DEG180))
#define HMM_DegToTurn ((float)(HMM_TURNHALF/HMM_DEG180))
#define HMM_TurnToRad ((float)(HMM_PI/HMM_TURNHALF))
#define HMM_TurnToDeg ((float)(HMM_DEG180/HMM_TURNHALF))

#if defined(HANDMADE_MATH_USE_RADIANS)
# define HMM_AngleRad(a) (a)
# define HMM_AngleDeg(a) ((a)*HMM_DegToRad)
# define HMM_AngleTurn(a) ((a)*HMM_TurnToRad)
#elif defined(HANDMADE_MATH_USE_DEGREES)
# define HMM_AngleRad(a) ((a)*HMM_RadToDeg)
# define HMM_AngleDeg(a) (a)
# define HMM_AngleTurn(a) ((a)*HMM_TurnToDeg)
#elif defined(HANDMADE_MATH_USE_TURNS)
# define HMM_AngleRad(a) ((a)*HMM_RadToTurn)
# define HMM_AngleDeg(a) ((a)*HMM_DegToTurn)
# define HMM_AngleTurn(a) (a)
#endif

#if !defined(HANDMADE_MATH_PROVIDE_MATH_FUNCTIONS)
# include <math.h>
# define HMM_SINF sinf
# define HMM_COSF cosf
# define HMM_TANF tanf
# define HMM_SQRTF sqrtf
# define HMM_ACOSF acosf
#endif

#if !defined(HMM_ANGLE_USER_TO_INTERNAL)
# define HMM_ANGLE_USER_TO_INTERNAL(a) (HMM_ToRad(a))
#endif

#if !defined(HMM_ANGLE_INTERNAL_TO_USER)
# if defined(HANDMADE_MATH_USE_RADIANS)
#  define HMM_ANGLE_INTERNAL_TO_USER(a) (a) 
# elif defined(HANDMADE_MATH_USE_DEGREES)
#  define HMM_ANGLE_INTERNAL_TO_USER(a) ((a)*HMM_RadToDeg)
# elif defined(HANDMADE_MATH_USE_TURNS)
#  define HMM_ANGLE_INTERNAL_TO_USER(a) ((a)*HMM_RadToTurn)
# endif
#endif

#define min(a, b) ((a) > (b) ? (b) : (a))
#define max(a, b) ((a) < (b) ? (b) : (a))
#define abs(a) ((a) > 0 ? (a) : -(a))
#define mod(a, m) (((a) % (m)) >= 0 ? ((a) % (m)) : (((a) % (m)) + (m)))
#define square(x) ((x) * (x))
#define sign(a) ((a) > 0 ? 1 : -1)

typedef union v2
{
    struct
    {
        float X, Y;
    };

    struct
    {
        float U, V;
    };

    struct
    {
        float Left, Right;
    };

    struct
    {
        float Width, Height;
    };

    float Elements[2];

#ifdef __cplusplus
    inline float &operator[](const int &Index)
    {
        return Elements[Index];
    }
#endif
} v2;

typedef union v3
{
    struct
    {
        float X, Y, Z;
    };

    struct
    {
        float U, V, W;
    };

    struct
    {
        float R, G, B;
    };

    struct
    {
        v2 XY;
        float _Ignored0;
    };

    struct
    {
        float _Ignored1;
        v2 YZ;
    };

    struct
    {
        v2 UV;
        float _Ignored2;
    };

    struct
    {
        float _Ignored3;
        v2 VW;
    };

    float Elements[3];

#ifdef __cplusplus
    inline float &operator[](const int &Index)
    {
        return Elements[Index];
    }
#endif
} v3;

typedef union v4
{
    struct
    {
        union
        {
            v3 XYZ;
            struct
            {
                float X, Y, Z;
            };
        };

        float W;
    };
    struct
    {
        union
        {
            v3 RGB;
            struct
            {
                float R, G, B;
            };
        };

        float A;
    };

    struct
    {
        v2 XY;
        float _Ignored0;
        float _Ignored1;
    };

    struct
    {
        float _Ignored2;
        v2 YZ;
        float _Ignored3;
    };

    struct
    {
        float _Ignored4;
        float _Ignored5;
        v2 ZW;
    };

    float Elements[4];

#ifdef HANDMADE_MATH__USE_SSE
    __m128 SSE;
#endif

#ifdef __cplusplus
    inline float &operator[](const int &Index)
    {
        return Elements[Index];
    }
#endif
} v4;

typedef union m2
{
    float Elements[2][2];
    v2 Columns[2];

#ifdef __cplusplus
    inline v2 &operator[](const int &Index)
    {
        return Columns[Index];
    }
#endif
} m2;
    
typedef union m3
{
    float Elements[3][3];
    v3 Columns[3];

#ifdef __cplusplus
    inline v3 &operator[](const int &Index)
    {
        return Columns[Index];
    }
#endif
} m3;

typedef union m4
{
    float Elements[4][4];
    v4 Columns[4];

#ifdef __cplusplus
    inline v4 &operator[](const int &Index)
    {
        return Columns[Index];
    }
#endif
} m4;

typedef union HMM_Quat
{
    struct
    {
        union
        {
            v3 XYZ;
            struct
            {
                float X, Y, Z;
            };
        };

        float W;
    };

    float Elements[4];

#ifdef HANDMADE_MATH__USE_SSE
    __m128 SSE;
#endif
} HMM_Quat;

typedef signed int HMM_Bool;

/*
 * Angle unit conversion functions
 */
static inline float HMM_ToRad(float Angle)
{
#if defined(HANDMADE_MATH_USE_RADIANS)
    float Result = Angle;
#elif defined(HANDMADE_MATH_USE_DEGREES) 
    float Result = Angle * HMM_DegToRad;
#elif defined(HANDMADE_MATH_USE_TURNS)
    float Result = Angle * HMM_TurnToRad;
#endif
    
    return Result;
}

static inline float HMM_ToDeg(float Angle)
{
#if defined(HANDMADE_MATH_USE_RADIANS)
    float Result = Angle * HMM_RadToDeg;
#elif defined(HANDMADE_MATH_USE_DEGREES) 
    float Result = Angle;
#elif defined(HANDMADE_MATH_USE_TURNS)
    float Result = Angle * HMM_TurnToDeg;
#endif
    
    return Result;
}

static inline float HMM_ToTurn(float Angle)
{
#if defined(HANDMADE_MATH_USE_RADIANS)
    float Result = Angle * HMM_RadToTurn;
#elif defined(HANDMADE_MATH_USE_DEGREES) 
    float Result = Angle * HMM_DegToTurn;
#elif defined(HANDMADE_MATH_USE_TURNS)
    float Result = Angle;
#endif
    
    return Result;
}

v2 vector_from_angle(float angle) {
    float angle_radians = angle * HMM_DegToRad;

    return v2 {
        .X = HMM_SINF(angle_radians),
        .Y = HMM_COSF(angle_radians),
    };
}

/*
 * Floating-point math functions
 */

COVERAGE(HMM_SinF, 1)
static inline float HMM_SinF(float Angle)
{
    ASSERT_COVERED(HMM_SinF);
    return HMM_SINF(HMM_ANGLE_USER_TO_INTERNAL(Angle));
}

COVERAGE(HMM_CosF, 1)
static inline float HMM_CosF(float Angle)
{
    ASSERT_COVERED(HMM_CosF);
    return HMM_COSF(HMM_ANGLE_USER_TO_INTERNAL(Angle));
}

COVERAGE(HMM_TanF, 1)
static inline float HMM_TanF(float Angle)
{
    ASSERT_COVERED(HMM_TanF);
    return HMM_TANF(HMM_ANGLE_USER_TO_INTERNAL(Angle));
}

COVERAGE(HMM_ACosF, 1)
static inline float HMM_ACosF(float Arg)
{
    ASSERT_COVERED(HMM_ACosF);
    return HMM_ANGLE_INTERNAL_TO_USER(HMM_ACOSF(Arg));
}

COVERAGE(HMM_SqrtF, 1)
static inline float HMM_SqrtF(float Float)
{
    ASSERT_COVERED(HMM_SqrtF);

    float Result;

#ifdef HANDMADE_MATH__USE_SSE
    __m128 In = _mm_set_ss(Float);
    __m128 Out = _mm_sqrt_ss(In);
    Result = _mm_cvtss_f32(Out);
#else
    Result = HMM_SQRTF(Float);
#endif

    return Result;
}

COVERAGE(HMM_InvSqrtF, 1)
static inline float HMM_InvSqrtF(float Float)
{
    ASSERT_COVERED(HMM_InvSqrtF);

    float Result;

    Result = 1.0f/HMM_SqrtF(Float);

    return Result;
}


/*
 * Utility functions
 */

COVERAGE(HMM_Lerp, 1)
static inline float HMM_Lerp(float A, float Time, float B)
{
    ASSERT_COVERED(HMM_Lerp);
    return (1.0f - Time) * A + Time * B;
}

COVERAGE(HMM_Clamp, 1)
static inline float HMM_Clamp(float Min, float Value, float Max)
{
    ASSERT_COVERED(HMM_Clamp);

    float Result = Value;

    if (Result < Min)
    {
        Result = Min;
    }

    if (Result > Max)
    {
        Result = Max;
    }

    return Result;
}


/*
 * vtor initialization
 */

COVERAGE(HMM_V2, 1)
static inline v2 HMM_V2(float X, float Y)
{
    ASSERT_COVERED(HMM_V2);

    v2 Result;
    Result.X = X;
    Result.Y = Y;

    return Result;
}

COVERAGE(HMM_V3, 1)
static inline v3 HMM_V3(float X, float Y, float Z)
{
    ASSERT_COVERED(HMM_V3);

    v3 Result;
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;

    return Result;
}

COVERAGE(HMM_V4, 1)
static inline v4 HMM_V4(float X, float Y, float Z, float W)
{
    ASSERT_COVERED(HMM_V4);

    v4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_setr_ps(X, Y, Z, W);
#else
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    Result.W = W;
#endif

    return Result;
}

COVERAGE(HMM_V4V, 1)
static inline v4 HMM_V4V(v3 vtor, float W)
{
    ASSERT_COVERED(HMM_V4V);

    v4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_setr_ps(vtor.X, vtor.Y, vtor.Z, W);
#else
    Result.XYZ = vtor;
    Result.W = W;
#endif

    return Result;
}


/*
 * Binary vtor operations
 */

COVERAGE(HMM_AddV2, 1)
static inline v2 HMM_AddV2(v2 Left, v2 Right)
{
    ASSERT_COVERED(HMM_AddV2);

    v2 Result;
    Result.X = Left.X + Right.X;
    Result.Y = Left.Y + Right.Y;

    return Result;
}

COVERAGE(HMM_AddV3, 1)
static inline v3 HMM_AddV3(v3 Left, v3 Right)
{
    ASSERT_COVERED(HMM_AddV3);

    v3 Result;
    Result.X = Left.X + Right.X;
    Result.Y = Left.Y + Right.Y;
    Result.Z = Left.Z + Right.Z;

    return Result;
}

COVERAGE(HMM_AddV4, 1)
static inline v4 HMM_AddV4(v4 Left, v4 Right)
{
    ASSERT_COVERED(HMM_AddV4);

    v4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_add_ps(Left.SSE, Right.SSE);
#else
    Result.X = Left.X + Right.X;
    Result.Y = Left.Y + Right.Y;
    Result.Z = Left.Z + Right.Z;
    Result.W = Left.W + Right.W;
#endif

    return Result;
}

COVERAGE(HMM_SubV2, 1)
static inline v2 HMM_SubV2(v2 Left, v2 Right)
{
    ASSERT_COVERED(HMM_SubV2);

    v2 Result;
    Result.X = Left.X - Right.X;
    Result.Y = Left.Y - Right.Y;

    return Result;
}

COVERAGE(HMM_SubV3, 1)
static inline v3 HMM_SubV3(v3 Left, v3 Right)
{
    ASSERT_COVERED(HMM_SubV3);

    v3 Result;
    Result.X = Left.X - Right.X;
    Result.Y = Left.Y - Right.Y;
    Result.Z = Left.Z - Right.Z;

    return Result;
}

COVERAGE(HMM_SubV4, 1)
static inline v4 HMM_SubV4(v4 Left, v4 Right)
{
    ASSERT_COVERED(HMM_SubV4);

    v4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_sub_ps(Left.SSE, Right.SSE);
#else
    Result.X = Left.X - Right.X;
    Result.Y = Left.Y - Right.Y;
    Result.Z = Left.Z - Right.Z;
    Result.W = Left.W - Right.W;
#endif

    return Result;
}

COVERAGE(HMM_MulV2, 1)
static inline v2 HMM_MulV2(v2 Left, v2 Right)
{
    ASSERT_COVERED(HMM_MulV2);

    v2 Result;
    Result.X = Left.X * Right.X;
    Result.Y = Left.Y * Right.Y;

    return Result;
}

COVERAGE(HMM_MulV2F, 1)
static inline v2 HMM_MulV2F(v2 Left, float Right)
{
    ASSERT_COVERED(HMM_MulV2F);

    v2 Result;
    Result.X = Left.X * Right;
    Result.Y = Left.Y * Right;

    return Result;
}

COVERAGE(HMM_MulV3, 1)
static inline v3 HMM_MulV3(v3 Left, v3 Right)
{
    ASSERT_COVERED(HMM_MulV3);

    v3 Result;
    Result.X = Left.X * Right.X;
    Result.Y = Left.Y * Right.Y;
    Result.Z = Left.Z * Right.Z;

    return Result;
}

COVERAGE(HMM_MulV3F, 1)
static inline v3 HMM_MulV3F(v3 Left, float Right)
{
    ASSERT_COVERED(HMM_MulV3F);

    v3 Result;
    Result.X = Left.X * Right;
    Result.Y = Left.Y * Right;
    Result.Z = Left.Z * Right;

    return Result;
}

COVERAGE(HMM_MulV4, 1)
static inline v4 HMM_MulV4(v4 Left, v4 Right)
{
    ASSERT_COVERED(HMM_MulV4);

    v4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_mul_ps(Left.SSE, Right.SSE);
#else
    Result.X = Left.X * Right.X;
    Result.Y = Left.Y * Right.Y;
    Result.Z = Left.Z * Right.Z;
    Result.W = Left.W * Right.W;
#endif

    return Result;
}

COVERAGE(HMM_MulV4F, 1)
static inline v4 HMM_MulV4F(v4 Left, float Right)
{
    ASSERT_COVERED(HMM_MulV4F);

    v4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    __m128 Scalar = _mm_set1_ps(Right);
    Result.SSE = _mm_mul_ps(Left.SSE, Scalar);
#else
    Result.X = Left.X * Right;
    Result.Y = Left.Y * Right;
    Result.Z = Left.Z * Right;
    Result.W = Left.W * Right;
#endif

    return Result;
}

COVERAGE(HMM_DivV2, 1)
static inline v2 HMM_DivV2(v2 Left, v2 Right)
{
    ASSERT_COVERED(HMM_DivV2);

    v2 Result;
    Result.X = Left.X / Right.X;
    Result.Y = Left.Y / Right.Y;

    return Result;
}

COVERAGE(HMM_DivV2F, 1)
static inline v2 HMM_DivV2F(v2 Left, float Right)
{
    ASSERT_COVERED(HMM_DivV2F);

    v2 Result;
    Result.X = Left.X / Right;
    Result.Y = Left.Y / Right;

    return Result;
}

COVERAGE(HMM_DivV3, 1)
static inline v3 HMM_DivV3(v3 Left, v3 Right)
{
    ASSERT_COVERED(HMM_DivV3);

    v3 Result;
    Result.X = Left.X / Right.X;
    Result.Y = Left.Y / Right.Y;
    Result.Z = Left.Z / Right.Z;

    return Result;
}

COVERAGE(HMM_DivV3F, 1)
static inline v3 HMM_DivV3F(v3 Left, float Right)
{
    ASSERT_COVERED(HMM_DivV3F);

    v3 Result;
    Result.X = Left.X / Right;
    Result.Y = Left.Y / Right;
    Result.Z = Left.Z / Right;

    return Result;
}

COVERAGE(HMM_DivV4, 1)
static inline v4 HMM_DivV4(v4 Left, v4 Right)
{
    ASSERT_COVERED(HMM_DivV4);

    v4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_div_ps(Left.SSE, Right.SSE);
#else
    Result.X = Left.X / Right.X;
    Result.Y = Left.Y / Right.Y;
    Result.Z = Left.Z / Right.Z;
    Result.W = Left.W / Right.W;
#endif

    return Result;
}

COVERAGE(HMM_DivV4F, 1)
static inline v4 HMM_DivV4F(v4 Left, float Right)
{
    ASSERT_COVERED(HMM_DivV4F);

    v4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    __m128 Scalar = _mm_set1_ps(Right);
    Result.SSE = _mm_div_ps(Left.SSE, Scalar);
#else
    Result.X = Left.X / Right;
    Result.Y = Left.Y / Right;
    Result.Z = Left.Z / Right;
    Result.W = Left.W / Right;
#endif

    return Result;
}

COVERAGE(HMM_EqV2, 1)
static inline HMM_Bool HMM_EqV2(v2 Left, v2 Right)
{
    ASSERT_COVERED(HMM_EqV2);
    return Left.X == Right.X && Left.Y == Right.Y;
}

COVERAGE(HMM_EqV3, 1)
static inline HMM_Bool HMM_EqV3(v3 Left, v3 Right)
{
    ASSERT_COVERED(HMM_EqV3);
    return Left.X == Right.X && Left.Y == Right.Y && Left.Z == Right.Z;
}

COVERAGE(HMM_EqV4, 1)
static inline HMM_Bool HMM_EqV4(v4 Left, v4 Right)
{
    ASSERT_COVERED(HMM_EqV4);
    return Left.X == Right.X && Left.Y == Right.Y && Left.Z == Right.Z && Left.W == Right.W;
}

COVERAGE(HMM_DotV2, 1)
static inline float HMM_DotV2(v2 Left, v2 Right)
{
    ASSERT_COVERED(HMM_DotV2);
    return (Left.X * Right.X) + (Left.Y * Right.Y);
}

COVERAGE(HMM_DotV3, 1)
static inline float HMM_DotV3(v3 Left, v3 Right)
{
    ASSERT_COVERED(HMM_DotV3);
    return (Left.X * Right.X) + (Left.Y * Right.Y) + (Left.Z * Right.Z);
}

COVERAGE(HMM_DotV4, 1)
static inline float HMM_DotV4(v4 Left, v4 Right)
{
    ASSERT_COVERED(HMM_DotV4);

    float Result;

    // NOTE(zak): IN the future if we wanna check what version SSE is support
    // we can use _mm_dp_ps (4.3) but for now we will use the old way.
    // Or a r = _mm_mul_ps(v1, v2), r = _mm_hadd_ps(r, r), r = _mm_hadd_ps(r, r) for SSE3
#ifdef HANDMADE_MATH__USE_SSE
    __m128 SSEResultOne = _mm_mul_ps(Left.SSE, Right.SSE);
    __m128 SSEResultTwo = _mm_shuffle_ps(SSEResultOne, SSEResultOne, _MM_SHUFFLE(2, 3, 0, 1));
    SSEResultOne = _mm_add_ps(SSEResultOne, SSEResultTwo);
    SSEResultTwo = _mm_shuffle_ps(SSEResultOne, SSEResultOne, _MM_SHUFFLE(0, 1, 2, 3));
    SSEResultOne = _mm_add_ps(SSEResultOne, SSEResultTwo);
    _mm_store_ss(&Result, SSEResultOne);
#else
    Result = ((Left.X * Right.X) + (Left.Z * Right.Z)) + ((Left.Y * Right.Y) + (Left.W * Right.W));
#endif

    return Result;
}

COVERAGE(HMM_Cross, 1)
static inline v3 HMM_Cross(v3 Left, v3 Right)
{
    ASSERT_COVERED(HMM_Cross);

    v3 Result;
    Result.X = (Left.Y * Right.Z) - (Left.Z * Right.Y);
    Result.Y = (Left.Z * Right.X) - (Left.X * Right.Z);
    Result.Z = (Left.X * Right.Y) - (Left.Y * Right.X);

    return Result;
}


/*
 * Unary vtor operations
 */

static inline float length_sqr(v2 A)
{
    return HMM_DotV2(A, A);
}

static inline float length_sqr(v3 A)
{
    return HMM_DotV3(A, A);
}

static inline float length_sqr(v4 A)
{
    return HMM_DotV4(A, A);
}

static inline float length(v2 A)
{
    return HMM_SqrtF(length_sqr(A));
}

static inline float length(v3 A)
{
    return HMM_SqrtF(length_sqr(A));
}

static inline float length(v4 A)
{
    return HMM_SqrtF(length_sqr(A));
}

static inline v2 norm(v2 A)
{
    return HMM_MulV2F(A, HMM_InvSqrtF(HMM_DotV2(A, A)));
}

static inline v3 norm(v3 A)
{
    return HMM_MulV3F(A, HMM_InvSqrtF(HMM_DotV3(A, A)));
}

static inline v4 norm(v4 A)
{
    return HMM_MulV4F(A, HMM_InvSqrtF(HMM_DotV4(A, A)));
}

/*
 * Utility vtor functions
 */

COVERAGE(HMM_LerpV2, 1)
static inline v2 HMM_LerpV2(v2 A, float Time, v2 B) 
{
    ASSERT_COVERED(HMM_LerpV2);
    return HMM_AddV2(HMM_MulV2F(A, 1.0f - Time), HMM_MulV2F(B, Time));
}

COVERAGE(HMM_LerpV3, 1)
static inline v3 HMM_LerpV3(v3 A, float Time, v3 B) 
{
    ASSERT_COVERED(HMM_LerpV3);
    return HMM_AddV3(HMM_MulV3F(A, 1.0f - Time), HMM_MulV3F(B, Time));
}

COVERAGE(HMM_LerpV4, 1)
static inline v4 HMM_LerpV4(v4 A, float Time, v4 B) 
{
    ASSERT_COVERED(HMM_LerpV4);
    return HMM_AddV4(HMM_MulV4F(A, 1.0f - Time), HMM_MulV4F(B, Time));
}

/*
 * SSE stuff
 */

COVERAGE(HMM_LinearCombineV4M4, 1)
static inline v4 HMM_LinearCombineV4M4(v4 Left, m4 Right)
{
    ASSERT_COVERED(HMM_LinearCombineV4M4);

    v4 Result;
#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_mul_ps(_mm_shuffle_ps(Left.SSE, Left.SSE, 0x00), Right.Columns[0].SSE);
    Result.SSE = _mm_add_ps(Result.SSE, _mm_mul_ps(_mm_shuffle_ps(Left.SSE, Left.SSE, 0x55), Right.Columns[1].SSE));
    Result.SSE = _mm_add_ps(Result.SSE, _mm_mul_ps(_mm_shuffle_ps(Left.SSE, Left.SSE, 0xaa), Right.Columns[2].SSE));
    Result.SSE = _mm_add_ps(Result.SSE, _mm_mul_ps(_mm_shuffle_ps(Left.SSE, Left.SSE, 0xff), Right.Columns[3].SSE));
#else
    Result.X = Left.Elements[0] * Right.Columns[0].X;
    Result.Y = Left.Elements[0] * Right.Columns[0].Y;
    Result.Z = Left.Elements[0] * Right.Columns[0].Z;
    Result.W = Left.Elements[0] * Right.Columns[0].W;

    Result.X += Left.Elements[1] * Right.Columns[1].X;
    Result.Y += Left.Elements[1] * Right.Columns[1].Y;
    Result.Z += Left.Elements[1] * Right.Columns[1].Z;
    Result.W += Left.Elements[1] * Right.Columns[1].W;

    Result.X += Left.Elements[2] * Right.Columns[2].X;
    Result.Y += Left.Elements[2] * Right.Columns[2].Y;
    Result.Z += Left.Elements[2] * Right.Columns[2].Z;
    Result.W += Left.Elements[2] * Right.Columns[2].W;

    Result.X += Left.Elements[3] * Right.Columns[3].X;
    Result.Y += Left.Elements[3] * Right.Columns[3].Y;
    Result.Z += Left.Elements[3] * Right.Columns[3].Z;
    Result.W += Left.Elements[3] * Right.Columns[3].W;
#endif

    return Result;
}

/*
 * 2x2 Matrices
 */

COVERAGE(HMM_M2, 1)
static inline m2 HMM_M2(void)
{
    ASSERT_COVERED(HMM_M2);
    m2 Result = {0};
    return Result;
}

COVERAGE(HMM_M2D, 1)
static inline m2 HMM_M2D(float Diagonal)
{
    ASSERT_COVERED(HMM_M2D);
    
    m2 Result = {0};
    Result.Elements[0][0] = Diagonal;
    Result.Elements[1][1] = Diagonal;

    return Result;
}

COVERAGE(HMM_TransposeM2, 1)
static inline m2 HMM_TransposeM2(m2 Matrix)
{
    ASSERT_COVERED(HMM_TransposeM2);
    
    m2 Result = Matrix;

    Result.Elements[0][1] = Matrix.Elements[1][0];
    Result.Elements[1][0] = Matrix.Elements[0][1];
    
    return Result;
}

COVERAGE(HMM_AddM2, 1)
static inline m2 HMM_AddM2(m2 Left, m2 Right)
{
    ASSERT_COVERED(HMM_AddM2);
    
    m2 Result;

    Result.Elements[0][0] = Left.Elements[0][0] + Right.Elements[0][0];
    Result.Elements[0][1] = Left.Elements[0][1] + Right.Elements[0][1];
    Result.Elements[1][0] = Left.Elements[1][0] + Right.Elements[1][0];
    Result.Elements[1][1] = Left.Elements[1][1] + Right.Elements[1][1];
   
    return Result;    
}

COVERAGE(HMM_SubM2, 1)
static inline m2 HMM_SubM2(m2 Left, m2 Right)
{
    ASSERT_COVERED(HMM_SubM2);
    
    m2 Result;

    Result.Elements[0][0] = Left.Elements[0][0] - Right.Elements[0][0];
    Result.Elements[0][1] = Left.Elements[0][1] - Right.Elements[0][1];
    Result.Elements[1][0] = Left.Elements[1][0] - Right.Elements[1][0];
    Result.Elements[1][1] = Left.Elements[1][1] - Right.Elements[1][1];
    
    return Result;
}

COVERAGE(HMM_MulM2V2, 1)
static inline v2 HMM_MulM2V2(m2 Matrix, v2 vtor)
{
    ASSERT_COVERED(HMM_MulM2V2);
    
    v2 Result;

    Result.X = vtor.Elements[0] * Matrix.Columns[0].X;
    Result.Y = vtor.Elements[0] * Matrix.Columns[0].Y;

    Result.X += vtor.Elements[1] * Matrix.Columns[1].X;
    Result.Y += vtor.Elements[1] * Matrix.Columns[1].Y;

    return Result;    
}

COVERAGE(HMM_MulM2, 1)
static inline m2 HMM_MulM2(m2 Left, m2 Right)
{
    ASSERT_COVERED(HMM_MulM2);
    
    m2 Result;
    Result.Columns[0] = HMM_MulM2V2(Left, Right.Columns[0]);
    Result.Columns[1] = HMM_MulM2V2(Left, Right.Columns[1]);

    return Result;    
}

COVERAGE(HMM_MulM2F, 1)
static inline m2 HMM_MulM2F(m2 Matrix, float Scalar)
{
    ASSERT_COVERED(HMM_MulM2F);
    
    m2 Result;

    Result.Elements[0][0] = Matrix.Elements[0][0] * Scalar;
    Result.Elements[0][1] = Matrix.Elements[0][1] * Scalar;
    Result.Elements[1][0] = Matrix.Elements[1][0] * Scalar;
    Result.Elements[1][1] = Matrix.Elements[1][1] * Scalar;
    
    return Result;
}

COVERAGE(HMM_DivM2F, 1)
static inline m2 HMM_DivM2F(m2 Matrix, float Scalar)
{
    ASSERT_COVERED(HMM_DivM2F);
    
    m2 Result;

    Result.Elements[0][0] = Matrix.Elements[0][0] / Scalar;
    Result.Elements[0][1] = Matrix.Elements[0][1] / Scalar;
    Result.Elements[1][0] = Matrix.Elements[1][0] / Scalar;
    Result.Elements[1][1] = Matrix.Elements[1][1] / Scalar;

    return Result;
}

COVERAGE(HMM_DeterminantM2, 1)
static inline float HMM_DeterminantM2(m2 Matrix) 
{
    ASSERT_COVERED(HMM_DeterminantM2);
    return Matrix.Elements[0][0]*Matrix.Elements[1][1] - Matrix.Elements[0][1]*Matrix.Elements[1][0];
}


COVERAGE(HMM_InvGeneralM2, 1)
static inline m2 HMM_InvGeneralM2(m2 Matrix) 
{
    ASSERT_COVERED(HMM_InvGeneralM2);

    m2 Result;
    float InvDeterminant = 1.0f / HMM_DeterminantM2(Matrix);
    Result.Elements[0][0] = InvDeterminant * +Matrix.Elements[1][1];
    Result.Elements[1][1] = InvDeterminant * +Matrix.Elements[0][0];
    Result.Elements[0][1] = InvDeterminant * -Matrix.Elements[0][1];
    Result.Elements[1][0] = InvDeterminant * -Matrix.Elements[1][0];

    return Result;
}

/*
 * 3x3 Matrices
 */

COVERAGE(HMM_M3, 1)
static inline m3 HMM_M3(void)
{
    ASSERT_COVERED(HMM_M3);
    m3 Result = {0};
    return Result;
}

COVERAGE(HMM_M3D, 1)
static inline m3 HMM_M3D(float Diagonal)
{
    ASSERT_COVERED(HMM_M3D);
    
    m3 Result = {0};
    Result.Elements[0][0] = Diagonal;
    Result.Elements[1][1] = Diagonal;
    Result.Elements[2][2] = Diagonal;

    return Result;
}

COVERAGE(HMM_TransposeM3, 1)
static inline m3 HMM_TransposeM3(m3 Matrix)
{
    ASSERT_COVERED(HMM_TransposeM3);

    m3 Result = Matrix;

    Result.Elements[0][1] = Matrix.Elements[1][0];
    Result.Elements[0][2] = Matrix.Elements[2][0];
    Result.Elements[1][0] = Matrix.Elements[0][1];
    Result.Elements[1][2] = Matrix.Elements[2][1];
    Result.Elements[2][1] = Matrix.Elements[1][2];
    Result.Elements[2][0] = Matrix.Elements[0][2];
    
    return Result;
}

COVERAGE(HMM_AddM3, 1)
static inline m3 HMM_AddM3(m3 Left, m3 Right)
{
    ASSERT_COVERED(HMM_AddM3);
    
    m3 Result;
    
    Result.Elements[0][0] = Left.Elements[0][0] + Right.Elements[0][0];
    Result.Elements[0][1] = Left.Elements[0][1] + Right.Elements[0][1];
    Result.Elements[0][2] = Left.Elements[0][2] + Right.Elements[0][2];
    Result.Elements[1][0] = Left.Elements[1][0] + Right.Elements[1][0];
    Result.Elements[1][1] = Left.Elements[1][1] + Right.Elements[1][1];
    Result.Elements[1][2] = Left.Elements[1][2] + Right.Elements[1][2];
    Result.Elements[2][0] = Left.Elements[2][0] + Right.Elements[2][0];
    Result.Elements[2][1] = Left.Elements[2][1] + Right.Elements[2][1];
    Result.Elements[2][2] = Left.Elements[2][2] + Right.Elements[2][2];

    return Result;    
}

COVERAGE(HMM_SubM3, 1)
static inline m3 HMM_SubM3(m3 Left, m3 Right)
{
    ASSERT_COVERED(HMM_SubM3);

    m3 Result;

    Result.Elements[0][0] = Left.Elements[0][0] - Right.Elements[0][0];
    Result.Elements[0][1] = Left.Elements[0][1] - Right.Elements[0][1];
    Result.Elements[0][2] = Left.Elements[0][2] - Right.Elements[0][2];
    Result.Elements[1][0] = Left.Elements[1][0] - Right.Elements[1][0];
    Result.Elements[1][1] = Left.Elements[1][1] - Right.Elements[1][1];
    Result.Elements[1][2] = Left.Elements[1][2] - Right.Elements[1][2];
    Result.Elements[2][0] = Left.Elements[2][0] - Right.Elements[2][0];
    Result.Elements[2][1] = Left.Elements[2][1] - Right.Elements[2][1];
    Result.Elements[2][2] = Left.Elements[2][2] - Right.Elements[2][2];

    return Result;
}

COVERAGE(HMM_MulM3V3, 1)
static inline v3 HMM_MulM3V3(m3 Matrix, v3 vtor)
{
    ASSERT_COVERED(HMM_MulM3V3);
    
    v3 Result;

    Result.X = vtor.Elements[0] * Matrix.Columns[0].X;
    Result.Y = vtor.Elements[0] * Matrix.Columns[0].Y;
    Result.Z = vtor.Elements[0] * Matrix.Columns[0].Z;

    Result.X += vtor.Elements[1] * Matrix.Columns[1].X;
    Result.Y += vtor.Elements[1] * Matrix.Columns[1].Y;
    Result.Z += vtor.Elements[1] * Matrix.Columns[1].Z;

    Result.X += vtor.Elements[2] * Matrix.Columns[2].X;
    Result.Y += vtor.Elements[2] * Matrix.Columns[2].Y;
    Result.Z += vtor.Elements[2] * Matrix.Columns[2].Z;
    
    return Result;    
}

COVERAGE(HMM_MulM3, 1)
static inline m3 HMM_MulM3(m3 Left, m3 Right)
{
    ASSERT_COVERED(HMM_MulM3);

    m3 Result;
    Result.Columns[0] = HMM_MulM3V3(Left, Right.Columns[0]);
    Result.Columns[1] = HMM_MulM3V3(Left, Right.Columns[1]);
    Result.Columns[2] = HMM_MulM3V3(Left, Right.Columns[2]);

    return Result;    
}

COVERAGE(HMM_MulM3F, 1)
static inline m3 HMM_MulM3F(m3 Matrix, float Scalar)
{
    ASSERT_COVERED(HMM_MulM3F);

    m3 Result;

    Result.Elements[0][0] = Matrix.Elements[0][0] * Scalar;
    Result.Elements[0][1] = Matrix.Elements[0][1] * Scalar;
    Result.Elements[0][2] = Matrix.Elements[0][2] * Scalar;
    Result.Elements[1][0] = Matrix.Elements[1][0] * Scalar;
    Result.Elements[1][1] = Matrix.Elements[1][1] * Scalar;
    Result.Elements[1][2] = Matrix.Elements[1][2] * Scalar;
    Result.Elements[2][0] = Matrix.Elements[2][0] * Scalar;
    Result.Elements[2][1] = Matrix.Elements[2][1] * Scalar;
    Result.Elements[2][2] = Matrix.Elements[2][2] * Scalar;

    return Result;            
}

COVERAGE(HMM_DivM3, 1)
static inline m3 HMM_DivM3F(m3 Matrix, float Scalar)
{
    ASSERT_COVERED(HMM_DivM3);

    m3 Result;
    
    Result.Elements[0][0] = Matrix.Elements[0][0] / Scalar;
    Result.Elements[0][1] = Matrix.Elements[0][1] / Scalar;
    Result.Elements[0][2] = Matrix.Elements[0][2] / Scalar;
    Result.Elements[1][0] = Matrix.Elements[1][0] / Scalar;
    Result.Elements[1][1] = Matrix.Elements[1][1] / Scalar;
    Result.Elements[1][2] = Matrix.Elements[1][2] / Scalar;
    Result.Elements[2][0] = Matrix.Elements[2][0] / Scalar;
    Result.Elements[2][1] = Matrix.Elements[2][1] / Scalar;
    Result.Elements[2][2] = Matrix.Elements[2][2] / Scalar;

    return Result;                    
}

COVERAGE(HMM_DeterminantM3, 1)
static inline float HMM_DeterminantM3(m3 Matrix) 
{
    ASSERT_COVERED(HMM_DeterminantM3);

    m3 Cross;
    Cross.Columns[0] = HMM_Cross(Matrix.Columns[1], Matrix.Columns[2]);
    Cross.Columns[1] = HMM_Cross(Matrix.Columns[2], Matrix.Columns[0]);
    Cross.Columns[2] = HMM_Cross(Matrix.Columns[0], Matrix.Columns[1]);

    return HMM_DotV3(Cross.Columns[2], Matrix.Columns[2]);
}

COVERAGE(HMM_InvGeneralM3, 1)
static inline m3 HMM_InvGeneralM3(m3 Matrix) 
{
    ASSERT_COVERED(HMM_InvGeneralM3);

    m3 Cross;
    Cross.Columns[0] = HMM_Cross(Matrix.Columns[1], Matrix.Columns[2]);
    Cross.Columns[1] = HMM_Cross(Matrix.Columns[2], Matrix.Columns[0]);
    Cross.Columns[2] = HMM_Cross(Matrix.Columns[0], Matrix.Columns[1]);

    float InvDeterminant = 1.0f / HMM_DotV3(Cross.Columns[2], Matrix.Columns[2]);

    m3 Result;
    Result.Columns[0] = HMM_MulV3F(Cross.Columns[0], InvDeterminant);
    Result.Columns[1] = HMM_MulV3F(Cross.Columns[1], InvDeterminant);
    Result.Columns[2] = HMM_MulV3F(Cross.Columns[2], InvDeterminant);

    return HMM_TransposeM3(Result);
}

/*
 * 4x4 Matrices
 */

COVERAGE(HMM_M4, 1)
static inline m4 HMM_M4(void)
{
    ASSERT_COVERED(HMM_M4);
    m4 Result = {0};
    return Result;
}

COVERAGE(HMM_M4D, 1)
static inline m4 HMM_M4D(float Diagonal)
{
    ASSERT_COVERED(HMM_M4D);

    m4 Result = {0};
    Result.Elements[0][0] = Diagonal;
    Result.Elements[1][1] = Diagonal;
    Result.Elements[2][2] = Diagonal;
    Result.Elements[3][3] = Diagonal;

    return Result;
}

COVERAGE(HMM_TransposeM4, 1)
static inline m4 HMM_TransposeM4(m4 Matrix)
{
    ASSERT_COVERED(HMM_TransposeM4);

    m4 Result = Matrix;
#ifdef HANDMADE_MATH__USE_SSE
    _MM_TRANSPOSE4_PS(Result.Columns[0].SSE, Result.Columns[1].SSE, Result.Columns[2].SSE, Result.Columns[3].SSE);
#else
    Result.Elements[0][1] = Matrix.Elements[1][0];
    Result.Elements[0][2] = Matrix.Elements[2][0];
    Result.Elements[0][3] = Matrix.Elements[3][0];
    Result.Elements[1][0] = Matrix.Elements[0][1];
    Result.Elements[1][2] = Matrix.Elements[2][1];
    Result.Elements[1][3] = Matrix.Elements[3][1];
    Result.Elements[2][1] = Matrix.Elements[1][2];
    Result.Elements[2][0] = Matrix.Elements[0][2];
    Result.Elements[2][3] = Matrix.Elements[3][2];
    Result.Elements[3][1] = Matrix.Elements[1][3];
    Result.Elements[3][2] = Matrix.Elements[2][3];
    Result.Elements[3][0] = Matrix.Elements[0][3];
#endif

    return Result;
}

COVERAGE(HMM_AddM4, 1)
static inline m4 HMM_AddM4(m4 Left, m4 Right)
{
    ASSERT_COVERED(HMM_AddM4);

    m4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.Columns[0].SSE = _mm_add_ps(Left.Columns[0].SSE, Right.Columns[0].SSE);
    Result.Columns[1].SSE = _mm_add_ps(Left.Columns[1].SSE, Right.Columns[1].SSE);
    Result.Columns[2].SSE = _mm_add_ps(Left.Columns[2].SSE, Right.Columns[2].SSE);
    Result.Columns[3].SSE = _mm_add_ps(Left.Columns[3].SSE, Right.Columns[3].SSE);
#else
    Result.Elements[0][0] = Left.Elements[0][0] + Right.Elements[0][0];
    Result.Elements[0][1] = Left.Elements[0][1] + Right.Elements[0][1];
    Result.Elements[0][2] = Left.Elements[0][2] + Right.Elements[0][2];
    Result.Elements[0][3] = Left.Elements[0][3] + Right.Elements[0][3];
    Result.Elements[1][0] = Left.Elements[1][0] + Right.Elements[1][0];
    Result.Elements[1][1] = Left.Elements[1][1] + Right.Elements[1][1];
    Result.Elements[1][2] = Left.Elements[1][2] + Right.Elements[1][2];
    Result.Elements[1][3] = Left.Elements[1][3] + Right.Elements[1][3];
    Result.Elements[2][0] = Left.Elements[2][0] + Right.Elements[2][0];
    Result.Elements[2][1] = Left.Elements[2][1] + Right.Elements[2][1];
    Result.Elements[2][2] = Left.Elements[2][2] + Right.Elements[2][2];
    Result.Elements[2][3] = Left.Elements[2][3] + Right.Elements[2][3];
    Result.Elements[3][0] = Left.Elements[3][0] + Right.Elements[3][0];
    Result.Elements[3][1] = Left.Elements[3][1] + Right.Elements[3][1];
    Result.Elements[3][2] = Left.Elements[3][2] + Right.Elements[3][2];
    Result.Elements[3][3] = Left.Elements[3][3] + Right.Elements[3][3];
#endif

    return Result;
}

COVERAGE(HMM_SubM4, 1)
static inline m4 HMM_SubM4(m4 Left, m4 Right)
{
    ASSERT_COVERED(HMM_SubM4);

    m4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.Columns[0].SSE = _mm_sub_ps(Left.Columns[0].SSE, Right.Columns[0].SSE);
    Result.Columns[1].SSE = _mm_sub_ps(Left.Columns[1].SSE, Right.Columns[1].SSE);
    Result.Columns[2].SSE = _mm_sub_ps(Left.Columns[2].SSE, Right.Columns[2].SSE);
    Result.Columns[3].SSE = _mm_sub_ps(Left.Columns[3].SSE, Right.Columns[3].SSE);
#else
    Result.Elements[0][0] = Left.Elements[0][0] - Right.Elements[0][0];
    Result.Elements[0][1] = Left.Elements[0][1] - Right.Elements[0][1];
    Result.Elements[0][2] = Left.Elements[0][2] - Right.Elements[0][2];
    Result.Elements[0][3] = Left.Elements[0][3] - Right.Elements[0][3];
    Result.Elements[1][0] = Left.Elements[1][0] - Right.Elements[1][0];
    Result.Elements[1][1] = Left.Elements[1][1] - Right.Elements[1][1];
    Result.Elements[1][2] = Left.Elements[1][2] - Right.Elements[1][2];
    Result.Elements[1][3] = Left.Elements[1][3] - Right.Elements[1][3];
    Result.Elements[2][0] = Left.Elements[2][0] - Right.Elements[2][0];
    Result.Elements[2][1] = Left.Elements[2][1] - Right.Elements[2][1];
    Result.Elements[2][2] = Left.Elements[2][2] - Right.Elements[2][2];
    Result.Elements[2][3] = Left.Elements[2][3] - Right.Elements[2][3];
    Result.Elements[3][0] = Left.Elements[3][0] - Right.Elements[3][0];
    Result.Elements[3][1] = Left.Elements[3][1] - Right.Elements[3][1];
    Result.Elements[3][2] = Left.Elements[3][2] - Right.Elements[3][2];
    Result.Elements[3][3] = Left.Elements[3][3] - Right.Elements[3][3];
#endif
 
    return Result;
}

COVERAGE(HMM_MulM4, 1)
static inline m4 HMM_MulM4(m4 Left, m4 Right)
{
    ASSERT_COVERED(HMM_MulM4);

    m4 Result;
    Result.Columns[0] = HMM_LinearCombineV4M4(Right.Columns[0], Left);
    Result.Columns[1] = HMM_LinearCombineV4M4(Right.Columns[1], Left);
    Result.Columns[2] = HMM_LinearCombineV4M4(Right.Columns[2], Left);
    Result.Columns[3] = HMM_LinearCombineV4M4(Right.Columns[3], Left);

    return Result;
}

COVERAGE(HMM_MulM4F, 1)
static inline m4 HMM_MulM4F(m4 Matrix, float Scalar)
{
    ASSERT_COVERED(HMM_MulM4F);

    m4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    __m128 SSEScalar = _mm_set1_ps(Scalar);
    Result.Columns[0].SSE = _mm_mul_ps(Matrix.Columns[0].SSE, SSEScalar);
    Result.Columns[1].SSE = _mm_mul_ps(Matrix.Columns[1].SSE, SSEScalar);
    Result.Columns[2].SSE = _mm_mul_ps(Matrix.Columns[2].SSE, SSEScalar);
    Result.Columns[3].SSE = _mm_mul_ps(Matrix.Columns[3].SSE, SSEScalar);
#else
    Result.Elements[0][0] = Matrix.Elements[0][0] * Scalar;
    Result.Elements[0][1] = Matrix.Elements[0][1] * Scalar;
    Result.Elements[0][2] = Matrix.Elements[0][2] * Scalar;
    Result.Elements[0][3] = Matrix.Elements[0][3] * Scalar;
    Result.Elements[1][0] = Matrix.Elements[1][0] * Scalar;
    Result.Elements[1][1] = Matrix.Elements[1][1] * Scalar;
    Result.Elements[1][2] = Matrix.Elements[1][2] * Scalar;
    Result.Elements[1][3] = Matrix.Elements[1][3] * Scalar;
    Result.Elements[2][0] = Matrix.Elements[2][0] * Scalar;
    Result.Elements[2][1] = Matrix.Elements[2][1] * Scalar;
    Result.Elements[2][2] = Matrix.Elements[2][2] * Scalar;
    Result.Elements[2][3] = Matrix.Elements[2][3] * Scalar;
    Result.Elements[3][0] = Matrix.Elements[3][0] * Scalar;
    Result.Elements[3][1] = Matrix.Elements[3][1] * Scalar;
    Result.Elements[3][2] = Matrix.Elements[3][2] * Scalar;
    Result.Elements[3][3] = Matrix.Elements[3][3] * Scalar;
#endif

    return Result;
}

COVERAGE(HMM_MulM4V4, 1)
static inline v4 HMM_MulM4V4(m4 Matrix, v4 vtor)
{
    ASSERT_COVERED(HMM_MulM4V4);
    return HMM_LinearCombineV4M4(vtor, Matrix);
}

COVERAGE(HMM_DivM4F, 1)
static inline m4 HMM_DivM4F(m4 Matrix, float Scalar)
{
    ASSERT_COVERED(HMM_DivM4F);

    m4 Result;

#ifdef HANDMADE_MATH__USE_SSE
    __m128 SSEScalar = _mm_set1_ps(Scalar);
    Result.Columns[0].SSE = _mm_div_ps(Matrix.Columns[0].SSE, SSEScalar);
    Result.Columns[1].SSE = _mm_div_ps(Matrix.Columns[1].SSE, SSEScalar);
    Result.Columns[2].SSE = _mm_div_ps(Matrix.Columns[2].SSE, SSEScalar);
    Result.Columns[3].SSE = _mm_div_ps(Matrix.Columns[3].SSE, SSEScalar);
#else
    Result.Elements[0][0] = Matrix.Elements[0][0] / Scalar;
    Result.Elements[0][1] = Matrix.Elements[0][1] / Scalar;
    Result.Elements[0][2] = Matrix.Elements[0][2] / Scalar;
    Result.Elements[0][3] = Matrix.Elements[0][3] / Scalar;
    Result.Elements[1][0] = Matrix.Elements[1][0] / Scalar;
    Result.Elements[1][1] = Matrix.Elements[1][1] / Scalar;
    Result.Elements[1][2] = Matrix.Elements[1][2] / Scalar;
    Result.Elements[1][3] = Matrix.Elements[1][3] / Scalar;
    Result.Elements[2][0] = Matrix.Elements[2][0] / Scalar;
    Result.Elements[2][1] = Matrix.Elements[2][1] / Scalar;
    Result.Elements[2][2] = Matrix.Elements[2][2] / Scalar;
    Result.Elements[2][3] = Matrix.Elements[2][3] / Scalar;
    Result.Elements[3][0] = Matrix.Elements[3][0] / Scalar;
    Result.Elements[3][1] = Matrix.Elements[3][1] / Scalar;
    Result.Elements[3][2] = Matrix.Elements[3][2] / Scalar;
    Result.Elements[3][3] = Matrix.Elements[3][3] / Scalar;
#endif

    return Result;
}

COVERAGE(HMM_DeterminantM4, 1)
static inline float HMM_DeterminantM4(m4 Matrix) 
{
    ASSERT_COVERED(HMM_DeterminantM4);

    v3 C01 = HMM_Cross(Matrix.Columns[0].XYZ, Matrix.Columns[1].XYZ);
    v3 C23 = HMM_Cross(Matrix.Columns[2].XYZ, Matrix.Columns[3].XYZ);
    v3 B10 = HMM_SubV3(HMM_MulV3F(Matrix.Columns[0].XYZ, Matrix.Columns[1].W), HMM_MulV3F(Matrix.Columns[1].XYZ, Matrix.Columns[0].W));
    v3 B32 = HMM_SubV3(HMM_MulV3F(Matrix.Columns[2].XYZ, Matrix.Columns[3].W), HMM_MulV3F(Matrix.Columns[3].XYZ, Matrix.Columns[2].W));
    
    return HMM_DotV3(C01, B32) + HMM_DotV3(C23, B10);
}

COVERAGE(HMM_InvGeneralM4, 1)
// Returns a general-purpose inverse of an m4. Note that special-purpose inverses of many transformations
// are available and will be more efficient.
static inline m4 HMM_InvGeneralM4(m4 Matrix) 
{
    ASSERT_COVERED(HMM_InvGeneralM4);

    v3 C01 = HMM_Cross(Matrix.Columns[0].XYZ, Matrix.Columns[1].XYZ);
    v3 C23 = HMM_Cross(Matrix.Columns[2].XYZ, Matrix.Columns[3].XYZ);
    v3 B10 = HMM_SubV3(HMM_MulV3F(Matrix.Columns[0].XYZ, Matrix.Columns[1].W), HMM_MulV3F(Matrix.Columns[1].XYZ, Matrix.Columns[0].W));
    v3 B32 = HMM_SubV3(HMM_MulV3F(Matrix.Columns[2].XYZ, Matrix.Columns[3].W), HMM_MulV3F(Matrix.Columns[3].XYZ, Matrix.Columns[2].W));
    
    float InvDeterminant = 1.0f / (HMM_DotV3(C01, B32) + HMM_DotV3(C23, B10));
    C01 = HMM_MulV3F(C01, InvDeterminant);
    C23 = HMM_MulV3F(C23, InvDeterminant);
    B10 = HMM_MulV3F(B10, InvDeterminant);
    B32 = HMM_MulV3F(B32, InvDeterminant);

    m4 Result;
    Result.Columns[0] = HMM_V4V(HMM_AddV3(HMM_Cross(Matrix.Columns[1].XYZ, B32), HMM_MulV3F(C23, Matrix.Columns[1].W)), -HMM_DotV3(Matrix.Columns[1].XYZ, C23));
    Result.Columns[1] = HMM_V4V(HMM_SubV3(HMM_Cross(B32, Matrix.Columns[0].XYZ), HMM_MulV3F(C23, Matrix.Columns[0].W)), +HMM_DotV3(Matrix.Columns[0].XYZ, C23));
    Result.Columns[2] = HMM_V4V(HMM_AddV3(HMM_Cross(Matrix.Columns[3].XYZ, B10), HMM_MulV3F(C01, Matrix.Columns[3].W)), -HMM_DotV3(Matrix.Columns[3].XYZ, C01));
    Result.Columns[3] = HMM_V4V(HMM_SubV3(HMM_Cross(B10, Matrix.Columns[2].XYZ), HMM_MulV3F(C01, Matrix.Columns[2].W)), +HMM_DotV3(Matrix.Columns[2].XYZ, C01));
        
    return HMM_TransposeM4(Result);
}

/*
 * Common graphics transformations
 */

COVERAGE(HMM_Orthographic_RH_NO, 1)
// Produces a right-handed orthographic projection matrix with Z ranging from -1 to 1 (the GL convention).
// Left, Right, Bottom, and Top specify the coordinates of their respective clipping planes.
// Near and Far specify the distances to the near and far clipping planes.
static inline m4 HMM_Orthographic_RH_NO(float Left, float Right, float Bottom, float Top, float Near, float Far)
{
    ASSERT_COVERED(HMM_Orthographic_RH_NO);

    m4 Result = {0};

    Result.Elements[0][0] = 2.0f / (Right - Left);
    Result.Elements[1][1] = 2.0f / (Top - Bottom);
    Result.Elements[2][2] = 2.0f / (Near - Far);
    Result.Elements[3][3] = 1.0f;

    Result.Elements[3][0] = (Left + Right) / (Left - Right);
    Result.Elements[3][1] = (Bottom + Top) / (Bottom - Top);
    Result.Elements[3][2] = (Near + Far) / (Near - Far);

    return Result;
}

COVERAGE(HMM_Orthographic_RH_ZO, 1)
// Produces a right-handed orthographic projection matrix with Z ranging from 0 to 1 (the DirectX convention).
// Left, Right, Bottom, and Top specify the coordinates of their respective clipping planes.
// Near and Far specify the distances to the near and far clipping planes.
static inline m4 HMM_Orthographic_RH_ZO(float Left, float Right, float Bottom, float Top, float Near, float Far)
{
    ASSERT_COVERED(HMM_Orthographic_RH_ZO);

    m4 Result = {0};

    Result.Elements[0][0] = 2.0f / (Right - Left);
    Result.Elements[1][1] = 2.0f / (Top - Bottom);
    Result.Elements[2][2] = 1.0f / (Near - Far);
    Result.Elements[3][3] = 1.0f;

    Result.Elements[3][0] = (Left + Right) / (Left - Right);
    Result.Elements[3][1] = (Bottom + Top) / (Bottom - Top);
    Result.Elements[3][2] = (Near) / (Near - Far);

    return Result;
}

COVERAGE(HMM_Orthographic_LH_NO, 1)
// Produces a left-handed orthographic projection matrix with Z ranging from -1 to 1 (the GL convention).
// Left, Right, Bottom, and Top specify the coordinates of their respective clipping planes.
// Near and Far specify the distances to the near and far clipping planes.
static inline m4 HMM_Orthographic_LH_NO(float Left, float Right, float Bottom, float Top, float Near, float Far)
{
    ASSERT_COVERED(HMM_Orthographic_LH_NO);

    m4 Result = HMM_Orthographic_RH_NO(Left, Right, Bottom, Top, Near, Far);
    Result.Elements[2][2] = -Result.Elements[2][2];
    
    return Result;
}

COVERAGE(HMM_Orthographic_LH_ZO, 1)
// Produces a left-handed orthographic projection matrix with Z ranging from 0 to 1 (the DirectX convention).
// Left, Right, Bottom, and Top specify the coordinates of their respective clipping planes.
// Near and Far specify the distances to the near and far clipping planes.
static inline m4 HMM_Orthographic_LH_ZO(float Left, float Right, float Bottom, float Top, float Near, float Far)
{
    ASSERT_COVERED(HMM_Orthographic_LH_ZO);

    m4 Result = HMM_Orthographic_RH_ZO(Left, Right, Bottom, Top, Near, Far);
    Result.Elements[2][2] = -Result.Elements[2][2];
    
    return Result;
}

COVERAGE(HMM_InvOrthographic, 1)
// Returns an inverse for the given orthographic projection matrix. Works for all orthographic
// projection matrices, regardless of handedness or NDC convention.
static inline m4 HMM_InvOrthographic(m4 OrthoMatrix)
{
    ASSERT_COVERED(HMM_InvOrthographic);

    m4 Result = {0};
    Result.Elements[0][0] = 1.0f / OrthoMatrix.Elements[0][0];
    Result.Elements[1][1] = 1.0f / OrthoMatrix.Elements[1][1];
    Result.Elements[2][2] = 1.0f / OrthoMatrix.Elements[2][2];
    Result.Elements[3][3] = 1.0f;
    
    Result.Elements[3][0] = -OrthoMatrix.Elements[3][0] * Result.Elements[0][0];
    Result.Elements[3][1] = -OrthoMatrix.Elements[3][1] * Result.Elements[1][1];
    Result.Elements[3][2] = -OrthoMatrix.Elements[3][2] * Result.Elements[2][2];

    return Result;
}

COVERAGE(HMM_Perspective_RH_NO, 1)
static inline m4 HMM_Perspective_RH_NO(float FOV, float AspectRatio, float Near, float Far)
{
    ASSERT_COVERED(HMM_Perspective_RH_NO);

    m4 Result = {0};

    // See https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluPerspective.xml

    float Cotangent = 1.0f / HMM_TanF(FOV / 2.0f);
    Result.Elements[0][0] = Cotangent / AspectRatio;
    Result.Elements[1][1] = Cotangent;
    Result.Elements[2][3] = -1.0f;

    Result.Elements[2][2] = (Near + Far) / (Near - Far);
    Result.Elements[3][2] = (2.0f * Near * Far) / (Near - Far);
    
    return Result;
}

COVERAGE(HMM_Perspective_RH_ZO, 1)
static inline m4 HMM_Perspective_RH_ZO(float FOV, float AspectRatio, float Near, float Far)
{
    ASSERT_COVERED(HMM_Perspective_RH_ZO);

    m4 Result = {0};

    // See https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluPerspective.xml

    float Cotangent = 1.0f / HMM_TanF(FOV / 2.0f);
    Result.Elements[0][0] = Cotangent / AspectRatio;
    Result.Elements[1][1] = Cotangent;
    Result.Elements[2][3] = -1.0f;

    Result.Elements[2][2] = (Far) / (Near - Far);
    Result.Elements[3][2] = (Near * Far) / (Near - Far);

    return Result;
}

COVERAGE(HMM_Perspective_LH_NO, 1)
static inline m4 HMM_Perspective_LH_NO(float FOV, float AspectRatio, float Near, float Far)
{ 
    ASSERT_COVERED(HMM_Perspective_LH_NO);

    m4 Result = HMM_Perspective_RH_NO(FOV, AspectRatio, Near, Far);
    Result.Elements[2][2] = -Result.Elements[2][2];
    Result.Elements[2][3] = -Result.Elements[2][3];
    
    return Result;
}

COVERAGE(HMM_Perspective_LH_ZO, 1)
static inline m4 HMM_Perspective_LH_ZO(float FOV, float AspectRatio, float Near, float Far)
{ 
    ASSERT_COVERED(HMM_Perspective_LH_ZO);

    m4 Result = HMM_Perspective_RH_ZO(FOV, AspectRatio, Near, Far);
    Result.Elements[2][2] = -Result.Elements[2][2];
    Result.Elements[2][3] = -Result.Elements[2][3];
    
    return Result;
}

COVERAGE(HMM_InvPerspective_RH, 1)
static inline m4 HMM_InvPerspective_RH(m4 PerspectiveMatrix)
{
    ASSERT_COVERED(HMM_InvPerspective_RH);

    m4 Result = {0};
    Result.Elements[0][0] = 1.0f / PerspectiveMatrix.Elements[0][0];
    Result.Elements[1][1] = 1.0f / PerspectiveMatrix.Elements[1][1];
    Result.Elements[2][2] = 0.0f;

    Result.Elements[2][3] = 1.0f / PerspectiveMatrix.Elements[3][2];
    Result.Elements[3][3] = PerspectiveMatrix.Elements[2][2] * Result.Elements[2][3];
    Result.Elements[3][2] = PerspectiveMatrix.Elements[2][3];

    return Result;
}

COVERAGE(HMM_InvPerspective_LH, 1)
static inline m4 HMM_InvPerspective_LH(m4 PerspectiveMatrix)
{
    ASSERT_COVERED(HMM_InvPerspective_LH);

    m4 Result = {0};
    Result.Elements[0][0] = 1.0f / PerspectiveMatrix.Elements[0][0];
    Result.Elements[1][1] = 1.0f / PerspectiveMatrix.Elements[1][1];
    Result.Elements[2][2] = 0.0f;

    Result.Elements[2][3] = 1.0f / PerspectiveMatrix.Elements[3][2];
    Result.Elements[3][3] = PerspectiveMatrix.Elements[2][2] * -Result.Elements[2][3];
    Result.Elements[3][2] = PerspectiveMatrix.Elements[2][3];

    return Result;
}

COVERAGE(HMM_Translate, 1)
static inline m4 HMM_Translate(v3 Translation)
{
    ASSERT_COVERED(HMM_Translate);

    m4 Result = HMM_M4D(1.0f);
    Result.Elements[3][0] = Translation.X;
    Result.Elements[3][1] = Translation.Y;
    Result.Elements[3][2] = Translation.Z;

    return Result;
}

COVERAGE(HMM_InvTranslate, 1)
static inline m4 HMM_InvTranslate(m4 TranslationMatrix)
{
    ASSERT_COVERED(HMM_InvTranslate);

    m4 Result = TranslationMatrix;
    Result.Elements[3][0] = -Result.Elements[3][0];
    Result.Elements[3][1] = -Result.Elements[3][1];
    Result.Elements[3][2] = -Result.Elements[3][2];

    return Result;
}

COVERAGE(HMM_Rotate_RH, 1)
static inline m4 HMM_Rotate_RH(float Angle, v3 Axis)
{
    ASSERT_COVERED(HMM_Rotate_RH);

    m4 Result = HMM_M4D(1.0f);

    Axis = norm(Axis);

    float SinTheta = HMM_SinF(Angle);
    float CosTheta = HMM_CosF(Angle);
    float CosValue = 1.0f - CosTheta;

    Result.Elements[0][0] = (Axis.X * Axis.X * CosValue) + CosTheta;
    Result.Elements[0][1] = (Axis.X * Axis.Y * CosValue) + (Axis.Z * SinTheta);
    Result.Elements[0][2] = (Axis.X * Axis.Z * CosValue) - (Axis.Y * SinTheta);

    Result.Elements[1][0] = (Axis.Y * Axis.X * CosValue) - (Axis.Z * SinTheta);
    Result.Elements[1][1] = (Axis.Y * Axis.Y * CosValue) + CosTheta;
    Result.Elements[1][2] = (Axis.Y * Axis.Z * CosValue) + (Axis.X * SinTheta);

    Result.Elements[2][0] = (Axis.Z * Axis.X * CosValue) + (Axis.Y * SinTheta);
    Result.Elements[2][1] = (Axis.Z * Axis.Y * CosValue) - (Axis.X * SinTheta);
    Result.Elements[2][2] = (Axis.Z * Axis.Z * CosValue) + CosTheta;

    return Result;
}

COVERAGE(HMM_Rotate_LH, 1)
static inline m4 HMM_Rotate_LH(float Angle, v3 Axis)
{
    ASSERT_COVERED(HMM_Rotate_LH);
    /* NOTE(lcf): Matrix will be inverse/transpose of RH. */
    return HMM_Rotate_RH(-Angle, Axis);
}

COVERAGE(HMM_InvRotate, 1)
static inline m4 HMM_InvRotate(m4 RotationMatrix)
{
    ASSERT_COVERED(HMM_InvRotate);
    return HMM_TransposeM4(RotationMatrix);
}

COVERAGE(HMM_Scale, 1)
static inline m4 HMM_Scale(v3 Scale)
{
    ASSERT_COVERED(HMM_Scale);

    m4 Result = HMM_M4D(1.0f);
    Result.Elements[0][0] = Scale.X;
    Result.Elements[1][1] = Scale.Y;
    Result.Elements[2][2] = Scale.Z;

    return Result;
}

COVERAGE(HMM_InvScale, 1)
static inline m4 HMM_InvScale(m4 ScaleMatrix) 
{
    ASSERT_COVERED(HMM_InvScale);

    m4 Result = ScaleMatrix;
    Result.Elements[0][0] = 1.0f / Result.Elements[0][0];
    Result.Elements[1][1] = 1.0f / Result.Elements[1][1];
    Result.Elements[2][2] = 1.0f / Result.Elements[2][2];

    return Result;
}

static inline m4 _HMM_LookAt(v3 F,  v3 S, v3 U,  v3 Eye)
{
    m4 Result;

    Result.Elements[0][0] = S.X;
    Result.Elements[0][1] = U.X;
    Result.Elements[0][2] = -F.X;
    Result.Elements[0][3] = 0.0f;

    Result.Elements[1][0] = S.Y;
    Result.Elements[1][1] = U.Y;
    Result.Elements[1][2] = -F.Y;
    Result.Elements[1][3] = 0.0f;

    Result.Elements[2][0] = S.Z;
    Result.Elements[2][1] = U.Z;
    Result.Elements[2][2] = -F.Z;
    Result.Elements[2][3] = 0.0f;

    Result.Elements[3][0] = -HMM_DotV3(S, Eye);
    Result.Elements[3][1] = -HMM_DotV3(U, Eye);
    Result.Elements[3][2] = HMM_DotV3(F, Eye);
    Result.Elements[3][3] = 1.0f;

    return Result;
}

COVERAGE(HMM_LookAt_RH, 1)
static inline m4 HMM_LookAt_RH(v3 Eye, v3 Center, v3 Up)
{
    ASSERT_COVERED(HMM_LookAt_RH);

    v3 F = norm(HMM_SubV3(Center, Eye));
    v3 S = norm(HMM_Cross(F, Up));
    v3 U = HMM_Cross(S, F);

    return _HMM_LookAt(F, S, U, Eye);
}

COVERAGE(HMM_LookAt_LH, 1)
static inline m4 HMM_LookAt_LH(v3 Eye, v3 Center, v3 Up)
{
    ASSERT_COVERED(HMM_LookAt_LH);

    v3 F = norm(HMM_SubV3(Eye, Center));
    v3 S = norm(HMM_Cross(F, Up));
    v3 U = HMM_Cross(S, F);

    return _HMM_LookAt(F, S, U, Eye);
}

COVERAGE(HMM_InvLookAt, 1)
static inline m4 HMM_InvLookAt(m4 Matrix)
{
    ASSERT_COVERED(HMM_InvLookAt);
    m4 Result;

    m3 Rotation = {0};
    Rotation.Columns[0] = Matrix.Columns[0].XYZ;
    Rotation.Columns[1] = Matrix.Columns[1].XYZ;
    Rotation.Columns[2] = Matrix.Columns[2].XYZ;
    Rotation = HMM_TransposeM3(Rotation);

    Result.Columns[0] = HMM_V4V(Rotation.Columns[0], 0.0f);
    Result.Columns[1] = HMM_V4V(Rotation.Columns[1], 0.0f);
    Result.Columns[2] = HMM_V4V(Rotation.Columns[2], 0.0f);
    Result.Columns[3] = HMM_MulV4F(Matrix.Columns[3], -1.0f);
    Result.Elements[3][0] = -1.0f * Matrix.Elements[3][0] /
        (Rotation.Elements[0][0] + Rotation.Elements[0][1] + Rotation.Elements[0][2]);
    Result.Elements[3][1] = -1.0f * Matrix.Elements[3][1] /
        (Rotation.Elements[1][0] + Rotation.Elements[1][1] + Rotation.Elements[1][2]);
    Result.Elements[3][2] = -1.0f * Matrix.Elements[3][2] /
        (Rotation.Elements[2][0] + Rotation.Elements[2][1] + Rotation.Elements[2][2]);
    Result.Elements[3][3] = 1.0f;

    return Result;
}

/*
 * Quaternion operations
 */

COVERAGE(HMM_Q, 1)
static inline HMM_Quat HMM_Q(float X, float Y, float Z, float W)
{
    ASSERT_COVERED(HMM_Q);

    HMM_Quat Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_setr_ps(X, Y, Z, W);
#else
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    Result.W = W;
#endif

    return Result;
}

COVERAGE(HMM_QV4, 1)
static inline HMM_Quat HMM_QV4(v4 vtor)
{
    ASSERT_COVERED(HMM_QV4);

    HMM_Quat Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = vtor.SSE;
#else
    Result.X = vtor.X;
    Result.Y = vtor.Y;
    Result.Z = vtor.Z;
    Result.W = vtor.W;
#endif

    return Result;
}

COVERAGE(HMM_AddQ, 1)
static inline HMM_Quat HMM_AddQ(HMM_Quat Left, HMM_Quat Right)
{
    ASSERT_COVERED(HMM_AddQ);

    HMM_Quat Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_add_ps(Left.SSE, Right.SSE);
#else

    Result.X = Left.X + Right.X;
    Result.Y = Left.Y + Right.Y;
    Result.Z = Left.Z + Right.Z;
    Result.W = Left.W + Right.W;
#endif

    return Result;
}

COVERAGE(HMM_SubQ, 1)
static inline HMM_Quat HMM_SubQ(HMM_Quat Left, HMM_Quat Right)
{
    ASSERT_COVERED(HMM_SubQ);

    HMM_Quat Result;

#ifdef HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_sub_ps(Left.SSE, Right.SSE);
#else
    Result.X = Left.X - Right.X;
    Result.Y = Left.Y - Right.Y;
    Result.Z = Left.Z - Right.Z;
    Result.W = Left.W - Right.W;
#endif

    return Result;
}

COVERAGE(HMM_MulQ, 1)
static inline HMM_Quat HMM_MulQ(HMM_Quat Left, HMM_Quat Right)
{
    ASSERT_COVERED(HMM_MulQ);

    HMM_Quat Result;

#ifdef HANDMADE_MATH__USE_SSE
    __m128 SSEResultOne = _mm_xor_ps(_mm_shuffle_ps(Left.SSE, Left.SSE, _MM_SHUFFLE(0, 0, 0, 0)), _mm_setr_ps(0.f, -0.f, 0.f, -0.f));
    __m128 SSEResultTwo = _mm_shuffle_ps(Right.SSE, Right.SSE, _MM_SHUFFLE(0, 1, 2, 3));
    __m128 SSEResultThree = _mm_mul_ps(SSEResultTwo, SSEResultOne);

    SSEResultOne = _mm_xor_ps(_mm_shuffle_ps(Left.SSE, Left.SSE, _MM_SHUFFLE(1, 1, 1, 1)) , _mm_setr_ps(0.f, 0.f, -0.f, -0.f));
    SSEResultTwo = _mm_shuffle_ps(Right.SSE, Right.SSE, _MM_SHUFFLE(1, 0, 3, 2));
    SSEResultThree = _mm_add_ps(SSEResultThree, _mm_mul_ps(SSEResultTwo, SSEResultOne));

    SSEResultOne = _mm_xor_ps(_mm_shuffle_ps(Left.SSE, Left.SSE, _MM_SHUFFLE(2, 2, 2, 2)), _mm_setr_ps(-0.f, 0.f, 0.f, -0.f));
    SSEResultTwo = _mm_shuffle_ps(Right.SSE, Right.SSE, _MM_SHUFFLE(2, 3, 0, 1));
    SSEResultThree = _mm_add_ps(SSEResultThree, _mm_mul_ps(SSEResultTwo, SSEResultOne));

    SSEResultOne = _mm_shuffle_ps(Left.SSE, Left.SSE, _MM_SHUFFLE(3, 3, 3, 3));
    SSEResultTwo = _mm_shuffle_ps(Right.SSE, Right.SSE, _MM_SHUFFLE(3, 2, 1, 0));
    Result.SSE = _mm_add_ps(SSEResultThree, _mm_mul_ps(SSEResultTwo, SSEResultOne));
#else
    Result.X =  Right.Elements[3] * +Left.Elements[0];
    Result.Y =  Right.Elements[2] * -Left.Elements[0];
    Result.Z =  Right.Elements[1] * +Left.Elements[0];
    Result.W =  Right.Elements[0] * -Left.Elements[0];

    Result.X += Right.Elements[2] * +Left.Elements[1];
    Result.Y += Right.Elements[3] * +Left.Elements[1];
    Result.Z += Right.Elements[0] * -Left.Elements[1];
    Result.W += Right.Elements[1] * -Left.Elements[1];
    
    Result.X += Right.Elements[1] * -Left.Elements[2];
    Result.Y += Right.Elements[0] * +Left.Elements[2];
    Result.Z += Right.Elements[3] * +Left.Elements[2];
    Result.W += Right.Elements[2] * -Left.Elements[2];

    Result.X += Right.Elements[0] * +Left.Elements[3];
    Result.Y += Right.Elements[1] * +Left.Elements[3];
    Result.Z += Right.Elements[2] * +Left.Elements[3];
    Result.W += Right.Elements[3] * +Left.Elements[3];
#endif

    return Result;
}

COVERAGE(HMM_MulQF, 1)
static inline HMM_Quat HMM_MulQF(HMM_Quat Left, float Multiplicative)
{
    ASSERT_COVERED(HMM_MulQF);

    HMM_Quat Result;

#ifdef HANDMADE_MATH__USE_SSE
    __m128 Scalar = _mm_set1_ps(Multiplicative);
    Result.SSE = _mm_mul_ps(Left.SSE, Scalar);
#else
    Result.X = Left.X * Multiplicative;
    Result.Y = Left.Y * Multiplicative;
    Result.Z = Left.Z * Multiplicative;
    Result.W = Left.W * Multiplicative;
#endif

    return Result;
}

COVERAGE(HMM_DivQF, 1)
static inline HMM_Quat HMM_DivQF(HMM_Quat Left, float Divnd)
{
    ASSERT_COVERED(HMM_DivQF);

    HMM_Quat Result;

#ifdef HANDMADE_MATH__USE_SSE
    __m128 Scalar = _mm_set1_ps(Divnd);
    Result.SSE = _mm_div_ps(Left.SSE, Scalar);
#else
    Result.X = Left.X / Divnd;
    Result.Y = Left.Y / Divnd;
    Result.Z = Left.Z / Divnd;
    Result.W = Left.W / Divnd;
#endif

    return Result;
}

COVERAGE(HMM_DotQ, 1)
static inline float HMM_DotQ(HMM_Quat Left, HMM_Quat Right)
{
    ASSERT_COVERED(HMM_DotQ);

    float Result;

#ifdef HANDMADE_MATH__USE_SSE
    __m128 SSEResultOne = _mm_mul_ps(Left.SSE, Right.SSE);
    __m128 SSEResultTwo = _mm_shuffle_ps(SSEResultOne, SSEResultOne, _MM_SHUFFLE(2, 3, 0, 1));
    SSEResultOne = _mm_add_ps(SSEResultOne, SSEResultTwo);
    SSEResultTwo = _mm_shuffle_ps(SSEResultOne, SSEResultOne, _MM_SHUFFLE(0, 1, 2, 3));
    SSEResultOne = _mm_add_ps(SSEResultOne, SSEResultTwo);
    _mm_store_ss(&Result, SSEResultOne);
#else
    Result = ((Left.X * Right.X) + (Left.Z * Right.Z)) + ((Left.Y * Right.Y) + (Left.W * Right.W));
#endif

    return Result;
}

COVERAGE(HMM_InvQ, 1)
static inline HMM_Quat HMM_InvQ(HMM_Quat Left)
{
    ASSERT_COVERED(HMM_InvQ);
    
    HMM_Quat Result;
    Result.X = -Left.X;
    Result.Y = -Left.Y;
    Result.Z = -Left.Z;
    Result.W = Left.W;

    return HMM_DivQF(Result, (HMM_DotQ(Left, Left)));
}

COVERAGE(HMM_NormQ, 1)
static inline HMM_Quat HMM_NormQ(HMM_Quat Quat)
{
    ASSERT_COVERED(HMM_NormQ);

    /* NOTE(lcf): Take advantage of SSE implementation in HMM_NormV4 */
    v4 v = {Quat.X, Quat.Y, Quat.Z, Quat.W};
    v = norm(v);
    HMM_Quat Result = {v.X, v.Y, v.Z, v.W};

    return Result;
}

static inline HMM_Quat _HMM_MixQ(HMM_Quat Left, float MixLeft, HMM_Quat Right, float MixRight) {
    HMM_Quat Result;

#ifdef HANDMADE_MATH__USE_SSE
    __m128 ScalarLeft = _mm_set1_ps(MixLeft);
    __m128 ScalarRight = _mm_set1_ps(MixRight);
    __m128 SSEResultOne = _mm_mul_ps(Left.SSE, ScalarLeft);
    __m128 SSEResultTwo = _mm_mul_ps(Right.SSE, ScalarRight);
    Result.SSE = _mm_add_ps(SSEResultOne, SSEResultTwo);
#else
    Result.X = Left.X*MixLeft + Right.X*MixRight;
    Result.Y = Left.Y*MixLeft + Right.Y*MixRight;
    Result.Z = Left.Z*MixLeft + Right.Z*MixRight;
    Result.W = Left.W*MixLeft + Right.W*MixRight;
#endif

    return Result;
}

COVERAGE(HMM_NLerp, 1)
static inline HMM_Quat HMM_NLerp(HMM_Quat Left, float Time, HMM_Quat Right)
{
    ASSERT_COVERED(HMM_NLerp);

    HMM_Quat Result = _HMM_MixQ(Left, 1.0f-Time, Right, Time);
    Result = HMM_NormQ(Result);

    return Result;
}

COVERAGE(HMM_SLerp, 1)
static inline HMM_Quat HMM_SLerp(HMM_Quat Left, float Time, HMM_Quat Right)
{
    ASSERT_COVERED(HMM_SLerp);

    HMM_Quat Result;

    float Cos_Theta = HMM_DotQ(Left, Right);

    if (Cos_Theta < 0.0f) { /* NOTE(lcf): Take shortest path on Hyper-sphere */
        Cos_Theta = -Cos_Theta;
        Right = HMM_Q(-Right.X, -Right.Y, -Right.Z, -Right.W);
    }
    
    /* NOTE(lcf): Use Normalized Linear interpolation when vtors are roughly not L.I. */
    if (Cos_Theta > 0.9995f) {
        Result = HMM_NLerp(Left, Time, Right);
    } else {
        float Angle = HMM_ACosF(Cos_Theta);
        float MixLeft = HMM_SinF((1.0f - Time) * Angle);
        float MixRight = HMM_SinF(Time * Angle);

        Result = _HMM_MixQ(Left, MixLeft, Right, MixRight);
        Result = HMM_NormQ(Result);
    }
    
    return Result;
}

COVERAGE(HMM_QToM4, 1)
static inline m4 HMM_QToM4(HMM_Quat Left)
{
    ASSERT_COVERED(HMM_QToM4);

    m4 Result;

    HMM_Quat NormalizedQ = HMM_NormQ(Left);

    float XX, YY, ZZ,
          XY, XZ, YZ,
          WX, WY, WZ;

    XX = NormalizedQ.X * NormalizedQ.X;
    YY = NormalizedQ.Y * NormalizedQ.Y;
    ZZ = NormalizedQ.Z * NormalizedQ.Z;
    XY = NormalizedQ.X * NormalizedQ.Y;
    XZ = NormalizedQ.X * NormalizedQ.Z;
    YZ = NormalizedQ.Y * NormalizedQ.Z;
    WX = NormalizedQ.W * NormalizedQ.X;
    WY = NormalizedQ.W * NormalizedQ.Y;
    WZ = NormalizedQ.W * NormalizedQ.Z;

    Result.Elements[0][0] = 1.0f - 2.0f * (YY + ZZ);
    Result.Elements[0][1] = 2.0f * (XY + WZ);
    Result.Elements[0][2] = 2.0f * (XZ - WY);
    Result.Elements[0][3] = 0.0f;

    Result.Elements[1][0] = 2.0f * (XY - WZ);
    Result.Elements[1][1] = 1.0f - 2.0f * (XX + ZZ);
    Result.Elements[1][2] = 2.0f * (YZ + WX);
    Result.Elements[1][3] = 0.0f;

    Result.Elements[2][0] = 2.0f * (XZ + WY);
    Result.Elements[2][1] = 2.0f * (YZ - WX);
    Result.Elements[2][2] = 1.0f - 2.0f * (XX + YY);
    Result.Elements[2][3] = 0.0f;

    Result.Elements[3][0] = 0.0f;
    Result.Elements[3][1] = 0.0f;
    Result.Elements[3][2] = 0.0f;
    Result.Elements[3][3] = 1.0f;

    return Result;
}

// This method taken from Mike Day at Insomniac Games.
// https://d3cw3dd2w32x2b.cloudfront.net/wp-content/uploads/2015/01/matrix-to-quat.pdf
//
// Note that as mentioned at the top of the paper, the paper assumes the matrix
// would be *post*-multiplied to a vtor to rotate it, meaning the matrix is
// the transpose of what we're dealing with. But, because our matrices are
// stored in column-major order, the indices *appear* to match the paper.
//
// For example, m12 in the paper is row 1, column 2. We need to transpose it to
// row 2, column 1. But, because the column comes first when referencing
// elements, it looks like M.Elements[1][2].
//
// Don't be confused! Or if you must be confused, at least trust this
// comment. :)
COVERAGE(HMM_M4ToQ_RH, 4)
static inline HMM_Quat HMM_M4ToQ_RH(m4 M)
{
    float T;
    HMM_Quat Q;

    if (M.Elements[2][2] < 0.0f) {
        if (M.Elements[0][0] > M.Elements[1][1]) {
            ASSERT_COVERED(HMM_M4ToQ_RH);

            T = 1 + M.Elements[0][0] - M.Elements[1][1] - M.Elements[2][2];
            Q = HMM_Q(
                T,
                M.Elements[0][1] + M.Elements[1][0],
                M.Elements[2][0] + M.Elements[0][2],
                M.Elements[1][2] - M.Elements[2][1]
            );
        } else {
            ASSERT_COVERED(HMM_M4ToQ_RH);

            T = 1 - M.Elements[0][0] + M.Elements[1][1] - M.Elements[2][2];
            Q = HMM_Q(
                M.Elements[0][1] + M.Elements[1][0],
                T,
                M.Elements[1][2] + M.Elements[2][1],
                M.Elements[2][0] - M.Elements[0][2]
            );
        }
    } else {
        if (M.Elements[0][0] < -M.Elements[1][1]) {
            ASSERT_COVERED(HMM_M4ToQ_RH);

            T = 1 - M.Elements[0][0] - M.Elements[1][1] + M.Elements[2][2];
            Q = HMM_Q(
                M.Elements[2][0] + M.Elements[0][2],
                M.Elements[1][2] + M.Elements[2][1],
                T,
                M.Elements[0][1] - M.Elements[1][0]
            );
        } else {
            ASSERT_COVERED(HMM_M4ToQ_RH);

            T = 1 + M.Elements[0][0] + M.Elements[1][1] + M.Elements[2][2];
            Q = HMM_Q(
                M.Elements[1][2] - M.Elements[2][1],
                M.Elements[2][0] - M.Elements[0][2],
                M.Elements[0][1] - M.Elements[1][0],
                T
            );
        }
    }

    Q = HMM_MulQF(Q, 0.5f / HMM_SqrtF(T));

    return Q;
}

COVERAGE(HMM_M4ToQ_LH, 4)
static inline HMM_Quat HMM_M4ToQ_LH(m4 M)
{
    float T;
    HMM_Quat Q;

    if (M.Elements[2][2] < 0.0f) {
        if (M.Elements[0][0] > M.Elements[1][1]) {
            ASSERT_COVERED(HMM_M4ToQ_LH);

            T = 1 + M.Elements[0][0] - M.Elements[1][1] - M.Elements[2][2];
            Q = HMM_Q(
                T,
                M.Elements[0][1] + M.Elements[1][0],
                M.Elements[2][0] + M.Elements[0][2],
                M.Elements[2][1] - M.Elements[1][2]
            );
        } else {
            ASSERT_COVERED(HMM_M4ToQ_LH);

            T = 1 - M.Elements[0][0] + M.Elements[1][1] - M.Elements[2][2];
            Q = HMM_Q(
                M.Elements[0][1] + M.Elements[1][0],
                T,
                M.Elements[1][2] + M.Elements[2][1],
                M.Elements[0][2] - M.Elements[2][0]
            );
        }
    } else {
        if (M.Elements[0][0] < -M.Elements[1][1]) {
            ASSERT_COVERED(HMM_M4ToQ_LH);

            T = 1 - M.Elements[0][0] - M.Elements[1][1] + M.Elements[2][2];
            Q = HMM_Q(
                M.Elements[2][0] + M.Elements[0][2],
                M.Elements[1][2] + M.Elements[2][1],
                T,
                M.Elements[1][0] - M.Elements[0][1]
            );
        } else {
            ASSERT_COVERED(HMM_M4ToQ_LH);

            T = 1 + M.Elements[0][0] + M.Elements[1][1] + M.Elements[2][2];
            Q = HMM_Q(
                M.Elements[2][1] - M.Elements[1][2],
                M.Elements[0][2] - M.Elements[2][0],
                M.Elements[1][0] - M.Elements[0][2],
                T
            );
        }
    }

    Q = HMM_MulQF(Q, 0.5f / HMM_SqrtF(T));

    return Q;
}


COVERAGE(HMM_QFromAxisAngle_RH, 1)
static inline HMM_Quat HMM_QFromAxisAngle_RH(v3 Axis, float AngleOfRotation)
{
    ASSERT_COVERED(HMM_QFromAxisAngle_RH);

    HMM_Quat Result;

    v3 AxisNormalized = norm(Axis);
    float SineOfRotation = HMM_SinF(AngleOfRotation / 2.0f);

    Result.XYZ = HMM_MulV3F(AxisNormalized, SineOfRotation);
    Result.W = HMM_CosF(AngleOfRotation / 2.0f);

    return Result;
}

COVERAGE(HMM_QFromAxisAngle_LH, 1)
static inline HMM_Quat HMM_QFromAxisAngle_LH(v3 Axis, float AngleOfRotation)
{
    ASSERT_COVERED(HMM_QFromAxisAngle_LH);

    return HMM_QFromAxisAngle_RH(Axis, -AngleOfRotation);
}


#ifdef __cplusplus

COVERAGE(HMM_NormQCPP, 1)
static inline HMM_Quat HMM_Norm(HMM_Quat A)
{
    ASSERT_COVERED(HMM_NormQCPP);
    return HMM_NormQ(A);
}

COVERAGE(HMM_DotV2CPP, 1)
static inline float HMM_Dot(v2 Left, v2 vTwo)
{
    ASSERT_COVERED(HMM_DotV2CPP);
    return HMM_DotV2(Left, vTwo);
}

COVERAGE(HMM_DotV3CPP, 1)
static inline float HMM_Dot(v3 Left, v3 vTwo)
{
    ASSERT_COVERED(HMM_DotV3CPP);
    return HMM_DotV3(Left, vTwo);
}

COVERAGE(HMM_DotV4CPP, 1)
static inline float HMM_Dot(v4 Left, v4 vTwo)
{
    ASSERT_COVERED(HMM_DotV4CPP);
    return HMM_DotV4(Left, vTwo);
}
 
COVERAGE(HMM_LerpV2CPP, 1)
static inline v2 HMM_Lerp(v2 Left, float Time, v2 Right) 
{
    ASSERT_COVERED(HMM_LerpV2CPP);
    return HMM_LerpV2(Left, Time, Right);
}

COVERAGE(HMM_LerpV3CPP, 1)
static inline v3 HMM_Lerp(v3 Left, float Time, v3 Right) 
{
    ASSERT_COVERED(HMM_LerpV3CPP);
    return HMM_LerpV3(Left, Time, Right);
}

COVERAGE(HMM_LerpV4CPP, 1)
static inline v4 HMM_Lerp(v4 Left, float Time, v4 Right) 
{
    ASSERT_COVERED(HMM_LerpV4CPP);
    return HMM_LerpV4(Left, Time, Right);
}

COVERAGE(HMM_TransposeM2CPP, 1)
static inline m2 HMM_Transpose(m2 Matrix)
{
    ASSERT_COVERED(HMM_TransposeM2CPP);
    return HMM_TransposeM2(Matrix);
}

COVERAGE(HMM_TransposeM3CPP, 1)
static inline m3 HMM_Transpose(m3 Matrix)
{
    ASSERT_COVERED(HMM_TransposeM3CPP);
    return HMM_TransposeM3(Matrix);
}

COVERAGE(HMM_TransposeM4CPP, 1)
static inline m4 HMM_Transpose(m4 Matrix)
{
    ASSERT_COVERED(HMM_TransposeM4CPP);
    return HMM_TransposeM4(Matrix);
}

COVERAGE(HMM_DeterminantM2CPP, 1)
static inline float HMM_Determinant(m2 Matrix)
{
    ASSERT_COVERED(HMM_DeterminantM2CPP);
    return HMM_DeterminantM2(Matrix);
}

COVERAGE(HMM_DeterminantM3CPP, 1)
static inline float HMM_Determinant(m3 Matrix)
{
    ASSERT_COVERED(HMM_DeterminantM3CPP);
    return HMM_DeterminantM3(Matrix);
}

COVERAGE(HMM_DeterminantM4CPP, 1)
static inline float HMM_Determinant(m4 Matrix)
{
    ASSERT_COVERED(HMM_DeterminantM4CPP);
    return HMM_DeterminantM4(Matrix);
}

COVERAGE(HMM_InvGeneralM2CPP, 1)
static inline m2 HMM_InvGeneral(m2 Matrix)
{
    ASSERT_COVERED(HMM_InvGeneralM2CPP);
    return HMM_InvGeneralM2(Matrix);
}

COVERAGE(HMM_InvGeneralM3CPP, 1)
static inline m3 HMM_InvGeneral(m3 Matrix)
{
    ASSERT_COVERED(HMM_InvGeneralM3CPP);
    return HMM_InvGeneralM3(Matrix);
}

COVERAGE(HMM_InvGeneralM4CPP, 1)
static inline m4 HMM_InvGeneral(m4 Matrix)
{
    ASSERT_COVERED(HMM_InvGeneralM4CPP);
    return HMM_InvGeneralM4(Matrix);
}

COVERAGE(HMM_DotQCPP, 1)
static inline float HMM_Dot(HMM_Quat QuatOne, HMM_Quat QuatTwo)
{
    ASSERT_COVERED(HMM_DotQCPP);
    return HMM_DotQ(QuatOne, QuatTwo);
}

COVERAGE(HMM_AddV2CPP, 1)
static inline v2 HMM_Add(v2 Left, v2 Right)
{
    ASSERT_COVERED(HMM_AddV2CPP);
    return HMM_AddV2(Left, Right);
}

COVERAGE(HMM_AddV3CPP, 1)
static inline v3 HMM_Add(v3 Left, v3 Right)
{
    ASSERT_COVERED(HMM_AddV3CPP);
    return HMM_AddV3(Left, Right);
}

COVERAGE(HMM_AddV4CPP, 1)
static inline v4 HMM_Add(v4 Left, v4 Right)
{
    ASSERT_COVERED(HMM_AddV4CPP);
    return HMM_AddV4(Left, Right);
}

COVERAGE(HMM_AddM2CPP, 1)
static inline m2 HMM_Add(m2 Left, m2 Right)
{
    ASSERT_COVERED(HMM_AddM2CPP);
    return HMM_AddM2(Left, Right);
}

COVERAGE(HMM_AddM3CPP, 1)
static inline m3 HMM_Add(m3 Left, m3 Right)
{
    ASSERT_COVERED(HMM_AddM3CPP);
    return HMM_AddM3(Left, Right);
}

COVERAGE(HMM_AddM4CPP, 1)
static inline m4 HMM_Add(m4 Left, m4 Right)
{
    ASSERT_COVERED(HMM_AddM4CPP);
    return HMM_AddM4(Left, Right);
}

COVERAGE(HMM_AddQCPP, 1)
static inline HMM_Quat HMM_Add(HMM_Quat Left, HMM_Quat Right)
{
    ASSERT_COVERED(HMM_AddQCPP);
    return HMM_AddQ(Left, Right);
}

COVERAGE(HMM_SubV2CPP, 1)
static inline v2 HMM_Sub(v2 Left, v2 Right)
{
    ASSERT_COVERED(HMM_SubV2CPP);
    return HMM_SubV2(Left, Right);
}

COVERAGE(HMM_SubV3CPP, 1)
static inline v3 HMM_Sub(v3 Left, v3 Right)
{
    ASSERT_COVERED(HMM_SubV3CPP);
    return HMM_SubV3(Left, Right);
}

COVERAGE(HMM_SubV4CPP, 1)
static inline v4 HMM_Sub(v4 Left, v4 Right)
{
    ASSERT_COVERED(HMM_SubV4CPP);
    return HMM_SubV4(Left, Right);
}

COVERAGE(HMM_SubM2CPP, 1)
static inline m2 HMM_Sub(m2 Left, m2 Right)
{
    ASSERT_COVERED(HMM_SubM2CPP);
    return HMM_SubM2(Left, Right);
}

COVERAGE(HMM_SubM3CPP, 1)
static inline m3 HMM_Sub(m3 Left, m3 Right)
{
    ASSERT_COVERED(HMM_SubM3CPP);
    return HMM_SubM3(Left, Right);
}

COVERAGE(HMM_SubM4CPP, 1)
static inline m4 HMM_Sub(m4 Left, m4 Right)
{
    ASSERT_COVERED(HMM_SubM4CPP);
    return HMM_SubM4(Left, Right);
}

COVERAGE(HMM_SubQCPP, 1)
static inline HMM_Quat HMM_Sub(HMM_Quat Left, HMM_Quat Right)
{
    ASSERT_COVERED(HMM_SubQCPP);
    return HMM_SubQ(Left, Right);
}

COVERAGE(HMM_MulV2CPP, 1)
static inline v2 HMM_Mul(v2 Left, v2 Right)
{
    ASSERT_COVERED(HMM_MulV2CPP);
    return HMM_MulV2(Left, Right);
}

COVERAGE(HMM_MulV2FCPP, 1)
static inline v2 HMM_Mul(v2 Left, float Right)
{
    ASSERT_COVERED(HMM_MulV2FCPP);
    return HMM_MulV2F(Left, Right);
}

COVERAGE(HMM_MulV3CPP, 1)
static inline v3 HMM_Mul(v3 Left, v3 Right)
{
    ASSERT_COVERED(HMM_MulV3CPP);
    return HMM_MulV3(Left, Right);
}

COVERAGE(HMM_MulV3FCPP, 1)
static inline v3 HMM_Mul(v3 Left, float Right)
{
    ASSERT_COVERED(HMM_MulV3FCPP);
    return HMM_MulV3F(Left, Right);
}

COVERAGE(HMM_MulV4CPP, 1)
static inline v4 HMM_Mul(v4 Left, v4 Right)
{
    ASSERT_COVERED(HMM_MulV4CPP);
    return HMM_MulV4(Left, Right);
}

COVERAGE(HMM_MulV4FCPP, 1)
static inline v4 HMM_Mul(v4 Left, float Right)
{
    ASSERT_COVERED(HMM_MulV4FCPP);
    return HMM_MulV4F(Left, Right);
}

COVERAGE(HMM_MulM2CPP, 1)
static inline m2 HMM_Mul(m2 Left, m2 Right)
{
    ASSERT_COVERED(HMM_MulM2CPP);
    return HMM_MulM2(Left, Right);
}

COVERAGE(HMM_MulM3CPP, 1)
static inline m3 HMM_Mul(m3 Left, m3 Right)
{
    ASSERT_COVERED(HMM_MulM3CPP);
    return HMM_MulM3(Left, Right);
}

COVERAGE(HMM_MulM4CPP, 1)
static inline m4 HMM_Mul(m4 Left, m4 Right)
{
    ASSERT_COVERED(HMM_MulM4CPP);
    return HMM_MulM4(Left, Right);
}

COVERAGE(HMM_MulM2FCPP, 1)
static inline m2 HMM_Mul(m2 Left, float Right)
{
    ASSERT_COVERED(HMM_MulM2FCPP);
    return HMM_MulM2F(Left, Right);
}

COVERAGE(HMM_MulM3FCPP, 1)
static inline m3 HMM_Mul(m3 Left, float Right)
{
    ASSERT_COVERED(HMM_MulM3FCPP);
    return HMM_MulM3F(Left, Right);
}

COVERAGE(HMM_MulM4FCPP, 1)
static inline m4 HMM_Mul(m4 Left, float Right)
{
    ASSERT_COVERED(HMM_MulM4FCPP);
    return HMM_MulM4F(Left, Right);
}

COVERAGE(HMM_MulM2V2CPP, 1)
static inline v2 HMM_Mul(m2 Matrix, v2 vtor)
{
    ASSERT_COVERED(HMM_MulM2V2CPP);
    return HMM_MulM2V2(Matrix, vtor);
}

COVERAGE(HMM_MulM3V3CPP, 1)
static inline v3 HMM_Mul(m3 Matrix, v3 vtor)
{
    ASSERT_COVERED(HMM_MulM3V3CPP);
    return HMM_MulM3V3(Matrix, vtor);
}

COVERAGE(HMM_MulM4V4CPP, 1)
static inline v4 HMM_Mul(m4 Matrix, v4 vtor)
{
    ASSERT_COVERED(HMM_MulM4V4CPP);
    return HMM_MulM4V4(Matrix, vtor);
}

COVERAGE(HMM_MulQCPP, 1)
static inline HMM_Quat HMM_Mul(HMM_Quat Left, HMM_Quat Right)
{
    ASSERT_COVERED(HMM_MulQCPP);
    return HMM_MulQ(Left, Right);
}

COVERAGE(HMM_MulQFCPP, 1)
static inline HMM_Quat HMM_Mul(HMM_Quat Left, float Right)
{
    ASSERT_COVERED(HMM_MulQFCPP);
    return HMM_MulQF(Left, Right);
}

COVERAGE(HMM_DivV2CPP, 1)
static inline v2 HMM_Div(v2 Left, v2 Right)
{
    ASSERT_COVERED(HMM_DivV2CPP);
    return HMM_DivV2(Left, Right);
}

COVERAGE(HMM_DivV2FCPP, 1)
static inline v2 HMM_Div(v2 Left, float Right)
{
    ASSERT_COVERED(HMM_DivV2FCPP);
    return HMM_DivV2F(Left, Right);
}

COVERAGE(HMM_DivV3CPP, 1)
static inline v3 HMM_Div(v3 Left, v3 Right)
{
    ASSERT_COVERED(HMM_DivV3CPP);
    return HMM_DivV3(Left, Right);
}

COVERAGE(HMM_DivV3FCPP, 1)
static inline v3 HMM_Div(v3 Left, float Right)
{
    ASSERT_COVERED(HMM_DivV3FCPP);
    return HMM_DivV3F(Left, Right);
}

COVERAGE(HMM_DivV4CPP, 1)
static inline v4 HMM_Div(v4 Left, v4 Right)
{
    ASSERT_COVERED(HMM_DivV4CPP);
    return HMM_DivV4(Left, Right);
}

COVERAGE(HMM_DivV4FCPP, 1)
static inline v4 HMM_Div(v4 Left, float Right)
{
    ASSERT_COVERED(HMM_DivV4FCPP);
    return HMM_DivV4F(Left, Right);
}

COVERAGE(HMM_DivM2FCPP, 1)
static inline m2 HMM_Div(m2 Left, float Right)
{
    ASSERT_COVERED(HMM_DivM2FCPP);
    return HMM_DivM2F(Left, Right);
}

COVERAGE(HMM_DivM3FCPP, 1)
static inline m3 HMM_Div(m3 Left, float Right)
{
    ASSERT_COVERED(HMM_DivM3FCPP);
    return HMM_DivM3F(Left, Right);
}

COVERAGE(HMM_DivM4FCPP, 1)
static inline m4 HMM_Div(m4 Left, float Right)
{
    ASSERT_COVERED(HMM_DivM4FCPP);
    return HMM_DivM4F(Left, Right);
}

COVERAGE(HMM_DivQFCPP, 1)
static inline HMM_Quat HMM_Div(HMM_Quat Left, float Right)
{
    ASSERT_COVERED(HMM_DivQFCPP);
    return HMM_DivQF(Left, Right);
}

COVERAGE(HMM_EqV2CPP, 1)
static inline HMM_Bool HMM_Eq(v2 Left, v2 Right)
{
    ASSERT_COVERED(HMM_EqV2CPP);
    return HMM_EqV2(Left, Right);
}

COVERAGE(HMM_EqV3CPP, 1)
static inline HMM_Bool HMM_Eq(v3 Left, v3 Right)
{
    ASSERT_COVERED(HMM_EqV3CPP);
    return HMM_EqV3(Left, Right);
}

COVERAGE(HMM_EqV4CPP, 1)
static inline HMM_Bool HMM_Eq(v4 Left, v4 Right)
{
    ASSERT_COVERED(HMM_EqV4CPP);
    return HMM_EqV4(Left, Right);
}

COVERAGE(HMM_AddV2Op, 1)
static inline v2 operator+(v2 Left, v2 Right)
{
    ASSERT_COVERED(HMM_AddV2Op);
    return HMM_AddV2(Left, Right);
}

COVERAGE(HMM_AddV3Op, 1)
static inline v3 operator+(v3 Left, v3 Right)
{
    ASSERT_COVERED(HMM_AddV3Op);
    return HMM_AddV3(Left, Right);
}

COVERAGE(HMM_AddV4Op, 1)
static inline v4 operator+(v4 Left, v4 Right)
{
    ASSERT_COVERED(HMM_AddV4Op);
    return HMM_AddV4(Left, Right);
}

COVERAGE(HMM_AddM2Op, 1)
static inline m2 operator+(m2 Left, m2 Right)
{
    ASSERT_COVERED(HMM_AddM2Op);
    return HMM_AddM2(Left, Right);
}

COVERAGE(HMM_AddM3Op, 1)
static inline m3 operator+(m3 Left, m3 Right)
{
    ASSERT_COVERED(HMM_AddM3Op);
    return HMM_AddM3(Left, Right);
}

COVERAGE(HMM_AddM4Op, 1)
static inline m4 operator+(m4 Left, m4 Right)
{
    ASSERT_COVERED(HMM_AddM4Op);
    return HMM_AddM4(Left, Right);
}

COVERAGE(HMM_AddQOp, 1)
static inline HMM_Quat operator+(HMM_Quat Left, HMM_Quat Right)
{
    ASSERT_COVERED(HMM_AddQOp);
    return HMM_AddQ(Left, Right);
}

COVERAGE(HMM_SubV2Op, 1)
static inline v2 operator-(v2 Left, v2 Right)
{
    ASSERT_COVERED(HMM_SubV2Op);
    return HMM_SubV2(Left, Right);
}

COVERAGE(HMM_SubV3Op, 1)
static inline v3 operator-(v3 Left, v3 Right)
{
    ASSERT_COVERED(HMM_SubV3Op);
    return HMM_SubV3(Left, Right);
}

COVERAGE(HMM_SubV4Op, 1)
static inline v4 operator-(v4 Left, v4 Right)
{
    ASSERT_COVERED(HMM_SubV4Op);
    return HMM_SubV4(Left, Right);
}

COVERAGE(HMM_SubM2Op, 1)
static inline m2 operator-(m2 Left, m2 Right)
{
    ASSERT_COVERED(HMM_SubM2Op);
    return HMM_SubM2(Left, Right);
}

COVERAGE(HMM_SubM3Op, 1)
static inline m3 operator-(m3 Left, m3 Right)
{
    ASSERT_COVERED(HMM_SubM3Op);
    return HMM_SubM3(Left, Right);
}

COVERAGE(HMM_SubM4Op, 1)
static inline m4 operator-(m4 Left, m4 Right)
{
    ASSERT_COVERED(HMM_SubM4Op);
    return HMM_SubM4(Left, Right);
}

COVERAGE(HMM_SubQOp, 1)
static inline HMM_Quat operator-(HMM_Quat Left, HMM_Quat Right)
{
    ASSERT_COVERED(HMM_SubQOp);
    return HMM_SubQ(Left, Right);
}

COVERAGE(HMM_MulV2Op, 1)
static inline v2 operator*(v2 Left, v2 Right)
{
    ASSERT_COVERED(HMM_MulV2Op);
    return HMM_MulV2(Left, Right);
}

COVERAGE(HMM_MulV3Op, 1)
static inline v3 operator*(v3 Left, v3 Right)
{
    ASSERT_COVERED(HMM_MulV3Op);
    return HMM_MulV3(Left, Right);
}

COVERAGE(HMM_MulV4Op, 1)
static inline v4 operator*(v4 Left, v4 Right)
{
    ASSERT_COVERED(HMM_MulV4Op);
    return HMM_MulV4(Left, Right);
}

COVERAGE(HMM_MulM2Op, 1)
static inline m2 operator*(m2 Left, m2 Right)
{
    ASSERT_COVERED(HMM_MulM2Op);
    return HMM_MulM2(Left, Right);
}

COVERAGE(HMM_MulM3Op, 1)
static inline m3 operator*(m3 Left, m3 Right)
{
    ASSERT_COVERED(HMM_MulM3Op);
    return HMM_MulM3(Left, Right);
}

COVERAGE(HMM_MulM4Op, 1)
static inline m4 operator*(m4 Left, m4 Right)
{
    ASSERT_COVERED(HMM_MulM4Op);
    return HMM_MulM4(Left, Right);
}

COVERAGE(HMM_MulQOp, 1)
static inline HMM_Quat operator*(HMM_Quat Left, HMM_Quat Right)
{
    ASSERT_COVERED(HMM_MulQOp);
    return HMM_MulQ(Left, Right);
}

COVERAGE(HMM_MulV2FOp, 1)
static inline v2 operator*(v2 Left, float Right)
{
    ASSERT_COVERED(HMM_MulV2FOp);
    return HMM_MulV2F(Left, Right);
}

COVERAGE(HMM_MulV3FOp, 1)
static inline v3 operator*(v3 Left, float Right)
{
    ASSERT_COVERED(HMM_MulV3FOp);
    return HMM_MulV3F(Left, Right);
}

COVERAGE(HMM_MulV4FOp, 1)
static inline v4 operator*(v4 Left, float Right)
{
    ASSERT_COVERED(HMM_MulV4FOp);
    return HMM_MulV4F(Left, Right);
}

COVERAGE(HMM_MulM2FOp, 1)
static inline m2 operator*(m2 Left, float Right)
{
    ASSERT_COVERED(HMM_MulM2FOp);
    return HMM_MulM2F(Left, Right);
}

COVERAGE(HMM_MulM3FOp, 1)
static inline m3 operator*(m3 Left, float Right)
{
    ASSERT_COVERED(HMM_MulM3FOp);
    return HMM_MulM3F(Left, Right);
}

COVERAGE(HMM_MulM4FOp, 1)
static inline m4 operator*(m4 Left, float Right)
{
    ASSERT_COVERED(HMM_MulM4FOp);
    return HMM_MulM4F(Left, Right);
}

COVERAGE(HMM_MulQFOp, 1)
static inline HMM_Quat operator*(HMM_Quat Left, float Right)
{
    ASSERT_COVERED(HMM_MulQFOp);
    return HMM_MulQF(Left, Right);
}

COVERAGE(HMM_MulV2FOpLeft, 1)
static inline v2 operator*(float Left, v2 Right)
{
    ASSERT_COVERED(HMM_MulV2FOpLeft);
    return HMM_MulV2F(Right, Left);
}

COVERAGE(HMM_MulV3FOpLeft, 1)
static inline v3 operator*(float Left, v3 Right)
{
    ASSERT_COVERED(HMM_MulV3FOpLeft);
    return HMM_MulV3F(Right, Left);
}

COVERAGE(HMM_MulV4FOpLeft, 1)
static inline v4 operator*(float Left, v4 Right)
{
    ASSERT_COVERED(HMM_MulV4FOpLeft);
    return HMM_MulV4F(Right, Left);
}

COVERAGE(HMM_MulM2FOpLeft, 1)
static inline m2 operator*(float Left, m2 Right)
{
    ASSERT_COVERED(HMM_MulM2FOpLeft);
    return HMM_MulM2F(Right, Left);
}

COVERAGE(HMM_MulM3FOpLeft, 1)
static inline m3 operator*(float Left, m3 Right)
{
    ASSERT_COVERED(HMM_MulM3FOpLeft);
    return HMM_MulM3F(Right, Left);
}

COVERAGE(HMM_MulM4FOpLeft, 1)
static inline m4 operator*(float Left, m4 Right)
{
    ASSERT_COVERED(HMM_MulM4FOpLeft);
    return HMM_MulM4F(Right, Left);
}

COVERAGE(HMM_MulQFOpLeft, 1)
static inline HMM_Quat operator*(float Left, HMM_Quat Right)
{
    ASSERT_COVERED(HMM_MulQFOpLeft);
    return HMM_MulQF(Right, Left);
}

COVERAGE(HMM_MulM2V2Op, 1)
static inline v2 operator*(m2 Matrix, v2 vtor)
{
    ASSERT_COVERED(HMM_MulM2V2Op);
    return HMM_MulM2V2(Matrix, vtor);
}

COVERAGE(HMM_MulM3V3Op, 1)
static inline v3 operator*(m3 Matrix, v3 vtor)
{
    ASSERT_COVERED(HMM_MulM3V3Op);
    return HMM_MulM3V3(Matrix, vtor);
}

COVERAGE(HMM_MulM4V4Op, 1)
static inline v4 operator*(m4 Matrix, v4 vtor)
{
    ASSERT_COVERED(HMM_MulM4V4Op);
    return HMM_MulM4V4(Matrix, vtor);
}

COVERAGE(HMM_DivV2Op, 1)
static inline v2 operator/(v2 Left, v2 Right)
{
    ASSERT_COVERED(HMM_DivV2Op);
    return HMM_DivV2(Left, Right);
}

COVERAGE(HMM_DivV3Op, 1)
static inline v3 operator/(v3 Left, v3 Right)
{
    ASSERT_COVERED(HMM_DivV3Op);
    return HMM_DivV3(Left, Right);
}

COVERAGE(HMM_DivV4Op, 1)
static inline v4 operator/(v4 Left, v4 Right)
{
    ASSERT_COVERED(HMM_DivV4Op);
    return HMM_DivV4(Left, Right);
}

COVERAGE(HMM_DivV2FOp, 1)
static inline v2 operator/(v2 Left, float Right)
{
    ASSERT_COVERED(HMM_DivV2FOp);
    return HMM_DivV2F(Left, Right);
}

COVERAGE(HMM_DivV3FOp, 1)
static inline v3 operator/(v3 Left, float Right)
{
    ASSERT_COVERED(HMM_DivV3FOp);
    return HMM_DivV3F(Left, Right);
}

COVERAGE(HMM_DivV4FOp, 1)
static inline v4 operator/(v4 Left, float Right)
{
    ASSERT_COVERED(HMM_DivV4FOp);
    return HMM_DivV4F(Left, Right);
}

COVERAGE(HMM_DivM4FOp, 1)
static inline m4 operator/(m4 Left, float Right)
{
    ASSERT_COVERED(HMM_DivM4FOp);
    return HMM_DivM4F(Left, Right);
}

COVERAGE(HMM_DivM3FOp, 1)
static inline m3 operator/(m3 Left, float Right)
{
    ASSERT_COVERED(HMM_DivM3FOp);
    return HMM_DivM3F(Left, Right);
}

COVERAGE(HMM_DivM2FOp, 1)
static inline m2 operator/(m2 Left, float Right)
{
    ASSERT_COVERED(HMM_DivM2FOp);
    return HMM_DivM2F(Left, Right);
}

COVERAGE(HMM_DivQFOp, 1)
static inline HMM_Quat operator/(HMM_Quat Left, float Right)
{
    ASSERT_COVERED(HMM_DivQFOp);
    return HMM_DivQF(Left, Right);
}

COVERAGE(HMM_AddV2Assign, 1)
static inline v2 &operator+=(v2 &Left, v2 Right)
{
    ASSERT_COVERED(HMM_AddV2Assign);
    return Left = Left + Right;
}

COVERAGE(HMM_AddV3Assign, 1)
static inline v3 &operator+=(v3 &Left, v3 Right)
{
    ASSERT_COVERED(HMM_AddV3Assign);
    return Left = Left + Right;
}

COVERAGE(HMM_AddV4Assign, 1)
static inline v4 &operator+=(v4 &Left, v4 Right)
{
    ASSERT_COVERED(HMM_AddV4Assign);
    return Left = Left + Right;
}

COVERAGE(HMM_AddM2Assign, 1)
static inline m2 &operator+=(m2 &Left, m2 Right)
{
    ASSERT_COVERED(HMM_AddM2Assign);
    return Left = Left + Right;
}

COVERAGE(HMM_AddM3Assign, 1)
static inline m3 &operator+=(m3 &Left, m3 Right)
{
    ASSERT_COVERED(HMM_AddM3Assign);
    return Left = Left + Right;
}

COVERAGE(HMM_AddM4Assign, 1)
static inline m4 &operator+=(m4 &Left, m4 Right)
{
    ASSERT_COVERED(HMM_AddM4Assign);
    return Left = Left + Right;
}

COVERAGE(HMM_AddQAssign, 1)
static inline HMM_Quat &operator+=(HMM_Quat &Left, HMM_Quat Right)
{
    ASSERT_COVERED(HMM_AddQAssign);
    return Left = Left + Right;
}

COVERAGE(HMM_SubV2Assign, 1)
static inline v2 &operator-=(v2 &Left, v2 Right)
{
    ASSERT_COVERED(HMM_SubV2Assign);
    return Left = Left - Right;
}

COVERAGE(HMM_SubV3Assign, 1)
static inline v3 &operator-=(v3 &Left, v3 Right)
{
    ASSERT_COVERED(HMM_SubV3Assign);
    return Left = Left - Right;
}

COVERAGE(HMM_SubV4Assign, 1)
static inline v4 &operator-=(v4 &Left, v4 Right)
{
    ASSERT_COVERED(HMM_SubV4Assign);
    return Left = Left - Right;
}

COVERAGE(HMM_SubM2Assign, 1)
static inline m2 &operator-=(m2 &Left, m2 Right)
{
    ASSERT_COVERED(HMM_SubM2Assign);
    return Left = Left - Right;
}

COVERAGE(HMM_SubM3Assign, 1)
static inline m3 &operator-=(m3 &Left, m3 Right)
{
    ASSERT_COVERED(HMM_SubM3Assign);
    return Left = Left - Right;
}

COVERAGE(HMM_SubM4Assign, 1)
static inline m4 &operator-=(m4 &Left, m4 Right)
{
    ASSERT_COVERED(HMM_SubM4Assign);
    return Left = Left - Right;
}

COVERAGE(HMM_SubQAssign, 1)
static inline HMM_Quat &operator-=(HMM_Quat &Left, HMM_Quat Right)
{
    ASSERT_COVERED(HMM_SubQAssign);
    return Left = Left - Right;
}

COVERAGE(HMM_MulV2Assign, 1)
static inline v2 &operator*=(v2 &Left, v2 Right)
{
    ASSERT_COVERED(HMM_MulV2Assign);
    return Left = Left * Right;
}

COVERAGE(HMM_MulV3Assign, 1)
static inline v3 &operator*=(v3 &Left, v3 Right)
{
    ASSERT_COVERED(HMM_MulV3Assign);
    return Left = Left * Right;
}

COVERAGE(HMM_MulV4Assign, 1)
static inline v4 &operator*=(v4 &Left, v4 Right)
{
    ASSERT_COVERED(HMM_MulV4Assign);
    return Left = Left * Right;
}

COVERAGE(HMM_MulV2FAssign, 1)
static inline v2 &operator*=(v2 &Left, float Right)
{
    ASSERT_COVERED(HMM_MulV2FAssign);
    return Left = Left * Right;
}

COVERAGE(HMM_MulV3FAssign, 1)
static inline v3 &operator*=(v3 &Left, float Right)
{
    ASSERT_COVERED(HMM_MulV3FAssign);
    return Left = Left * Right;
}

COVERAGE(HMM_MulV4FAssign, 1)
static inline v4 &operator*=(v4 &Left, float Right)
{
    ASSERT_COVERED(HMM_MulV4FAssign);
    return Left = Left * Right;
}

COVERAGE(HMM_MulM2FAssign, 1)
static inline m2 &operator*=(m2 &Left, float Right)
{
    ASSERT_COVERED(HMM_MulM2FAssign);
    return Left = Left * Right;
}

COVERAGE(HMM_MulM3FAssign, 1)
static inline m3 &operator*=(m3 &Left, float Right)
{
    ASSERT_COVERED(HMM_MulM3FAssign);
    return Left = Left * Right;
}

COVERAGE(HMM_MulM4FAssign, 1)
static inline m4 &operator*=(m4 &Left, float Right)
{
    ASSERT_COVERED(HMM_MulM4FAssign);
    return Left = Left * Right;
}

COVERAGE(HMM_MulQFAssign, 1)
static inline HMM_Quat &operator*=(HMM_Quat &Left, float Right)
{
    ASSERT_COVERED(HMM_MulQFAssign);
    return Left = Left * Right;
}

COVERAGE(HMM_DivV2Assign, 1)
static inline v2 &operator/=(v2 &Left, v2 Right)
{
    ASSERT_COVERED(HMM_DivV2Assign);
    return Left = Left / Right;
}

COVERAGE(HMM_DivV3Assign, 1)
static inline v3 &operator/=(v3 &Left, v3 Right)
{
    ASSERT_COVERED(HMM_DivV3Assign);
    return Left = Left / Right;
}

COVERAGE(HMM_DivV4Assign, 1)
static inline v4 &operator/=(v4 &Left, v4 Right)
{
    ASSERT_COVERED(HMM_DivV4Assign);
    return Left = Left / Right;
}

COVERAGE(HMM_DivV2FAssign, 1)
static inline v2 &operator/=(v2 &Left, float Right)
{
    ASSERT_COVERED(HMM_DivV2FAssign);
    return Left = Left / Right;
}

COVERAGE(HMM_DivV3FAssign, 1)
static inline v3 &operator/=(v3 &Left, float Right)
{
    ASSERT_COVERED(HMM_DivV3FAssign);
    return Left = Left / Right;
}

COVERAGE(HMM_DivV4FAssign, 1)
static inline v4 &operator/=(v4 &Left, float Right)
{
    ASSERT_COVERED(HMM_DivV4FAssign);
    return Left = Left / Right;
}

COVERAGE(HMM_DivM4FAssign, 1)
static inline m4 &operator/=(m4 &Left, float Right)
{
    ASSERT_COVERED(HMM_DivM4FAssign);
    return Left = Left / Right;
}

COVERAGE(HMM_DivQFAssign, 1)
static inline HMM_Quat &operator/=(HMM_Quat &Left, float Right)
{
    ASSERT_COVERED(HMM_DivQFAssign);
    return Left = Left / Right;
}

COVERAGE(HMM_EqV2Op, 1)
static inline HMM_Bool operator==(v2 Left, v2 Right)
{
    ASSERT_COVERED(HMM_EqV2Op);
    return HMM_EqV2(Left, Right);
}

COVERAGE(HMM_EqV3Op, 1)
static inline HMM_Bool operator==(v3 Left, v3 Right)
{
    ASSERT_COVERED(HMM_EqV3Op);
    return HMM_EqV3(Left, Right);
}

COVERAGE(HMM_EqV4Op, 1)
static inline HMM_Bool operator==(v4 Left, v4 Right)
{
    ASSERT_COVERED(HMM_EqV4Op);
    return HMM_EqV4(Left, Right);
}

COVERAGE(HMM_EqV2OpNot, 1)
static inline HMM_Bool operator!=(v2 Left, v2 Right)
{
    ASSERT_COVERED(HMM_EqV2OpNot);
    return !HMM_EqV2(Left, Right);
}

COVERAGE(HMM_EqV3OpNot, 1)
static inline HMM_Bool operator!=(v3 Left, v3 Right)
{
    ASSERT_COVERED(HMM_EqV3OpNot);
    return !HMM_EqV3(Left, Right);
}

COVERAGE(HMM_EqV4OpNot, 1)
static inline HMM_Bool operator!=(v4 Left, v4 Right)
{
    ASSERT_COVERED(HMM_EqV4OpNot);
    return !HMM_EqV4(Left, Right);
}

COVERAGE(HMM_UnaryMinusV2, 1)
static inline v2 operator-(v2 In)
{
    ASSERT_COVERED(HMM_UnaryMinusV2);

    v2 Result;
    Result.X = -In.X;
    Result.Y = -In.Y;

    return Result;
}

COVERAGE(HMM_UnaryMinusV3, 1)
static inline v3 operator-(v3 In)
{
    ASSERT_COVERED(HMM_UnaryMinusV3);

    v3 Result;
    Result.X = -In.X;
    Result.Y = -In.Y;
    Result.Z = -In.Z;

    return Result;
}

COVERAGE(HMM_UnaryMinusV4, 1)
static inline v4 operator-(v4 In)
{
    ASSERT_COVERED(HMM_UnaryMinusV4);

    v4 Result;
#if HANDMADE_MATH__USE_SSE
    Result.SSE = _mm_xor_ps(In.SSE, _mm_set1_ps(-0.0f));
#else
    Result.X = -In.X;
    Result.Y = -In.Y;
    Result.Z = -In.Z;
    Result.W = -In.W;
#endif

    return Result;
}

#endif /* __cplusplus*/

#ifdef HANDMADE_MATH__USE_C11_GENERICS
#define HMM_Add(A, B) _Generic((A), \
        v2: HMM_AddV2, \
        v3: HMM_AddV3, \
        v4: HMM_AddV4, \
        m2: HMM_AddM2, \
        m3: HMM_AddM3, \
        m4: HMM_AddM4, \
        HMM_Quat: HMM_AddQ \
)(A, B)

#define HMM_Sub(A, B) _Generic((A), \
        v2: HMM_SubV2, \
        v3: HMM_SubV3, \
        v4: HMM_SubV4, \
        m2: HMM_SubM2, \
        m3: HMM_SubM3, \
        m4: HMM_SubM4, \
        HMM_Quat: HMM_SubQ \
)(A, B)

#define HMM_Mul(A, B) _Generic((B), \
     float: _Generic((A), \
        v2: HMM_MulV2F, \
        v3: HMM_MulV3F, \
        v4: HMM_MulV4F, \
        m2: HMM_MulM2F, \
        m3: HMM_MulM3F, \
        m4: HMM_MulM4F, \
        HMM_Quat: HMM_MulQF \
     ), \
     m2: HMM_MulM2, \
     m3: HMM_MulM3, \
     m4: HMM_MulM4, \
     HMM_Quat: HMM_MulQ, \
     default: _Generic((A), \
        v2: HMM_MulV2, \
        v3: HMM_MulV3, \
        v4: HMM_MulV4, \
        m2: HMM_MulM2V2, \
        m3: HMM_MulM3V3, \
        m4: HMM_MulM4V4 \
    ) \
)(A, B)

#define HMM_Div(A, B) _Generic((B), \
     float: _Generic((A), \
        m2: HMM_DivM2F, \
        m3: HMM_DivM3F, \
        m4: HMM_DivM4F, \
        v2: HMM_DivV2F, \
        v3: HMM_DivV3F, \
        v4: HMM_DivV4F, \
        HMM_Quat: HMM_DivQF  \
     ), \
     m2: HMM_DivM2, \
     m3: HMM_DivM3, \
     m4: HMM_DivM4, \
     HMM_Quat: HMM_DivQ, \
     default: _Generic((A), \
        v2: HMM_DivV2, \
        v3: HMM_DivV3, \
        v4: HMM_DivV4  \
    ) \
)(A, B)

#define HMM_Len(A) _Generic((A), \
        v2: HMM_LenV2, \
        v3: HMM_LenV3, \
        v4: HMM_LenV4  \
)(A)

#define HMM_LenSqr(A) _Generic((A), \
        v2: HMM_LenSqrV2, \
        v3: HMM_LenSqrV3, \
        v4: HMM_LenSqrV4  \
)(A)

#define HMM_Norm(A) _Generic((A), \
        v2: HMM_NormV2, \
        v3: HMM_NormV3, \
        v4: HMM_NormV4  \
)(A)

#define HMM_Dot(A, B) _Generic((A), \
        v2: HMM_DotV2, \
        v3: HMM_DotV3, \
        v4: HMM_DotV4  \
)(A, B)

#define HMM_Lerp(A, T, B) _Generic((A), \
        float: HMM_Lerp, \
        v2: HMM_LerpV2, \
        v3: HMM_LerpV3, \
        v4: HMM_LerpV4 \
)(A, T, B)

#define HMM_Eq(A, B) _Generic((A), \
        v2: HMM_EqV2, \
        v3: HMM_EqV3, \
        v4: HMM_EqV4  \
)(A, B)

#define HMM_Transpose(M) _Generic((M), \
        m2: HMM_TransposeM2, \
        m3: HMM_TransposeM3, \
        m4: HMM_TransposeM4  \
)(M)

#define HMM_Determinant(M) _Generic((M), \
        m2: HMM_DeterminantM2, \
        m3: HMM_DeterminantM3, \
        m4: HMM_DeterminantM4  \
)(M)

#define HMM_InvGeneral(M) _Generic((M), \
        m2: HMM_InvGeneralM2, \
        m3: HMM_InvGeneralM3, \
        m4: HMM_InvGeneralM4  \
)(M)

#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif /* HANDMADE_MATH_H */



