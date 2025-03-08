// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_COMPRESSION_CHECKSUMADLER32SIMDIMPL_P_H_INCLUDED
#define BLEND2D_COMPRESSION_CHECKSUMADLER32SIMDIMPL_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../compression/checksum_p.h"
#include "../simd/simd_p.h"

//! \cond INTERNAL

namespace bl {
namespace Compression {
namespace Checksum {
namespace {

BL_INLINE void adler32ReduceAddU32(uint32_t& s1, uint32_t& s2, SIMD::Vec4xU32& v1, SIMD::Vec4xU32& v2) noexcept {
  using namespace SIMD;

  v1 += swizzle_u32<2, 3, 0, 1>(v1);
  v2 += swizzle_u32<2, 3, 0, 1>(v2);
  v1 += swizzle_u32<1, 0, 3, 2>(v1);
  v2 += swizzle_u32<1, 0, 3, 2>(v2);

  s1 += cast_to_u32(v1);
  s2 += cast_to_u32(v2);
}

BL_INLINE SIMD::Vec4xU32 adler32WideSumToU32(const SIMD::Vec8xU16& values, const SIMD::Vec8xU16& mulPred) noexcept {
  using namespace SIMD;

#if BL_TARGET_ARCH_X86
  return vec_cast<Vec4xU32>(maddw_i16_i32(values, mulPred));
#else
  Vec4xU32 a = mul_lo_u16_u32(values, mulPred);
  Vec4xU32 b = mul_hi_u16_u32(values, mulPred);

  return a + b;
#endif
}

BL_INLINE SIMD::Vec4xU32 adler32ByteSum(const SIMD::Vec16xU8& v0, const SIMD::Vec16xU8& v1) noexcept {
  using namespace SIMD;

#if BL_TARGET_ARCH_X86
  Vec4xU32 a = vec_cast<Vec4xU32>(sad_u8_u64(v0, make_zero<Vec16xU8>()));
  Vec4xU32 b = vec_cast<Vec4xU32>(sad_u8_u64(v1, make_zero<Vec16xU8>()));

  return a + b;
#else
  Vec8xU16 a16 = addl_lo_u8_to_u16(v0, v1);
  Vec8xU16 b16 = addl_hi_u8_to_u16(v0, v1);
  Vec4xU32 a32 = addl_lo_u16_to_u32(a16, b16);
  Vec4xU32 b32 = addl_hi_u16_to_u32(a16, b16);

  return a32 + b32;
#endif
}

BL_INLINE uint32_t adler32Update_SIMD(uint32_t checksum, const uint8_t* data, size_t size) noexcept {
  using namespace SIMD;

  constexpr uint32_t kBlockSize = 32;
  constexpr uint32_t kBlockMaxCount = 4096u / kBlockSize;

  uint32_t s1 = checksum & 0xFFFFu;
  uint32_t s2 = checksum >> 16;

  {
    size_t n = blMin<size_t>(IntOps::alignUpDiff(uintptr_t(data), 16u), size);
    if (n) {
      size -= n;

      BL_NOUNROLL
      do {
        s1 += *data++;
        s2 += s1;
      } while (--n);

      s1 %= kAdler32Divisor;
      s2 %= kAdler32Divisor;
    }
  }

  if (size >= kBlockSize) {
    // SIMD code using the same approach as libdeflate. The main loop is multiplication free, but needs to widen
    // 8-bit items to 16-bit items so they could be summed - the sums of 16-bit elements cannot exceed INT16_MAX
    // (signed) so we can use `maddw_i16_i32()` later to combine the sums into 32-bit accumulator.
    size_t remainingBlocks = size / kBlockSize;
    size &= kBlockSize - 1;

    Vec8xU16 mulPred0 = make128_u16(25, 26, 27, 28, 29, 30, 31, 32);
    Vec8xU16 mulPred1 = make128_u16(17, 18, 19, 20, 21, 22, 23, 24);
    Vec8xU16 mulPred2 = make128_u16( 9, 10, 11, 12, 13, 14, 15, 16);
    Vec8xU16 mulPred3 = make128_u16( 1,  2,  3,  4,  5,  6,  7,  8);

    do {
      size_t n = blMin<size_t>(remainingBlocks, kBlockMaxCount);

      BL_ASSERT(n > 0u);
      remainingBlocks -= n;

      Vec4xU32 vec_s1 = make_zero<Vec4xU32>();
      Vec4xU32 vec_s2 = make_zero<Vec4xU32>();

      Vec8xU16 wideSum0 = make_zero<Vec8xU16>();
      Vec8xU16 wideSum1 = make_zero<Vec8xU16>();
      Vec8xU16 wideSum2 = make_zero<Vec8xU16>();
      Vec8xU16 wideSum3 = make_zero<Vec8xU16>();

      s2 += s1 * (uint32_t(n) * kBlockSize);

      do {
        Vec16xU8 v0 = loada_128<Vec16xU8>(data +  0u);
        Vec16xU8 v1 = loada_128<Vec16xU8>(data + 16u);
        Vec4xU32 byteSum = adler32ByteSum(v0, v1);

        vec_s2 += vec_s1;
        data += kBlockSize;

        wideSum0 += vec_cast<Vec8xU16>(unpack_lo64_u8_u16(v0));
        wideSum1 += vec_cast<Vec8xU16>(unpack_hi64_u8_u16(v0));

        wideSum2 += vec_cast<Vec8xU16>(unpack_lo64_u8_u16(v1));
        wideSum3 += vec_cast<Vec8xU16>(unpack_hi64_u8_u16(v1));
        vec_s1 += byteSum;
      } while (--n);

      Vec4xU32 t0 = adler32WideSumToU32(wideSum0, mulPred0);
      Vec4xU32 t1 = adler32WideSumToU32(wideSum1, mulPred1);
      Vec4xU32 t2 = adler32WideSumToU32(wideSum2, mulPred2);
      Vec4xU32 t3 = adler32WideSumToU32(wideSum3, mulPred3);

      vec_s2 = slli_u32<4 + 1>(vec_s2);

      t0 += t1;
      t2 += t3;

      vec_s2 += t0;
      vec_s2 += t2;

      adler32ReduceAddU32(s1, s2, vec_s1, vec_s2);
      s1 %= kAdler32Divisor;
      s2 %= kAdler32Divisor;
    } while (remainingBlocks);
  }

  if (size) {
    do {
      s1 += *data++;
      s2 += s1;
    } while (--size);

    s1 %= kAdler32Divisor;
    s2 %= kAdler32Divisor;
  }

  return s1 | (s2 << 16);
}

} // {anonymous}
} // {Checksum}
} // {Compression}
} // {bl}

//! \endcond

#endif // BLEND2D_COMPRESSION_CHECKSUMADLER32SIMDIMPL_P_H_INCLUDED
