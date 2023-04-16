// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_SIMD_X86_P_H_INCLUDED
#define BLEND2D_SIMD_X86_P_H_INCLUDED

#include "tables_p.h"
#include "support/memops_p.h"

#if defined(_MSC_VER)
  #include <intrin.h>
#endif

#if defined(BL_TARGET_OPT_SSE)
  #include <xmmintrin.h>
#endif

#if defined(BL_TARGET_OPT_SSE2)
  #include <emmintrin.h>
#endif

#if defined(BL_TARGET_OPT_SSE3) && !defined(_MSC_VER)
  #include <pmmintrin.h>
#endif

#if defined(BL_TARGET_OPT_SSSE3)
  #include <tmmintrin.h>
#endif

#if defined(BL_TARGET_OPT_SSE4_1)
  #include <smmintrin.h>
#endif

#if defined(BL_TARGET_OPT_SSE4_2)
  #include <nmmintrin.h>
#endif

#if defined(BL_TARGET_OPT_AVX) || defined(BL_TARGET_OPT_AVX2)
  #include <immintrin.h>
#endif

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! SIMD namespace contains helper functions to access SIMD intrinsics. The names of these functions correspond to
//! names of functions used by the dynamic pipeline generator.
namespace SIMD {

// SIMD - Features
// ===============

#if defined(BL_TARGET_OPT_AVX2)
  #define BL_TARGET_SIMD_I 256
  #define BL_TARGET_SIMD_F 256
  #define BL_TARGET_SIMD_D 256
#elif defined(BL_TARGET_OPT_AVX)
  #define BL_TARGET_SIMD_I 128
  #define BL_TARGET_SIMD_F 256
  #define BL_TARGET_SIMD_D 256
#elif defined(BL_TARGET_OPT_SSE2)
  #define BL_TARGET_SIMD_I 128
  #define BL_TARGET_SIMD_F 128
  #define BL_TARGET_SIMD_D 128
#else
  #define BL_TARGET_SIMD_I 0
  #define BL_TARGET_SIMD_F 0
  #define BL_TARGET_SIMD_D 0
#endif

// SIMD - Types
// ============

#if defined(BL_TARGET_OPT_SSE2)
typedef __m128i Vec128I;
typedef __m128  Vec128F;
typedef __m128d Vec128D;

typedef BL_UNALIGNED_TYPE(__m128i, 1) Vec128I_Unaligned;
typedef BL_UNALIGNED_TYPE(__m128 , 1) Vec128F_Unaligned;
typedef BL_UNALIGNED_TYPE(__m128d, 1) Vec128D_Unaligned;
#endif

// 256-bit types (including integers) are accessible through AVX as AVX also include conversion instructions between
// integer types and FP types.
#if defined(BL_TARGET_OPT_AVX)
typedef __m256i Vec256I;
typedef __m256  Vec256F;
typedef __m256d Vec256D;

typedef BL_UNALIGNED_TYPE(__m256i, 1) Vec256I_Unaligned;
typedef BL_UNALIGNED_TYPE(__m256 , 1) Vec256F_Unaligned;
typedef BL_UNALIGNED_TYPE(__m256d, 1) Vec256D_Unaligned;
#endif

// Must be in anonymous namespace.
namespace {

// SIMD - Cast
// ===========

template<typename Out, typename In>
BL_INLINE const Out& v_const_as(const In* c) noexcept { return *reinterpret_cast<const Out*>(c); }

template<typename DstT, typename SrcT>
BL_INLINE DstT v_cast(const SrcT& x) noexcept { return x; }

#if defined(BL_TARGET_OPT_SSE2)
template<> BL_INLINE Vec128F v_cast(const Vec128I& x) noexcept { return _mm_castsi128_ps(x); }
template<> BL_INLINE Vec128D v_cast(const Vec128I& x) noexcept { return _mm_castsi128_pd(x); }
template<> BL_INLINE Vec128I v_cast(const Vec128F& x) noexcept { return _mm_castps_si128(x); }
template<> BL_INLINE Vec128D v_cast(const Vec128F& x) noexcept { return _mm_castps_pd(x); }
template<> BL_INLINE Vec128I v_cast(const Vec128D& x) noexcept { return _mm_castpd_si128(x); }
template<> BL_INLINE Vec128F v_cast(const Vec128D& x) noexcept { return _mm_castpd_ps(x); }
#endif

#if defined(BL_TARGET_OPT_AVX)
template<> BL_INLINE Vec128I v_cast(const Vec256I& x) noexcept { return _mm256_castsi256_si128(x); }
template<> BL_INLINE Vec256I v_cast(const Vec128I& x) noexcept { return _mm256_castsi128_si256(x); }

template<> BL_INLINE Vec128F v_cast(const Vec256F& x) noexcept { return _mm256_castps256_ps128(x); }
template<> BL_INLINE Vec256F v_cast(const Vec128F& x) noexcept { return _mm256_castps128_ps256(x); }

template<> BL_INLINE Vec128D v_cast(const Vec256D& x) noexcept { return _mm256_castpd256_pd128(x); }
template<> BL_INLINE Vec256D v_cast(const Vec128D& x) noexcept { return _mm256_castpd128_pd256(x); }

template<> BL_INLINE Vec256D v_cast(const Vec256F& x) noexcept { return _mm256_castps_pd(x); }
template<> BL_INLINE Vec256F v_cast(const Vec256D& x) noexcept { return _mm256_castpd_ps(x); }

template<> BL_INLINE Vec256F v_cast(const Vec256I& x) noexcept { return _mm256_castsi256_ps(x); }
template<> BL_INLINE Vec256I v_cast(const Vec256F& x) noexcept { return _mm256_castps_si256(x); }

template<> BL_INLINE Vec256D v_cast(const Vec256I& x) noexcept { return _mm256_castsi256_pd(x); }
template<> BL_INLINE Vec256I v_cast(const Vec256D& x) noexcept { return _mm256_castpd_si256(x); }
#endif

// SIMD - Vec128 - Zero
// ====================

#if defined(BL_TARGET_OPT_SSE2)
BL_INLINE Vec128I v_zero_i128() noexcept { return _mm_setzero_si128(); }
BL_INLINE Vec128F v_zero_f128() noexcept { return _mm_setzero_ps(); }
BL_INLINE Vec128D v_zero_d128() noexcept { return _mm_setzero_pd(); }
#endif

// SIMD - Vec128 - Fill Value
// ==========================

#if defined(BL_TARGET_OPT_SSE2)
BL_INLINE Vec128I v_fill_i128_i8(int8_t x) noexcept { return _mm_set1_epi8(x); }
BL_INLINE Vec128I v_fill_i128_i16(int16_t x) noexcept { return _mm_set1_epi16(x); }
BL_INLINE Vec128I v_fill_i128_i32(int32_t x) noexcept { return _mm_set1_epi32(x); }

BL_INLINE Vec128I v_fill_i128_i32(int32_t x1, int32_t x0) noexcept { return _mm_set_epi32(x1, x0, x1, x0); }
BL_INLINE Vec128I v_fill_i128_i32(int32_t x3, int32_t x2, int32_t x1, int32_t x0) noexcept { return _mm_set_epi32(x3, x2, x1, x0); }

#if BL_TARGET_ARCH_BITS >= 64
BL_INLINE Vec128I v_fill_i128_i64(int64_t x) noexcept { return _mm_set1_epi64x(x); }
#else
BL_INLINE Vec128I v_fill_i128_i64(int64_t x) noexcept { return v_fill_i128_i32(int32_t(uint64_t(x) >> 32), int32_t(x & 0xFFFFFFFFu)); }
#endif

BL_INLINE Vec128I v_fill_i128_i64(int64_t x1, int64_t x0) noexcept {
  return v_fill_i128_i32(int32_t(uint64_t(x1) >> 32), int32_t(x1 & 0xFFFFFFFFu),
                         int32_t(uint64_t(x0) >> 32), int32_t(x0 & 0xFFFFFFFFu));
}

BL_INLINE Vec128I v_fill_i128_u8(uint8_t x) noexcept { return v_fill_i128_i8(int8_t(x)); }
BL_INLINE Vec128I v_fill_i128_u16(uint16_t x) noexcept { return v_fill_i128_i16(int16_t(x)); }
BL_INLINE Vec128I v_fill_i128_u32(uint32_t x) noexcept { return v_fill_i128_i32(int32_t(x)); }
BL_INLINE Vec128I v_fill_i128_u32(uint32_t x1, uint32_t x0) noexcept { return v_fill_i128_i32(int32_t(x1), int32_t(x0), int32_t(x1), int32_t(x0)); }
BL_INLINE Vec128I v_fill_i128_u32(uint32_t x3, uint32_t x2, uint32_t x1, uint32_t x0) noexcept { return v_fill_i128_i32(int32_t(x3), int32_t(x2), int32_t(x1), int32_t(x0)); }
BL_INLINE Vec128I v_fill_i128_u64(uint64_t x) noexcept { return v_fill_i128_i64(int64_t(x)); }
BL_INLINE Vec128I v_fill_i128_u64(uint64_t x1, uint64_t x0) noexcept { return v_fill_i128_i64(int64_t(x1), int64_t(x0)); }

BL_INLINE Vec128F v_fill_f128(float x) noexcept { return _mm_set1_ps(x); }
BL_INLINE Vec128F v_fill_f128(float x3, float x2, float x1, float x0) noexcept { return _mm_set_ps(x3, x2, x1, x0); }

BL_INLINE Vec128D v_fill_d128(double x) noexcept { return _mm_set1_pd(x); }
BL_INLINE Vec128D v_fill_d128(double x1, double x0) noexcept { return _mm_set_pd(x1, x0); }
#endif

// SIMD - Vec128 - Load & Store
// ============================

#if defined(BL_TARGET_OPT_SSE2)
BL_INLINE Vec128I v_load_i16(const void* p) noexcept { return _mm_cvtsi32_si128(*static_cast<BL_UNALIGNED_TYPE(const uint16_t, 1)*>(p)); }
BL_INLINE Vec128I v_load_i32(const void* p) noexcept { return _mm_cvtsi32_si128(*static_cast<BL_UNALIGNED_TYPE(const int, 1)*>(p)); }
BL_INLINE Vec128F v_load_f32(const void* p) noexcept { return _mm_load_ss(static_cast<const float*>(p)); }
BL_INLINE Vec128I v_load_i64(const void* p) noexcept { return _mm_loadl_epi64(static_cast<const Vec128I*>(p)); }
BL_INLINE Vec128F v_load_2xf32(const void* p) noexcept { return v_cast<Vec128F>(v_load_i64(p)); }
BL_INLINE Vec128D v_load_f64(const void* p) noexcept { return _mm_load_sd(static_cast<const double*>(p)); }
BL_INLINE Vec128I v_loada_i128(const void* p) noexcept { return _mm_load_si128(static_cast<const Vec128I*>(p)); }
BL_INLINE Vec128F v_loada_f128(const void* p) noexcept { return _mm_load_ps(static_cast<const float*>(p)); }
BL_INLINE Vec128D v_loada_d128(const void* p) noexcept { return _mm_load_pd(static_cast<const double*>(p)); }
BL_INLINE Vec128I v_loadu_i128(const void* p) noexcept { return _mm_loadu_si128(static_cast<const Vec128I*>(p)); }
BL_INLINE Vec128F v_loadu_f128(const void* p) noexcept { return _mm_loadu_ps(static_cast<const float*>(p)); }
BL_INLINE Vec128D v_loadu_d128(const void* p) noexcept { return _mm_loadu_pd(static_cast<const double*>(p)); }

BL_INLINE Vec128I v_loadl_i64(const Vec128I& x, const void* p) noexcept { return v_cast<Vec128I>(_mm_loadl_pd(v_cast<Vec128D>(x), static_cast<const double*>(p))); }
BL_INLINE Vec128I v_loadh_i64(const Vec128I& x, const void* p) noexcept { return v_cast<Vec128I>(_mm_loadh_pd(v_cast<Vec128D>(x), static_cast<const double*>(p))); }
BL_INLINE Vec128F v_loadl_2xf32(const Vec128F& x, const void* p) noexcept { return _mm_loadl_pi(x, static_cast<const __m64*>(p)); }
BL_INLINE Vec128F v_loadh_2xf32(const Vec128F& x, const void* p) noexcept { return _mm_loadh_pi(x, static_cast<const __m64*>(p)); }
BL_INLINE Vec128D v_loadl_f64(const Vec128D& x, const void* p) noexcept { return _mm_loadl_pd(x, static_cast<const double*>(p)); }
BL_INLINE Vec128D v_loadh_f64(const Vec128D& x, const void* p) noexcept { return _mm_loadh_pd(x, static_cast<const double*>(p)); }

#if defined(BL_TARGET_OPT_SSE4_1) && defined(_MSC_VER) && !defined(__clang__)
// MSCV won't emit a single instruction if we use v_load_iXX() to load from memory.
BL_INLINE Vec128I v_load_i64_i8_i16(const void* p) noexcept { return _mm_cvtepi8_epi16(*static_cast<const Vec128I_Unaligned*>(p)); }
BL_INLINE Vec128I v_load_i64_u8_u16(const void* p) noexcept { return _mm_cvtepu8_epi16(*static_cast<const Vec128I_Unaligned*>(p)); }
BL_INLINE Vec128I v_load_i32_i8_i32(const void* p) noexcept { return _mm_cvtepi8_epi32(*static_cast<const Vec128I_Unaligned*>(p)); }
BL_INLINE Vec128I v_load_i32_u8_u32(const void* p) noexcept { return _mm_cvtepu8_epi32(*static_cast<const Vec128I_Unaligned*>(p)); }
BL_INLINE Vec128I v_load_i16_i8_i64(const void* p) noexcept { return _mm_cvtepi8_epi64(*static_cast<const Vec128I_Unaligned*>(p)); }
BL_INLINE Vec128I v_load_i16_u8_u64(const void* p) noexcept { return _mm_cvtepu8_epi64(*static_cast<const Vec128I_Unaligned*>(p)); }
#elif defined(BL_TARGET_OPT_SSE4_1)
BL_INLINE Vec128I v_load_i64_i8_i16(const void* p) noexcept { return _mm_cvtepi8_epi16(v_load_i64(p)); }
BL_INLINE Vec128I v_load_i64_u8_u16(const void* p) noexcept { return _mm_cvtepu8_epi16(v_load_i64(p)); }
BL_INLINE Vec128I v_load_i32_i8_i32(const void* p) noexcept { return _mm_cvtepi8_epi32(v_load_i32(p)); }
BL_INLINE Vec128I v_load_i32_u8_u32(const void* p) noexcept { return _mm_cvtepu8_epi32(v_load_i32(p)); }
BL_INLINE Vec128I v_load_i16_i8_i64(const void* p) noexcept { return _mm_cvtepi8_epi64(v_load_i16(p)); }
BL_INLINE Vec128I v_load_i16_u8_u64(const void* p) noexcept { return _mm_cvtepu8_epi64(v_load_i16(p)); }
#endif

#if defined(BL_TARGET_OPT_AVX2)
BL_INLINE Vec128I v_load_i128_mask32(const void* p, const Vec128I& mask) noexcept { return _mm_maskload_epi32(static_cast<const int*>(p), mask); }
BL_INLINE Vec128I v_load_i128_mask64(const void* p, const Vec128I& mask) noexcept { return _mm_maskload_epi64(static_cast<const long long*>(p), mask); }
#endif

#if defined(BL_TARGET_OPT_AVX)
BL_INLINE Vec128F v_load_f128_mask32(const void* p, const Vec128F& mask) noexcept { return _mm_maskload_ps(static_cast<const float*>(p), v_cast<Vec128I>(mask)); }
BL_INLINE Vec128D v_load_d128_mask64(const void* p, const Vec128D& mask) noexcept { return _mm_maskload_pd(static_cast<const double*>(p), v_cast<Vec128I>(mask)); }
#endif

BL_INLINE void v_store_i32(void* p, const Vec128I& x) noexcept { static_cast<int*>(p)[0] = _mm_cvtsi128_si32(x); }
BL_INLINE void v_store_f32(void* p, const Vec128F& x) noexcept { _mm_store_ss(static_cast<float*>(p), x); }
BL_INLINE void v_store_i64(void* p, const Vec128I& x) noexcept { _mm_storel_epi64(static_cast<Vec128I*>(p), x); }
BL_INLINE void v_store_2xf32(void* p, const Vec128F& x) noexcept { _mm_storel_pi(static_cast<__m64*>(p), x); }
BL_INLINE void v_store_f64(void* p, const Vec128D& x) noexcept { _mm_store_sd(static_cast<double*>(p), x); }
BL_INLINE void v_storel_i64(void* p, const Vec128I& x) noexcept { _mm_storel_epi64(static_cast<Vec128I*>(p), x); }
BL_INLINE void v_storeh_i64(void* p, const Vec128I& x) noexcept { _mm_storeh_pd(static_cast<double*>(p), v_cast<Vec128D>(x)); }
BL_INLINE void v_storel_2xf32(void* p, const Vec128F& x) noexcept { _mm_storel_pi(static_cast<__m64*>(p), x); }
BL_INLINE void v_storeh_2xf32(void* p, const Vec128F& x) noexcept { _mm_storeh_pi(static_cast<__m64*>(p), x); }
BL_INLINE void v_storel_f64(void* p, const Vec128D& x) noexcept { _mm_storel_pd(static_cast<double*>(p), x); }
BL_INLINE void v_storeh_f64(void* p, const Vec128D& x) noexcept { _mm_storeh_pd(static_cast<double*>(p), x); }
BL_INLINE void v_storea_i128(void* p, const Vec128I& x) noexcept { _mm_store_si128(static_cast<Vec128I*>(p), x); }
BL_INLINE void v_storea_f128(void* p, const Vec128F& x) noexcept { _mm_store_ps(static_cast<float*>(p), x); }
BL_INLINE void v_storea_d128(void* p, const Vec128D& x) noexcept { _mm_store_pd(static_cast<double*>(p), x); }
BL_INLINE void v_storeu_i128(void* p, const Vec128I& x) noexcept { _mm_storeu_si128(static_cast<Vec128I*>(p), x); }
BL_INLINE void v_storeu_f128(void* p, const Vec128F& x) noexcept { _mm_storeu_ps(static_cast<float*>(p), x); }
BL_INLINE void v_storeu_d128(void* p, const Vec128D& x) noexcept { _mm_storeu_pd(static_cast<double*>(p), x); }

#if defined(BL_TARGET_OPT_AVX2)
BL_INLINE void v_storeu_i128_mask32(void* p, const Vec128I& x, const Vec128I& mask) noexcept { _mm_maskstore_epi32(static_cast<int*>(p), mask, x); }
BL_INLINE void v_storeu_i128_mask64(void* p, const Vec128I& x, const Vec128I& mask) noexcept { _mm_maskstore_epi64(static_cast<long long*>(p), mask, x); }
#endif

#if defined(BL_TARGET_OPT_AVX)
BL_INLINE void v_storeu_128f_mask32(void* p, const Vec128F& x, const Vec128F& mask) noexcept { _mm_maskstore_ps(static_cast<float*>(p), v_cast<Vec128I>(mask), x); }
BL_INLINE void v_storeu_128d_mask64(void* p, const Vec128D& x, const Vec128D& mask) noexcept { _mm_maskstore_pd(static_cast<double*>(p), v_cast<Vec128I>(mask), x); }
#endif

// SIMD - Vec128 - Insert & Extract
// ================================

template<uint32_t I>
BL_INLINE Vec128I v_insert_u16(const Vec128I& x, uint32_t y) noexcept { return _mm_insert_epi16(x, int16_t(y), I); }

template<uint32_t I>
BL_INLINE Vec128I v_insertm_u16(const Vec128I& x, const void* p) noexcept { return _mm_insert_epi16(x, BLMemOps::readU16u(p), I); }

template<uint32_t I>
BL_INLINE uint32_t v_extract_u16(const Vec128I& x) noexcept { return uint32_t(_mm_extract_epi16(x, I)); }

#if defined(BL_TARGET_OPT_SSE4_1)
template<uint32_t I>
BL_INLINE Vec128I v_insert_u8(const Vec128I& x, uint32_t y) noexcept { return _mm_insert_epi8(x, int8_t(y), I); }
template<uint32_t I>
BL_INLINE Vec128I v_insert_u32(const Vec128I& x, uint32_t y) noexcept { return _mm_insert_epi32(x, int(y), I); }

template<uint32_t I>
BL_INLINE Vec128I v_insertm_u8(const Vec128I& x, const void* p) noexcept { return _mm_insert_epi8(x, BLMemOps::readU8(p), I); }
template<uint32_t I>
BL_INLINE Vec128I v_insertm_u32(const Vec128I& x, const void* p) noexcept { return _mm_insert_epi32(x, BLMemOps::readU32u(p), I); }

// Convenience function used by RGB24 fetchers.
template<uint32_t I>
BL_INLINE Vec128I v_insertm_u24(const Vec128I& x, const void* p) noexcept {
  const uint8_t* p8 = static_cast<const uint8_t*>(p);
  if ((I & 0x1) == 0)
    return _mm_insert_epi8(_mm_insert_epi16(x, BLMemOps::readU16u(p8), I / 2), BLMemOps::readU8(p8 + 2), I + 2);
  else
    return _mm_insert_epi16(_mm_insert_epi8(x, BLMemOps::readU8(p8), I), BLMemOps::readU16u(p8 + 1), (I + 1) / 2);
}

template<uint32_t I>
BL_INLINE uint32_t v_extract_u8(const Vec128I& x) noexcept { return uint32_t(_mm_extract_epi8(x, I)); }
template<uint32_t I>
BL_INLINE uint32_t v_extract_u32(const Vec128I& x) noexcept { return uint32_t(_mm_extract_epi32(x, I)); }
#endif

// SIMD - Vec128 - Conversion
// ==========================

#if defined(BL_TARGET_OPT_SSE2)
BL_INLINE Vec128I v_i128_from_i32(int32_t x) noexcept { return _mm_cvtsi32_si128(int(x)); }
BL_INLINE Vec128I v_i128_from_u32(uint32_t x) noexcept { return _mm_cvtsi32_si128(int(x)); }

#if BL_TARGET_ARCH_BITS >= 64
BL_INLINE Vec128I v_i128_from_i64(int64_t x) noexcept { return _mm_cvtsi64_si128(x); }
#else
BL_INLINE Vec128I v_i128_from_i64(int64_t x) noexcept { return _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&x)); }
#endif
BL_INLINE Vec128I v_i128_from_u64(uint64_t x) noexcept { return v_i128_from_i64(int64_t(x)); }

#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
// See: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=70708
BL_INLINE Vec128F v_f128_from_f32(float x) noexcept {
  Vec128F reg;
  __asm__("" : "=x" (reg) : "0" (x));
  return reg;
}

BL_INLINE Vec128D v_d128_from_f64(double x) noexcept {
  Vec128D reg;
  __asm__("" : "=x" (reg) : "0" (x));
  return reg;
}
#else
BL_INLINE Vec128F v_f128_from_f32(float x) noexcept { return _mm_set_ss(x); }
BL_INLINE Vec128D v_d128_from_f64(double x) noexcept { return _mm_set_sd(x); }
#endif

BL_INLINE int32_t v_get_i32(const Vec128I& x) noexcept { return int32_t(_mm_cvtsi128_si32(x)); }
BL_INLINE uint32_t v_get_u32(const Vec128I& x) noexcept { return uint32_t(_mm_cvtsi128_si32(x)); }

#if BL_TARGET_ARCH_BITS >= 64
BL_INLINE int64_t v_get_i64(const Vec128I& x) noexcept { return int64_t(_mm_cvtsi128_si64(x)); }
#else
BL_INLINE int64_t v_get_i64(const Vec128I& x) noexcept {
  int64_t result;
  _mm_storel_epi64(reinterpret_cast<__m128i*>(&result), x);
  return result;
}
#endif
BL_INLINE uint64_t v_get_u64(const Vec128I& x) noexcept { return uint64_t(v_get_i64(x)); }

BL_INLINE float v_get_f32(const Vec128F& x) noexcept { return _mm_cvtss_f32(x); }
BL_INLINE double v_get_f64(const Vec128D& x) noexcept { return _mm_cvtsd_f64(x); }

BL_INLINE Vec128F s_cvt_i32_f32(int32_t x) noexcept { return _mm_cvtsi32_ss(v_zero_f128(), x); }
BL_INLINE Vec128D s_cvt_i32_f64(int32_t x) noexcept { return _mm_cvtsi32_sd(v_zero_d128(), x); }

BL_INLINE int32_t s_cvt_f32_i32(const Vec128F& x) noexcept { return _mm_cvtss_si32(x); }
BL_INLINE int32_t s_cvt_f64_i32(const Vec128D& x) noexcept { return _mm_cvtsd_si32(x); }

BL_INLINE int32_t s_cvtt_f32_i32(const Vec128F& x) noexcept { return _mm_cvttss_si32(x); }
BL_INLINE int32_t s_cvtt_f64_i32(const Vec128D& x) noexcept { return _mm_cvttsd_si32(x); }

#if BL_TARGET_ARCH_BITS >= 64
BL_INLINE Vec128F s_cvt_i64_f32(int64_t x) noexcept { return _mm_cvtsi64_ss(v_zero_f128(), x); }
BL_INLINE Vec128D s_cvt_i64_f64(int64_t x) noexcept { return _mm_cvtsi64_sd(v_zero_d128(), x); }

BL_INLINE int64_t s_cvt_f32_i64(const Vec128F& x) noexcept { return _mm_cvtss_si64(x); }
BL_INLINE int64_t s_cvt_f64_i64(const Vec128D& x) noexcept { return _mm_cvtsd_si64(x); }

BL_INLINE int64_t s_cvtt_f32_i64(const Vec128F& x) noexcept { return _mm_cvttss_si64(x); }
BL_INLINE int64_t s_cvtt_f64_i64(const Vec128D& x) noexcept { return _mm_cvttsd_si64(x); }
#endif

BL_INLINE Vec128I v_cvt_f32_i32(const Vec128F& x) noexcept { return _mm_cvtps_epi32(x); }
BL_INLINE Vec128I v_cvt_f64_i32(const Vec128D& x) noexcept { return _mm_cvtpd_epi32(x); }

BL_INLINE Vec128I v_cvtt_f32_i32(const Vec128F& x) noexcept { return _mm_cvttps_epi32(x); }
BL_INLINE Vec128I v_cvtt_f64_i32(const Vec128D& x) noexcept { return _mm_cvttpd_epi32(x); }

BL_INLINE Vec128F v_cvt_f64_f32(const Vec128D& x) noexcept { return _mm_cvtpd_ps(x); }
BL_INLINE Vec128F v_cvt_i32_f32(const Vec128I& x) noexcept { return _mm_cvtepi32_ps(x); }

BL_INLINE Vec128D v_cvt_2xi32_f64(const Vec128I& x) noexcept { return _mm_cvtepi32_pd(x); }
BL_INLINE Vec128D v_cvt_2xf32_f64(const Vec128F& x) noexcept { return _mm_cvtps_pd(x); }

#endif

// SIMD - Vec128 - Shuffling & Permutations
// ========================================

#if defined(BL_TARGET_OPT_SSSE3)
BL_INLINE Vec128I v_shuffle_i8(const Vec128I& x, const Vec128I& y) noexcept { return _mm_shuffle_epi8(x, y); }
#endif

#if defined(BL_TARGET_OPT_SSE2)
template<uint8_t D, uint8_t C, uint8_t B, uint8_t A>
BL_INLINE Vec128F v_shuffle_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_shuffle_ps(x, y, _MM_SHUFFLE(D, C, B, A)); }

template<uint8_t D, uint8_t C, uint8_t B, uint8_t A>
BL_INLINE Vec128I v_shuffle_i32(const Vec128I& x, const Vec128I& y) noexcept { return v_cast<Vec128I>(_mm_shuffle_ps(v_cast<Vec128F>(x), v_cast<Vec128F>(y), _MM_SHUFFLE(D, C, B, A))); }

template<uint8_t B, uint8_t A>
BL_INLINE Vec128D v_shuffle_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_shuffle_pd(x, y, (B << 1) | A); }

template<uint8_t D, uint8_t C, uint8_t B, uint8_t A>
BL_INLINE Vec128I v_swizzle_lo_i16(const Vec128I& x) noexcept { return _mm_shufflelo_epi16(x, _MM_SHUFFLE(D, C, B, A)); }
template<uint8_t D, uint8_t C, uint8_t B, uint8_t A>
BL_INLINE Vec128I v_swizzle_hi_i16(const Vec128I& x) noexcept { return _mm_shufflehi_epi16(x, _MM_SHUFFLE(D, C, B, A)); }

template<uint8_t D, uint8_t C, uint8_t B, uint8_t A>
BL_INLINE Vec128I v_swizzle_i16(const Vec128I& x) noexcept { return v_swizzle_hi_i16<D, C, B, A>(v_swizzle_lo_i16<D, C, B, A>(x)); }
template<uint8_t D, uint8_t C, uint8_t B, uint8_t A>
BL_INLINE Vec128I v_swizzle_i32(const Vec128I& x) noexcept { return _mm_shuffle_epi32(x, _MM_SHUFFLE(D, C, B, A)); }
template<uint8_t B, uint8_t A>
BL_INLINE Vec128I v_swizzle_i64(const Vec128I& x) noexcept { return v_swizzle_i32<B*2 + 1, B*2, A*2 + 1, A*2>(x); }

#if defined(BL_TARGET_OPT_SSE2) && !defined(BL_TARGET_OPT_AVX)
template<uint8_t D, uint8_t C, uint8_t B, uint8_t A>
BL_INLINE Vec128F v_swizzle_f32(const Vec128F& x) noexcept { return v_cast<Vec128F>(v_swizzle_i32<D, C, B, A>(v_cast<Vec128I>(x))); }
template<uint8_t B, uint8_t A>
BL_INLINE Vec128F v_swizzle_2xf32(const Vec128F& x) noexcept { return v_cast<Vec128F>(v_swizzle_i64<B, A>(v_cast<Vec128I>(x))); }
#else
template<uint8_t D, uint8_t C, uint8_t B, uint8_t A>
BL_INLINE Vec128F v_swizzle_f32(const Vec128F& x) noexcept { return v_shuffle_f32<D, C, B, A>(x, x); }
template<uint8_t B, uint8_t A>
BL_INLINE Vec128F v_swizzle_2xf32(const Vec128F& x) noexcept { return v_swizzle_f32<B*2 + 1, B*2, A*2 + 1, A*2>(x); }
#endif

#if defined(BL_TARGET_OPT_AVX)
template<uint8_t B, uint8_t A>
BL_INLINE Vec128D v_swizzle_f64(const Vec128D& x) noexcept { return v_shuffle_f64<B, A>(x, x); }
#else
template<uint8_t B, uint8_t A>
BL_INLINE Vec128D v_swizzle_f64(const Vec128D& x) noexcept { return v_cast<Vec128D>(v_swizzle_i64<B, A>(v_cast<Vec128I>(x))); }
#endif

BL_INLINE Vec128I v_swap_i32(const Vec128I& x) noexcept { return v_swizzle_i32<2, 3, 0, 1>(x); }
BL_INLINE Vec128I v_swap_i64(const Vec128I& x) noexcept { return v_swizzle_i64<0, 1>(x); }
BL_INLINE Vec128F v_swap_2xf32(const Vec128F& x) noexcept { return v_swizzle_2xf32<0, 1>(x); }
BL_INLINE Vec128D v_swap_f64(const Vec128D& x) noexcept { return v_swizzle_f64<0, 1>(x); }

BL_INLINE Vec128I v_dupl_i64(const Vec128I& x) noexcept { return v_swizzle_i64<0, 0>(x); }
BL_INLINE Vec128I v_duph_i64(const Vec128I& x) noexcept { return v_swizzle_i64<1, 1>(x); }

BL_INLINE Vec128F v_dupl_f32(const Vec128F& x) noexcept { return v_swizzle_f32<2, 2, 0, 0>(x); }
BL_INLINE Vec128F v_duph_f32(const Vec128F& x) noexcept { return v_swizzle_f32<3, 3, 1, 1>(x); }

BL_INLINE Vec128F v_dupl_2xf32(const Vec128F& x) noexcept { return v_swizzle_2xf32<0, 0>(x); }
BL_INLINE Vec128F v_duph_2xf32(const Vec128F& x) noexcept { return v_swizzle_2xf32<1, 1>(x); }

BL_INLINE Vec128D v_dupl_f64(const Vec128D& x) noexcept { return v_swizzle_f64<0, 0>(x); }
BL_INLINE Vec128D v_duph_f64(const Vec128D& x) noexcept { return v_swizzle_f64<1, 1>(x); }

#if defined(BL_TARGET_OPT_AVX2)
BL_INLINE Vec128I v_splat128_i8(const Vec128I& x) noexcept { return _mm_broadcastb_epi8(x); }
BL_INLINE Vec128I v_splat128_i16(const Vec128I& x) noexcept { return _mm_broadcastw_epi16(x); }
BL_INLINE Vec128I v_splat128_i32(const Vec128I& x) noexcept { return _mm_broadcastd_epi32(x); }
BL_INLINE Vec128I v_splat128_i64(const Vec128I& x) noexcept { return _mm_broadcastq_epi64(x); }
#else
BL_INLINE Vec128I v_splat128_i64(const Vec128I& x) noexcept { return _mm_shuffle_epi32(x, _MM_SHUFFLE(1, 0, 1, 0)); }
BL_INLINE Vec128I v_splat128_i32(const Vec128I& x) noexcept { return _mm_shuffle_epi32(x, _MM_SHUFFLE(0, 0, 0, 0)); }
BL_INLINE Vec128I v_splat128_i16(const Vec128I& x) noexcept { return v_splat128_i32(_mm_unpacklo_epi16(x, x)); }
BL_INLINE Vec128I v_splat128_i8(const Vec128I& x) noexcept { return v_splat128_i16(_mm_unpacklo_epi8(x, x)); }
#endif

BL_INLINE Vec128I v_interleave_lo_i8(const Vec128I& x, const Vec128I& y) noexcept { return _mm_unpacklo_epi8(x, y); }
BL_INLINE Vec128I v_interleave_hi_i8(const Vec128I& x, const Vec128I& y) noexcept { return _mm_unpackhi_epi8(x, y); }

BL_INLINE Vec128I v_interleave_lo_i16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_unpacklo_epi16(x, y); }
BL_INLINE Vec128I v_interleave_hi_i16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_unpackhi_epi16(x, y); }

BL_INLINE Vec128I v_interleave_lo_i32(const Vec128I& x, const Vec128I& y) noexcept { return _mm_unpacklo_epi32(x, y); }
BL_INLINE Vec128I v_interleave_hi_i32(const Vec128I& x, const Vec128I& y) noexcept { return _mm_unpackhi_epi32(x, y); }

BL_INLINE Vec128F v_interleave_lo_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_unpacklo_ps(x, y); }
BL_INLINE Vec128F v_interleave_hi_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_unpackhi_ps(x, y); }

BL_INLINE Vec128I v_interleave_lo_i64(const Vec128I& x, const Vec128I& y) noexcept { return _mm_unpacklo_epi64(x, y); }
BL_INLINE Vec128I v_interleave_hi_i64(const Vec128I& x, const Vec128I& y) noexcept { return _mm_unpackhi_epi64(x, y); }

BL_INLINE Vec128D v_interleave_lo_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_unpacklo_pd(x, y); }
BL_INLINE Vec128D v_interleave_hi_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_unpackhi_pd(x, y); }
#endif

#if defined(BL_TARGET_OPT_SSSE3)
template<int N_BYTES>
BL_INLINE Vec128I v_alignr_i8(const Vec128I& x, const Vec128I& y) noexcept { return _mm_alignr_epi8(x, y, N_BYTES); }
#endif

#if defined(BL_TARGET_OPT_SSE3)
BL_INLINE Vec128F v_broadcast_f128_64(const void* p) noexcept { return v_cast<Vec128F>(_mm_loaddup_pd(static_cast<const double*>(p))); }
BL_INLINE Vec128D v_broadcast_d128_64(const void* p) noexcept { return _mm_loaddup_pd(static_cast<const double*>(p)); }
#else
BL_INLINE Vec128F v_broadcast_f128_64(const void* p) noexcept { return v_dupl_2xf32(v_load_2xf32(p)); }
BL_INLINE Vec128D v_broadcast_d128_64(const void* p) noexcept { return v_dupl_f64(v_load_f64(p)); }
#endif

// SIMD - Vec128 - Bitwise Operations & Masking
// ============================================

#if defined(BL_TARGET_OPT_SSE2)
BL_INLINE Vec128I v_or(const Vec128I& x, const Vec128I& y) noexcept { return _mm_or_si128(x, y); }
BL_INLINE Vec128F v_or(const Vec128F& x, const Vec128F& y) noexcept { return _mm_or_ps(x, y); }
BL_INLINE Vec128D v_or(const Vec128D& x, const Vec128D& y) noexcept { return _mm_or_pd(x, y); }

BL_INLINE Vec128I v_or(const Vec128I& x, const Vec128I& y, const Vec128I& z) noexcept { return v_or(v_or(x, y), z); }
BL_INLINE Vec128F v_or(const Vec128F& x, const Vec128F& y, const Vec128F& z) noexcept { return v_or(v_or(x, y), z); }
BL_INLINE Vec128D v_or(const Vec128D& x, const Vec128D& y, const Vec128D& z) noexcept { return v_or(v_or(x, y), z); }

BL_INLINE Vec128I v_xor(const Vec128I& x, const Vec128I& y) noexcept { return _mm_xor_si128(x, y); }
BL_INLINE Vec128F v_xor(const Vec128F& x, const Vec128F& y) noexcept { return _mm_xor_ps(x, y); }
BL_INLINE Vec128D v_xor(const Vec128D& x, const Vec128D& y) noexcept { return _mm_xor_pd(x, y); }

BL_INLINE Vec128I v_xor(const Vec128I& x, const Vec128I& y, const Vec128I& z) noexcept { return v_xor(v_xor(x, y), z); }
BL_INLINE Vec128F v_xor(const Vec128F& x, const Vec128F& y, const Vec128F& z) noexcept { return v_xor(v_xor(x, y), z); }
BL_INLINE Vec128D v_xor(const Vec128D& x, const Vec128D& y, const Vec128D& z) noexcept { return v_xor(v_xor(x, y), z); }

BL_INLINE Vec128I v_and(const Vec128I& x, const Vec128I& y) noexcept { return _mm_and_si128(x, y); }
BL_INLINE Vec128F v_and(const Vec128F& x, const Vec128F& y) noexcept { return _mm_and_ps(x, y); }
BL_INLINE Vec128D v_and(const Vec128D& x, const Vec128D& y) noexcept { return _mm_and_pd(x, y); }

BL_INLINE Vec128I v_and(const Vec128I& x, const Vec128I& y, const Vec128I& z) noexcept { return v_and(v_and(x, y), z); }
BL_INLINE Vec128F v_and(const Vec128F& x, const Vec128F& y, const Vec128F& z) noexcept { return v_and(v_and(x, y), z); }
BL_INLINE Vec128D v_and(const Vec128D& x, const Vec128D& y, const Vec128D& z) noexcept { return v_and(v_and(x, y), z); }

BL_INLINE Vec128I v_nand(const Vec128I& x, const Vec128I& y) noexcept { return _mm_andnot_si128(x, y); }
BL_INLINE Vec128F v_nand(const Vec128F& x, const Vec128F& y) noexcept { return _mm_andnot_ps(x, y); }
BL_INLINE Vec128D v_nand(const Vec128D& x, const Vec128D& y) noexcept { return _mm_andnot_pd(x, y); }

BL_INLINE Vec128I v_blend_mask(const Vec128I& x, const Vec128I& y, const Vec128I& mask) noexcept { return v_or(v_and(y, mask), v_nand(mask, x)); }
BL_INLINE Vec128F v_blend_mask(const Vec128F& x, const Vec128F& y, const Vec128F& mask) noexcept { return v_or(v_and(y, mask), v_nand(mask, x)); }
BL_INLINE Vec128D v_blend_mask(const Vec128D& x, const Vec128D& y, const Vec128D& mask) noexcept { return v_or(v_and(y, mask), v_nand(mask, x)); }

#if defined(BL_TARGET_OPT_SSE4_1)
BL_INLINE Vec128I v_blend_i8(const Vec128I& x, const Vec128I& y, const Vec128I& mask) noexcept { return _mm_blendv_epi8(x, y, mask); }
#else
BL_INLINE Vec128I v_blend_i8(const Vec128I& x, const Vec128I& y, const Vec128I& mask) noexcept { return v_blend_mask(x, y, mask); }
#endif

template<uint8_t N_BITS> BL_INLINE Vec128I v_sll_i16(const Vec128I& x) noexcept { return N_BITS ? _mm_slli_epi16(x, N_BITS) : x; }
template<uint8_t N_BITS> BL_INLINE Vec128I v_sll_i32(const Vec128I& x) noexcept { return N_BITS ? _mm_slli_epi32(x, N_BITS) : x; }
template<uint8_t N_BITS> BL_INLINE Vec128I v_sll_i64(const Vec128I& x) noexcept { return N_BITS ? _mm_slli_epi64(x, N_BITS) : x; }

template<uint8_t N_BITS> BL_INLINE Vec128I v_srl_i16(const Vec128I& x) noexcept { return N_BITS ? _mm_srli_epi16(x, N_BITS) : x; }
template<uint8_t N_BITS> BL_INLINE Vec128I v_srl_i32(const Vec128I& x) noexcept { return N_BITS ? _mm_srli_epi32(x, N_BITS) : x; }
template<uint8_t N_BITS> BL_INLINE Vec128I v_srl_i64(const Vec128I& x) noexcept { return N_BITS ? _mm_srli_epi64(x, N_BITS) : x; }

template<uint8_t N_BITS> BL_INLINE Vec128I v_sra_i16(const Vec128I& x) noexcept { return N_BITS ? _mm_srai_epi16(x, N_BITS) : x; }
template<uint8_t N_BITS> BL_INLINE Vec128I v_sra_i32(const Vec128I& x) noexcept { return N_BITS ? _mm_srai_epi32(x, N_BITS) : x; }

template<uint8_t N_BYTES> BL_INLINE Vec128I v_sllb_i128(const Vec128I& x) noexcept { return N_BYTES ? _mm_slli_si128(x, N_BYTES) : x; }
template<uint8_t N_BYTES> BL_INLINE Vec128I v_srlb_i128(const Vec128I& x) noexcept { return N_BYTES ? _mm_srli_si128(x, N_BYTES) : x; }

#if defined(BL_TARGET_OPT_SSE4_1)
BL_INLINE bool v_test_zero(const Vec128I& x) noexcept { return _mm_testz_si128(x, x); }
#else
BL_INLINE bool v_test_zero(const Vec128I& x) noexcept { return !_mm_movemask_epi8(_mm_cmpeq_epi8(x, _mm_setzero_si128())); }
#endif

BL_INLINE bool v_test_mask_i8(const Vec128I& x, uint32_t bits0_15) noexcept { return _mm_movemask_epi8(v_cast<Vec128I>(x)) == int(bits0_15); }
BL_INLINE bool v_test_mask_i32(const Vec128I& x, uint32_t bits0_3) noexcept { return _mm_movemask_ps(v_cast<Vec128F>(x)) == int(bits0_3); }
BL_INLINE bool v_test_mask_i64(const Vec128I& x, uint32_t bits0_1) noexcept { return _mm_movemask_pd(v_cast<Vec128D>(x)) == int(bits0_1); }

BL_INLINE bool v_test_mask_f32(const Vec128F& x, uint32_t bits0_3) noexcept { return _mm_movemask_ps(v_cast<Vec128F>(x)) == int(bits0_3); }
BL_INLINE bool v_test_mask_f64(const Vec128D& x, uint32_t bits0_1) noexcept { return _mm_movemask_pd(v_cast<Vec128D>(x)) == int(bits0_1); }
#endif

// SIMD - Vec128 - Integer Packing & Unpacking
// ===========================================

#if defined(BL_TARGET_OPT_SSE2)
BL_INLINE Vec128I v_packs_i16_i8(const Vec128I& x, const Vec128I& y) noexcept { return _mm_packs_epi16(x, y); }
BL_INLINE Vec128I v_packs_i16_u8(const Vec128I& x, const Vec128I& y) noexcept { return _mm_packus_epi16(x, y); }
BL_INLINE Vec128I v_packs_i32_i16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_packs_epi32(x, y); }

BL_INLINE Vec128I v_packs_i16_i8(const Vec128I& x) noexcept { return v_packs_i16_i8(x, x); }
BL_INLINE Vec128I v_packs_i16_u8(const Vec128I& x) noexcept { return v_packs_i16_u8(x, x); }
BL_INLINE Vec128I v_packs_i32_i16(const Vec128I& x) noexcept { return v_packs_i32_i16(x, x); }

#if defined(BL_TARGET_OPT_SSE4_1)
BL_INLINE Vec128I v_packs_i32_u16(const Vec128I& x) noexcept { return _mm_packus_epi32(x, x); }
BL_INLINE Vec128I v_packs_i32_u16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_packus_epi32(x, y); }
#else
BL_INLINE Vec128I v_packs_i32_u16(const Vec128I& x) noexcept {
  Vec128I xShifted = _mm_srai_epi32(_mm_slli_epi32(x, 16), 16);
  return _mm_packs_epi32(xShifted, xShifted);
}
BL_INLINE Vec128I v_packs_i32_u16(const Vec128I& x, const Vec128I& y) noexcept {
  Vec128I xShifted = _mm_srai_epi32(_mm_slli_epi32(x, 16), 16);
  Vec128I yShifted = _mm_srai_epi32(_mm_slli_epi32(y, 16), 16);
  return _mm_packs_epi32(xShifted, yShifted);
}
#endif

BL_INLINE Vec128I v_packs_i32_i8(const Vec128I& x) noexcept { return v_packs_i16_i8(v_packs_i32_i16(x)); }
BL_INLINE Vec128I v_packs_i32_u8(const Vec128I& x) noexcept { return v_packs_i16_u8(v_packs_i32_i16(x)); }

BL_INLINE Vec128I v_packs_i32_i8(const Vec128I& x, const Vec128I& y) noexcept { return v_packs_i16_i8(v_packs_i32_i16(x, y)); }
BL_INLINE Vec128I v_packs_i32_u8(const Vec128I& x, const Vec128I& y) noexcept { return v_packs_i16_u8(v_packs_i32_i16(x, y)); }

BL_INLINE Vec128I v_packs_i32_i8(const Vec128I& x, const Vec128I& y, const Vec128I& z, const Vec128I& w) noexcept { return v_packs_i16_i8(v_packs_i32_i16(x, y), v_packs_i32_i16(z, w)); }
BL_INLINE Vec128I v_packs_i32_u8(const Vec128I& x, const Vec128I& y, const Vec128I& z, const Vec128I& w) noexcept { return v_packs_i16_u8(v_packs_i32_i16(x, y), v_packs_i32_i16(z, w)); }

// These assume that HI bytes of all inputs are always zero, so the implementation
// can decide between packing with signed/unsigned saturation or vector swizzling.
BL_INLINE Vec128I v_packz_u16_u8(const Vec128I& x) noexcept { return v_packs_i16_u8(x); }
BL_INLINE Vec128I v_packz_u16_u8(const Vec128I& x, const Vec128I& y) noexcept { return v_packs_i16_u8(x, y); }

#if defined(BL_TARGET_OPT_SSE4_1) || !defined(BL_TARGET_OPT_SSSE3)
BL_INLINE Vec128I v_packz_u32_u16(const Vec128I& x) noexcept { return v_packs_i32_u16(x); }
BL_INLINE Vec128I v_packz_u32_u16(const Vec128I& x, const Vec128I& y) noexcept { return v_packs_i32_u16(x, y); }
#else
BL_INLINE Vec128I v_packz_u32_u16(const Vec128I& x) noexcept {
  return v_shuffle_i8(x, v_const_as<Vec128I>(&blCommonTable.pshufb_xx76xx54xx32xx10_to_7654321076543210));
}

BL_INLINE Vec128I v_packz_u32_u16(const Vec128I& x, const Vec128I& y) noexcept {
  Vec128I xLo = v_shuffle_i8(x, v_const_as<Vec128I>(&blCommonTable.pshufb_xx76xx54xx32xx10_to_7654321076543210));
  Vec128I yLo = v_shuffle_i8(y, v_const_as<Vec128I>(&blCommonTable.pshufb_xx76xx54xx32xx10_to_7654321076543210));
  return _mm_unpacklo_epi64(xLo, yLo);
}
#endif

#if defined(BL_TARGET_OPT_SSSE3)
BL_INLINE Vec128I v_packz_u32_u8(const Vec128I& x) noexcept { return v_shuffle_i8(x, v_const_as<Vec128I>(&blCommonTable.pshufb_xxx3xxx2xxx1xxx0_to_3210321032103210)); }
#else
BL_INLINE Vec128I v_packz_u32_u8(const Vec128I& x) noexcept { return v_packs_i16_u8(v_packs_i32_i16(x)); }
#endif

BL_INLINE Vec128I v_packz_u32_u8(const Vec128I& x, const Vec128I& y) noexcept { return v_packs_i16_u8(v_packs_i32_i16(x, y)); }
BL_INLINE Vec128I v_packz_u32_u8(const Vec128I& x, const Vec128I& y, const Vec128I& z, const Vec128I& w) noexcept { return v_packs_i16_u8(v_packs_i32_i16(x, y), v_packs_i32_i16(z, w)); }
#endif

#if defined(BL_TARGET_OPT_SSE4_1)
BL_INLINE Vec128I v_unpack_lo_u8_u16(const Vec128I& x) noexcept { return _mm_cvtepu8_epi16(x); }
BL_INLINE Vec128I v_unpack_lo_u16_u32(const Vec128I& x) noexcept { return _mm_cvtepu16_epi32(x); }
BL_INLINE Vec128I v_unpack_lo_u32_u64(const Vec128I& x) noexcept { return _mm_cvtepu32_epi64(x); }
#else
BL_INLINE Vec128I v_unpack_lo_u8_u16(const Vec128I& x) noexcept { return _mm_unpacklo_epi8(x, _mm_setzero_si128()); }
BL_INLINE Vec128I v_unpack_lo_u16_u32(const Vec128I& x) noexcept { return _mm_unpacklo_epi16(x, _mm_setzero_si128()); }
BL_INLINE Vec128I v_unpack_lo_u32_u64(const Vec128I& x) noexcept { return _mm_unpacklo_epi32(x, _mm_setzero_si128()); }
#endif

BL_INLINE Vec128I v_unpack_hi_u8_u16(const Vec128I& x) noexcept { return _mm_unpackhi_epi8(x, _mm_setzero_si128()); }
BL_INLINE Vec128I v_unpack_hi_u16_u32(const Vec128I& x) noexcept { return _mm_unpackhi_epi16(x, _mm_setzero_si128()); }
BL_INLINE Vec128I v_unpack_hi_u32_u64(const Vec128I& x) noexcept { return _mm_unpackhi_epi32(x, _mm_setzero_si128()); }

// SIMD - Vec128 - Integer Operations
// ==================================

#if defined(BL_TARGET_OPT_SSE2)
BL_INLINE Vec128I v_add_i8(const Vec128I& x, const Vec128I& y) noexcept { return _mm_add_epi8(x, y); }
BL_INLINE Vec128I v_add_i16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_add_epi16(x, y); }
BL_INLINE Vec128I v_add_i32(const Vec128I& x, const Vec128I& y) noexcept { return _mm_add_epi32(x, y); }
BL_INLINE Vec128I v_add_i64(const Vec128I& x, const Vec128I& y) noexcept { return _mm_add_epi64(x, y); }

BL_INLINE Vec128I v_adds_i8(const Vec128I& x, const Vec128I& y) noexcept { return _mm_adds_epi8(x, y); }
BL_INLINE Vec128I v_adds_u8(const Vec128I& x, const Vec128I& y) noexcept { return _mm_adds_epu8(x, y); }
BL_INLINE Vec128I v_adds_i16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_adds_epi16(x, y); }
BL_INLINE Vec128I v_adds_u16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_adds_epu16(x, y); }

BL_INLINE Vec128I v_sub_i8(const Vec128I& x, const Vec128I& y) noexcept { return _mm_sub_epi8(x, y); }
BL_INLINE Vec128I v_sub_i16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_sub_epi16(x, y); }
BL_INLINE Vec128I v_sub_i32(const Vec128I& x, const Vec128I& y) noexcept { return _mm_sub_epi32(x, y); }
BL_INLINE Vec128I v_sub_i64(const Vec128I& x, const Vec128I& y) noexcept { return _mm_sub_epi64(x, y); }

BL_INLINE Vec128I v_subs_i8(const Vec128I& x, const Vec128I& y) noexcept { return _mm_subs_epi8(x, y); }
BL_INLINE Vec128I v_subs_u8(const Vec128I& x, const Vec128I& y) noexcept { return _mm_subs_epu8(x, y); }
BL_INLINE Vec128I v_subs_i16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_subs_epi16(x, y); }
BL_INLINE Vec128I v_subs_u16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_subs_epu16(x, y); }

BL_INLINE Vec128I v_mul_i16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_mullo_epi16(x, y); }
BL_INLINE Vec128I v_mul_u16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_mullo_epi16(x, y); }
BL_INLINE Vec128I v_mulh_i16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_mulhi_epi16(x, y); }
BL_INLINE Vec128I v_mulh_u16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_mulhi_epu16(x, y); }

#if defined(BL_TARGET_OPT_SSE4_1)
BL_INLINE Vec128I v_mul_i32(const Vec128I& x, const Vec128I& y) noexcept { return _mm_mullo_epi32(x, y); }
BL_INLINE Vec128I v_mul_u32(const Vec128I& x, const Vec128I& y) noexcept { return _mm_mullo_epi32(x, y); }
#endif

BL_INLINE Vec128I v_madd_i16_i32(const Vec128I& x, const Vec128I& y) noexcept { return _mm_madd_epi16(x, y); }

#if defined(BL_TARGET_OPT_SSE4_1)
BL_INLINE Vec128I v_min_i8(const Vec128I& x, const Vec128I& y) noexcept { return _mm_min_epi8(x, y); }
BL_INLINE Vec128I v_max_i8(const Vec128I& x, const Vec128I& y) noexcept { return _mm_max_epi8(x, y); }
#else
BL_INLINE Vec128I v_min_i8(const Vec128I& x, const Vec128I& y) noexcept { return v_blend_i8(y, x, _mm_cmpgt_epi8(x, y)); }
BL_INLINE Vec128I v_max_i8(const Vec128I& x, const Vec128I& y) noexcept { return v_blend_i8(x, y, _mm_cmpgt_epi8(x, y)); }
#endif

BL_INLINE Vec128I v_min_u8(const Vec128I& x, const Vec128I& y) noexcept { return _mm_min_epu8(x, y); }
BL_INLINE Vec128I v_max_u8(const Vec128I& x, const Vec128I& y) noexcept { return _mm_max_epu8(x, y); }

BL_INLINE Vec128I v_min_i16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_min_epi16(x, y); }
BL_INLINE Vec128I v_max_i16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_max_epi16(x, y); }

#if defined(BL_TARGET_OPT_SSE4_1)
BL_INLINE Vec128I v_min_u16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_min_epu16(x, y); }
BL_INLINE Vec128I v_max_u16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_max_epu16(x, y); }
#else
BL_INLINE Vec128I v_min_u16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_sub_epi16(x, _mm_subs_epu16(x, y)); }
BL_INLINE Vec128I v_max_u16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_add_epi16(x, _mm_subs_epu16(x, y)); }
#endif

#if defined(BL_TARGET_OPT_SSE4_1)
BL_INLINE Vec128I v_min_i32(const Vec128I& x, const Vec128I& y) noexcept { return _mm_min_epi32(x, y); }
BL_INLINE Vec128I v_max_i32(const Vec128I& x, const Vec128I& y) noexcept { return _mm_max_epi32(x, y); }
#else
BL_INLINE Vec128I v_min_i32(const Vec128I& x, const Vec128I& y) noexcept { return v_blend_i8(y, x, _mm_cmpgt_epi32(x, y)); }
BL_INLINE Vec128I v_max_i32(const Vec128I& x, const Vec128I& y) noexcept { return v_blend_i8(x, y, _mm_cmpgt_epi32(x, y)); }
#endif

BL_INLINE Vec128I v_cmp_eq_i8(const Vec128I& x, const Vec128I& y) noexcept { return _mm_cmpeq_epi8(x, y); }
BL_INLINE Vec128I v_cmp_gt_i8(const Vec128I& x, const Vec128I& y) noexcept { return _mm_cmpgt_epi8(x, y); }

BL_INLINE Vec128I v_cmp_eq_i16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_cmpeq_epi16(x, y); }
BL_INLINE Vec128I v_cmp_gt_i16(const Vec128I& x, const Vec128I& y) noexcept { return _mm_cmpgt_epi16(x, y); }

BL_INLINE Vec128I v_cmp_eq_i32(const Vec128I& x, const Vec128I& y) noexcept { return _mm_cmpeq_epi32(x, y); }
BL_INLINE Vec128I v_cmp_gt_i32(const Vec128I& x, const Vec128I& y) noexcept { return _mm_cmpgt_epi32(x, y); }

#if defined(BL_TARGET_OPT_SSSE3)
BL_INLINE Vec128I v_abs_i8(const Vec128I& x) noexcept { return _mm_abs_epi8(x); }
BL_INLINE Vec128I v_abs_i16(const Vec128I& x) noexcept { return _mm_abs_epi16(x); }
BL_INLINE Vec128I v_abs_i32(const Vec128I& x) noexcept { return _mm_abs_epi32(x); }
#else
BL_INLINE Vec128I v_abs_i8(const Vec128I& x) noexcept { return v_min_u8(v_sub_i8(v_zero_i128(), x), x); }
BL_INLINE Vec128I v_abs_i16(const Vec128I& x) noexcept { return v_max_i16(v_sub_i16(v_zero_i128(), x), x); }
BL_INLINE Vec128I v_abs_i32(const Vec128I& x) noexcept { Vec128I y = v_sra_i32<31>(x); return v_sub_i32(v_xor(x, y), y); }
#endif

BL_INLINE Vec128I v_div255_u16(const Vec128I& x) noexcept {
  Vec128I y = v_add_i16(x, v_const_as<Vec128I>(&blCommonTable.i_0080008000800080));
  return v_mulh_u16(y, v_const_as<Vec128I>(&blCommonTable.i_0101010101010101));
}
#endif

// SIMD - Vec128 - Floating Point Operations
// =========================================

#if defined(BL_TARGET_OPT_SSE2)
BL_INLINE Vec128F s_add_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_add_ss(x, y); }
BL_INLINE Vec128D s_add_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_add_sd(x, y); }
BL_INLINE Vec128F s_sub_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_sub_ss(x, y); }
BL_INLINE Vec128D s_sub_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_sub_sd(x, y); }
BL_INLINE Vec128F s_mul_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_mul_ss(x, y); }
BL_INLINE Vec128D s_mul_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_mul_sd(x, y); }
BL_INLINE Vec128F s_div_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_div_ss(x, y); }
BL_INLINE Vec128D s_div_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_div_sd(x, y); }
BL_INLINE Vec128F s_min_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_min_ss(x, y); }
BL_INLINE Vec128D s_min_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_min_sd(x, y); }
BL_INLINE Vec128F s_max_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_max_ss(x, y); }
BL_INLINE Vec128D s_max_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_max_sd(x, y); }

BL_INLINE Vec128F s_sqrt_f32(const Vec128F& x) noexcept { return _mm_sqrt_ss(x); }
BL_INLINE Vec128D s_sqrt_f64(const Vec128D& x) noexcept { return _mm_sqrt_sd(x, x); }

BL_INLINE Vec128F s_cmp_eq_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_cmpeq_ss(x, y); }
BL_INLINE Vec128D s_cmp_eq_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_cmpeq_sd(x, y); }
BL_INLINE Vec128F s_cmp_ne_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_cmpneq_ss(x, y); }
BL_INLINE Vec128D s_cmp_ne_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_cmpneq_sd(x, y); }
BL_INLINE Vec128F s_cmp_ge_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_cmpge_ss(x, y); }
BL_INLINE Vec128D s_cmp_ge_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_cmpge_sd(x, y); }
BL_INLINE Vec128F s_cmp_gt_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_cmpgt_ss(x, y); }
BL_INLINE Vec128D s_cmp_gt_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_cmpgt_sd(x, y); }
BL_INLINE Vec128F s_cmp_le_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_cmple_ss(x, y); }
BL_INLINE Vec128D s_cmp_le_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_cmple_sd(x, y); }
BL_INLINE Vec128F s_cmp_lt_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_cmplt_ss(x, y); }
BL_INLINE Vec128D s_cmp_lt_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_cmplt_sd(x, y); }

BL_INLINE Vec128F v_add_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_add_ps(x, y); }
BL_INLINE Vec128D v_add_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_add_pd(x, y); }
BL_INLINE Vec128F v_sub_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_sub_ps(x, y); }
BL_INLINE Vec128D v_sub_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_sub_pd(x, y); }
BL_INLINE Vec128F v_mul_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_mul_ps(x, y); }
BL_INLINE Vec128D v_mul_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_mul_pd(x, y); }
BL_INLINE Vec128F v_div_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_div_ps(x, y); }
BL_INLINE Vec128D v_div_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_div_pd(x, y); }
BL_INLINE Vec128F v_min_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_min_ps(x, y); }
BL_INLINE Vec128D v_min_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_min_pd(x, y); }
BL_INLINE Vec128F v_max_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_max_ps(x, y); }
BL_INLINE Vec128D v_max_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_max_pd(x, y); }

BL_INLINE Vec128F v_sqrt_f32(const Vec128F& x) noexcept { return _mm_sqrt_ps(x); }
BL_INLINE Vec128D v_sqrt_f64(const Vec128D& x) noexcept { return _mm_sqrt_pd(x); }

BL_INLINE Vec128F v_cmp_eq_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_cmpeq_ps(x, y); }
BL_INLINE Vec128D v_cmp_eq_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_cmpeq_pd(x, y); }
BL_INLINE Vec128F v_cmp_ne_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_cmpneq_ps(x, y); }
BL_INLINE Vec128D v_cmp_ne_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_cmpneq_pd(x, y); }
BL_INLINE Vec128F v_cmp_ge_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_cmpge_ps(x, y); }
BL_INLINE Vec128D v_cmp_ge_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_cmpge_pd(x, y); }
BL_INLINE Vec128F v_cmp_gt_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_cmpgt_ps(x, y); }
BL_INLINE Vec128D v_cmp_gt_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_cmpgt_pd(x, y); }
BL_INLINE Vec128F v_cmp_le_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_cmple_ps(x, y); }
BL_INLINE Vec128D v_cmp_le_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_cmple_pd(x, y); }
BL_INLINE Vec128F v_cmp_lt_f32(const Vec128F& x, const Vec128F& y) noexcept { return _mm_cmplt_ps(x, y); }
BL_INLINE Vec128D v_cmp_lt_f64(const Vec128D& x, const Vec128D& y) noexcept { return _mm_cmplt_pd(x, y); }
#endif

// SIMD - Vec256 - Zero Value
// ==========================

#if defined(BL_TARGET_OPT_AVX)
BL_INLINE Vec256I v_zero_i256() noexcept { return _mm256_setzero_si256(); }
BL_INLINE Vec256F v_zero_f256() noexcept { return _mm256_setzero_ps(); }
BL_INLINE Vec256D v_zero_d256() noexcept { return _mm256_setzero_pd(); }
#endif

// SIMD - Vec256 - Fill Value
// ==========================

#if defined(BL_TARGET_OPT_AVX2)
BL_INLINE Vec256I v_fill_i256_i8(int8_t x) noexcept { return _mm256_set1_epi8(x); }
BL_INLINE Vec256I v_fill_i256_i16(int16_t x) noexcept { return _mm256_set1_epi16(x); }

BL_INLINE Vec256I v_fill_i256_i32(int32_t x) noexcept { return _mm256_set1_epi32(x); }
BL_INLINE Vec256I v_fill_i256_i32(int32_t x1, int32_t x0) noexcept { return _mm256_set_epi32(x1, x0, x1, x0, x1, x0, x1, x0); }
BL_INLINE Vec256I v_fill_i256_i32(int32_t x3, int32_t x2, int32_t x1, int32_t x0) noexcept { return _mm256_set_epi32(x3, x2, x1, x0, x3, x2, x1, x0); }
BL_INLINE Vec256I v_fill_i256_i32(int32_t x7, int32_t x6, int32_t x5, int32_t x4, int32_t x3, int32_t x2, int32_t x1, int32_t x0) noexcept { return _mm256_set_epi32(x7, x6, x5, x4, x3, x2, x1, x0); }

#if BL_TARGET_ARCH_BITS >= 64
BL_INLINE Vec256I v_fill_i256_i64(int64_t x) noexcept { return _mm256_set1_epi64x(x); }
#else
BL_INLINE Vec256I v_fill_i256_i64(int64_t x) noexcept { return v_fill_i256_i32(int32_t(uint64_t(x) >> 32), int32_t(x & 0xFFFFFFFFu)); }
#endif

BL_INLINE Vec256I v_fill_i256_i64(int64_t x1, int64_t x0) noexcept {
  return v_fill_i256_i32(int32_t(uint64_t(x1) >> 32), int32_t(x1 & 0xFFFFFFFFu),
                         int32_t(uint64_t(x0) >> 32), int32_t(x0 & 0xFFFFFFFFu),
                         int32_t(uint64_t(x1) >> 32), int32_t(x1 & 0xFFFFFFFFu),
                         int32_t(uint64_t(x0) >> 32), int32_t(x0 & 0xFFFFFFFFu));
}

BL_INLINE Vec256I v_fill_i256_i64(int64_t x3, int64_t x2, int64_t x1, int64_t x0) noexcept {
  return v_fill_i256_i32(int32_t(uint64_t(x3) >> 32), int32_t(x3 & 0xFFFFFFFFu),
                         int32_t(uint64_t(x2) >> 32), int32_t(x2 & 0xFFFFFFFFu),
                         int32_t(uint64_t(x1) >> 32), int32_t(x1 & 0xFFFFFFFFu),
                         int32_t(uint64_t(x0) >> 32), int32_t(x0 & 0xFFFFFFFFu));
}

BL_INLINE Vec256I v_fill_i256_u8(uint8_t x) noexcept { return v_fill_i256_i8(int8_t(x)); }
BL_INLINE Vec256I v_fill_i256_u16(uint16_t x) noexcept { return v_fill_i256_i16(int16_t(x)); }
BL_INLINE Vec256I v_fill_i256_u32(uint32_t x) noexcept { return v_fill_i256_i32(int32_t(x)); }
BL_INLINE Vec256I v_fill_i256_u64(uint64_t x) noexcept { return v_fill_i256_i64(int64_t(x)); }

BL_INLINE Vec256I v_fill_i256_u32(uint32_t x1, uint32_t x0) noexcept {
  return v_fill_i256_i32(int32_t(x1), int32_t(x0), int32_t(x1), int32_t(x0),
                         int32_t(x1), int32_t(x0), int32_t(x1), int32_t(x0));
}

BL_INLINE Vec256I v_fill_i256_u32(uint32_t x3, uint32_t x2, uint32_t x1, uint32_t x0) noexcept {
  return v_fill_i256_i32(int32_t(x3), int32_t(x2), int32_t(x1), int32_t(x0),
                         int32_t(x3), int32_t(x2), int32_t(x1), int32_t(x0));
}

BL_INLINE Vec256I v_fill_i256_u32(uint32_t x7, uint32_t x6, uint32_t x5, uint32_t x4, uint32_t x3, uint32_t x2, uint32_t x1, uint32_t x0) noexcept {
  return v_fill_i256_i32(int32_t(x7), int32_t(x6), int32_t(x5), int32_t(x4),
                         int32_t(x3), int32_t(x2), int32_t(x1), int32_t(x0));
}

BL_INLINE Vec256I v_fill_i256_u64(uint64_t x1, uint64_t x0) noexcept {
  return v_fill_i256_i64(int64_t(x1), int64_t(x0));
}

BL_INLINE Vec256I v_fill_i256_u64(uint64_t x3, uint64_t x2, uint64_t x1, uint64_t x0) noexcept {
  return v_fill_i256_i64(int64_t(x3), int64_t(x2), int64_t(x1), int64_t(x0));
}

BL_INLINE Vec256I v_fill_i256_i128(const Vec128I& hi, const Vec128I& lo) noexcept {
#if defined(__clang__) || defined(_MSC_VER) || (defined(__GNUC__) && __GNUC__ >= 8)
  return _mm256_set_m128i(hi, lo);
#else
  return _mm256_inserti128_si256(v_cast<Vec256I>(lo), hi, 1);
#endif
}
#endif

#if defined(BL_TARGET_OPT_AVX)
BL_INLINE Vec256F v_fill_f256(float x) noexcept { return _mm256_set1_ps(x); }
BL_INLINE Vec256F v_fill_f256(float x1, float x0) noexcept { return _mm256_set_ps(x1, x0, x1, x0, x1, x0, x1, x0); }
BL_INLINE Vec256F v_fill_f256(float x3, float x2, float x1, float x0) noexcept { return _mm256_set_ps(x3, x2, x1, x0, x3, x2, x1, x0); }
BL_INLINE Vec256F v_fill_f256(float x7, float x6, float x5, float x4, float x3, float x2, float x1, float x0) noexcept { return _mm256_set_ps(x7, x6, x5, x4, x3, x2, x1, x0); }

BL_INLINE Vec256D v_fill_d256(double x) noexcept { return _mm256_set1_pd(x); }
BL_INLINE Vec256D v_fill_d256(double x1, double x0) noexcept { return _mm256_set_pd(x1, x0, x1, x0); }
BL_INLINE Vec256D v_fill_d256(double x3, double x2, double x1, double x0) noexcept { return _mm256_set_pd(x3, x2, x1, x0); }
#endif

// SIMD - Vec256 - Load & Store
// ============================

#if defined(BL_TARGET_OPT_AVX2)
BL_INLINE Vec256I v_load_i256_32(const void* p) noexcept { return v_cast<Vec256I>(v_load_i32(p)); }
BL_INLINE Vec256I v_load_i256_64(const void* p) noexcept { return v_cast<Vec256I>(v_load_i64(p)); }
BL_INLINE Vec256I v_loada_i256_128(const void* p) noexcept { return v_cast<Vec256I>(v_loada_i128(p)); }
BL_INLINE Vec256I v_loadu_i256_128(const void* p) noexcept { return v_cast<Vec256I>(v_loadu_i128(p)); }
BL_INLINE Vec256I v_loada_i256(const void* p) noexcept { return _mm256_load_si256(static_cast<const Vec256I*>(p)); }
BL_INLINE Vec256I v_loadu_i256(const void* p) noexcept { return _mm256_loadu_si256(static_cast<const Vec256I*>(p)); }

BL_INLINE Vec256I v_loadu_i256_mask32(const void* p, const Vec256I& mask) noexcept { return _mm256_maskload_epi32(static_cast<const int*>(p), mask); }
BL_INLINE Vec256I v_loadu_i256_mask64(const void* p, const Vec256I& mask) noexcept { return _mm256_maskload_epi64(static_cast<const long long*>(p), mask); }

BL_INLINE Vec256I v_loada_i128_i8_i16(const void* p) noexcept { return _mm256_cvtepi8_epi16(*static_cast<const Vec128I*>(p)); }
BL_INLINE Vec256I v_loadu_i128_i8_i16(const void* p) noexcept { return _mm256_cvtepi8_epi16(*static_cast<const Vec128I_Unaligned*>(p)); }
BL_INLINE Vec256I v_loada_i128_u8_u16(const void* p) noexcept { return _mm256_cvtepu8_epi16(*static_cast<const Vec128I*>(p)); }
BL_INLINE Vec256I v_loadu_i128_u8_u16(const void* p) noexcept { return _mm256_cvtepu8_epi16(*static_cast<const Vec128I_Unaligned*>(p)); }

// MSCV won't emit a single instruction if we use v_load_iXX() to load from memory.
#if defined(_MSC_VER) && !defined(__clang__)
BL_INLINE Vec256I v_load_i64_i8_i32(const void* p) noexcept { return _mm256_cvtepi8_epi32(*static_cast<const Vec128I_Unaligned*>(p)); }
BL_INLINE Vec256I v_load_i64_u8_u32(const void* p) noexcept { return _mm256_cvtepu8_epi32(*static_cast<const Vec128I_Unaligned*>(p)); }
BL_INLINE Vec256I v_load_i32_i8_i64(const void* p) noexcept { return _mm256_cvtepi8_epi64(*static_cast<const Vec128I_Unaligned*>(p)); }
BL_INLINE Vec256I v_load_i32_u8_u64(const void* p) noexcept { return _mm256_cvtepu8_epi64(*static_cast<const Vec128I_Unaligned*>(p)); }
#else
BL_INLINE Vec256I v_load_i64_i8_i32(const void* p) noexcept { return _mm256_cvtepi8_epi32(v_load_i64(p)); }
BL_INLINE Vec256I v_load_i64_u8_u32(const void* p) noexcept { return _mm256_cvtepu8_epi32(v_load_i64(p)); }
BL_INLINE Vec256I v_load_i32_i8_i64(const void* p) noexcept { return _mm256_cvtepi8_epi64(v_load_i32(p)); }
BL_INLINE Vec256I v_load_i32_u8_u64(const void* p) noexcept { return _mm256_cvtepu8_epi64(v_load_i32(p)); }
#endif

BL_INLINE void v_store_i32(void* p, const Vec256I& x) noexcept { v_store_i32(p, v_cast<Vec128I>(x)); }
BL_INLINE void v_store_i64(void* p, const Vec256I& x) noexcept { v_store_i64(p, v_cast<Vec128I>(x)); }
BL_INLINE void v_storea_i128(void* p, const Vec256I& x) noexcept { v_storea_i128(p, v_cast<Vec128I>(x)); }
BL_INLINE void v_storeu_i128(void* p, const Vec256I& x) noexcept { v_storeu_i128(p, v_cast<Vec128I>(x)); }
BL_INLINE void v_storea_i256(void* p, const Vec256I& x) noexcept { _mm256_store_si256(static_cast<Vec256I*>(p), x); }
BL_INLINE void v_storeu_i256(void* p, const Vec256I& x) noexcept { _mm256_storeu_si256(static_cast<Vec256I*>(p), x); }
BL_INLINE void v_storeu_i256_mask32(void* p, const Vec256I& x, const Vec256I& mask) noexcept { _mm256_maskstore_epi32(static_cast<int*>(p), mask, x); }
BL_INLINE void v_storeu_i256_mask64(void* p, const Vec256I& x, const Vec256I& mask) noexcept { _mm256_maskstore_epi64(static_cast<long long*>(p), mask, x); }

BL_INLINE void v_storel_i64(void* p, const Vec256I& x) noexcept { v_storel_i64(p, v_cast<Vec128I>(x)); }
BL_INLINE void v_storeh_i64(void* p, const Vec256I& x) noexcept { v_storeh_i64(p, v_cast<Vec128I>(x)); }
#endif

#if defined(BL_TARGET_OPT_AVX)
BL_INLINE Vec256F v_load_f256_32(const void* p) noexcept { return v_cast<Vec256F>(v_load_f32(p)); }
BL_INLINE Vec256F v_load_f256_64(const void* p) noexcept { return v_cast<Vec256F>(v_load_2xf32(p)); }
BL_INLINE Vec256D v_load_d256_64(const void* p) noexcept { return v_cast<Vec256D>(v_load_f64(p)); }
BL_INLINE Vec256F v_loadu_f256_128(const void* p) noexcept { return v_cast<Vec256F>(v_loadu_f128(p)); }
BL_INLINE Vec256D v_loadu_d256_128(const void* p) noexcept { return v_cast<Vec256D>(v_loadu_d128(p)); }
BL_INLINE Vec256F v_loada_f256_128(const void* p) noexcept { return v_cast<Vec256F>(v_loada_f128(p)); }
BL_INLINE Vec256D v_loada_d256_128(const void* p) noexcept { return v_cast<Vec256D>(v_loada_d128(p)); }
BL_INLINE Vec256F v_loadu_f256(const void* p) noexcept { return _mm256_loadu_ps(static_cast<const float*>(p)); }
BL_INLINE Vec256D v_loadu_d256(const void* p) noexcept { return _mm256_loadu_pd(static_cast<const double*>(p)); }
BL_INLINE Vec256F v_loada_f256(const void* p) noexcept { return _mm256_load_ps(static_cast<const float*>(p)); }
BL_INLINE Vec256D v_loada_d256(const void* p) noexcept { return _mm256_load_pd(static_cast<const double*>(p)); }

BL_INLINE Vec256F v_loadu_f256_mask32(const void* p, const Vec256F& mask) noexcept { return _mm256_maskload_ps(static_cast<const float*>(p), v_cast<Vec256I>(mask)); }
BL_INLINE Vec256D v_loadu_d256_mask64(const void* p, const Vec256D& mask) noexcept { return _mm256_maskload_pd(static_cast<const double*>(p), v_cast<Vec256I>(mask)); }

BL_INLINE void v_store_f32(void* p, const Vec256F& x) noexcept { v_store_f32(p, v_cast<Vec128F>(x)); }
BL_INLINE void v_store_2xf32(void* p, const Vec256F& x) noexcept { v_store_2xf32(p, v_cast<Vec128F>(x)); }
BL_INLINE void v_store_f64(void* p, const Vec256D& x) noexcept { v_store_f64(p, v_cast<Vec128D>(x)); }
BL_INLINE void v_storel_2xf32(void* p, const Vec256F& x) noexcept { v_storel_2xf32(p, v_cast<Vec128F>(x)); }
BL_INLINE void v_storeh_2xf32(void* p, const Vec256F& x) noexcept { v_storeh_2xf32(p, v_cast<Vec128F>(x)); }
BL_INLINE void v_storel_f64(void* p, const Vec256D& x) noexcept { v_storel_f64(p, v_cast<Vec128D>(x)); }
BL_INLINE void v_storeh_f64(void* p, const Vec256D& x) noexcept { v_storeh_f64(p, v_cast<Vec128D>(x)); }
BL_INLINE void v_storea_f128(void* p, const Vec256F& x) noexcept { v_storea_f128(p, v_cast<Vec128F>(x)); }
BL_INLINE void v_storea_d128(void* p, const Vec256D& x) noexcept { v_storea_d128(p, v_cast<Vec128D>(x)); }
BL_INLINE void v_storeu_f128(void* p, const Vec256F& x) noexcept { v_storeu_f128(p, v_cast<Vec128F>(x)); }
BL_INLINE void v_storeu_d128(void* p, const Vec256D& x) noexcept { v_storeu_d128(p, v_cast<Vec128D>(x)); }
BL_INLINE void v_storea_f256(void* p, const Vec256F& x) noexcept { _mm256_store_ps(static_cast<float*>(p), x); }
BL_INLINE void v_storea_d256(void* p, const Vec256D& x) noexcept { _mm256_store_pd(static_cast<double*>(p), x); }
BL_INLINE void v_storeu_f256(void* p, const Vec256F& x) noexcept { _mm256_storeu_ps(static_cast<float*>(p), x); }
BL_INLINE void v_storeu_d256(void* p, const Vec256D& x) noexcept { _mm256_storeu_pd(static_cast<double*>(p), x); }

BL_INLINE void v_storeu_256f_mask32(void* p, const Vec256F& x, const Vec256F& mask) noexcept { _mm256_maskstore_ps(static_cast<float*>(p), v_cast<Vec256I>(mask), x); }
BL_INLINE void v_storeu_256d_mask64(void* p, const Vec256D& x, const Vec256D& mask) noexcept { _mm256_maskstore_pd(static_cast<double*>(p), v_cast<Vec256I>(mask), x); }
#endif

// SIMD - Vec256 - Insert & Extract
// ================================

// SIMD - Vec256 - Conversion
// ==========================

#if defined(BL_TARGET_OPT_AVX)
BL_INLINE int32_t v_get_i32(const Vec256I& x) noexcept { return v_get_i32(v_cast<Vec128I>(x)); }
BL_INLINE int64_t v_get_i64(const Vec256I& x) noexcept { return v_get_i64(v_cast<Vec128I>(x)); }
BL_INLINE uint32_t v_get_u32(const Vec256I& x) noexcept { return v_get_u32(v_cast<Vec128I>(x)); }
BL_INLINE uint64_t v_get_u64(const Vec256I& x) noexcept { return v_get_u64(v_cast<Vec128I>(x)); }

BL_INLINE float v_get_f32(const Vec256F& x) noexcept { return v_get_f32(v_cast<Vec128F>(x)); }
BL_INLINE double v_get_f64(const Vec256D& x) noexcept { return v_get_f64(v_cast<Vec128D>(x)); }

BL_INLINE int32_t s_cvt_f32_i32(const Vec256F& x) noexcept { return s_cvt_f32_i32(v_cast<Vec128F>(x)); }
BL_INLINE int32_t s_cvtt_f32_i32(const Vec256F& x) noexcept { return s_cvtt_f32_i32(v_cast<Vec128F>(x)); }

BL_INLINE int32_t s_cvt_f64_i32(const Vec256D& x) noexcept { return s_cvt_f64_i32(v_cast<Vec128D>(x)); }
BL_INLINE int32_t s_cvtt_f64_i32(const Vec256D& x) noexcept { return s_cvtt_f64_i32(v_cast<Vec128D>(x)); }

#if BL_TARGET_ARCH_BITS >= 64
BL_INLINE int64_t s_cvt_f32_i64(const Vec256F& x) noexcept { return s_cvt_f32_i64(v_cast<Vec128F>(x)); }
BL_INLINE int64_t s_cvtt_f32_i64(const Vec256F& x) noexcept { return s_cvtt_f32_i64(v_cast<Vec128F>(x)); }

BL_INLINE int64_t s_cvt_f64_i64(const Vec256D& x) noexcept { return s_cvt_f64_i64(v_cast<Vec128D>(x)); }
BL_INLINE int64_t s_cvtt_f64_i64(const Vec256D& x) noexcept { return s_cvtt_f64_i64(v_cast<Vec128D>(x)); }
#endif
#endif

#if defined(BL_TARGET_OPT_AVX)
BL_INLINE Vec256F v_cvt_i32_f32(const Vec256I& x) noexcept { return _mm256_cvtepi32_ps(x); }
BL_INLINE Vec256D v_cvt_4xi32_f64(const Vec128I& x) noexcept { return _mm256_cvtepi32_pd(v_cast<Vec128I>(x)); }
BL_INLINE Vec256D v_cvt_4xi32_f64(const Vec256I& x) noexcept { return _mm256_cvtepi32_pd(v_cast<Vec128I>(x)); }

BL_INLINE Vec256I v_cvt_f32_i32(const Vec256F& x) noexcept { return _mm256_cvtps_epi32(x); }
BL_INLINE Vec256I v_cvtt_f32_i32(const Vec256F& x) noexcept { return _mm256_cvttps_epi32(x); }

BL_INLINE Vec128F v_cvt_f64_f32(const Vec256D& x) noexcept { return _mm256_cvtpd_ps(x); }
BL_INLINE Vec256D v_cvt_4xf32_f64(const Vec128F& x) noexcept { return _mm256_cvtps_pd(v_cast<Vec128F>(x)); }
BL_INLINE Vec256D v_cvt_4xf32_f64(const Vec256F& x) noexcept { return _mm256_cvtps_pd(v_cast<Vec128F>(x)); }

BL_INLINE Vec128I v_cvt_f64_i32(const Vec256D& x) noexcept { return _mm256_cvtpd_epi32(x); }
BL_INLINE Vec128I v_cvtt_f64_i32(const Vec256D& x) noexcept { return _mm256_cvttpd_epi32(x); }
#endif

// SIMD - Vec256 - Shuffling & Permutations
// ========================================

#if defined(BL_TARGET_OPT_AVX2)
BL_INLINE Vec256I v_shuffle_i8(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_shuffle_epi8(x, y); }
#endif

#if defined(BL_TARGET_OPT_AVX)
template<uint8_t D, uint8_t C, uint8_t B, uint8_t A>
BL_INLINE Vec256F v_shuffle_32(const Vec256F& x, const Vec256F& y) noexcept { return _mm256_shuffle_ps(x, y, _MM_SHUFFLE(D, C, B, A)); }
template<uint8_t B, uint8_t A>
BL_INLINE Vec256D v_shuffle_64(const Vec256D& x, const Vec256D& y) noexcept { return _mm256_shuffle_pd(x, y, (B << 3) | (A << 2) | (B << 1) | A); }
#endif

#if defined(BL_TARGET_OPT_AVX2)
template<uint8_t D, uint8_t C, uint8_t B, uint8_t A>
BL_INLINE Vec256I v_swizzle_lo_i16(const Vec256I& x) noexcept { return _mm256_shufflelo_epi16(x, _MM_SHUFFLE(D, C, B, A)); }
template<uint8_t D, uint8_t C, uint8_t B, uint8_t A>
BL_INLINE Vec256I v_swizzle_hi_i16(const Vec256I& x) noexcept { return _mm256_shufflehi_epi16(x, _MM_SHUFFLE(D, C, B, A)); }

template<uint8_t D, uint8_t C, uint8_t B, uint8_t A>
BL_INLINE Vec256I v_swizzle_i16(const Vec256I& x) noexcept { return v_swizzle_hi_i16<D, C, B, A>(v_swizzle_lo_i16<D, C, B, A>(x)); }
template<uint8_t D, uint8_t C, uint8_t B, uint8_t A>
BL_INLINE Vec256I v_swizzle_i32(const Vec256I& x) noexcept { return _mm256_shuffle_epi32(x, _MM_SHUFFLE(D, C, B, A)); }
template<uint8_t B, uint8_t A>
BL_INLINE Vec256I v_swizzle_i64(const Vec256I& x) noexcept { return v_swizzle_i32<B*2 + 1, B*2, A*2 + 1, A*2>(x); }
#endif

#if defined(BL_TARGET_OPT_AVX)
template<uint8_t D, uint8_t C, uint8_t B, uint8_t A>
BL_INLINE Vec256F v_swizzle_f32(const Vec256F& x) noexcept { return v_shuffle_32<D, C, B, A>(x, x); }
template<uint8_t B, uint8_t A>
BL_INLINE Vec256F v_swizzle_2xf32(const Vec256F& x) noexcept { return v_shuffle_32<B*2 + 1, B*2, A*2 + 1, A*2>(x, x); }
template<uint8_t B, uint8_t A>
BL_INLINE Vec256D v_swizzle_f64(const Vec256D& x) noexcept { return v_shuffle_64<B, A>(x, x); }
#endif

#if defined(BL_TARGET_OPT_AVX2)
template<uint8_t D, uint8_t C, uint8_t B, uint8_t A>
BL_INLINE Vec256I v_permute_i64(const Vec256I& x) noexcept { return _mm256_permute4x64_epi64(x, _MM_SHUFFLE(D, C, B, A)); }

template<uint8_t B, uint8_t A>
BL_INLINE Vec256I v_permute_i128(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_permute2x128_si256(x, y, ((B & 0xF) << 4) + (A & 0xF)); }
template<uint8_t B, uint8_t A>
BL_INLINE Vec256I v_permute_i128(const Vec256I& x) noexcept { return v_permute_i128<B, A>(x, x); }

#endif

#if defined(BL_TARGET_OPT_AVX)
template<uint8_t B, uint8_t A>
BL_INLINE Vec256F v_permute_f128(const Vec256F& x, const Vec256F& y) noexcept { return _mm256_permute2f128_ps(x, y, ((B & 0xF) << 4) + (A & 0xF)); }
template<uint8_t B, uint8_t A>
BL_INLINE Vec256F v_permute_f128(const Vec256F& x) noexcept { return v_permute_f128<B, A>(x, x); }
template<uint8_t B, uint8_t A>
BL_INLINE Vec256D v_permute_d128(const Vec256D& x, const Vec256D& y) noexcept { return _mm256_permute2f128_pd(x, y, ((B & 0xF) << 4) + (A & 0xF)); }
template<uint8_t B, uint8_t A>
BL_INLINE Vec256D v_permute_d128(const Vec256D& x) noexcept { return v_permute_d128<B, A>(x, x); }
#endif

#if defined(BL_TARGET_OPT_AVX2)
BL_INLINE Vec256I v_swap_i32(const Vec256I& x) noexcept { return v_swizzle_i32<2, 3, 0, 1>(x); }
BL_INLINE Vec256I v_swap_i64(const Vec256I& x) noexcept { return v_swizzle_i64<0, 1>(x); }
BL_INLINE Vec256I v_swap_i128(const Vec256I& x) noexcept { return v_permute_i128<0, 1>(x); }

BL_INLINE Vec256I v_dupl_i64(const Vec256I& x) noexcept { return v_swizzle_i64<0, 0>(x); }
BL_INLINE Vec256I v_duph_i64(const Vec256I& x) noexcept { return v_swizzle_i64<1, 1>(x); }

BL_INLINE Vec256I v_dupl_i128(const Vec128I& x) noexcept { return v_permute_i128<0, 0>(v_cast<Vec256I>(x)); }
BL_INLINE Vec256I v_dupl_i128(const Vec256I& x) noexcept { return v_permute_i128<0, 0>(x); }
BL_INLINE Vec256I v_duph_i128(const Vec256I& x) noexcept { return v_permute_i128<1, 1>(x); }

BL_INLINE Vec256I v_splat256_i8(const Vec128I& x) noexcept { return _mm256_broadcastb_epi8(x); }
BL_INLINE Vec256I v_splat256_i8(const Vec256I& x) noexcept { return _mm256_broadcastb_epi8(v_cast<Vec128I>(x)); }

BL_INLINE Vec256I v_splat256_i16(const Vec128I& x) noexcept { return _mm256_broadcastw_epi16(x); }
BL_INLINE Vec256I v_splat256_i16(const Vec256I& x) noexcept { return _mm256_broadcastw_epi16(v_cast<Vec128I>(x)); }

BL_INLINE Vec256I v_splat256_i32(const Vec128I& x) noexcept { return _mm256_broadcastd_epi32(x); }
BL_INLINE Vec256I v_splat256_i32(const Vec256I& x) noexcept { return _mm256_broadcastd_epi32(v_cast<Vec128I>(x)); }

BL_INLINE Vec256I v_splat256_i64(const Vec128I& x) noexcept { return _mm256_broadcastq_epi64(x); }
BL_INLINE Vec256I v_splat256_i64(const Vec256I& x) noexcept { return _mm256_broadcastq_epi64(v_cast<Vec128I>(x)); }

BL_INLINE Vec256I v_splat256_i128(const Vec128I& x) noexcept { return _mm256_broadcastsi128_si256(x); }
BL_INLINE Vec256I v_splat256_i128(const Vec256I& x) noexcept { return _mm256_broadcastsi128_si256(v_cast<Vec128I>(x)); }

BL_INLINE Vec256I v_interleave_lo_i8(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_unpacklo_epi8(x, y); }
BL_INLINE Vec256I v_interleave_lo_i16(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_unpacklo_epi16(x, y); }
BL_INLINE Vec256I v_interleave_lo_i32(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_unpacklo_epi32(x, y); }
BL_INLINE Vec256I v_interleave_lo_i64(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_unpacklo_epi64(x, y); }
BL_INLINE Vec256I v_interleave_lo_i128(const Vec128I& x, const Vec128I& y) noexcept { return v_permute_i128<2, 0>(v_cast<Vec256I>(x), v_cast<Vec256I>(y)); }
BL_INLINE Vec256I v_interleave_lo_i128(const Vec256I& x, const Vec256I& y) noexcept { return v_permute_i128<2, 0>(x, y); }

BL_INLINE Vec256I v_interleave_hi_i8(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_unpackhi_epi8(x, y); }
BL_INLINE Vec256I v_interleave_hi_i16(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_unpackhi_epi16(x, y); }
BL_INLINE Vec256I v_interleave_hi_i32(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_unpackhi_epi32(x, y); }
BL_INLINE Vec256I v_interleave_hi_i64(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_unpackhi_epi64(x, y); }
BL_INLINE Vec256I v_interleave_hi_i128(const Vec256I& x, const Vec256I& y) noexcept { return v_permute_i128<3, 1>(x, y); }

template<int N_BYTES>
BL_INLINE Vec256I v_alignr_i8(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_alignr_epi8(x, y, N_BYTES); }
#endif

#if defined(BL_TARGET_OPT_AVX)
BL_INLINE Vec256F v_swap_2xf32(const Vec256F& x) noexcept { return v_swizzle_2xf32<0, 1>(x); }
BL_INLINE Vec256D v_swap_f64(const Vec256D& x) noexcept { return v_swizzle_f64<0, 1>(x); }
BL_INLINE Vec256F v_swap_f128(const Vec256F& x) noexcept { return v_permute_f128<0, 1>(x); }
BL_INLINE Vec256D v_swap_d128(const Vec256D& x) noexcept { return v_permute_d128<0, 1>(x); }

BL_INLINE Vec256F v_dupl_f32(const Vec256F& x) noexcept { return v_swizzle_f32<2, 2, 0, 0>(x); }
BL_INLINE Vec256F v_duph_f32(const Vec256F& x) noexcept { return v_swizzle_f32<3, 3, 1, 1>(x); }

BL_INLINE Vec256F v_dupl_2xf32(const Vec256F& x) noexcept { return v_swizzle_2xf32<0, 0>(x); }
BL_INLINE Vec256F v_duph_2xf32(const Vec256F& x) noexcept { return v_swizzle_2xf32<1, 1>(x); }
BL_INLINE Vec256D v_dupl_f64(const Vec256D& x) noexcept { return v_swizzle_f64<0, 0>(x); }
BL_INLINE Vec256D v_duph_f64(const Vec256D& x) noexcept { return v_swizzle_f64<1, 1>(x); }

BL_INLINE Vec256F v_dupl_f128(const Vec128F& x) noexcept { return v_permute_f128<0, 0>(v_cast<Vec256F>(x)); }
BL_INLINE Vec256D v_dupl_d128(const Vec128D& x) noexcept { return v_permute_d128<0, 0>(v_cast<Vec256D>(x)); }
BL_INLINE Vec256F v_dupl_f128(const Vec256F& x) noexcept { return v_permute_f128<0, 0>(x); }
BL_INLINE Vec256D v_dupl_d128(const Vec256D& x) noexcept { return v_permute_d128<0, 0>(x); }
BL_INLINE Vec256F v_duph_f128(const Vec256F& x) noexcept { return v_permute_f128<1, 1>(x); }
BL_INLINE Vec256D v_duph_d128(const Vec256D& x) noexcept { return v_permute_d128<1, 1>(x); }

#if defined(BL_TARGET_OPT_AVX2)
BL_INLINE Vec256F v_splat256_f32(const Vec128F& x) noexcept { return _mm256_broadcastss_ps(v_cast<Vec128F>(x)); }
BL_INLINE Vec256F v_splat256_f32(const Vec256F& x) noexcept { return _mm256_broadcastss_ps(v_cast<Vec128F>(x)); }
BL_INLINE Vec256D v_splat256_f64(const Vec128D& x) noexcept { return _mm256_broadcastsd_pd(v_cast<Vec128D>(x)); }
BL_INLINE Vec256D v_splat256_f64(const Vec256D& x) noexcept { return _mm256_broadcastsd_pd(v_cast<Vec128D>(x)); }
#else
BL_INLINE Vec256F v_splat256_f32(const Vec128F& x) noexcept { return v_dupl_f128(v_swizzle_f32<0, 0, 0, 0>(v_cast<Vec128F>(x))); }
BL_INLINE Vec256F v_splat256_f32(const Vec256F& x) noexcept { return v_dupl_f128(v_swizzle_f32<0, 0, 0, 0>(v_cast<Vec128F>(x))); }
BL_INLINE Vec256D v_splat256_f64(const Vec128D& x) noexcept { return v_dupl_d128(v_swizzle_f64<0, 0>(v_cast<Vec128D>(x))); }
BL_INLINE Vec256D v_splat256_f64(const Vec256D& x) noexcept { return v_dupl_d128(v_swizzle_f64<0, 0>(v_cast<Vec128D>(x))); }
#endif

BL_INLINE Vec256F v_interleave_lo_32(const Vec256F& x, const Vec256F& y) noexcept { return _mm256_unpacklo_ps(x, y); }
BL_INLINE Vec256F v_interleave_hi_32(const Vec256F& x, const Vec256F& y) noexcept { return _mm256_unpackhi_ps(x, y); }

BL_INLINE Vec256D v_interleave_lo_64(const Vec256D& x, const Vec256D& y) noexcept { return _mm256_unpacklo_pd(x, y); }
BL_INLINE Vec256D v_interleave_hi_64(const Vec256D& x, const Vec256D& y) noexcept { return _mm256_unpackhi_pd(x, y); }

BL_INLINE Vec128F v_broadcast_f128_32(const void* p) noexcept { return v_cast<Vec128F>(_mm_broadcast_ss(static_cast<const float*>(p))); }
BL_INLINE Vec256F v_broadcast_f256_32(const void* p) noexcept { return v_cast<Vec256F>(_mm256_broadcast_ss(static_cast<const float*>(p))); }
BL_INLINE Vec256F v_broadcast_f256_64(const void* p) noexcept { return v_cast<Vec256F>(_mm256_broadcast_sd(static_cast<const double*>(p))); }
BL_INLINE Vec256F v_broadcast_f256_128(const void* p) noexcept { return v_cast<Vec256F>(_mm256_broadcast_ps(static_cast<const __m128*>(p))); }

BL_INLINE Vec256D v_broadcast_d256_64(const void* p) noexcept { return _mm256_broadcast_sd(static_cast<const double*>(p)); }
BL_INLINE Vec256D v_broadcast_d256_128(const void* p) noexcept { return _mm256_broadcast_pd(static_cast<const __m128d*>(p)); }
#endif

// SIMD - Vec256 - Bitwise Operations & Masking
// ============================================

#if defined(BL_TARGET_OPT_AVX2)
BL_INLINE Vec256I v_or(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_or_si256(x, y); }
BL_INLINE Vec256I v_xor(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_xor_si256(x, y); }
BL_INLINE Vec256I v_and(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_and_si256(x, y); }
BL_INLINE Vec256I v_nand(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_andnot_si256(x, y); }

BL_INLINE Vec256I v_or(const Vec256I& x, const Vec256I& y, const Vec256I& z) noexcept { return v_or(v_or(x, y), z); }
BL_INLINE Vec256I v_xor(const Vec256I& x, const Vec256I& y, const Vec256I& z) noexcept { return v_xor(v_xor(x, y), z); }
BL_INLINE Vec256I v_and(const Vec256I& x, const Vec256I& y, const Vec256I& z) noexcept { return v_and(v_and(x, y), z); }

BL_INLINE Vec256I v_blend_mask(const Vec256I& x, const Vec256I& y, const Vec256I& mask) noexcept { return v_or(v_nand(mask, x), v_and(y, mask)); }
BL_INLINE Vec256I v_blend_i8(const Vec256I& x, const Vec256I& y, const Vec256I& mask) noexcept { return _mm256_blendv_epi8(x, y, mask); }

template<uint8_t N_BITS> BL_INLINE Vec256I v_sll_i16(const Vec256I& x) noexcept { return N_BITS ? _mm256_slli_epi16(x, N_BITS) : x; }
template<uint8_t N_BITS> BL_INLINE Vec256I v_sll_i32(const Vec256I& x) noexcept { return N_BITS ? _mm256_slli_epi32(x, N_BITS) : x; }
template<uint8_t N_BITS> BL_INLINE Vec256I v_sll_i64(const Vec256I& x) noexcept { return N_BITS ? _mm256_slli_epi64(x, N_BITS) : x; }

template<uint8_t N_BITS> BL_INLINE Vec256I v_srl_i16(const Vec256I& x) noexcept { return N_BITS ? _mm256_srli_epi16(x, N_BITS) : x; }
template<uint8_t N_BITS> BL_INLINE Vec256I v_srl_i32(const Vec256I& x) noexcept { return N_BITS ? _mm256_srli_epi32(x, N_BITS) : x; }
template<uint8_t N_BITS> BL_INLINE Vec256I v_srl_i64(const Vec256I& x) noexcept { return N_BITS ? _mm256_srli_epi64(x, N_BITS) : x; }

template<uint8_t N_BITS> BL_INLINE Vec256I v_sra_i16(const Vec256I& x) noexcept { return N_BITS ? _mm256_srai_epi16(x, N_BITS) : x; }
template<uint8_t N_BITS> BL_INLINE Vec256I v_sra_i32(const Vec256I& x) noexcept { return N_BITS ? _mm256_srai_epi32(x, N_BITS) : x; }

template<uint8_t N_BYTES> BL_INLINE Vec256I v_sllb_i128(const Vec256I& x) noexcept { return N_BYTES ? _mm256_slli_si256(x, N_BYTES) : x; }
template<uint8_t N_BYTES> BL_INLINE Vec256I v_srlb_i128(const Vec256I& x) noexcept { return N_BYTES ? _mm256_srli_si256(x, N_BYTES) : x; }

BL_INLINE bool v_test_mask_i8(const Vec256I& x, uint32_t bits0_31) noexcept { return _mm256_movemask_epi8(x) == int(bits0_31); }
BL_INLINE bool v_test_mask_i32(const Vec256I& x, uint32_t bits0_7) noexcept { return _mm256_movemask_ps(v_cast<Vec256F>(x)) == int(bits0_7); }
BL_INLINE bool v_test_mask_i64(const Vec256I& x, uint32_t bits0_3) noexcept { return _mm256_movemask_pd(v_cast<Vec256D>(x)) == int(bits0_3); }
#endif

#if defined(BL_TARGET_OPT_AVX)
BL_INLINE Vec256F v_or(const Vec256F& x, const Vec256F& y) noexcept { return _mm256_or_ps(x, y); }
BL_INLINE Vec256D v_or(const Vec256D& x, const Vec256D& y) noexcept { return _mm256_or_pd(x, y); }

BL_INLINE Vec256F v_or(const Vec256F& x, const Vec256F& y, const Vec256F& z) noexcept { return v_or(v_or(x, y), z); }
BL_INLINE Vec256D v_or(const Vec256D& x, const Vec256D& y, const Vec256D& z) noexcept { return v_or(v_or(x, y), z); }

BL_INLINE Vec256F v_xor(const Vec256F& x, const Vec256F& y) noexcept { return _mm256_xor_ps(x, y); }
BL_INLINE Vec256D v_xor(const Vec256D& x, const Vec256D& y) noexcept { return _mm256_xor_pd(x, y); }

BL_INLINE Vec256F v_xor(const Vec256F& x, const Vec256F& y, const Vec256F& z) noexcept { return v_xor(v_xor(x, y), z); }
BL_INLINE Vec256D v_xor(const Vec256D& x, const Vec256D& y, const Vec256D& z) noexcept { return v_xor(v_xor(x, y), z); }

BL_INLINE Vec256F v_and(const Vec256F& x, const Vec256F& y) noexcept { return _mm256_and_ps(x, y); }
BL_INLINE Vec256D v_and(const Vec256D& x, const Vec256D& y) noexcept { return _mm256_and_pd(x, y); }

BL_INLINE Vec256F v_and(const Vec256F& x, const Vec256F& y, const Vec256F& z) noexcept { return v_and(v_and(x, y), z); }
BL_INLINE Vec256D v_and(const Vec256D& x, const Vec256D& y, const Vec256D& z) noexcept { return v_and(v_and(x, y), z); }

BL_INLINE Vec256F v_nand(const Vec256F& x, const Vec256F& y) noexcept { return _mm256_andnot_ps(x, y); }
BL_INLINE Vec256D v_nand(const Vec256D& x, const Vec256D& y) noexcept { return _mm256_andnot_pd(x, y); }

BL_INLINE Vec256F v_blend_mask(const Vec256F& x, const Vec256F& y, const Vec256F& mask) noexcept { return v_or(v_nand(mask, x), v_and(y, mask)); }
BL_INLINE Vec256D v_blend_mask(const Vec256D& x, const Vec256D& y, const Vec256D& mask) noexcept { return v_or(v_nand(mask, x), v_and(y, mask)); }

BL_INLINE bool v_test_zero(const Vec256I& x) noexcept { return _mm256_testz_si256(x, x); }

BL_INLINE bool v_test_mask_f32(const Vec256F& x, int bits0_7) noexcept { return _mm256_movemask_ps(x) == bits0_7; }
BL_INLINE bool v_test_mask_f64(const Vec256D& x, int bits0_3) noexcept { return _mm256_movemask_pd(x) == bits0_3; }
#endif

// SIMD - Vec256 - Integer Packing & Unpacking
// ===========================================

#if defined(BL_TARGET_OPT_AVX2)
BL_INLINE Vec256I v_packs_i16_i8(const Vec256I& x) noexcept { return _mm256_packs_epi16(x, x); }
BL_INLINE Vec256I v_packs_i16_u8(const Vec256I& x) noexcept { return _mm256_packus_epi16(x, x); }
BL_INLINE Vec256I v_packs_i32_i16(const Vec256I& x) noexcept { return _mm256_packs_epi32(x, x); }
BL_INLINE Vec256I v_packs_i32_u16(const Vec256I& x) noexcept { return _mm256_packus_epi32(x, x); }

BL_INLINE Vec256I v_packs_i16_i8(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_packs_epi16(x, y); }
BL_INLINE Vec256I v_packs_i16_u8(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_packus_epi16(x, y); }
BL_INLINE Vec256I v_packs_i32_i16(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_packs_epi32(x, y); }
BL_INLINE Vec256I v_packs_i32_u16(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_packus_epi32(x, y); }

BL_INLINE Vec256I v_packs_i32_i8(const Vec256I& x) noexcept { return v_packs_i16_i8(v_packs_i32_i16(x)); }
BL_INLINE Vec256I v_packs_i32_u8(const Vec256I& x) noexcept { return v_packs_i16_u8(v_packs_i32_i16(x)); }

BL_INLINE Vec256I v_packs_i32_i8(const Vec256I& x, const Vec256I& y) noexcept { return v_packs_i16_i8(v_packs_i32_i16(x, y)); }
BL_INLINE Vec256I v_packs_i32_u8(const Vec256I& x, const Vec256I& y) noexcept { return v_packs_i16_u8(v_packs_i32_i16(x, y)); }
BL_INLINE Vec256I v_packz_u32_u8(const Vec256I& x, const Vec256I& y) noexcept { return v_packs_i16_u8(v_packs_i32_i16(x, y)); }

BL_INLINE Vec256I v_packs_i32_i8(const Vec256I& x, const Vec256I& y, const Vec256I& z, const Vec256I& w) noexcept { return v_packs_i16_i8(v_packs_i32_i16(x, y), v_packs_i32_i16(z, w)); }
BL_INLINE Vec256I v_packs_i32_u8(const Vec256I& x, const Vec256I& y, const Vec256I& z, const Vec256I& w) noexcept { return v_packs_i16_u8(v_packs_i32_i16(x, y), v_packs_i32_i16(z, w)); }
BL_INLINE Vec256I v_packz_u32_u8(const Vec256I& x, const Vec256I& y, const Vec256I& z, const Vec256I& w) noexcept { return v_packs_i16_u8(v_packs_i32_i16(x, y), v_packs_i32_i16(z, w)); }

BL_INLINE Vec256I v_unpack256_u8_u16(const Vec128I& x) noexcept { return _mm256_cvtepu8_epi16(x); }
BL_INLINE Vec256I v_unpack256_u8_u32(const Vec128I& x) noexcept { return _mm256_cvtepu8_epi32(x); }
BL_INLINE Vec256I v_unpack256_u8_u64(const Vec128I& x) noexcept { return _mm256_cvtepu8_epi64(x); }
BL_INLINE Vec256I v_unpack256_u16_u32(const Vec128I& x) noexcept { return _mm256_cvtepu16_epi32(x); }
BL_INLINE Vec256I v_unpack256_u16_u64(const Vec128I& x) noexcept { return _mm256_cvtepu16_epi64(x); }
BL_INLINE Vec256I v_unpack256_u32_u64(const Vec128I& x) noexcept { return _mm256_cvtepu32_epi64(x); }
#endif

// SIMD - Vec256 - Integer Operations
// ==================================

#if defined(BL_TARGET_OPT_AVX2)
BL_INLINE Vec256I v_add_i8(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_add_epi8(x, y); }
BL_INLINE Vec256I v_add_i16(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_add_epi16(x, y); }
BL_INLINE Vec256I v_add_i32(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_add_epi32(x, y); }
BL_INLINE Vec256I v_add_i64(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_add_epi64(x, y); }
BL_INLINE Vec256I v_adds_i8(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_adds_epi8(x, y); }
BL_INLINE Vec256I v_adds_u8(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_adds_epu8(x, y); }
BL_INLINE Vec256I v_adds_i16(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_adds_epi16(x, y); }
BL_INLINE Vec256I v_adds_u16(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_adds_epu16(x, y); }
BL_INLINE Vec256I v_sub_i8(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_sub_epi8(x, y); }
BL_INLINE Vec256I v_sub_i16(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_sub_epi16(x, y); }
BL_INLINE Vec256I v_sub_i32(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_sub_epi32(x, y); }
BL_INLINE Vec256I v_sub_i64(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_sub_epi64(x, y); }
BL_INLINE Vec256I v_subs_i8(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_subs_epi8(x, y); }
BL_INLINE Vec256I v_subs_u8(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_subs_epu8(x, y); }
BL_INLINE Vec256I v_subs_i16(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_subs_epi16(x, y); }
BL_INLINE Vec256I v_subs_u16(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_subs_epu16(x, y); }

BL_INLINE Vec256I v_mul_i16(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_mullo_epi16(x, y); }
BL_INLINE Vec256I v_mul_u16(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_mullo_epi16(x, y); }
BL_INLINE Vec256I v_mulh_i16(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_mulhi_epi16(x, y); }
BL_INLINE Vec256I v_mulh_u16(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_mulhi_epu16(x, y); }
BL_INLINE Vec256I v_mul_i32(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_mullo_epi32(x, y); }
BL_INLINE Vec256I v_mul_u32(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_mullo_epi32(x, y); }

BL_INLINE Vec256I v_madd_i16_i32(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_madd_epi16(x, y); }

BL_INLINE Vec256I v_min_i8(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_min_epi8(x, y); }
BL_INLINE Vec256I v_min_u8(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_min_epu8(x, y); }
BL_INLINE Vec256I v_min_i16(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_min_epi16(x, y); }
BL_INLINE Vec256I v_min_u16(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_min_epu16(x, y); }
BL_INLINE Vec256I v_min_i32(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_min_epi32(x, y); }
BL_INLINE Vec256I v_min_u32(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_min_epu32(x, y); }

BL_INLINE Vec256I v_max_i8(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_max_epi8(x, y); }
BL_INLINE Vec256I v_max_u8(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_max_epu8(x, y); }
BL_INLINE Vec256I v_max_i16(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_max_epi16(x, y); }
BL_INLINE Vec256I v_max_u16(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_max_epu16(x, y); }
BL_INLINE Vec256I v_max_i32(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_max_epi32(x, y); }
BL_INLINE Vec256I v_max_u32(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_max_epu32(x, y); }

BL_INLINE Vec256I v_cmp_eq_i8(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_cmpeq_epi8(x, y); }
BL_INLINE Vec256I v_cmp_eq_i16(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_cmpeq_epi16(x, y); }
BL_INLINE Vec256I v_cmp_eq_i32(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_cmpeq_epi32(x, y); }
BL_INLINE Vec256I v_cmp_gt_i8(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_cmpgt_epi8(x, y); }
BL_INLINE Vec256I v_cmp_gt_i16(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_cmpgt_epi16(x, y); }
BL_INLINE Vec256I v_cmp_gt_i32(const Vec256I& x, const Vec256I& y) noexcept { return _mm256_cmpgt_epi32(x, y); }

BL_INLINE Vec256I v_div255_u16(const Vec256I& x) noexcept {
  Vec256I y = v_add_i16(x, v_const_as<Vec256I>(&blCommonTable.i_0080008000800080));
  return v_mulh_u16(y, v_const_as<Vec256I>(&blCommonTable.i_0101010101010101));
}
#endif

// SIMD - Vec256 - Floating Point Operations
// =========================================

#if defined(BL_TARGET_OPT_AVX)
BL_INLINE Vec256F s_add_f32(const Vec256F& x, const Vec256F& y) noexcept { return v_cast<Vec256F>(s_add_f32(v_cast<Vec128F>(x), v_cast<Vec128F>(y))); }
BL_INLINE Vec256D s_add_f64(const Vec256D& x, const Vec256D& y) noexcept { return v_cast<Vec256D>(s_add_f64(v_cast<Vec128D>(x), v_cast<Vec128D>(y))); }
BL_INLINE Vec256F s_sub_f32(const Vec256F& x, const Vec256F& y) noexcept { return v_cast<Vec256F>(s_sub_f32(v_cast<Vec128F>(x), v_cast<Vec128F>(y))); }
BL_INLINE Vec256D s_sub_f64(const Vec256D& x, const Vec256D& y) noexcept { return v_cast<Vec256D>(s_sub_f64(v_cast<Vec128D>(x), v_cast<Vec128D>(y))); }
BL_INLINE Vec256F s_mul_f32(const Vec256F& x, const Vec256F& y) noexcept { return v_cast<Vec256F>(s_mul_f32(v_cast<Vec128F>(x), v_cast<Vec128F>(y))); }
BL_INLINE Vec256D s_mul_f64(const Vec256D& x, const Vec256D& y) noexcept { return v_cast<Vec256D>(s_mul_f64(v_cast<Vec128D>(x), v_cast<Vec128D>(y))); }
BL_INLINE Vec256F s_div_f32(const Vec256F& x, const Vec256F& y) noexcept { return v_cast<Vec256F>(s_div_f32(v_cast<Vec128F>(x), v_cast<Vec128F>(y))); }
BL_INLINE Vec256D s_div_f64(const Vec256D& x, const Vec256D& y) noexcept { return v_cast<Vec256D>(s_div_f64(v_cast<Vec128D>(x), v_cast<Vec128D>(y))); }
BL_INLINE Vec256F s_min_f32(const Vec256F& x, const Vec256F& y) noexcept { return v_cast<Vec256F>(s_min_f32(v_cast<Vec128F>(x), v_cast<Vec128F>(y))); }
BL_INLINE Vec256D s_min_f64(const Vec256D& x, const Vec256D& y) noexcept { return v_cast<Vec256D>(s_min_f64(v_cast<Vec128D>(x), v_cast<Vec128D>(y))); }
BL_INLINE Vec256F s_max_f32(const Vec256F& x, const Vec256F& y) noexcept { return v_cast<Vec256F>(s_max_f32(v_cast<Vec128F>(x), v_cast<Vec128F>(y))); }
BL_INLINE Vec256D s_max_f64(const Vec256D& x, const Vec256D& y) noexcept { return v_cast<Vec256D>(s_max_f64(v_cast<Vec128D>(x), v_cast<Vec128D>(y))); }

BL_INLINE Vec256F s_sqrt_f32(const Vec256F& x) noexcept { return v_cast<Vec256F>(s_sqrt_f32(v_cast<Vec128F>(x))); }
BL_INLINE Vec256D s_sqrt_f64(const Vec256D& x) noexcept { return v_cast<Vec256D>(s_sqrt_f64(v_cast<Vec128D>(x))); }

BL_INLINE Vec256F s_cmp_eq_f32(const Vec256F& x, const Vec256F& y) noexcept { return v_cast<Vec256F>(s_cmp_eq_f32(v_cast<Vec128F>(x), v_cast<Vec128F>(y))); }
BL_INLINE Vec256D s_cmp_eq_f64(const Vec256D& x, const Vec256D& y) noexcept { return v_cast<Vec256D>(s_cmp_eq_f64(v_cast<Vec128D>(x), v_cast<Vec128D>(y))); }
BL_INLINE Vec256F s_cmp_ne_f32(const Vec256F& x, const Vec256F& y) noexcept { return v_cast<Vec256F>(s_cmp_ne_f32(v_cast<Vec128F>(x), v_cast<Vec128F>(y))); }
BL_INLINE Vec256D s_cmp_ne_f64(const Vec256D& x, const Vec256D& y) noexcept { return v_cast<Vec256D>(s_cmp_ne_f64(v_cast<Vec128D>(x), v_cast<Vec128D>(y))); }
BL_INLINE Vec256F s_cmp_ge_f32(const Vec256F& x, const Vec256F& y) noexcept { return v_cast<Vec256F>(s_cmp_ge_f32(v_cast<Vec128F>(x), v_cast<Vec128F>(y))); }
BL_INLINE Vec256D s_cmp_ge_f64(const Vec256D& x, const Vec256D& y) noexcept { return v_cast<Vec256D>(s_cmp_ge_f64(v_cast<Vec128D>(x), v_cast<Vec128D>(y))); }
BL_INLINE Vec256F s_cmp_gt_f32(const Vec256F& x, const Vec256F& y) noexcept { return v_cast<Vec256F>(s_cmp_gt_f32(v_cast<Vec128F>(x), v_cast<Vec128F>(y))); }
BL_INLINE Vec256D s_cmp_gt_f64(const Vec256D& x, const Vec256D& y) noexcept { return v_cast<Vec256D>(s_cmp_gt_f64(v_cast<Vec128D>(x), v_cast<Vec128D>(y))); }
BL_INLINE Vec256F s_cmp_le_f32(const Vec256F& x, const Vec256F& y) noexcept { return v_cast<Vec256F>(s_cmp_le_f32(v_cast<Vec128F>(x), v_cast<Vec128F>(y))); }
BL_INLINE Vec256D s_cmp_le_f64(const Vec256D& x, const Vec256D& y) noexcept { return v_cast<Vec256D>(s_cmp_le_f64(v_cast<Vec128D>(x), v_cast<Vec128D>(y))); }
BL_INLINE Vec256F s_cmp_lt_f32(const Vec256F& x, const Vec256F& y) noexcept { return v_cast<Vec256F>(s_cmp_lt_f32(v_cast<Vec128F>(x), v_cast<Vec128F>(y))); }
BL_INLINE Vec256D s_cmp_lt_f64(const Vec256D& x, const Vec256D& y) noexcept { return v_cast<Vec256D>(s_cmp_lt_f64(v_cast<Vec128D>(x), v_cast<Vec128D>(y))); }

BL_INLINE Vec256F v_add_f32(const Vec256F& x, const Vec256F& y) noexcept { return _mm256_add_ps(x, y); }
BL_INLINE Vec256D v_add_f64(const Vec256D& x, const Vec256D& y) noexcept { return _mm256_add_pd(x, y); }
BL_INLINE Vec256F v_sub_f32(const Vec256F& x, const Vec256F& y) noexcept { return _mm256_sub_ps(x, y); }
BL_INLINE Vec256D v_sub_f64(const Vec256D& x, const Vec256D& y) noexcept { return _mm256_sub_pd(x, y); }
BL_INLINE Vec256F v_mul_f32(const Vec256F& x, const Vec256F& y) noexcept { return _mm256_mul_ps(x, y); }
BL_INLINE Vec256D v_mul_f64(const Vec256D& x, const Vec256D& y) noexcept { return _mm256_mul_pd(x, y); }
BL_INLINE Vec256F v_div_f32(const Vec256F& x, const Vec256F& y) noexcept { return _mm256_div_ps(x, y); }
BL_INLINE Vec256D v_div_f64(const Vec256D& x, const Vec256D& y) noexcept { return _mm256_div_pd(x, y); }
BL_INLINE Vec256F v_min_f32(const Vec256F& x, const Vec256F& y) noexcept { return _mm256_min_ps(x, y); }
BL_INLINE Vec256D v_min_f64(const Vec256D& x, const Vec256D& y) noexcept { return _mm256_min_pd(x, y); }
BL_INLINE Vec256F v_max_f32(const Vec256F& x, const Vec256F& y) noexcept { return _mm256_max_ps(x, y); }
BL_INLINE Vec256D v_max_f64(const Vec256D& x, const Vec256D& y) noexcept { return _mm256_max_pd(x, y); }

BL_INLINE Vec256F v_sqrt_f32(const Vec256F& x) noexcept { return _mm256_sqrt_ps(x); }
BL_INLINE Vec256D v_sqrt_f64(const Vec256D& x) noexcept { return _mm256_sqrt_pd(x); }

BL_INLINE Vec256F v_cmp_eq_f32(const Vec256F& x, const Vec256F& y) noexcept { return _mm256_cmp_ps(x, y, _CMP_EQ_OQ); }
BL_INLINE Vec256D v_cmp_eq_f64(const Vec256D& x, const Vec256D& y) noexcept { return _mm256_cmp_pd(x, y, _CMP_EQ_OQ); }
BL_INLINE Vec256F v_cmp_ne_f32(const Vec256F& x, const Vec256F& y) noexcept { return _mm256_cmp_ps(x, y, _CMP_NEQ_OQ); }
BL_INLINE Vec256D v_cmp_ne_f64(const Vec256D& x, const Vec256D& y) noexcept { return _mm256_cmp_pd(x, y, _CMP_NEQ_OQ); }
BL_INLINE Vec256F v_cmp_ge_f32(const Vec256F& x, const Vec256F& y) noexcept { return _mm256_cmp_ps(x, y, _CMP_GE_OQ); }
BL_INLINE Vec256D v_cmp_ge_f64(const Vec256D& x, const Vec256D& y) noexcept { return _mm256_cmp_pd(x, y, _CMP_GE_OQ); }
BL_INLINE Vec256F v_cmp_gt_f32(const Vec256F& x, const Vec256F& y) noexcept { return _mm256_cmp_ps(x, y, _CMP_GT_OQ); }
BL_INLINE Vec256D v_cmp_gt_f64(const Vec256D& x, const Vec256D& y) noexcept { return _mm256_cmp_pd(x, y, _CMP_GT_OQ); }
BL_INLINE Vec256F v_cmp_le_f32(const Vec256F& x, const Vec256F& y) noexcept { return _mm256_cmp_ps(x, y, _CMP_LE_OQ); }
BL_INLINE Vec256D v_cmp_le_f64(const Vec256D& x, const Vec256D& y) noexcept { return _mm256_cmp_pd(x, y, _CMP_LE_OQ); }
BL_INLINE Vec256F v_cmp_lt_f32(const Vec256F& x, const Vec256F& y) noexcept { return _mm256_cmp_ps(x, y, _CMP_LT_OQ); }
BL_INLINE Vec256D v_cmp_lt_f64(const Vec256D& x, const Vec256D& y) noexcept { return _mm256_cmp_pd(x, y, _CMP_LE_OQ); }
#endif

#endif

} // {anonymous}
} // {SIMD}

//! \}
//! \endcond

#endif // BLEND2D_SIMD_X86_P_H_INCLUDED
