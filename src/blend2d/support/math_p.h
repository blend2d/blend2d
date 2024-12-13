// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_SUPPORT_MATH_P_H_INCLUDED
#define BLEND2D_SUPPORT_MATH_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../geometry.h"
#include "../simd/simd_p.h"
#include "../tables/tables_p.h"
#include "../support/mathconst_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {
namespace Math {
namespace {

//! \name Floating Point Constants
//! \{

//! Returns infinity of `T` type.
//!
//! \note `T` should be floating point.
template<typename T>
BL_NODISCARD
static BL_INLINE_NODEBUG constexpr T inf() noexcept { return std::numeric_limits<T>::infinity(); }

//! Returns a quiet NaN of `T` type.
//!
//! \note `T` should be floating point.
template<typename T>
BL_NODISCARD
static BL_INLINE_NODEBUG constexpr T nan() noexcept { return std::numeric_limits<T>::quiet_NaN(); }

template<typename T> constexpr T epsilon() noexcept = delete;
template<> BL_INLINE_NODEBUG constexpr float epsilon<float>() noexcept { return 1e-8f; }
template<> BL_INLINE_NODEBUG constexpr double epsilon<double>() noexcept { return 1e-14; }

//! \}

//! \name Floating Point Testing
//! \{

static BL_INLINE_NODEBUG bool isNaN(float x) noexcept { return std::isnan(x); }
static BL_INLINE_NODEBUG bool isNaN(double x) noexcept { return std::isnan(x); }

template<typename T, typename... Args>
static BL_INLINE_NODEBUG bool isNaN(const T& first, Args&&... args) noexcept {
  return bool(unsigned(isNaN(first)) | unsigned(isNaN(BLInternal::forward<Args>(args)...)));
}

static BL_INLINE_NODEBUG bool isInf(float x) noexcept { return std::isinf(x); }
static BL_INLINE_NODEBUG bool isInf(double x) noexcept { return std::isinf(x); }

template<typename T, typename... Args>
static BL_INLINE_NODEBUG bool isInf(const T& first, Args&&... args) noexcept {
  return bool(unsigned(isInf(first)) | unsigned(isInf(BLInternal::forward<Args>(args)...)));
}

static BL_INLINE_NODEBUG bool isFinite(const float& x) noexcept { return std::isfinite(x); }
static BL_INLINE_NODEBUG bool isFinite(const double& x) noexcept { return std::isfinite(x); }

static BL_INLINE_NODEBUG bool isFinite(const BLPoint& p) noexcept;
static BL_INLINE_NODEBUG bool isFinite(const BLBox& b) noexcept;
static BL_INLINE_NODEBUG bool isFinite(const BLRect& r) noexcept;

template<typename T, typename... Args>
static BL_INLINE_NODEBUG bool isFinite(T first, Args&&... args) noexcept {
  return bool(unsigned(isFinite(first)) & unsigned(isFinite(BLInternal::forward<Args>(args)...)));
}

static BL_INLINE_NODEBUG bool isFinite(const BLPoint& p) noexcept { return isFinite(p.x, p.y); }
static BL_INLINE_NODEBUG bool isFinite(const BLBox& b) noexcept { return isFinite(b.x0, b.y0, b.x1, b.y1); }
static BL_INLINE_NODEBUG bool isFinite(const BLRect& r) noexcept { return isFinite(r.x, r.y, r.w, r.h); }

static BL_INLINE_NODEBUG bool isNaN(const BLPoint& p) noexcept { return isNaN(p.x, p.y); }

template<typename T>
static BL_INLINE_NODEBUG constexpr bool isNear(T x, T y, T eps = epsilon<T>()) noexcept { return blAbs(x - y) <= eps; }

template<typename T>
static BL_INLINE_NODEBUG constexpr bool isNearZero(T x, T eps = epsilon<T>()) noexcept { return blAbs(x) <= eps; }

template<typename T>
static BL_INLINE_NODEBUG constexpr bool isNearZeroPositive(T x, T eps = epsilon<T>()) noexcept { return x >= T(0) && x <= eps; }

template<typename T>
static BL_INLINE_NODEBUG constexpr bool isNearOne(T x, T eps = epsilon<T>()) noexcept { return isNear(x, T(1), eps); }

//! Check if `x` is within [0, 1] range (inclusive).
template<typename T>
static BL_INLINE bool isBetween0And1(const T& x) noexcept { return bool(unsigned(x >= T(0)) & unsigned(x <= T(1))); }

//! \}

//! \name Sum of Arguments
//! \{

template<typename T>
static BL_INLINE_NODEBUG constexpr T sum(const T& first) { return first; }

template<typename T, typename... Args>
static BL_INLINE_NODEBUG constexpr T sum(const T& first, Args&&... args) { return first + sum(BLInternal::forward<Args>(args)...); }

//! \}

//! \name Miscellaneous Functions
//! \{

static BL_INLINE_NODEBUG float copySign(float x, float y) noexcept { return std::copysign(x, y); }
static BL_INLINE_NODEBUG double copySign(double x, double y) noexcept { return std::copysign(x, y); }
static BL_INLINE_NODEBUG BLPoint copySign(const BLPoint& a, const BLPoint& b) noexcept { return BLPoint(copySign(a.x, b.x), copySign(a.y, b.y)); }

static BL_INLINE_NODEBUG float cutOff(float x, uint32_t bits) noexcept {
  uint32_t msk = (uint32_t(1) << bits) - 1u;
  return blBitCast<float>(blBitCast<uint32_t>(x) & ~msk);
}

static BL_INLINE_NODEBUG double cutOff(double x, uint32_t bits) noexcept {
  uint64_t msk = (uint64_t(1) << bits) - 1u;
  return blBitCast<double>(blBitCast<uint64_t>(x) & ~msk);
}

//! \}

//! \name FMA or Mul+Add (depending on target / compile flags)
//! \{

static BL_INLINE_NODEBUG float madd(float x, float y, float a) noexcept { return x * y + a; }
static BL_INLINE_NODEBUG double madd(double x, double y, double a) noexcept { return x * y + a; }

//! \}

//! \name Rounding
//! \{

#if defined(BL_TARGET_OPT_SSE4_1)

namespace {

template<int ControlFlags>
BL_INLINE __m128 bl_roundf_sse4_1(float x) noexcept {
  __m128 y = SIMD::cast_from_f32(x).v;
  return _mm_round_ss(y, y, ControlFlags | _MM_FROUND_NO_EXC);
}

template<int ControlFlags>
BL_INLINE __m128d bl_roundd_sse4_1(double x) noexcept {
  __m128d y = SIMD::cast_from_f64(x).v;
  return _mm_round_sd(y, y, ControlFlags | _MM_FROUND_NO_EXC);
}

} // {anonymous}

static BL_INLINE float nearby(float x) noexcept { return _mm_cvtss_f32(bl_roundf_sse4_1<_MM_FROUND_CUR_DIRECTION>(x)); }
static BL_INLINE double nearby(double x) noexcept { return _mm_cvtsd_f64(bl_roundd_sse4_1<_MM_FROUND_CUR_DIRECTION>(x)); }
static BL_INLINE float trunc(float x) noexcept { return _mm_cvtss_f32(bl_roundf_sse4_1<_MM_FROUND_TO_ZERO>(x)); }
static BL_INLINE double trunc(double x) noexcept { return _mm_cvtsd_f64(bl_roundd_sse4_1<_MM_FROUND_TO_ZERO>(x)); }
static BL_INLINE float floor(float x) noexcept { return _mm_cvtss_f32(bl_roundf_sse4_1<_MM_FROUND_TO_NEG_INF>(x)); }
static BL_INLINE double floor(double x) noexcept { return _mm_cvtsd_f64(bl_roundd_sse4_1<_MM_FROUND_TO_NEG_INF>(x)); }
static BL_INLINE float ceil(float x) noexcept { return _mm_cvtss_f32(bl_roundf_sse4_1<_MM_FROUND_TO_POS_INF>(x)); }
static BL_INLINE double ceil(double x) noexcept { return _mm_cvtsd_f64(bl_roundd_sse4_1<_MM_FROUND_TO_POS_INF>(x)); }

#elif defined(BL_TARGET_OPT_SSE2)

// Rounding is very expensive on pre-SSE4.1 X86 hardware as it requires to alter rounding bits of FPU/SSE states.
// The only method which is cheap is `rint()`, which uses the current FPU/SSE rounding mode to round a floating
// point. The code below can be used to implement `roundeven()`, which can be then used to implement any other
// rounding operation. Blend2D implementation then assumes that `nearby` is round to even as it's the default
// mode CPU is setup to and in general software like math libraries expect that this mode is used.
//
// Single Precision
// ----------------
//
// ```
// float roundeven(float x) {
//   float magic = x >= 0 ? pow(2, 22) : pow(2, 22) + pow(2, 21);
//   return x >= magic ? x : x + magic - magic;
// }
// ```
//
// Double Precision
// ----------------
//
// ```
// double roundeven(double x) {
//   double magic = x >= 0 ? pow(2, 52) : pow(2, 52) + pow(2, 51);
//   return x >= magic ? x : x + magic - magic;
// }
// ```

static BL_INLINE float nearby(float x) noexcept {
  using namespace SIMD;

  Vec4xF32 src = cast_from_f32(x);
  Vec4xF32 cvt = Vec4xF32{_mm_cvt_si2ss(src.v, _mm_cvt_ss2si(src.v))};
  Vec4xF32 src_abs = src & commonTable.f32_abs.as<Vec4xF32>();
  Vec4xF32 mask = cmp_lt_1xf32(src_abs, commonTable.f32_round_max.as<Vec4xF32>());
  Vec4xF32 result = blendv_bits(src, cvt, mask);

  return cast_to_f32(result);
}

static BL_INLINE float trunc(float x) noexcept {
  using namespace SIMD;

  Vec4xF32 src = cast_from_f32(x);
  Vec4xF32 cvt = Vec4xF32{_mm_cvt_si2ss(src.v, _mm_cvtt_ss2si(src.v))};
  Vec4xF32 src_abs = src & commonTable.f32_abs.as<Vec4xF32>();
  Vec4xF32 mask = cmp_lt_1xf32(src_abs, commonTable.f32_round_max.as<Vec4xF32>());
  Vec4xF32 result = blendv_bits(src, cvt, mask);

  return cast_to_f32(result);
}

static BL_INLINE float floor(float x) noexcept {
  using namespace SIMD;

  Vec4xF32 src = cast_from_f32(x);
  Vec4xF32 magic = slli_u32<32 - 9>(srli_u32<31>(src)) | commonTable.f32_round_max.as<Vec4xF32>();

  Vec4xF32 mask = cmp_ge_1xf32(src, magic);
  Vec4xF32 rounded = sub_1xf32(add_1xf32(src, magic), magic);
  Vec4xF32 maybeone = cmp_lt_1xf32(src, rounded) & commonTable.f32_1.as<Vec4xF32>();

  return cast_to_f32(blendv_bits(sub_1xf32(rounded, maybeone), src, mask));
}

static BL_INLINE float ceil(float x) noexcept {
  using namespace SIMD;

  Vec4xF32 src = cast_from_f32(x);
  Vec4xF32 magic = slli_u32<32 - 9>(srli_u32<31>(src)) | commonTable.f32_round_max.as<Vec4xF32>();

  Vec4xF32 mask = cmp_ge_1xf32(src, magic);
  Vec4xF32 rounded = sub_1xf32(add_1xf32(src, magic), magic);
  Vec4xF32 maybeone = cmp_gt_1xf32(src, rounded) & commonTable.f32_1.as<Vec4xF32>();

  return cast_to_f32(blendv_bits(add_1xf32(rounded, maybeone), src, mask));
}

static BL_INLINE double nearby(double x) noexcept {
  using namespace SIMD;

  Vec2xF64 src = cast_from_f64(x);
  Vec2xF64 mask = cmp_lt_1xf64(src, commonTable.f64_round_max.as<Vec2xF64>());
  Vec2xF64 magic = mask & (commonTable.f64_round_max.as<Vec2xF64>() | slli_u64<64 - 13>(srli_u64<63>(src)));
  Vec2xF64 rounded = sub_1xf64(add_1xf64(src, magic), magic);

  return cast_to_f64(rounded);
}

static BL_INLINE double trunc(double x) noexcept {
  using namespace SIMD;

  Vec2xF64 src = cast_from_f64(x);
  Vec2xF64 msk_abs = commonTable.f64_abs.as<Vec2xF64>();
  Vec2xF64 src_abs = src & msk_abs;

  Vec2xF64 sign = andnot(msk_abs, src);
  Vec2xF64 magic = commonTable.f64_round_max.as<Vec2xF64>();

  Vec2xF64 mask = cmp_ge_1xf64(src_abs, magic) | sign;
  Vec2xF64 rounded = sub_1xf64(add_1xf64(src_abs, magic), magic);
  Vec2xF64 maybeone = cmp_lt_1xf64(src_abs, rounded) & commonTable.f64_1.as<Vec2xF64>();

  return cast_to_f64(blendv_bits(sub_1xf64(rounded, maybeone), src, mask));
}

static BL_INLINE double floor(double x) noexcept {
  using namespace SIMD;

  Vec2xF64 src = cast_from_f64(x);
  Vec2xF64 magic = slli_u64<64 - 13>(srli_u64<63>(src)) | commonTable.f64_round_max.as<Vec2xF64>();

  Vec2xF64 mask = cmp_ge_1xf64(src, magic);
  Vec2xF64 rounded = sub_1xf64(add_1xf64(src, magic), magic);
  Vec2xF64 maybeone = cmp_lt_1xf64(src, rounded) & commonTable.f64_1.as<Vec2xF64>();

  return cast_to_f64(blendv_bits(sub_1xf64(rounded, maybeone), src, mask));
}

static BL_INLINE double ceil(double x) noexcept {
  using namespace SIMD;

  Vec2xF64 src = cast_from_f64(x);
  Vec2xF64 magic = slli_u64<64 - 13>(srli_u64<63>(src)) | commonTable.f64_round_max.as<Vec2xF64>();

  Vec2xF64 mask = cmp_ge_1xf64(src, magic);
  Vec2xF64 rounded = sub_1xf64(add_1xf64(src, magic), magic);
  Vec2xF64 maybeone = cmp_gt_1xf64(src, rounded) & commonTable.f64_1.as<Vec2xF64>();

  return cast_to_f64(blendv_bits(add_1xf64(rounded, maybeone), src, mask));
}

#else

BL_PRAGMA_FAST_MATH_PUSH

static BL_INLINE_NODEBUG float nearby(float x) noexcept { return ::rintf(x); }
static BL_INLINE_NODEBUG double nearby(double x) noexcept { return ::rint(x); }
static BL_INLINE_NODEBUG float trunc(float x) noexcept { return ::truncf(x); }
static BL_INLINE_NODEBUG double trunc(double x) noexcept { return ::trunc(x); }
static BL_INLINE_NODEBUG float floor(float x) noexcept { return ::floorf(x); }
static BL_INLINE_NODEBUG double floor(double x) noexcept { return ::floor(x); }
static BL_INLINE_NODEBUG float ceil(float x) noexcept { return ::ceilf(x); }
static BL_INLINE_NODEBUG double ceil(double x) noexcept { return ::ceil(x); }

BL_PRAGMA_FAST_MATH_POP

#endif

static BL_INLINE float round(float x) noexcept { float y = floor(x); return y + (x - y >= 0.5f ? 1.0f : 0.0f); }
static BL_INLINE double round(double x) noexcept { double y = floor(x); return y + (x - y >= 0.5 ? 1.0 : 0.0); }

//! \}

//! \name Rounding to Integer
//! \{

static BL_INLINE int nearbyToInt(float x) noexcept {
#if defined(BL_TARGET_OPT_SSE)
  return SIMD::cvt_f32_to_scalar_i32(SIMD::cast_from_f32(x));
#else
  return int(lrintf(x));
#endif
}

static BL_INLINE int nearbyToInt(double x) noexcept {
#if defined(BL_TARGET_OPT_SSE2)
  return SIMD::cvt_f64_to_scalar_i32(SIMD::cast_from_f64(x));
#else
  return int(lrint(x));
#endif
}

static BL_INLINE int truncToInt(float x) noexcept { return int(x); }
static BL_INLINE int truncToInt(double x) noexcept { return int(x); }

static BL_INLINE BLBoxI truncToInt(const BLBox& box) noexcept {
  return BLBoxI(truncToInt(box.x0),
                truncToInt(box.y0),
                truncToInt(box.x1),
                truncToInt(box.y1));
}

#if defined(BL_TARGET_OPT_SSE4_1)
static BL_INLINE int floorToInt(float x) noexcept { return _mm_cvttss_si32(bl_roundf_sse4_1<_MM_FROUND_TO_NEG_INF>(x)); }
static BL_INLINE int floorToInt(double x) noexcept { return _mm_cvttsd_si32(bl_roundd_sse4_1<_MM_FROUND_TO_NEG_INF>(x)); }

static BL_INLINE int ceilToInt(float x) noexcept { return _mm_cvttss_si32(bl_roundf_sse4_1<_MM_FROUND_TO_POS_INF>(x)); }
static BL_INLINE int ceilToInt(double x) noexcept { return _mm_cvttsd_si32(bl_roundd_sse4_1<_MM_FROUND_TO_POS_INF>(x)); }
#else
static BL_INLINE int floorToInt(float x) noexcept { int y = nearbyToInt(x); return y - (float(y) > x); }
static BL_INLINE int floorToInt(double x) noexcept { int y = nearbyToInt(x); return y - (double(y) > x); }

static BL_INLINE int ceilToInt(float x) noexcept { int y = nearbyToInt(x); return y + (float(y) < x); }
static BL_INLINE int ceilToInt(double x) noexcept { int y = nearbyToInt(x); return y + (double(y) < x); }
#endif

static BL_INLINE int roundToInt(float x) noexcept { int y = nearbyToInt(x); return y + (float(y) - x == -0.5f); }
static BL_INLINE int roundToInt(double x) noexcept { int y = nearbyToInt(x); return y + (double(y) - x == -0.5);  }

static BL_INLINE int64_t nearbyToInt64(float x) noexcept {
#if BL_TARGET_ARCH_X86 == 64
  return SIMD::cvt_f32_to_scalar_i64(SIMD::cast_from_f32(x));
#elif BL_TARGET_ARCH_X86 == 32 && defined(__GNUC__)
  int64_t y;
  __asm__ __volatile__("flds %1\n" "fistpq %0\n" : "=m" (y) : "m" (x));
  return y;
#elif BL_TARGET_ARCH_X86 == 32 && defined(_MSC_VER)
  int64_t y;
  __asm {
    fld   dword ptr [x]
    fistp qword ptr [y]
  }
  return y;
#else
  return int64_t(llrintf(x));
#endif
}

static BL_INLINE int64_t nearbyToInt64(double x) noexcept {
#if BL_TARGET_ARCH_X86 == 64
  return SIMD::cvt_f64_to_scalar_i64(SIMD::cast_from_f64(x));
#elif BL_TARGET_ARCH_X86 == 32 && defined(__GNUC__)
  int64_t y;
  __asm__ __volatile__("fldl %1\n" "fistpq %0\n" : "=m" (y) : "m" (x));
  return y;
#elif BL_TARGET_ARCH_X86 == 32 && defined(_MSC_VER)
  int64_t y;
  __asm {
    fld   qword ptr [x]
    fistp qword ptr [y]
  }
  return y;
#else
  return int64_t(llrint(x));
#endif
}

static BL_INLINE_NODEBUG int64_t truncToInt64(float x) noexcept { return int64_t(x); }
static BL_INLINE_NODEBUG int64_t truncToInt64(double x) noexcept { return int64_t(x); }

static BL_INLINE_NODEBUG int64_t floorToInt64(float x) noexcept { int64_t y = truncToInt64(x); return y - int64_t(float(y) > x); }
static BL_INLINE_NODEBUG int64_t floorToInt64(double x) noexcept { int64_t y = truncToInt64(x); return y - int64_t(double(y) > x); }

static BL_INLINE_NODEBUG int64_t ceilToInt64(float x) noexcept { int64_t y = truncToInt64(x); return y - int64_t(float(y) < x); }
static BL_INLINE_NODEBUG int64_t ceilToInt64(double x) noexcept { int64_t y = truncToInt64(x); return y - int64_t(double(y) < x); }

static BL_INLINE_NODEBUG int64_t roundToInt64(float x) noexcept { int64_t y = nearbyToInt64(x); return y + int64_t(float(y) - x == -0.5f); }
static BL_INLINE_NODEBUG int64_t roundToInt64(double x) noexcept { int64_t y = nearbyToInt64(x); return y + int64_t(double(y) - x == -0.5);  }

//! \}

//! \name Fraction & Repeat
//! \{

//! Returns a fractional part of `x`.
//!
//! \note Fractional part returned is always equal or greater than zero. The
//! implementation is compatible to many shader implementations defined as
//! `frac(x) == x - floor(x)`, which would return `0.25` for `-1.75`.
template<typename T>
static BL_INLINE_NODEBUG T frac(T x) noexcept { return x - floor(x); }

//! Repeats the given value `x` in `y`, returning a value that is always equal
//! to or greater than zero and lesser than `y`. The return of `repeat(x, 1.0)`
//! should be identical to the return of `frac(x)`.
template<typename T>
static BL_INLINE T repeat(T x, T y) noexcept {
  T a = x;
  if (a >= y || a <= -y)
    a = std::fmod(a, y);
  if (a < T(0))
    a += y;
  return a;
}

//! \}

//! \name Power Functions
//! \{

template<typename T> BL_INLINE_NODEBUG constexpr T square(const T& x) noexcept { return x * x; }
template<typename T> BL_INLINE_NODEBUG constexpr T cube(const T& x) noexcept { return x * x * x; }

static BL_INLINE_NODEBUG float pow(float x, float y) noexcept { return ::powf(x, y); }
static BL_INLINE_NODEBUG double pow(double x, double y) noexcept { return ::pow(x, y); }

BL_PRAGMA_FAST_MATH_PUSH

static BL_INLINE_NODEBUG float sqrt(float x) noexcept { return ::sqrtf(x); }
static BL_INLINE_NODEBUG double sqrt(double x) noexcept { return ::sqrt(x); }

BL_PRAGMA_FAST_MATH_POP

static BL_INLINE_NODEBUG BLPoint sqrt(const BLPoint& p) noexcept { return BLPoint(sqrt(p.x), sqrt(p.y)); }

static BL_INLINE_NODEBUG float cbrt(float x) noexcept { return ::cbrtf(x); }
static BL_INLINE_NODEBUG double cbrt(double x) noexcept { return ::cbrt(x); }

static BL_INLINE_NODEBUG float hypot(float x, float y) noexcept { return ::hypotf(x, y); }
static BL_INLINE_NODEBUG double hypot(double x, double y) noexcept { return ::hypot(x, y); }

//! \}

//! \name Trigonometric Functions
//! \{

static BL_INLINE_NODEBUG float sin(float x) noexcept { return ::sinf(x); }
static BL_INLINE_NODEBUG double sin(double x) noexcept { return ::sin(x); }

static BL_INLINE_NODEBUG float cos(float x) noexcept { return ::cosf(x); }
static BL_INLINE_NODEBUG double cos(double x) noexcept { return ::cos(x); }

static BL_INLINE_NODEBUG float tan(float x) noexcept { return ::tanf(x); }
static BL_INLINE_NODEBUG double tan(double x) noexcept { return ::tan(x); }

static BL_INLINE_NODEBUG float asin(float x) noexcept { return ::asinf(x); }
static BL_INLINE_NODEBUG double asin(double x) noexcept { return ::asin(x); }

static BL_INLINE_NODEBUG float acos(float x) noexcept { return ::acosf(x); }
static BL_INLINE_NODEBUG double acos(double x) noexcept { return ::acos(x); }

static BL_INLINE_NODEBUG float atan(float x) noexcept { return ::atanf(x); }
static BL_INLINE_NODEBUG double atan(double x) noexcept { return ::atan(x); }

static BL_INLINE_NODEBUG float atan2(float y, float x) noexcept { return ::atan2f(y, x); }
static BL_INLINE_NODEBUG double atan2(double y, double x) noexcept { return ::atan2(y, x); }

//! \}

//! \name Linear Interpolation
//! \{

//! Linear interpolation of `a` and `b` at `t`.
//!
//! Returns `(a - t * a) + t * b`.
//!
//! \note This function should work with most geometric types Blend2D offers
//! that use double precision, however, it's not compatible with integral types.
template<typename V, typename T = double>
static BL_INLINE_NODEBUG V lerp(const V& a, const V& b, const T& t) noexcept {
  return (a - t * a) + t * b;
}

// Linear interpolation of `a` and `b` at `t=0.5`.
template<typename T>
static BL_INLINE_NODEBUG T lerp(const T& a, const T& b) noexcept {
  return 0.5 * a + 0.5 * b;
}

//! Alternative LERP implementation that is faster, but won't handle pathological
//! inputs. It should only be used in places in which it's known that such inputs
//! cannot happen.
template<typename V, typename T = double>
static BL_INLINE_NODEBUG V fastLerp(const V& a, const V& b, const T& t) noexcept {
  return a + t * (b - a);
}

//! Alternative LERP implementation at `t=0.5`.
template<typename T>
static BL_INLINE_NODEBUG T fastLerp(const T& a, const T& b) noexcept {
  return 0.5 * (a + b);
}

//! \}

//! \name Quadratic Roots
//! \{

//! Solve a quadratic polynomial `Ax^2 + Bx + C = 0` and store the result in `dst`.
//!
//! Returns the number of roots found within [tMin, tMax] - `0` to `2`.
//!
//! Resources:
//!   - http://stackoverflow.com/questions/4503849/quadratic-equation-in-ada/4504415#4504415
//!   - http://people.csail.mit.edu/bkph/articles/Quadratics.pdf
//!
//! The standard equation:
//!
//!   ```
//!   x0 = (-b + sqrt(delta)) / 2a
//!   x1 = (-b - sqrt(delta)) / 2a
//!   ```
//!
//! When 4*a*c < b*b, computing x0 involves subtracting close numbers, and makes
//! you lose accuracy, so use the following instead:
//!
//!   ```
//!   x0 = 2c / (-b - sqrt(delta))
//!   x1 = 2c / (-b + sqrt(delta))
//!   ```
//!
//! Which yields a better x0, but whose x1 has the same problem as x0 had above.
//! The correct way to compute the roots is therefore:
//!
//!   ```
//!   q  = -0.5 * (b + sign(b) * sqrt(delta))
//!   x0 = q / a
//!   x1 = c / q
//!   ```
//!
//! \note This is a branchless version designed to be easily inlinable.
static BL_INLINE size_t quadRoots(double dst[2], double a, double b, double c, double tMin, double tMax) noexcept {
  double d = blMax(b * b - 4.0 * a * c, 0.0);
  double s = sqrt(d);
  double q = -0.5 * (b + copySign(s, b));

  double t0 = q / a;
  double t1 = c / q;

  double x0 = blMin(t0, t1);
  double x1 = blMax(t1, t0);

  dst[0] = x0;
  size_t n = size_t((x0 >= tMin) & (x0 <= tMax));

  dst[n] = x1;
  n += size_t((x1 > x0) & (x1 >= tMin) & (x1 <= tMax));

  return n;
}

//! \overload
static BL_INLINE size_t quadRoots(double dst[2], const double poly[3], double tMin, double tMax) noexcept {
  return quadRoots(dst, poly[0], poly[1], poly[2], tMin, tMax);
}

//! Like `quadRoots()`, but always returns two roots and doesn't sort them.
static BL_INLINE size_t simplifiedQuadRoots(double dst[2], double a, double b, double c) noexcept {
  double d = blMax(b * b - 4.0 * a * c, 0.0);
  double s = sqrt(d);
  double q = -0.5 * (b + copySign(s, b));

  dst[0] = q / a;
  dst[1] = c / q;
  return 2;
}

static BL_INLINE size_t simplifiedQuadRoots(BLPoint dst[2], const BLPoint& a, const BLPoint& b, const BLPoint& c) noexcept {
  BLPoint d = blMax(b * b - 4.0 * a * c, 0.0);
  BLPoint s = sqrt(d);
  BLPoint q = -0.5 * (b + copySign(s, b));

  dst[0] = q / a;
  dst[1] = c / q;

  return 2;
}

//! \}

} // {anonymous}

//! \name Cubic Roots
//! \{

//! Solve a cubic polynomial and store the result in `dst`.
//!
//! Returns the number of roots found within [tMin, tMax] - `0` to `3`.
BL_HIDDEN size_t cubicRoots(double* dst, const double* poly, double tMin, double tMax) noexcept;

static BL_INLINE size_t cubicRoots(double dst[3], double a, double b, double c, double d, double tMin, double tMax) noexcept {
  double poly[4] = { a, b, c, d };
  return cubicRoots(dst, poly, tMin, tMax);
}

//! \}

} // {Math}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_SUPPORT_MATH_P_H_INCLUDED
