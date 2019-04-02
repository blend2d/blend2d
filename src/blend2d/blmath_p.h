// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLMATH_P_H
#define BLEND2D_BLMATH_P_H

#include "./blapi-internal_p.h"
#include "./blsimd_p.h"
#include "./blsupport_p.h"
#include "./bltables_p.h"

// ============================================================================
// [Global Constants]
// ============================================================================

static constexpr double BL_MATH_PI            = 3.14159265358979323846;  //!< pi.
static constexpr double BL_MATH_1p5_PI        = 4.71238898038468985769;  //!< pi * 1.5.
static constexpr double BL_MATH_2_PI          = 6.28318530717958647692;  //!< pi * 2.
static constexpr double BL_MATH_PI_DIV_2      = 1.57079632679489661923;  //!< pi / 2.
static constexpr double BL_MATH_PI_DIV_3      = 1.04719755119659774615;  //!< pi / 3.
static constexpr double BL_MATH_PI_DIV_4      = 0.78539816339744830962;  //!< pi / 4.

static constexpr double BL_SQRT_0p5           = 0.70710678118654746172;  //!< sqrt(0.5).
static constexpr double BL_SQRT_2             = 1.41421356237309504880;  //!< sqrt(2).
static constexpr double BL_SQRT_3             = 1.73205080756887729353;  //!< sqrt(3).

static constexpr double BL_MATH_AFTER_0       = 0.49e-323;               //!< First value after 0.0.
static constexpr double BL_MATH_BEFORE_1      = 0.999999999999999889;    //!< First value before 1.0.

static constexpr double BL_MATH_ANGLE_EPSILON = 1e-8;

//! Constant that is used to approximate elliptic arcs with cubic curves.
//! Since it's an approximation there are various approaches that can be
//! used to calculate the best value. The most used KAPPA is:
//!
//!   k = (4/3) * (sqrt(2) - 1) ~= 0.55228474983
//!
//! which has a maximum error of 0.00027253. However, according to this post
//!
//!   http://spencermortensen.com/articles/bezier-circle/
//!
//! the maximum error can be further reduced by 28% if we change the
//! approximation constraint to have the maximum radial distance from the
//! circle to the curve as small as possible. The an alternative constant
//!
//!   k = 1/2 +- sqrt(12 - 20*c - 3*c^2)/(4 - 6*c) ~= 0.551915024494.
//!
//! can be used to reduce the maximum error to 0.00019608. We don't use the
//! alternative, because we need to caculate the KAPPA for arcs that are not
//! 90deg, in that case the KAPPA must be calculated for such angles.
static constexpr double BL_MATH_KAPPA = 0.55228474983;

// ============================================================================
// [Helper Functions]
// ============================================================================

template<typename T>
static constexpr T blSum(const T& first) { return first; }

template<typename T, typename... Args>
static constexpr T blSum(const T& first, Args&&... args) { return first + blSum(std::forward<Args>(args)...); }

// ============================================================================
// [Classification & Limits]
// ============================================================================

template<typename T> constexpr T blEpsilon() noexcept = delete;
template<> constexpr float blEpsilon<float>() noexcept { return 1e-8f; }
template<> constexpr double blEpsilon<double>() noexcept { return 1e-14; }

static BL_INLINE bool blIsNaN(float x) noexcept { return std::isnan(x); }
static BL_INLINE bool blIsNaN(double x) noexcept { return std::isnan(x); }

static BL_INLINE bool blIsInf(float x) noexcept { return std::isinf(x); }
static BL_INLINE bool blIsInf(double x) noexcept { return std::isinf(x); }

static BL_INLINE bool blIsFinite(float x) noexcept { return std::isfinite(x); }
static BL_INLINE bool blIsFinite(double x) noexcept { return std::isfinite(x); }

template<typename T, typename... Args>
static BL_INLINE bool blIsNaN(const T& first, Args&&... args) noexcept {
  return blIsNaN(first) | blIsNaN(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
static BL_INLINE bool blIsInf(const T& first, Args&&... args) noexcept {
  return blIsInf(first) | blIsInf(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
static BL_INLINE bool blIsFinite(const T& first, Args&&... args) noexcept {
  return blIsFinite(first) & blIsFinite(std::forward<Args>(args)...);
}

// ============================================================================
// [Miscellaneous]
// ============================================================================

static BL_INLINE float blCopySign(float x, float y) noexcept { return std::copysign(x, y); }
static BL_INLINE double blCopySign(double x, double y) noexcept { return std::copysign(x, y); }

// ============================================================================
// [Rounding]
// ============================================================================


#if defined(BL_TARGET_OPT_SSE4_1)

namespace {

template<int ControlFlags>
BL_INLINE __m128 bl_roundf_sse4_1(float x) noexcept {
  __m128 y = SIMD::vcvtf32f128(x);
  return _mm_round_ss(y, y, ControlFlags | _MM_FROUND_NO_EXC);
}

template<int ControlFlags>
BL_INLINE __m128d bl_roundd_sse4_1(double x) noexcept {
  __m128d y = SIMD::vcvtd64d128(x);
  return _mm_round_sd(y, y, ControlFlags | _MM_FROUND_NO_EXC);
}

} // {anonymous}

static BL_INLINE float  blNearby(float  x) noexcept { return SIMD::vcvtf128f32(bl_roundf_sse4_1<_MM_FROUND_CUR_DIRECTION>(x)); }
static BL_INLINE double blNearby(double x) noexcept { return SIMD::vcvtd128d64(bl_roundd_sse4_1<_MM_FROUND_CUR_DIRECTION>(x)); }
static BL_INLINE float  blTrunc (float  x) noexcept { return SIMD::vcvtf128f32(bl_roundf_sse4_1<_MM_FROUND_TO_ZERO      >(x)); }
static BL_INLINE double blTrunc (double x) noexcept { return SIMD::vcvtd128d64(bl_roundd_sse4_1<_MM_FROUND_TO_ZERO      >(x)); }
static BL_INLINE float  blFloor (float  x) noexcept { return SIMD::vcvtf128f32(bl_roundf_sse4_1<_MM_FROUND_TO_NEG_INF   >(x)); }
static BL_INLINE double blFloor (double x) noexcept { return SIMD::vcvtd128d64(bl_roundd_sse4_1<_MM_FROUND_TO_NEG_INF   >(x)); }
static BL_INLINE float  blCeil  (float  x) noexcept { return SIMD::vcvtf128f32(bl_roundf_sse4_1<_MM_FROUND_TO_POS_INF   >(x)); }
static BL_INLINE double blCeil  (double x) noexcept { return SIMD::vcvtd128d64(bl_roundd_sse4_1<_MM_FROUND_TO_POS_INF   >(x)); }

#elif defined(BL_TARGET_OPT_SSE2)

// Rounding is very expensive on pre-SSE4.1 X86 hardware as it requires to alter
// rounding bits of FPU/SSE states. The only method which is cheap is `rint()`,
// which uses the current FPU/SSE rounding mode to round a floating point. The
// code below can be used to implement `roundeven()`, which can be then used to
// implement any other rounding operation. Blend2D implementation then assumes
// that `blNearby` is round to even as it's the default mode CPU is setup to.
//
// Single Precision
// ----------------
//
// ```
// float roundeven(float x) {
//   float maxn = pow(2, 23);                  // 8388608
//   float magic = pow(2, 23) + pow(2, 22);    // 12582912
//   return x >= maxn ? x : x + magic - magic;
// }
// ```
//
// Double Precision
// ----------------
//
// ```
// double roundeven(double x) {
//   double maxn = pow(2, 52);                 // 4503599627370496
//   double magic = pow(2, 52) + pow(2, 51);   // 6755399441055744
//   return x >= maxn ? x : x + magic - magic;
// }
// ```

static BL_INLINE float blNearby(float x) noexcept {
  using namespace SIMD;

  F128 src = vcvtf32f128(x);
  F128 magic = v_const_as<F128>(blCommonTable.f128_round_magic);

  F128 mask = vcmpgess(src, v_const_as<F128>(blCommonTable.f128_round_max));
  F128 rounded = vsubss(vaddss(src, magic), magic);

  return vcvtf128f32(vblendmask(rounded, src, mask));
}

static BL_INLINE float blTrunc(float x) noexcept {
  using namespace SIMD;

  F128 src = vcvtf32f128(x);

  F128 msk_abs = v_const_as<F128>(blCommonTable.f128_abs);
  F128 src_abs = vand(src, msk_abs);

  F128 sign = vandnot_a(msk_abs, src);
  F128 magic = v_const_as<F128>(blCommonTable.f128_round_magic);

  F128 mask = vor(vcmpgess(src_abs, v_const_as<F128>(blCommonTable.f128_round_max)), sign);
  F128 rounded = vsubss(vaddss(src_abs, magic), magic);
  F128 maybeone = vand(vcmpltss(src_abs, rounded), v_const_as<F128>(blCommonTable.f128_1));

  return vcvtf128f32(vblendmask(vsubss(rounded, maybeone), src, mask));
}

static BL_INLINE float blFloor(float x) noexcept {
  using namespace SIMD;

  F128 src = vcvtf32f128(x);
  F128 magic = v_const_as<F128>(blCommonTable.f128_round_magic);

  F128 mask = vcmpgess(src, v_const_as<F128>(blCommonTable.f128_round_max));
  F128 rounded = vsubss(vaddss(src, magic), magic);
  F128 maybeone = vand(vcmpltss(src, rounded), v_const_as<F128>(blCommonTable.f128_1));

  return vcvtf128f32(vblendmask(vsubss(rounded, maybeone), src, mask));
}

static BL_INLINE float blCeil(float x) noexcept {
  using namespace SIMD;

  F128 src = SIMD::vcvtf32f128(x);
  F128 magic = v_const_as<F128>(blCommonTable.f128_round_magic);

  F128 mask = vcmpgess(src, v_const_as<F128>(blCommonTable.f128_round_max));
  F128 rounded = vsubss(vaddss(src, magic), magic);
  F128 maybeone = vand(vcmpgtss(src, rounded), v_const_as<F128>(blCommonTable.f128_1));

  return vcvtf128f32(vblendmask(vaddss(rounded, maybeone), src, mask));
}

static BL_INLINE double blNearby(double x) noexcept {
  using namespace SIMD;

  D128 src = vcvtd64d128(x);
  D128 magic = v_const_as<D128>(blCommonTable.d128_round_magic);

  D128 mask = vcmpgesd(src, v_const_as<D128>(blCommonTable.d128_round_max));
  D128 rounded = vsubsd(vaddsd(src, magic), magic);

  return vcvtd128d64(vblendmask(rounded, src, mask));
}

static BL_INLINE double blTrunc(double x) noexcept {
  using namespace SIMD;

  static const uint64_t kSepMask[1] = { 0x7FFFFFFFFFFFFFFFu };

  D128 src = vcvtd64d128(x);
  D128 msk_abs = _mm_load_sd(reinterpret_cast<const double*>(kSepMask));
  D128 src_abs = vand(src, msk_abs);

  D128 sign = vandnot_a(msk_abs, src);
  D128 magic = v_const_as<D128>(blCommonTable.d128_round_magic);

  D128 mask = vor(vcmpgesd(src_abs, v_const_as<D128>(blCommonTable.d128_round_max)), sign);
  D128 rounded = vsubsd(vaddsd(src_abs, magic), magic);
  D128 maybeone = vand(vcmpltsd(src_abs, rounded), v_const_as<D128>(blCommonTable.d128_1));

  return vcvtd128d64(vblendmask(vsubsd(rounded, maybeone), src, mask));
}

static BL_INLINE double blFloor(double x) noexcept {
  using namespace SIMD;

  D128 src = vcvtd64d128(x);
  D128 magic = v_const_as<D128>(blCommonTable.d128_round_magic);

  D128 mask = vcmpgesd(src, v_const_as<D128>(blCommonTable.d128_round_max));
  D128 rounded = vsubsd(vaddsd(src, magic), magic);
  D128 maybeone = vand(vcmpltsd(src, rounded), v_const_as<D128>(blCommonTable.d128_1));

  return vcvtd128d64(vblendmask(vsubsd(rounded, maybeone), src, mask));
}

static BL_INLINE double blCeil(double x) noexcept {
  using namespace SIMD;

  D128 src = vcvtd64d128(x);
  D128 magic = v_const_as<D128>(blCommonTable.d128_round_magic);

  D128 mask = vcmpgesd(src, v_const_as<D128>(blCommonTable.d128_round_max));
  D128 rounded = vsubsd(vaddsd(src, magic), magic);
  D128 maybeone = vand(vcmpgtsd(src, rounded), v_const_as<D128>(blCommonTable.d128_1));

  return vcvtd128d64(vblendmask(vaddsd(rounded, maybeone), src, mask));
}

#else

BL_PRAGMA_FAST_MATH_PUSH

static BL_INLINE float  blNearby(float x) noexcept { return rintf(x); }
static BL_INLINE double blNearby(double x) noexcept { return rint(x); }
static BL_INLINE float  blTrunc(float x) noexcept { return truncf(x); }
static BL_INLINE double blTrunc(double x) noexcept { return trunc(x); }
static BL_INLINE float  blFloor(float x) noexcept { return floorf(x); }
static BL_INLINE double blFloor(double x) noexcept { return floor(x); }
static BL_INLINE float  blCeil(float x) noexcept { return ceilf(x); }
static BL_INLINE double blCeil(double x) noexcept { return ceil(x); }

BL_PRAGMA_FAST_MATH_POP

#endif

static BL_INLINE float blRound(float x) noexcept { float y = blFloor(x); return y + (x - y >= 0.5f ? 1.0f : 0.0f); }
static BL_INLINE double blRound(double x) noexcept { double y = blFloor(x); return y + (x - y >= 0.5 ? 1.0 : 0.0); }

// ============================================================================
// [Rounding to Integer]
// ============================================================================

static BL_INLINE int blNearbyToInt(float x) noexcept {
#if defined(BL_TARGET_OPT_SSE)
  return _mm_cvtss_si32(SIMD::vcvtf32f128(x));
#elif BL_TARGET_ARCH_X86 == 32 && defined(_MSC_VER)
  int y;
  __asm {
    fld   dword ptr [x]
    fistp dword ptr [y]
  }
  return y;
#elif BL_TARGET_ARCH_X86 == 32 && defined(__GNUC__)
  int y;
  __asm__ __volatile__("flds %1\n" "fistpl %0\n" : "=m" (y) : "m" (x));
  return y;
#else
  #error "[blend2d] blNearbyToInt() - Unsupported architecture or compiler."
#endif
}

static BL_INLINE int blNearbyToInt(double x) noexcept {
#if defined(BL_TARGET_OPT_SSE2)
  return _mm_cvtsd_si32(SIMD::vcvtd64d128(x));
#elif BL_TARGET_ARCH_X86 == 32 && defined(_MSC_VER)
  int y;
  __asm {
    fld   qword ptr [x]
    fistp dword ptr [y]
  }
  return y;
#elif BL_TARGET_ARCH_X86 == 32 && defined(__GNUC__)
  int y;
  __asm__ __volatile__("fldl %1\n" "fistpl %0\n" : "=m" (y) : "m" (x));
  return y;
#else
  #error "[blend2d] blNearbyToInt() - Unsupported architecture or compiler."
#endif
}

static BL_INLINE int blTruncToInt(float x) noexcept { return int(x); }
static BL_INLINE int blTruncToInt(double x) noexcept { return int(x); }

#if defined(BL_TARGET_OPT_SSE4_1)
static BL_INLINE int blFloorToInt(float x) noexcept { return _mm_cvttss_si32(bl_roundf_sse4_1<_MM_FROUND_TO_NEG_INF>(x)); }
static BL_INLINE int blFloorToInt(double x) noexcept { return _mm_cvttsd_si32(bl_roundd_sse4_1<_MM_FROUND_TO_NEG_INF>(x)); }

static BL_INLINE int blCeilToInt(float x) noexcept { return _mm_cvttss_si32(bl_roundf_sse4_1<_MM_FROUND_TO_POS_INF>(x)); }
static BL_INLINE int blCeilToInt(double x) noexcept { return _mm_cvttsd_si32(bl_roundd_sse4_1<_MM_FROUND_TO_POS_INF>(x)); }
#else
static BL_INLINE int blFloorToInt(float x) noexcept { int y = blNearbyToInt(x); return y - (float(y) > x); }
static BL_INLINE int blFloorToInt(double x) noexcept { int y = blNearbyToInt(x); return y - (double(y) > x); }

static BL_INLINE int blCeilToInt(float x) noexcept { int y = blNearbyToInt(x); return y + (float(y) < x); }
static BL_INLINE int blCeilToInt(double x) noexcept { int y = blNearbyToInt(x); return y + (double(y) < x); }
#endif

static BL_INLINE int blRoundToInt(float x) noexcept { int y = blNearbyToInt(x); return y + (float(y) - x == -0.5f); }
static BL_INLINE int blRoundToInt(double x) noexcept { int y = blNearbyToInt(x); return y + (double(y) - x == -0.5);  }

static BL_INLINE int64_t blNearbyToInt64(float x) noexcept {
#if BL_TARGET_ARCH_X86 == 64
  return _mm_cvtss_si64(SIMD::vcvtf32f128(x));
#elif BL_TARGET_ARCH_X86 == 32 && defined(_MSC_VER)
  int64_t y;
  __asm {
    fld   dword ptr [x]
    fistp qword ptr [y]
  }
  return y;
#elif BL_TARGET_ARCH_X86 == 32 && defined(__GNUC__)
  int64_t y;
  __asm__ __volatile__("flds %1\n" "fistpq %0\n" : "=m" (y) : "m" (x));
  return y;
#else
  return int64_t(llrintf(x));
#endif
}

static BL_INLINE int64_t blNearbyToInt64(double x) noexcept {
#if BL_TARGET_ARCH_X86 == 64
  return _mm_cvtsd_si64(_mm_set_sd(x));
#elif BL_TARGET_ARCH_X86 == 32 && defined(_MSC_VER)
  int64_t y;
  __asm {
    fld   qword ptr [x]
    fistp qword ptr [y]
  }
  return y;
#elif BL_TARGET_ARCH_X86 == 32 && defined(__GNUC__)
  int64_t y;
  __asm__ __volatile__("fldl %1\n" "fistpq %0\n" : "=m" (y) : "m" (x));
  return y;
#else
  return int64_t(llrint(x));
#endif
}

static BL_INLINE int64_t blTruncToInt64(float x) noexcept { return int64_t(x); }
static BL_INLINE int64_t blTruncToInt64(double x) noexcept { return int64_t(x); }

static BL_INLINE int64_t blFloorToInt64(float x) noexcept { int64_t y = blTruncToInt64(x); return y - int64_t(float(y) > x); }
static BL_INLINE int64_t blFloorToInt64(double x) noexcept { int64_t y = blTruncToInt64(x); return y - int64_t(double(y) > x); }

static BL_INLINE int64_t blCeilToInt64(float x) noexcept { int64_t y = blTruncToInt64(x); return y - int64_t(float(y) < x); }
static BL_INLINE int64_t blCeilToInt64(double x) noexcept { int64_t y = blTruncToInt64(x); return y - int64_t(double(y) < x); }

static BL_INLINE int64_t blRoundToInt64(float x) noexcept { int64_t y = blNearbyToInt64(x); return y + int64_t(float(y) - x == -0.5f); }
static BL_INLINE int64_t blRoundToInt64(double x) noexcept { int64_t y = blNearbyToInt64(x); return y + int64_t(double(y) - x == -0.5);  }

// ============================================================================
// [Fraction / Repeat]
// ============================================================================

//! Get a fractional part of `x`.
//!
//! NOTE: Fractional part returned is always equal or greater than zero. The
//! implementation is compatible to many shader implementations defined as
//! `frac(x) == x - floor(x)`, which would return `0.25` for `-1.75`.
template<typename T>
static BL_INLINE T blFrac(T x) noexcept { return x - blFloor(x); }

//! Repeat the given value `x` in `y`, returning a value that is always equal
//! to or greater than zero and lesser than `y`. The return of `repeat(x, 1.0)`
//! should be identical to the return of `frac(x)`.
template<typename T>
static BL_INLINE T blRepeat(T x, T y) noexcept {
  T a = x;
  if (a >= y || a <= -y)
    a = std::fmod(a, y);
  if (a < T(0))
    a += y;
  return a;
}

// ============================================================================
// [Power]
// ============================================================================

static BL_INLINE float blPow(float x, float y) noexcept { return powf(x, y); }
static BL_INLINE double blPow(double x, double y) noexcept { return pow(x, y); }

template<typename T> constexpr T blSquare(const T& x) noexcept { return x * x; }
template<typename T> constexpr T blPow3(const T& x) noexcept { return x * x * x; }

BL_PRAGMA_FAST_MATH_PUSH

static BL_INLINE float blSqrt(float x) noexcept { return sqrtf(x); }
static BL_INLINE double blSqrt(double x) noexcept { return sqrt(x); }

BL_PRAGMA_FAST_MATH_POP

static BL_INLINE float blCbrt(float x) noexcept { return cbrtf(x); }
static BL_INLINE double blCbrt(double x) noexcept { return cbrt(x); }

static BL_INLINE float blHypot(float x, float y) noexcept { return hypotf(x, y); }
static BL_INLINE double blHypot(double x, double y) noexcept { return hypot(x, y); }

// ============================================================================
// [Trigonometry]
// ============================================================================

static BL_INLINE float blSin(float x) noexcept { return sinf(x); }
static BL_INLINE double blSin(double x) noexcept { return sin(x); }

static BL_INLINE float blCos(float x) noexcept { return cosf(x); }
static BL_INLINE double blCos(double x) noexcept { return cos(x); }

static BL_INLINE float blTan(float x) noexcept { return tanf(x); }
static BL_INLINE double blTan(double x) noexcept { return tan(x); }

static BL_INLINE float blAsin(float x) noexcept { return asinf(x); }
static BL_INLINE double blAsin(double x) noexcept { return asin(x); }

static BL_INLINE float blAcos(float x) noexcept { return acosf(x); }
static BL_INLINE double blAcos(double x) noexcept { return acos(x); }

static BL_INLINE float blAtan(float x) noexcept { return atanf(x); }
static BL_INLINE double blAtan(double x) noexcept { return atan(x); }

static BL_INLINE float blAtan2(float y, float x) noexcept { return atan2f(y, x); }
static BL_INLINE double blAtan2(double y, double x) noexcept { return atan2(y, x); }

// ============================================================================
// [Linear Interpolation]
// ============================================================================

//! Linear interpolation of `a` and `b` at `t`.
//!
//! Returns `a * (1 - t) + b * t`.
//!
//! NOTE: This function should work with most geometric types Blend2D offers
//! that use double precision, however, it's not compatible with integral types.
template<typename V, typename T = double>
static BL_INLINE V blLerp(const V& a, const V& b, const T& t) noexcept {
  return (a * (1.0 - t)) + (b * t);
}

// Linear interpolation of `a` and `b` at `t=0.5`.
template<typename T>
static BL_INLINE T blLerp(const T& a, const T& b) noexcept {
  return blLerp(a, b, 0.5);
}

// ============================================================================
// [Roots]
// ============================================================================

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
//! NOTE: This is a branchless version designed to be easily inlineable.
static BL_INLINE size_t blQuadRoots(double dst[2], double a, double b, double c, double tMin, double tMax) noexcept {
  double d = blMax(b * b - 4.0 * a * c, 0.0);
  double s = blSqrt(d);
  double q = -0.5 * (b + blCopySign(s, b));

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
static BL_INLINE size_t blQuadRoots(double dst[2], const double poly[3], double tMin, double tMax) noexcept {
  return blQuadRoots(dst, poly[0], poly[1], poly[2], tMin, tMax);
}

//! Like `blQuadRoots()`, but always returns two roots and doesn't sort them.
static BL_INLINE size_t blSimplifiedQuadRoots(double dst[2], double a, double b, double c) noexcept {
  double d = blMax(b * b - 4.0 * a * c, 0.0);
  double s = blSqrt(d);
  double q = -0.5 * (b + blCopySign(s, b));

  dst[0] = q / a;
  dst[1] = c / q;
  return 2;
}

//! Solve a cubic polynomial and store the result in `dst`.
//!
//! Returns the number of roots found within [tMin, tMax] - `0` to `3`.
BL_HIDDEN size_t blCubicRoots(double* dst, const double* poly, double tMin, double tMax) noexcept;

//! \overload
static BL_INLINE size_t blCubicRoots(double dst[3], double a, double b, double c, double d, double tMin, double tMax) noexcept {
  double poly[4] = { a, b, c, d };
  return ::blCubicRoots(dst, poly, tMin, tMax);
}

//! Solve an Nth degree polynomial and store the result in `dst`.
//!
//! Returns the number of roots found [tMin, tMax] - `0` to `n - 1`.
BL_HIDDEN size_t blPolyRoots(double* dst, const double* poly, int n, double tMin, double tMax) noexcept;

// ============================================================================
// [blIsBetween0And1]
// ============================================================================

//! Check if `x` is within [0, 1] range (inclusive).
template<typename T>
static BL_INLINE bool blIsBetween0And1(const T& x) noexcept { return x >= T(0) && x <= T(1); }

// ============================================================================
// [BLMath - Near]
// ============================================================================

template<typename T>
static constexpr bool isNear(T x, T y, T eps = blEpsilon<T>()) noexcept { return blAbs(x - y) <= eps; }

template<typename T>
static constexpr bool isNearZero(T x, T eps = blEpsilon<T>()) noexcept { return blAbs(x) <= eps; }

template<typename T>
static constexpr bool isNearZeroPositive(T x, T eps = blEpsilon<T>()) noexcept { return x >= T(0) && x <= eps; }

template<typename T>
static constexpr bool isNearOne(T x, T eps = blEpsilon<T>()) noexcept { return isNear(x, T(1), eps); }

#endif // BLEND2D_BLMATH_P_H
