// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_SIMD_NEON_P_H_INCLUDED
#define BLEND2D_SIMD_NEON_P_H_INCLUDED

#include "tables_p.h"

#include <arm_neon.h>

#if defined(__clang__)
  #define BL_HAVE_BUILTIN_SHUFFLEVECTOR 1
#elif defined(__GNUC__) && (__GNUC__ >= 12)
  #define BL_HAVE_BUILTIN_SHUFFLEVECTOR 1
#else
  #define BL_HAVE_BUILTIN_SHUFFLEVECTOR 0
#endif

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace SIMD {

#define BL_TARGET_SIMD_I 128
#define BL_TARGET_SIMD_F 128

#if BL_TARGET_ARCH_BITS == 64
  #define BL_TARGET_SIMD_D 128
#else
  #define BL_TARGET_SIMD_D 0
#endif

struct alignas(16) R128 {
  union {
    int8x16_t   i8;
    uint8x16_t  u8;
    int16x8_t   i16;
    uint16x8_t  u16;
    int32x4_t   i32;
    uint32x4_t  u32;
    int64x2_t   i64;
    uint64x2_t  u64;
    float32x4_t f32;
#if BL_TARGET_SIMD_D
    float64x2_t f64;
#endif
  };

  BL_INLINE R128() noexcept = default;
  BL_INLINE R128(const R128& in) noexcept = default;

  BL_INLINE R128(const int8x16_t& in) noexcept : i8(in) {}
  BL_INLINE R128(const uint8x16_t& in) noexcept : u8(in) {}
  BL_INLINE R128(const int16x8_t& in) noexcept : i16(in) {}
  BL_INLINE R128(const uint16x8_t& in) noexcept : u16(in) {}
  BL_INLINE R128(const int32x4_t& in) noexcept : i32(in) {}
  BL_INLINE R128(const uint32x4_t& in) noexcept : u32(in) {}
  BL_INLINE R128(const int64x2_t& in) noexcept : i64(in) {}
  BL_INLINE R128(const uint64x2_t& in) noexcept : u64(in) {}
  BL_INLINE R128(const float32x4_t& in) noexcept : f32(in) {}

  BL_INLINE R128(const int8x8x2_t& in) noexcept : i8(vcombine_s8(in.val[0], in.val[1])) {}
  BL_INLINE R128(const uint8x8x2_t& in) noexcept : u8(vcombine_u8(in.val[0], in.val[1])) {}
  BL_INLINE R128(const int16x4x2_t& in) noexcept : i16(vcombine_s16(in.val[0], in.val[1])) {}
  BL_INLINE R128(const uint16x4x2_t& in) noexcept : u16(vcombine_u16(in.val[0], in.val[1])) {}
  BL_INLINE R128(const int32x2x2_t& in) noexcept : i32(vcombine_s32(in.val[0], in.val[1])) {}
  BL_INLINE R128(const uint32x2x2_t& in) noexcept : u32(vcombine_u32(in.val[0], in.val[1])) {}
  BL_INLINE R128(const int64x1x2_t& in) noexcept : i64(vcombine_s64(in.val[0], in.val[1])) {}
  BL_INLINE R128(const uint64x1x2_t& in) noexcept : u64(vcombine_u64(in.val[0], in.val[1])) {}
  BL_INLINE R128(const float32x2x2_t& in) noexcept : f32(vcombine_f32(in.val[0], in.val[1])) {}

#if BL_TARGET_SIMD_D
  BL_INLINE R128(const float64x2_t& in) noexcept : f64(in) {}
  BL_INLINE R128(const float64x1x2_t& in) noexcept : f64(vcombine_f64(in.val[0], in.val[1])) {}
#endif
};

typedef R128 Vec128I;
typedef R128 Vec128F;
typedef R128 Vec128D;

// Must be in anonymous namespace.
namespace {

BL_INLINE void prefetch0(const void* p) noexcept { blUnused(p); }
BL_INLINE void prefetch1(const void* p) noexcept { blUnused(p); }
BL_INLINE void prefetch2(const void* p) noexcept { blUnused(p); }

template<typename Out, typename In>
BL_INLINE const Out& v_const_as(const In* c) noexcept {
  return *reinterpret_cast<const Out*>(c);
}

template<typename DST, typename SRC_IN> BL_INLINE DST v_cast(const SRC_IN& x) noexcept {
  return reinterpret_cast<const DST&>(x);
}

BL_INLINE Vec128I v_zero_i128() noexcept { return Vec128I(vdupq_n_s8(0)); }

BL_INLINE Vec128I v_add_i8(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vaddq_s8(x.i8, y.i8)); }
BL_INLINE Vec128I v_add_i16(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vaddq_s16(x.i16, y.i16)); }
BL_INLINE Vec128I v_add_i32(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vaddq_s32(x.i32, y.i32)); }
BL_INLINE Vec128I v_add_i64(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vaddq_s64(x.i64, y.i64)); }

BL_INLINE Vec128I v_adds_i8(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vqaddq_s8(x.i8, y.i8)); }
BL_INLINE Vec128I v_adds_i16(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vqaddq_s16(x.i16, y.i16)); }
BL_INLINE Vec128I v_adds_i32(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vqaddq_s32(x.i32, y.i32)); }
BL_INLINE Vec128I v_adds_i64(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vqaddq_s64(x.i64, y.i64)); }

BL_INLINE Vec128I v_adds_u8(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vqaddq_u8(x.u8, y.u8)); }
BL_INLINE Vec128I v_adds_u16(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vqaddq_u16(x.u16, y.u16)); }
BL_INLINE Vec128I v_adds_u32(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vqaddq_u32(x.u32, y.u32)); }
BL_INLINE Vec128I v_adds_u64(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vqaddq_u64(x.u64, y.u64)); }

BL_INLINE Vec128I v_sub_i8(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vsubq_s8(x.i8, y.i8)); }
BL_INLINE Vec128I v_sub_i16(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vsubq_s16(x.i16, y.i16)); }
BL_INLINE Vec128I v_sub_i32(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vsubq_s32(x.i32, y.i32)); }
BL_INLINE Vec128I v_sub_i64(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vsubq_s64(x.i64, y.i64)); }

BL_INLINE Vec128I v_subs_i8(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vqsubq_s8(x.i8, y.i8)); }
BL_INLINE Vec128I v_subs_i16(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vqsubq_s16(x.i16, y.i16)); }
BL_INLINE Vec128I v_subs_i32(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vqsubq_s32(x.i32, y.i32)); }
BL_INLINE Vec128I v_subs_i64(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vqsubq_s64(x.i64, y.i64)); }

BL_INLINE Vec128I v_subs_u8(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vqsubq_u8(x.u8, y.u8)); }
BL_INLINE Vec128I v_subs_u16(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vqsubq_u16(x.u16, y.u16)); }
BL_INLINE Vec128I v_subs_u32(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vqsubq_u32(x.u32, y.u32)); }
BL_INLINE Vec128I v_subs_u64(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vqsubq_u64(x.u64, y.u64)); }

BL_INLINE Vec128I v_mul_i8(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vmulq_s8(x.i8, y.i8)); }
BL_INLINE Vec128I v_mul_i16(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vmulq_s16(x.i16, y.i16)); }
BL_INLINE Vec128I v_mul_i32(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vmulq_s32(x.i32, y.i32)); }

BL_INLINE Vec128I v_mul_u8(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vmulq_u8(x.u8, y.u8)); }
BL_INLINE Vec128I v_mul_u16(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vmulq_u16(x.u16, y.u16)); }
BL_INLINE Vec128I v_mul_u32(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vmulq_u32(x.u32, y.u32)); }

BL_INLINE Vec128I v_min_i8(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vminq_s8(x.i8, y.i8)); }
BL_INLINE Vec128I v_min_i16(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vminq_s16(x.i16, y.i16)); }
BL_INLINE Vec128I v_min_i32(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vminq_s32(x.i32, y.i32)); }

BL_INLINE Vec128I v_min_u8(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vminq_u8(x.u8, y.u8)); }
BL_INLINE Vec128I v_min_u16(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vminq_u16(x.u16, y.u16)); }
BL_INLINE Vec128I v_min_u32(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vminq_u32(x.u32, y.u32)); }

BL_INLINE Vec128I v_max_i8(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vmaxq_s8(x.i8, y.i8)); }
BL_INLINE Vec128I v_max_i16(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vmaxq_s16(x.i16, y.i16)); }
BL_INLINE Vec128I v_max_i32(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vmaxq_s32(x.i32, y.i32)); }

BL_INLINE Vec128I v_max_u8(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vmaxq_u8(x.u8, y.u8)); }
BL_INLINE Vec128I v_max_u16(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vmaxq_u16(x.u16, y.u16)); }
BL_INLINE Vec128I v_max_u32(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vmaxq_u32(x.u32, y.u32)); }

BL_INLINE Vec128I v_cmp_eq_i8(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vceqq_s8(x.i8, y.i8)); }
BL_INLINE Vec128I v_cmp_eq_i16(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vceqq_s16(x.i16, y.i16)); }
BL_INLINE Vec128I v_cmp_eq_i32(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vceqq_s32(x.i32, y.i32)); }

BL_INLINE Vec128I v_cmp_gt_i8(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vcgtq_s8(x.i8, y.i8)); }
BL_INLINE Vec128I v_cmp_gt_i16(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vcgtq_s16(x.i16, y.i16)); }
BL_INLINE Vec128I v_cmp_gt_i32(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vcgtq_s32(x.i32, y.i32)); }

BL_INLINE Vec128I v_cmp_gt_u8(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vcgtq_u8(x.u8, y.u8)); }
BL_INLINE Vec128I v_cmp_gt_u16(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vcgtq_u16(x.u16, y.u16)); }
BL_INLINE Vec128I v_cmp_gt_u32(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vcgtq_u32(x.u32, y.u32)); }

template<uint8_t N> BL_INLINE Vec128I v_sll_i8(const Vec128I& x) noexcept { return Vec128I(vshlq_n_u8(x.u8, N)); }
template<uint8_t N> BL_INLINE Vec128I v_sll_i16(const Vec128I& x) noexcept { return Vec128I(vshlq_n_u16(x.u16, N)); }
template<uint8_t N> BL_INLINE Vec128I v_sll_i32(const Vec128I& x) noexcept { return Vec128I(vshlq_n_u32(x.u32, N)); }
template<uint8_t N> BL_INLINE Vec128I v_sll_i64(const Vec128I& x) noexcept { return Vec128I(vshlq_n_u64(x.u64, N)); }

template<uint8_t N> BL_INLINE Vec128I v_srl_i8(const Vec128I& x) noexcept { return Vec128I(vshrq_n_u8(x.u8, N)); }
template<uint8_t N> BL_INLINE Vec128I v_srl_i16(const Vec128I& x) noexcept { return Vec128I(vshrq_n_u16(x.u16, N)); }
template<uint8_t N> BL_INLINE Vec128I v_srl_i32(const Vec128I& x) noexcept { return Vec128I(vshrq_n_u32(x.u32, N)); }
template<uint8_t N> BL_INLINE Vec128I v_srl_i64(const Vec128I& x) noexcept { return Vec128I(vshrq_n_u64(x.u64, N)); }

template<uint8_t N> BL_INLINE Vec128I v_sra_i8(const Vec128I& x) noexcept { return Vec128I(vshrq_n_s8(x.i8, N)); }
template<uint8_t N> BL_INLINE Vec128I v_sra_i16(const Vec128I& x) noexcept { return Vec128I(vshrq_n_s16(x.i16, N)); }
template<uint8_t N> BL_INLINE Vec128I v_sra_i32(const Vec128I& x) noexcept { return Vec128I(vshrq_n_s32(x.i32, N)); }
template<uint8_t N> BL_INLINE Vec128I v_sra_i64(const Vec128I& x) noexcept { return Vec128I(vshrq_n_s64(x.i64, N)); }

template<uint8_t N> BL_INLINE Vec128I v_sllb_i128(const Vec128I& x) noexcept { return Vec128I(vextq_s8(v_zero_i128().i8, x.i8, 16 - N)); }
template<uint8_t N> BL_INLINE Vec128I v_srlb_i128(const Vec128I& x) noexcept { return Vec128I(vextq_s8(x.i8, v_zero_i128().i8,      N)); }

BL_INLINE Vec128I v_or(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vorrq_u64(x.u64, y.u64)); }
BL_INLINE Vec128I v_xor(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(veorq_u64(x.u64, y.u64)); }
BL_INLINE Vec128I v_and(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vandq_u64(x.u64, y.u64)); }
BL_INLINE Vec128I v_nand(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vbicq_u64(y.u64, x.u64)); }

BL_INLINE Vec128I v_i128_from_i32(int32_t x) noexcept { return Vec128I(vsetq_lane_s32(x, vdupq_n_s32(0), 0)); }
BL_INLINE Vec128I v_i128_from_u32(uint32_t x) noexcept { return Vec128I(vsetq_lane_u32(x, vdupq_n_u32(0), 0)); }

BL_INLINE int32_t v_get_i32(const Vec128I& x) noexcept { return vgetq_lane_s32(x.i32, 0); }
BL_INLINE uint32_t v_get_u32(const Vec128I& x) noexcept { return vgetq_lane_u32(x.u32, 0); }

template<uint8_t I>
BL_INLINE int32_t v_get_lane_i32(const Vec128I& x) noexcept { return vgetq_lane_s32(x.i32, I); }
template<uint8_t I>
BL_INLINE uint32_t v_get_lane_u32(const Vec128I& x) noexcept { return vgetq_lane_u32(x.u32, I); }

BL_INLINE Vec128I v_interleave_lo_i8(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vzip_s8(vget_low_s8(x.i8), vget_low_s8(y.i8))); }
BL_INLINE Vec128I v_interleave_lo_i16(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vzip_s16(vget_low_s16(x.i16), vget_low_s16(y.i16))); }
BL_INLINE Vec128I v_interleave_lo_i32(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vzip_s32(vget_low_s32(x.i32), vget_low_s32(y.i32))); }
BL_INLINE Vec128I v_interleave_lo_i64(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vcombine_s64(vget_low_s64(x.i64), vget_low_s64(y.i64))); }

BL_INLINE Vec128I v_interleave_hi_i8(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vzip_s8(vget_high_s8(x.i8), vget_high_s8(y.i8))); }
BL_INLINE Vec128I v_interleave_hi_i16(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vzip_s16(vget_high_s16(x.i16), vget_high_s16(y.i16))); }
BL_INLINE Vec128I v_interleave_hi_i32(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vzip_s32(vget_high_s32(x.i32), vget_high_s32(y.i32))); }
BL_INLINE Vec128I v_interleave_hi_i64(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vcombine_s64(vget_high_s64(x.i64), vget_high_s64(y.i64))); }

BL_INLINE Vec128I v_packs_i16_i8(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vcombine_s8(vqmovn_s16(x.i16), vqmovn_s16(y.i16))); }
BL_INLINE Vec128I v_packs_i16_u8(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vcombine_u8(vqmovun_s16(x.i16), vqmovun_s16(y.i16))); }
BL_INLINE Vec128I v_packs_u16_u8(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vcombine_u8(vqmovn_u16(x.u16), vqmovn_u16(y.u16))); }

BL_INLINE Vec128I v_packs_i32_i16(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vcombine_s16(vqmovn_s32(x.i32), vqmovn_s32(y.i32))); }
BL_INLINE Vec128I v_packs_i32_u16(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vcombine_u16(vqmovun_s32(x.i32), vqmovun_s32(y.i32))); }
BL_INLINE Vec128I v_packs_u32_u16(const Vec128I& x, const Vec128I& y) noexcept { return Vec128I(vcombine_u16(vqmovn_u32(x.u32), vqmovn_u32(y.u32))); }

BL_INLINE Vec128I v_packs_i16_i8(const Vec128I& x) noexcept { return Vec128I(vcombine_s8(vqmovn_s16(x.i16), vqmovn_s16(x.i16))); }
BL_INLINE Vec128I v_packs_i16_u8(const Vec128I& x) noexcept { return Vec128I(vcombine_u8(vqmovun_s16(x.i16), vqmovun_s16(x.i16))); }
BL_INLINE Vec128I v_packs_u16_u8(const Vec128I& x) noexcept { return Vec128I(vcombine_u8(vqmovn_u16(x.u16), vqmovn_u16(x.u16))); }

BL_INLINE Vec128I v_packs_i32_i16(const Vec128I& x) noexcept { return Vec128I(vcombine_s16(vqmovn_s32(x.i32), vqmovn_s32(x.i32))); }
BL_INLINE Vec128I v_packs_i32_u16(const Vec128I& x) noexcept { return Vec128I(vcombine_u16(vqmovun_s32(x.i32), vqmovun_s32(x.i32))); }
BL_INLINE Vec128I v_packs_u32_u16(const Vec128I& x) noexcept { return Vec128I(vcombine_u16(vqmovn_u32(x.u32), vqmovn_u32(x.u32))); }

BL_INLINE Vec128I v_packz_u16_u8(const Vec128I& x, const Vec128I& y) noexcept { return v_packs_u16_u8(x, y); }
BL_INLINE Vec128I v_packz_u32_u16(const Vec128I& x, const Vec128I& y) noexcept { return v_packs_u32_u16(x, y); }
BL_INLINE Vec128I v_packz_u16_u8(const Vec128I& x) noexcept { return v_packs_u16_u8(x); }
BL_INLINE Vec128I v_packz_u32_u16(const Vec128I& x) noexcept { return v_packs_u32_u16(x); }

BL_INLINE Vec128I v_unpack_lo_u8_u16(const Vec128I& x) noexcept { return Vec128I(vmovl_u8(vget_low_u8(x.u8))); }
BL_INLINE Vec128I v_unpack_lo_u16_u32(const Vec128I& x) noexcept { return Vec128I(vmovl_u16(vget_low_u16(x.u16))); }
BL_INLINE Vec128I v_unpack_lo_u32_u64(const Vec128I& x) noexcept { return Vec128I(vmovl_u32(vget_low_u32(x.u32))); }

BL_INLINE Vec128I v_unpack_lo_i8_i16(const Vec128I& x) noexcept { return Vec128I(vmovl_s8(vget_low_s8(x.i8))); }
BL_INLINE Vec128I v_unpack_lo_i16_i32(const Vec128I& x) noexcept { return Vec128I(vmovl_s16(vget_low_s16(x.i16))); }
BL_INLINE Vec128I v_unpack_lo_i32_i64(const Vec128I& x) noexcept { return Vec128I(vmovl_s32(vget_low_s32(x.i32))); }

BL_INLINE Vec128I v_unpack_hi_u8_u16(const Vec128I& x) noexcept { return Vec128I(vmovl_u8(vget_high_u8(x.u8))); }
BL_INLINE Vec128I v_unpack_hi_u16_u32(const Vec128I& x) noexcept { return Vec128I(vmovl_u16(vget_high_u16(x.u16))); }
BL_INLINE Vec128I v_unpack_hi_u32_u64(const Vec128I& x) noexcept { return Vec128I(vmovl_u32(vget_high_u32(x.u32))); }

BL_INLINE Vec128I v_unpack_hi_i8_i16(const Vec128I& x) noexcept { return Vec128I(vmovl_s8(vget_high_s8(x.i8))); }
BL_INLINE Vec128I v_unpack_hi_i16_i32(const Vec128I& x) noexcept { return Vec128I(vmovl_s16(vget_high_s16(x.i16))); }
BL_INLINE Vec128I v_unpack_hi_i32_i64(const Vec128I& x) noexcept { return Vec128I(vmovl_s32(vget_high_s32(x.i32))); }

BL_INLINE Vec128I v_fill_i128_i8(int8_t x) noexcept { return Vec128I(vdupq_n_s8(x)); }
BL_INLINE Vec128I v_fill_i128_i16(int16_t x) noexcept { return Vec128I(vdupq_n_s16(x)); }
BL_INLINE Vec128I v_fill_i128_i32(int32_t x) noexcept { return Vec128I(vdupq_n_s32(x)); }
BL_INLINE Vec128I v_fill_i128_i64(int64_t x) noexcept { return Vec128I(vdupq_n_s64(x)); }

BL_INLINE Vec128I v_fill_i128_u8(uint8_t x) noexcept { return Vec128I(vdupq_n_u8(x)); }
BL_INLINE Vec128I v_fill_i128_u16(uint16_t x) noexcept { return Vec128I(vdupq_n_u16(x)); }
BL_INLINE Vec128I v_fill_i128_u32(uint32_t x) noexcept { return Vec128I(vdupq_n_u32(x)); }
BL_INLINE Vec128I v_fill_i128_u64(uint64_t x) noexcept { return Vec128I(vdupq_n_u64(x)); }

BL_INLINE Vec128I v_swap_i64(const Vec128I& x) noexcept { return Vec128I(vcombine_u64(vget_high_u64(x.u64), vget_low_u64(x.u64))); }
BL_INLINE Vec128I v_dupl_i64(const Vec128I& x) noexcept { return Vec128I(vcombine_u64(vget_low_u64(x.u64), vget_low_u64(x.u64))); }
BL_INLINE Vec128I v_duph_i64(const Vec128I& x) noexcept { return Vec128I(vcombine_u64(vget_high_u64(x.u64), vget_high_u64(x.u64))); }

BL_INLINE Vec128I v_load_i32(const void* p) noexcept { return Vec128I(vld1q_lane_u32(static_cast<const uint32_t*>(p), vdupq_n_u32(0), 0)); }
BL_INLINE Vec128I v_load_i64(const void* p) noexcept { return Vec128I(vld1q_lane_u64(static_cast<const uint64_t*>(p), vdupq_n_u64(0), 0)); }
BL_INLINE Vec128I v_loada_i128(const void* p) noexcept { return Vec128I(vld1q_u64((const uint64_t*)__builtin_assume_aligned(p, 16))); }
BL_INLINE Vec128I v_loadu_i128(const void* p) noexcept { return Vec128I(vld1q_u8((const uint8_t*)p)); }

BL_INLINE Vec128I v_loadh_i64(const Vec128I& x, const void* p) noexcept { return Vec128I(vld1q_lane_u64(static_cast<const uint64_t*>(p), x.u64, 1)); }

BL_INLINE void v_store_i32(void* p, const Vec128I& x) noexcept { vst1q_lane_u32((uint32_t*)p, x.u32, 0); }
BL_INLINE void v_store_i64(void* p, const Vec128I& x) noexcept { vst1q_lane_u64((uint64_t*)p, x.u64, 0); }
BL_INLINE void v_storea_i128(void* p, const Vec128I& x) noexcept { vst1q_u64((uint64_t*)__builtin_assume_aligned(p, 16), x.u64); }
BL_INLINE void v_storeu_i128(void* p, const Vec128I& x) noexcept { vst1q_u8((uint8_t*)p, x.u8); }

BL_INLINE void v_storel_i64(void* p, const Vec128I& x) noexcept { vst1q_lane_u64((uint64_t*)p, x.u64, 0); }
BL_INLINE void v_storeh_i64(void* p, const Vec128I& x) noexcept { vst1q_lane_u64((uint64_t*)p, x.u64, 1); }

BL_INLINE Vec128I v_blend_mask(const Vec128I& x, const Vec128I& y, const Vec128I& mask) noexcept { return v_or(v_nand(mask, x), v_and(y, mask)); }

BL_INLINE Vec128I v_abs_i8(const Vec128I& x) noexcept { return Vec128I(vabsq_s8(x.i8)); }
BL_INLINE Vec128I v_abs_i16(const Vec128I& x) noexcept { return Vec128I(vabsq_s16(x.i16)); }
BL_INLINE Vec128I v_abs_i32(const Vec128I& x) noexcept { return Vec128I(vabsq_s32(x.i32)); }

static constexpr uint8_t mm_shuffle_predicate_u8(uint32_t a, uint32_t b, uint32_t c, uint32_t d) noexcept {
  return (uint8_t)((a << 6) | (b << 4) | (c << 2) | (d));
}

template<uint8_t I>
static Vec128I v_dup_lane_i8(const Vec128I& x) noexcept {
#if BL_TARGET_ARCH_BITS == 64
  return Vec128I(vdupq_laneq_u8(x.u8, I));
#else
  return Vec128I(vdupq_n_u8(vgetq_lane_u8(x.u8, I)));
#endif
}

template<uint8_t I>
static Vec128I v_dup_lane_i16(const Vec128I& x) noexcept {
#if BL_TARGET_ARCH_BITS == 64
  return Vec128I(vdupq_laneq_u16(x.u16, I));
#else
  return Vec128I(vdupq_n_u16(vgetq_lane_u16(x.u16, I)));
#endif
}

template<uint8_t I>
static Vec128I v_dup_lane_i32(const Vec128I& x) noexcept {
#if BL_TARGET_ARCH_BITS == 64
  return Vec128I(vdupq_laneq_u32(x.u32, I));
#else
  return Vec128I(vdupq_n_u32(vgetq_lane_u32(x.u32, I)));
#endif
}

template<uint8_t D, uint8_t C, uint8_t B, uint8_t A>
BL_INLINE Vec128I v_swizzle_lo_i16(const Vec128I& x) noexcept {
#if BL_HAVE_BUILTIN_SHUFFLEVECTOR
  return __builtin_shufflevector(x.u16, x.u16, A, B, C, D, 4, 5, 6, 7);
#else
  return Vec128I(uint16x8_t{x.u16[A], x.u16[B], x.u16[C], x.u16[D], x.u16[4], x.u16[5], x.u16[6], x.u16[7]});
#endif
}

template<uint8_t D, uint8_t C, uint8_t B, uint8_t A>
BL_INLINE Vec128I v_swizzle_hi_i16(const Vec128I& x) noexcept {
#if BL_HAVE_BUILTIN_SHUFFLEVECTOR
  return __builtin_shufflevector(x.u16, x.u16, 0, 1, 2, 3, 4 + A, 4 + B, 4 + C, 4 + D);
#else
  return Vec128I(uint16x8_t{x.u16[0], x.u16[1], x.u16[2], x.u16[3], x.u16[4 + A], x.u16[4 + B], x.u16[4 + C], x.u16[4 + D]});
#endif
}

template<uint8_t D, uint8_t C, uint8_t B, uint8_t A>
BL_INLINE Vec128I v_swizzle_i16(const Vec128I& x) noexcept {
#if BL_HAVE_BUILTIN_SHUFFLEVECTOR
  return __builtin_shufflevector(x.u16, x.u16, A, B, C, D, 4 + A, 4 + B, 4 + C, 4 + D);
#else
  return Vec128I(uint16x8_t{x.u16[A], x.u16[B], x.u16[C], x.u16[D], x.u16[4 + A], x.u16[4 + B], x.u16[4 + C], x.u16[4 + D]});
#endif
}

template<uint8_t D, uint8_t C, uint8_t B, uint8_t A>
BL_INLINE Vec128I v_swizzle_i32(const Vec128I& x) noexcept {
#if BL_HAVE_BUILTIN_SHUFFLEVECTOR
  return Vec128I(__builtin_shufflevector(x.u32, x.u32, A, B, C, D));
#else
  switch (mm_shuffle_predicate_u8(D, C, B, A)) {
    case mm_shuffle_predicate_u8(0, 0, 0, 0): {
      return v_dup_lane_i32<0>(x);
    }

    case mm_shuffle_predicate_u8(0, 1, 0, 1): {
      uint32x2_t t = vrev64_u32(vget_low_u32(x.u32));
      return Vec128I(vcombine_u32(t, t));
    }

    case mm_shuffle_predicate_u8(1, 0, 1, 0): {
      uint32x2_t t = vget_low_u32(x.u32);
      return Vec128I(vcombine_u32(t, t));
    }

    case mm_shuffle_predicate_u8(1, 0, 3, 2): {
      return v_swap_i64(x);
    }

    case mm_shuffle_predicate_u8(1, 1, 1, 1): {
      return v_dup_lane_i32<1>(x);
    }

    case mm_shuffle_predicate_u8(2, 2, 2, 2): {
      return v_dup_lane_i32<2>(x);
    }

    case mm_shuffle_predicate_u8(2, 3, 2, 3): {
      uint32x2_t t = vrev64_u32(vget_high_u32(x.u32));
      return Vec128I(vcombine_u32(t, t));
    }

    case mm_shuffle_predicate_u8(3, 2, 1, 0): {
      return x;
    }

    case mm_shuffle_predicate_u8(3, 2, 3, 2): {
      uint32x2_t t = vget_high_u32(x.u32);
      return Vec128I(vcombine_u32(t, t));
    }

    case mm_shuffle_predicate_u8(3, 3, 3, 3): {
      return v_dup_lane_i32<3>(x);
    }

    default: {
      return Vec128I(uint32x4_t{x.u32[A], x.u32[B], x.u32[C], x.u32[D]});
    }
  }
#endif
}

BL_INLINE Vec128I v_div255_u16(const Vec128I& x) noexcept {
  Vec128I y = v_add_i16(x, v_const_as<Vec128I>(&blCommonTable.i_0080008000800080));
  return v_srl_i16<8>(Vec128I(vsraq_n_u16(y.u16, y.u16, 8)));
}

} // {anonymous}
} // {SIMD}

//! \}
//! \endcond

#endif // BLEND2D_SIMD_NEON_P_H_INCLUDED
