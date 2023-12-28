// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#if defined(BL_TARGET_OPT_SSE2)

#include "../codec/pngops_p.h"
#include "../simd/simd_p.h"
#include "../support/intops_p.h"
#include "../support/memops_p.h"

namespace bl {
namespace Png {

// bl::Png::Opts - InverseFilter - SSE2
// ====================================

BLResult BL_CDECL inverseFilterImpl_SSE2(uint8_t* p, uint32_t bpp, uint32_t bpl, uint32_t h) noexcept {
  using namespace SIMD;

  BL_ASSERT(bpp > 0);
  BL_ASSERT(bpl > 1);
  BL_ASSERT(h   > 0);

  uint32_t y = h;
  uint8_t* u = nullptr;

  // Subtract one BYTE that is used to store the filter type.
  bpl--;

  // First row uses a special filter that doesn't access the previous row,
  // which is assumed to contain all zeros.
  uint32_t filterType = *p++;
  if (BL_UNLIKELY(filterType >= BL_PNG_FILTER_TYPE_COUNT))
    return blTraceError(BL_ERROR_INVALID_DATA);
  filterType = simplifyFilterOfFirstRow(filterType);

  #define BL_PNG_PAETH(DST, A, B, C)                                           \
    do {                                                                       \
      Vec16xU8 MinAB = min_i16(A, B);                                          \
      Vec16xU8 MaxAB = max_i16(A, B);                                          \
      Vec16xU8 DivAB = mulh_u16(sub_i16(MaxAB, MinAB), rcp3);                  \
                                                                               \
      MinAB = sub_i16(MinAB, C);                                               \
      MaxAB = sub_i16(MaxAB, C);                                               \
                                                                               \
      DST = add_i16(C  , andnot(srai_i16<15>(add_i16(DivAB, MinAB)), MaxAB));  \
      DST = add_i16(DST, andnot(srai_i16<15>(sub_i16(DivAB, MaxAB)), MinAB));  \
    } while (0)

  #define BL_PNG_SLL_ADDB_1X(P0, T0, SHIFT)                                    \
    do {                                                                       \
      T0 = sllb_u128<SHIFT>(P0);                                               \
      P0 = add_i8(P0, T0);                                                     \
    } while (0)

  #define BL_PNG_SLL_ADDB_2X(P0, T0, P1, T1, SHIFT)                            \
    do {                                                                       \
      T0 = sllb_u128<SHIFT>(P0);                                               \
      T1 = sllb_u128<SHIFT>(P1);                                               \
      P0 = add_i8(P0, T0);                                                     \
      P1 = add_i8(P1, T1);                                                     \
    } while (0)

  for (;;) {
    uint32_t i;

    switch (filterType) {
      case BL_PNG_FILTER_TYPE_NONE:
        p += bpl;
        break;

      // This is one of the easiest filters to parallelize. Although it looks like the data dependency
      // is too high, it's simply additions, which are really easy to parallelize. The following formula:
      //
      //     Y1' = BYTE(Y1 + Y0')
      //     Y2' = BYTE(Y2 + Y1')
      //     Y3' = BYTE(Y3 + Y2')
      //     Y4' = BYTE(Y4 + Y3')
      //
      // Expanded to (with byte casts removed, as they are implicit in our case):
      //
      //     Y1' = Y1 + Y0'
      //     Y2' = Y2 + Y1 + Y0'
      //     Y3' = Y3 + Y2 + Y1 + Y0'
      //     Y4' = Y4 + Y3 + Y2 + Y1 + Y0'
      //
      // Can be implemented like this by taking advantage of SIMD:
      //
      //     +-----------+-----------+-----------+-----------+----->
      //     |    Y1     |    Y2     |    Y3     |    Y4     | ...
      //     +-----------+-----------+-----------+-----------+----->
      //                   Shift by 1 and PADDB
      //     +-----------+-----------+-----------+-----------+
      //     |           |    Y1     |    Y2     |    Y3     | ----+
      //     +-----------+-----------+-----------+-----------+     |
      //                                                           |
      //     +-----------+-----------+-----------+-----------+     |
      //     |    Y1     |   Y1+Y2   |   Y2+Y3   |   Y3+Y4   | <---+
      //     +-----------+-----------+-----------+-----------+
      //                   Shift by 2 and PADDB
      //     +-----------+-----------+-----------+-----------+
      //     |           |           |    Y1     |   Y1+Y2   | ----+
      //     +-----------+-----------+-----------+-----------+     |
      //                                                           |
      //     +-----------+-----------+-----------+-----------+     |
      //     |    Y1     |   Y1+Y2   | Y1+Y2+Y3  |Y1+Y2+Y3+Y4| <---+
      //     +-----------+-----------+-----------+-----------+
      //
      // The size of the register doesn't matter here. The Y0' dependency has been omitted to make the
      // flow cleaner, however, it can be added to Y1 before processing or it can be shifted to the
      // first cell so the first addition would be performed against [Y0', Y1, Y2, Y3].
      case BL_PNG_FILTER_TYPE_SUB: {
        i = bpl - bpp;

        if (i >= 32) {
          // Align to 16-BYTE boundary.
          uint32_t j = uint32_t(IntOps::alignUpDiff(uintptr_t(p + bpp), 16));
          for (i -= j; j != 0; j--, p++)
            p[bpp] = applySumFilter(p[bpp], p[0]);

          if (bpp == 1) {
            Vec16xU8 p0, p1, p2, p3;
            Vec16xU8 t0, t2;

            // Process 64 BYTEs at a time.
            p0 = cast_from_u32<Vec16xU8>(p[0]);
            while (i >= 64) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 1));
              p1 = loada<Vec16xU8>(p + 17);
              p2 = loada<Vec16xU8>(p + 33);
              p3 = loada<Vec16xU8>(p + 49);

              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t2, 1);
              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t2, 2);
              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t2, 4);
              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t2, 8);
              storea<Vec16xU8>(p + 1, p0);

              p0 = srlb_u128<15>(p0);
              t2 = srlb_u128<15>(p2);
              p1 = add_i8(p1, p0);
              p3 = add_i8(p3, t2);

              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t2, 1);
              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t2, 2);
              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t2, 4);
              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t2, 8);
              storea<Vec16xU8>(p + 17, p1);

              p1 = interleave_hi_u8(p1, p1);
              p1 = interleave_hi_u16(p1, p1);
              p1 = swizzle_u32<3, 3, 3, 3>(p1);

              p2 = add_i8(p2, p1);
              p3 = add_i8(p3, p1);

              storea<Vec16xU8>(p + 33, p2);
              storea<Vec16xU8>(p + 49, p3);
              p0 = srlb_u128<15>(p3);

              p += 64;
              i -= 64;
            }

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 1));

              BL_PNG_SLL_ADDB_1X(p0, t0, 1);
              BL_PNG_SLL_ADDB_1X(p0, t0, 2);
              BL_PNG_SLL_ADDB_1X(p0, t0, 4);
              BL_PNG_SLL_ADDB_1X(p0, t0, 8);

              storea(p + 1, p0);
              p0 = srlb_u128<15>(p0);

              p += 16;
              i -= 16;
            }
          }
          else if (bpp == 2) {
            Vec16xU8 p0, p1, p2, p3;
            Vec16xU8 t0, t2;

            // Process 64 BYTEs at a time.
            p0 = cast_from_u32<Vec16xU8>(MemOps::readU16a(p));
            while (i >= 64) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 2));
              p1 = loada<Vec16xU8>(p + 18);
              p2 = loada<Vec16xU8>(p + 34);
              p3 = loada<Vec16xU8>(p + 50);

              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t2, 2);
              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t2, 4);
              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t2, 8);
              storea(p + 2, p0);

              p0 = srlb_u128<14>(p0);
              t2 = srlb_u128<14>(p2);
              p1 = add_i8(p1, p0);
              p3 = add_i8(p3, t2);

              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t2, 2);
              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t2, 4);
              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t2, 8);
              storea(p + 18, p1);

              p1 = interleave_hi_u16(p1, p1);
              p1 = swizzle_u32<3, 3, 3, 3>(p1);

              p2 = add_i8(p2, p1);
              p3 = add_i8(p3, p1);

              storea(p + 34, p2);
              storea(p + 50, p3);
              p0 = srlb_u128<14>(p3);

              p += 64;
              i -= 64;
            }

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 2));
              BL_PNG_SLL_ADDB_1X(p0, t0, 2);
              BL_PNG_SLL_ADDB_1X(p0, t0, 4);
              BL_PNG_SLL_ADDB_1X(p0, t0, 8);

              storea(p + 2, p0);
              p0 = srlb_u128<14>(p0);

              p += 16;
              i -= 16;
            }
          }
          else if (bpp == 3) {
            Vec16xU8 p0, p1, p2, p3;
            Vec16xU8 t0, t2;
            Vec16xU8 ext3b = make128_u32<Vec16xU8>(0x01000001u);

            // Process 64 BYTEs at a time.
            p0 = cast_from_u32<Vec16xU8>(MemOps::readU32u(p) & 0x00FFFFFFu);
            while (i >= 64) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 3));
              p1 = loada<Vec16xU8>(p + 19);
              p2 = loada<Vec16xU8>(p + 35);

              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t2, 3);
              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t2, 6);
              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t2, 12);

              p3 = loada<Vec16xU8>(p + 51);
              t0 = srlb_u128<13>(p0);
              t2 = srlb_u128<13>(p2);

              p1 = add_i8(p1, t0);
              p3 = add_i8(p3, t2);

              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t2, 3);
              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t2, 6);
              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t2, 12);
              storea(p + 3, p0);

              p0 = swizzle_u32<3, 3, 3, 3>(p1);
              p0 = srli_u32<8>(p0);
              p0 = mulw_u32(p0, ext3b);

              p0 = swizzle_lo_u16<0, 2, 1, 0>(p0);
              p0 = swizzle_hi_u16<1, 0, 2, 1>(p0);

              storea(p + 19, p1);
              p2 = add_i8(p2, p0);
              p0 = swizzle_u32<1, 3, 2, 1>(p0);

              storea(p + 35, p2);
              p0 = add_i8(p0, p3);

              storea(p + 51, p0);
              p0 = srlb_u128<13>(p0);

              p += 64;
              i -= 64;
            }

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 3));

              BL_PNG_SLL_ADDB_1X(p0, t0, 3);
              BL_PNG_SLL_ADDB_1X(p0, t0, 6);
              BL_PNG_SLL_ADDB_1X(p0, t0, 12);

              storea(p + 3, p0);
              p0 = srlb_u128<13>(p0);

              p += 16;
              i -= 16;
            }
          }
          else if (bpp == 4) {
            Vec16xU8 p0, p1, p2, p3;
            Vec16xU8 t0, t1, t2;

            // Process 64 BYTEs at a time.
            p0 = cast_from_u32<Vec16xU8>(MemOps::readU32a(p));
            while (i >= 64) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 4));
              p1 = loada<Vec16xU8>(p + 20);
              p2 = loada<Vec16xU8>(p + 36);
              p3 = loada<Vec16xU8>(p + 52);

              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t1, 4);
              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t1, 8);
              storea(p + 4, p0);

              p0 = srlb_u128<12>(p0);
              t2 = srlb_u128<12>(p2);

              p1 = add_i8(p1, p0);
              p3 = add_i8(p3, t2);

              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t1, 4);
              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t1, 8);

              p0 = swizzle_u32<3, 3, 3, 3>(p1);
              storea(p + 20, p1);

              p2 = add_i8(p2, p0);
              p0 = add_i8(p0, p3);

              storea(p + 36, p2);
              storea(p + 52, p0);
              p0 = srlb_u128<12>(p0);

              p += 64;
              i -= 64;
            }

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 4));

              BL_PNG_SLL_ADDB_1X(p0, t0, 4);
              BL_PNG_SLL_ADDB_1X(p0, t0, 8);
              storea(p + 4, p0);
              p0 = srlb_u128<12>(p0);

              p += 16;
              i -= 16;
            }
          }
          else if (bpp == 6) {
            Vec16xU8 p0, p1, p2, p3;
            Vec16xU8 t0, t1;

            p0 = loadu_64<Vec16xU8>(p);
            p0 = slli_i64<16>(p0);
            p0 = srli_u64<16>(p0);

            // Process 64 BYTEs at a time.
            while (i >= 64) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 6));
              p1 = loada<Vec16xU8>(p + 22);
              p2 = loada<Vec16xU8>(p + 38);

              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t1, 6);
              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t1, 12);

              p3 = loada<Vec16xU8>(p + 54);
              storea(p + 6, p0);

              p0 = srlb_u128<10>(p0);
              t1 = srlb_u128<10>(p2);

              p1 = add_i8(p1, p0);
              p3 = add_i8(p3, t1);

              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t1, 6);
              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t1, 12);
              p0 = dup_hi_u64(p1);

              p0 = swizzle_lo_u16<1, 3, 2, 1>(p0);
              p0 = swizzle_hi_u16<2, 1, 3, 2>(p0);

              storea(p + 22, p1);
              p2 = add_i8(p2, p0);
              p0 = swizzle_u32<1, 3, 2, 1>(p0);

              storea(p + 38, p2);
              p0 = add_i8(p0, p3);

              storea(p + 54, p0);
              p0 = srlb_u128<10>(p0);

              p += 64;
              i -= 64;
            }

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 6));

              BL_PNG_SLL_ADDB_1X(p0, t0, 6);
              BL_PNG_SLL_ADDB_1X(p0, t0, 12);

              storea(p + 6, p0);
              p0 = srlb_u128<10>(p0);

              p += 16;
              i -= 16;
            }
          }
          else if (bpp == 8) {
            Vec16xU8 p0, p1, p2, p3;
            Vec16xU8 t0, t1, t2;

            // Process 64 BYTEs at a time.
            p0 = loadu_64<Vec16xU8>(p);
            while (i >= 64) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 8));
              p1 = loada<Vec16xU8>(p + 24);
              p2 = loada<Vec16xU8>(p + 40);
              p3 = loada<Vec16xU8>(p + 56);

              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t1, 8);
              storea(p + 8, p0);

              p0 = srlb_u128<8>(p0);
              t2 = dup_hi_u64(p2);
              p1 = add_i8(p1, p0);

              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t1, 8);
              p0 = dup_hi_u64(p1);
              p3 = add_i8(p3, t2);
              storea(p + 24, p1);

              p2 = add_i8(p2, p0);
              p0 = add_i8(p0, p3);

              storea(p + 40, p2);
              storea(p + 56, p0);
              p0 = srlb_u128<8>(p0);

              p += 64;
              i -= 64;
            }

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 8));
              BL_PNG_SLL_ADDB_1X(p0, t0, 8);

              storea(p + 8, p0);
              p0 = srlb_u128<8>(p0);

              p += 16;
              i -= 16;
            }
          }
        }

        for (; i != 0; i--, p++)
          p[bpp] = applySumFilter(p[bpp], p[0]);

        p += bpp;
        break;
      }

      // This is actually the easiest filter that doesn't require any kind of specialization for a particular BPP.
      // Even C++ compiler like GCC/Clang is able to parallelize a naive implementation. However, MSVC compiler
      // was not able to parallelize the naive implementation so the SSE2 implementation was provided to make this
      // code perform well while compiled by any compiler.
      //
      //     +-----------+-----------+-----------+-----------+
      //     |    Y1     |    Y2     |    Y3     |    Y4     |
      //     +-----------+-----------+-----------+-----------+
      //                         [V]PADDB
      //     +-----------+-----------+-----------+-----------+
      //     |    U1     |    U2     |    U3     |    U4     | ----+
      //     +-----------+-----------+-----------+-----------+     |
      //                                                           |
      //     +-----------+-----------+-----------+-----------+     |
      //     |   Y1+U1   |   Y2+U2   |   Y3+U3   |   Y4+U4   | <---+
      //     +-----------+-----------+-----------+-----------+
      case BL_PNG_FILTER_TYPE_UP: {
        BL_ASSERT(u != nullptr);
        i = bpl;

        if (i >= 24) {
          // Align to 16-BYTE boundary.
          uint32_t j = uint32_t(IntOps::alignUpDiff(uintptr_t(p), 16));
          for (i -= j; j != 0; j--, p++, u++)
            p[0] = applySumFilter(p[0], u[0]);

          // Process 64 BYTEs at a time.
          while (i >= 64) {
            Vec16xU8 u0 = loadu<Vec16xU8>(u +  0);
            Vec16xU8 u1 = loadu<Vec16xU8>(u + 16);
            Vec16xU8 p0 = add_i8(u0, loada<Vec16xU8>(p +  0));
            Vec16xU8 p1 = add_i8(u1, loada<Vec16xU8>(p + 16));

            Vec16xU8 u2 = loadu<Vec16xU8>(u + 32);
            Vec16xU8 u3 = loadu<Vec16xU8>(u + 48);
            Vec16xU8 p2 = add_i8(u2, loada<Vec16xU8>(p + 32));
            Vec16xU8 p3 = add_i8(u3, loada<Vec16xU8>(p + 48));

            storea(p     , p0);
            storea(p + 16, p1);
            storea(p + 32, p2);
            storea(p + 48, p3);

            p += 64;
            u += 64;
            i -= 64;
          }

          // Process 8 BYTEs at a time.
          while (i >= 8) {
            Vec16xU8 u0 = loadu_64<Vec16xU8>(u);
            Vec16xU8 p0 = loada_64<Vec16xU8>(p);

            storea_64(p, add_i8(p0, u0));

            p += 8;
            u += 8;
            i -= 8;
          }
        }

        for (; i != 0; i--, p++, u++)
          p[0] = applySumFilter(p[0], u[0]);
        break;
      }

      // This filter is extremely difficult for low BPP values as there is a huge sequential data dependency,
      // I didn't succeeded to solve it. 1-3 BPP implementations are pretty bad and I would like to hear about
      // a way to improve those. The implementation for 4 BPP and more is pretty good, as there is less data
      // dependency between individual bytes.
      //
      // Sequential Approach:
      //
      //     Y1' = byte((2*Y1 + U1 + Y0') >> 1)
      //     Y2' = byte((2*Y2 + U2 + Y1') >> 1)
      //     Y3' = byte((2*Y3 + U3 + Y2') >> 1)
      //     Y4' = byte((2*Y4 + U4 + Y3') >> 1)
      //     Y5' = ...
      //
      // Expanded, `U1 + Y0'` replaced with `U1`:
      //
      //     Y1' = byte((2*Y1 + U1) >> 1)
      //     Y2' = byte((2*Y2 + U2 + byte((2*Y1 + U1) >> 1)) >> 1)
      //     Y3' = byte((2*Y3 + U3 + byte((2*Y2 + U2 + byte((2*Y1 + U1) >> 1)) >> 1)) >> 1)
      //     Y4' = byte((2*Y4 + U4 + byte((2*Y3 + U3 + byte((2*Y2 + U2 + byte((2*Y1 + U1) >> 1)) >> 1)) >> 1)) >> 1)
      //     Y5' = ...
      case BL_PNG_FILTER_TYPE_AVG: {
        BL_ASSERT(u != nullptr);

        for (i = 0; i < bpp; i++)
          p[i] = applySumFilter(p[i], u[i] >> 1);

        i = bpl - bpp;
        u += bpp;

        if (i >= 32) {
          // Align to 16-BYTE boundary.
          uint32_t j = uint32_t(IntOps::alignUpDiff(uintptr_t(p + bpp), 16));
          Vec16xU8 zero = make_zero<Vec16xU8>();

          for (i -= j; j != 0; j--, p++, u++)
            p[bpp] = applySumFilter(p[bpp], applyAvgFilter(p[0], u[0]));

          if (bpp == 1) {
            // This is one of the most difficult AVG filters. 1-BPP has a huge sequential dependency, which is
            // nearly impossible to parallelize. The code below is the best I could have written, it's a mixture
            // of C++ and SIMD. Maybe using a pure C would be even better than this code, but, I tried to take
            // advantage of 8 BYTE fetches at least. Unrolling the loop any further doesn't lead to an improvement.
            //
            // I know that the code looks terrible, but it's a bit faster than a pure specialized C++ implementation
            // I used to have before.
            uint32_t t0 = p[0];
            uint32_t t1;

            // Process 8 BYTEs at a time.
            while (i >= 8) {
              Vec16xU8 p0 = loada_64<Vec16xU8>(p + 1);
              Vec16xU8 u0 = loadu_64<Vec16xU8>(u);

              p0 = interleave_lo_u8(p0, zero);
              u0 = interleave_lo_u8(u0, zero);

              p0 = slli_i16<1>(p0);
              p0 = add_i16(p0, u0);

              t1 = cast_to_u32(p0);
              p0 = srlb_u128<4>(p0);
              t0 = ((t0 + t1) >> 1) & 0xFF; t1 >>= 16;
              p[1] = uint8_t(t0);

              t0 = ((t0 + t1) >> 1) & 0xFF;
              t1 = cast_to_u32(p0);
              p0 = srlb_u128<4>(p0);
              p[2] = uint8_t(t0);

              t0 = ((t0 + t1) >> 1) & 0xFF; t1 >>= 16;
              p[3] = uint8_t(t0);

              t0 = ((t0 + t1) >> 1) & 0xFF;
              t1 = cast_to_u32(p0);
              p0 = srlb_u128<4>(p0);
              p[4] = uint8_t(t0);

              t0 = ((t0 + t1) >> 1) & 0xFF; t1 >>= 16;
              p[5] = uint8_t(t0);

              t0 = ((t0 + t1) >> 1) & 0xFF;
              t1 = cast_to_u32(p0);
              p[6] = uint8_t(t0);

              t0 = ((t0 + t1) >> 1) & 0xFF; t1 >>= 16;
              p[7] = uint8_t(t0);

              t0 = ((t0 + t1) >> 1) & 0xFF;
              p[8] = uint8_t(t0);

              p += 8;
              u += 8;
              i -= 8;
            }
          }
          // TODO: [PNG] Not complete / Not working.
          /*
          else if (bpp == 2) {
          }
          else if (bpp == 3) {
          }
          */
          else if (bpp == 4) {
            Vec16xU8 m00FF = make128_u32<Vec16xU8>(0x00FF00FFu);
            Vec16xU8 m01FF = make128_u32<Vec16xU8>(0x01FF01FFu);
            Vec16xU8 t1 = interleave_lo_u8(loada_32<Vec16xU8>(p), zero);

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              Vec16xU8 p0, p1;
              Vec16xU8 u0, u1;

              p0 = loada<Vec16xU8>(p + 4);
              u0 = loadu<Vec16xU8>(u);

              p1 = p0;                             // HI | Move Ln
              p0 = interleave_lo_u8(p0, zero);     // LO | Unpack Ln

              u1 = u0;                             // HI | Move Up
              p0 = slli_i16<1>(p0);                // LO | << 1

              u0 = interleave_lo_u8(u0, zero);     // LO | Unpack Up
              p0 = add_i16(p0, t1);                // LO | Add Last

              p1 = interleave_hi_u8(p1, zero);     // HI | Unpack Ln
              p0 = add_i16(p0, u0);                // LO | Add Up
              p0 = p0 & m01FF;                     // LO | & 0x01FE

              u1 = interleave_hi_u8(u1, zero);     // HI | Unpack Up
              t1 = sllb_u128<8>(p0);               // LO | Get Last
              p0 = slli_i16<1>(p0);                // LO | << 1

              p1 = slli_i16<1>(p1);                // HI | << 1
              p0 = add_i16(p0, t1);                // LO | Add Last
              p0 = srli_u16<2>(p0);                // LO | >> 2

              p1 = add_i16(p1, u1);                // HI | Add Up
              p0 = p0 & m00FF;                     // LO | & 0x00FF
              t1 = srlb_u128<8>(p0);               // LO | Get Last

              p1 = add_i16(p1, t1);                // HI | Add Last
              p1 = p1 & m01FF;                     // HI | & 0x01FE

              t1 = sllb_u128<8>(p1);               // HI | Get Last
              p1 = slli_i16<1>(p1);                // HI | << 1

              t1 = add_i16(t1, p1);                // HI | Add Last
              t1 = srli_u16<2>(t1);                // HI | >> 2
              t1 = t1 & m00FF;                     // HI | & 0x00FF

              p0 = packz_128_u16_u8(p0, t1);
              t1 = srlb_u128<8>(t1);               // HI | Get Last
              storea(p + 4, p0);

              p += 16;
              u += 16;
              i -= 16;
            }
          }
          else if (bpp == 6) {
            Vec16xU8 t1 = loadu_64<Vec16xU8>(p);

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              Vec16xU8 p0, p1, p2;
              Vec16xU8 u0, u1, u2;

              u0 = loadu<Vec16xU8>(u);
              t1 = interleave_lo_u8(t1, zero);
              p0 = loada<Vec16xU8>(p + 6);

              p1 = srlb_u128<6>(p0);               // P1 | Extract
              u1 = srlb_u128<6>(u0);               // P1 | Extract

              p2 = srlb_u128<12>(p0);              // P2 | Extract
              u2 = srlb_u128<12>(u0);              // P2 | Extract

              p0 = interleave_lo_u8(p0, zero);     // P0 | Unpack
              u0 = interleave_lo_u8(u0, zero);     // P0 | Unpack

              p1 = interleave_lo_u8(p1, zero);     // P1 | Unpack
              u1 = interleave_lo_u8(u1, zero);     // P1 | Unpack

              p2 = interleave_lo_u8(p2, zero);     // P2 | Unpack
              u2 = interleave_lo_u8(u2, zero);     // P2 | Unpack

              u0 = add_i16(u0, t1);                // P0 | Add Last
              u0 = srli_u16<1>(u0);                // P0 | >> 1
              p0 = add_i8(p0, u0);                 // P0 | Add (Up+Last)/2

              u1 = add_i16(u1, p0);                // P1 | Add P0
              u1 = srli_u16<1>(u1);                // P1 | >> 1
              p1 = add_i8(p1, u1);                 // P1 | Add (Up+Last)/2

              u2 = add_i16(u2, p1);                // P2 | Add P1
              u2 = srli_u16<1>(u2);                // P2 | >> 1
              p2 = add_i8(p2, u2);                 // P2 | Add (Up+Last)/2

              p0 = sllb_u128<4>(p0);
              p0 = packz_128_u16_u8(p0, p1);
              p0 = sllb_u128<2>(p0);
              p0 = srlb_u128<4>(p0);

              p2 = packz_128_u16_u8(p2, p2);
              p2 = sllb_u128<12>(p2);
              p0 = p0 | p2;

              storea(p + 6, p0);
              t1 = srlb_u128<10>(p0);

              p += 16;
              u += 16;
              i -= 16;
            }
          }
          else if (bpp == 8) {
            // Process 16 BYTEs at a time.
            Vec16xU8 t1 = interleave_lo_u8(loadu_64<Vec16xU8>(p), zero);

            while (i >= 16) {
              Vec16xU8 p0, p1;
              Vec16xU8 u0, u1;

              u0 = loadu<Vec16xU8>(u);
              p0 = loada<Vec16xU8>(p + 8);

              u1 = u0;                             // HI | Move Up
              p1 = p0;                             // HI | Move Ln
              u0 = interleave_lo_u8(u0, zero);     // LO | Unpack Up
              p0 = interleave_lo_u8(p0, zero);     // LO | Unpack Ln

              u0 = add_i16(u0, t1);                // LO | Add Last
              p1 = interleave_hi_u8(p1, zero);     // HI | Unpack Ln
              u0 = srli_u16<1>(u0);                // LO | >> 1
              u1 = interleave_hi_u8(u1, zero);     // HI | Unpack Up

              p0 = add_i8(p0, u0);                 // LO | Add (Up+Last)/2
              u1 = add_i16(u1, p0);                // HI | Add LO
              u1 = srli_u16<1>(u1);                // HI | >> 1
              p1 = add_i8(p1, u1);                 // HI | Add (Up+LO)/2

              p0 = packz_128_u16_u8(p0, p1);
              t1 = p1;                             // HI | Get Last
              storea(p + 8, p0);

              p += 16;
              u += 16;
              i -= 16;
            }
          }
        }

        for (; i != 0; i--, p++, u++)
          p[bpp] = applySumFilter(p[bpp], applyAvgFilter(p[0], u[0]));

        p += bpp;
        break;
      }

      case BL_PNG_FILTER_TYPE_PAETH: {
        BL_ASSERT(u != nullptr);

        if (bpp == 1) {
          // There is not much to optimize for 1BPP. The only thing this code
          // does is to keep `p0` and `u0` values from the current iteration
          // to the next one (they become `pz` and `uz`).
          uint32_t pz = 0;
          uint32_t uz = 0;
          uint32_t u0;

          for (i = 0; i < bpl; i++) {
            u0 = u[i];
            pz = (uint32_t(p[i]) + blPngPaethFilter(pz, u0, uz)) & 0xFF;

            p[i] = uint8_t(pz);
            uz = u0;
          }

          p += bpl;
        }
        else {
          for (i = 0; i < bpp; i++)
            p[i] = applySumFilter(p[i], u[i]);

          i = bpl - bpp;

          if (i >= 32) {
            // Align to 16-BYTE boundary.
            uint32_t j = uint32_t(IntOps::alignUpDiff(uintptr_t(p + bpp), 16));

            Vec16xU8 zero = make_zero<Vec16xU8>();
            Vec16xU8 rcp3 = make128_u16<Vec16xU8>(0xABu << 7);

            for (i -= j; j != 0; j--, p++, u++)
              p[bpp] = applySumFilter(p[bpp], blPngPaethFilter(p[0], u[bpp], u[0]));

            // TODO: [PNG] Not complete.
            /*
            if (bpp == 2) {
            }
            */
            if (bpp == 3) {
              Vec16xU8 pz = interleave_lo_u8(cast_from_u32<Vec16xU8>(MemOps::readU32u(p) & 0x00FFFFFFu), zero);
              Vec16xU8 uz = interleave_lo_u8(cast_from_u32<Vec16xU8>(MemOps::readU32u(u) & 0x00FFFFFFu), zero);
              Vec16xU8 mask = make128_u32<Vec16xU8>(0u, 0u, 0x0000FFFFu, 0xFFFFFFFFu);

              // Process 8 BYTEs at a time.
              while (i >= 8) {
                Vec16xU8 p0, p1;
                Vec16xU8 u0, u1;

                u0 = loadu_64<Vec16xU8>(u + 3);
                p0 = loadu_64<Vec16xU8>(p + 3);

                u0 = interleave_lo_u8(u0, zero);
                p0 = interleave_lo_u8(p0, zero);
                u1 = srlb_u128<6>(u0);

                BL_PNG_PAETH(uz, pz, u0, uz);
                uz = uz & mask;
                p0 = add_i8(p0, uz);

                BL_PNG_PAETH(uz, p0, u1, u0);
                uz = uz & mask;
                uz = sllb_u128<6>(uz);
                p0 = add_i8(p0, uz);

                p1 = srlb_u128<6>(p0);
                u0 = srlb_u128<6>(u1);

                BL_PNG_PAETH(u0, p1, u0, u1);
                u0 = sllb_u128<12>(u0);

                p0 = add_i8(p0, u0);
                pz = srlb_u128<10>(p0);
                uz = srlb_u128<4>(u1);

                p0 = packz_128_u16_u8(p0, p0);
                storeu_64(p + 3, p0);

                p += 8;
                u += 8;
                i -= 8;
              }
            }
            else if (bpp == 4) {
              Vec16xU8 pz = interleave_lo_u8(loada_32<Vec16xU8>(p), zero);
              Vec16xU8 uz = interleave_lo_u8(loadu_32<Vec16xU8>(u), zero);
              Vec16xU8 mask = make128_u32<Vec16xU8>(0u, 0u, 0xFFFFFFFFu, 0xFFFFFFFFu);

              // Process 16 BYTEs at a time.
              while (i >= 16) {
                Vec16xU8 p0, p1;
                Vec16xU8 u0, u1;

                p0 = loada<Vec16xU8>(p + 4);
                u0 = loadu<Vec16xU8>(u + 4);

                p1 = interleave_hi_u8(p0, zero);
                p0 = interleave_lo_u8(p0, zero);
                u1 = interleave_hi_u8(u0, zero);
                u0 = interleave_lo_u8(u0, zero);

                BL_PNG_PAETH(uz, pz, u0, uz);
                uz = uz & mask;
                p0 = add_i8(p0, uz);
                uz = swap_u64(u0);

                BL_PNG_PAETH(u0, p0, uz, u0);
                u0 = sllb_u128<8>(u0);
                p0 = add_i8(p0, u0);
                pz = srlb_u128<8>(p0);

                BL_PNG_PAETH(uz, pz, u1, uz);
                uz = uz & mask;
                p1 = add_i8(p1, uz);
                uz = swap_u64(u1);

                BL_PNG_PAETH(u1, p1, uz, u1);
                u1 = sllb_u128<8>(u1);
                p1 = add_i8(p1, u1);
                pz = srlb_u128<8>(p1);

                p0 = packz_128_u16_u8(p0, p1);
                storea(p + 4, p0);

                p += 16;
                u += 16;
                i -= 16;
              }
            }
            else if (bpp == 6) {
              Vec16xU8 pz = interleave_lo_u8(loadu_64<Vec16xU8>(p), zero);
              Vec16xU8 uz = interleave_lo_u8(loadu_64<Vec16xU8>(u), zero);

              // Process 16 BYTEs at a time.
              while (i >= 16) {
                Vec16xU8 p0, p1, p2;
                Vec16xU8 u0, u1, u2;

                p0 = loada<Vec16xU8>(p + 6);
                u0 = loadu<Vec16xU8>(u + 6);

                p1 = srlb_u128<6>(p0);
                p0 = interleave_lo_u8(p0, zero);
                u1 = srlb_u128<6>(u0);
                u0 = interleave_lo_u8(u0, zero);

                BL_PNG_PAETH(uz, pz, u0, uz);
                p0 = add_i8(p0, uz);
                p2 = srlb_u128<6>(p1);
                u2 = srlb_u128<6>(u1);
                p1 = interleave_lo_u8(p1, zero);
                u1 = interleave_lo_u8(u1, zero);

                BL_PNG_PAETH(u0, p0, u1, u0);
                p1 = add_i8(p1, u0);
                p2 = interleave_lo_u8(p2, zero);
                u2 = interleave_lo_u8(u2, zero);

                BL_PNG_PAETH(u0, p1, u2, u1);
                p2 = add_i8(p2, u0);

                p0 = sllb_u128<4>(p0);
                p0 = packz_128_u16_u8(p0, p1);
                p0 = sllb_u128<2>(p0);
                p0 = srlb_u128<4>(p0);

                p2 = dup_lo_u64(p2);
                u2 = dup_lo_u64(u2);

                pz = swizzle_u32<3, 3, 1, 0>(interleave_hi_u32(p1, p2));
                uz = swizzle_u32<3, 3, 1, 0>(interleave_hi_u32(u1, u2));

                p2 = packz_128_u16_u8(p2, p2);
                p2 = sllb_u128<12>(p2);

                p0 = p0 | p2;
                storea(p + 6, p0);

                p += 16;
                u += 16;
                i -= 16;
              }
            }
            else if (bpp == 8) {
              Vec16xU8 pz = interleave_lo_u8(loadu_64<Vec16xU8>(p), zero);
              Vec16xU8 uz = interleave_lo_u8(loadu_64<Vec16xU8>(u), zero);

              // Process 16 BYTEs at a time.
              while (i >= 16) {
                Vec16xU8 p0, p1;
                Vec16xU8 u0, u1;

                p0 = loada<Vec16xU8>(p + 8);
                u0 = loadu<Vec16xU8>(u + 8);

                p1 = interleave_hi_u8(p0, zero);
                p0 = interleave_lo_u8(p0, zero);
                u1 = interleave_hi_u8(u0, zero);
                u0 = interleave_lo_u8(u0, zero);

                BL_PNG_PAETH(uz, pz, u0, uz);
                p0 = add_i8(p0, uz);

                BL_PNG_PAETH(pz, p0, u1, u0);
                pz = add_i8(pz, p1);
                uz = u1;

                p0 = packz_128_u16_u8(p0, pz);
                storea(p + 8, p0);

                p += 16;
                u += 16;
                i -= 16;
              }
            }
          }

          for (; i != 0; i--, p++, u++)
            p[bpp] = applySumFilter(p[bpp], blPngPaethFilter(p[0], u[bpp], u[0]));

          p += bpp;
        }
        break;
      }

      case BL_PNG_FILTER_TYPE_AVG0: {
        for (i = bpl - bpp; i != 0; i--, p++)
          p[bpp] = applySumFilter(p[bpp], p[0] >> 1);

        p += bpp;
        break;
      }
    }

    if (--y == 0)
      break;

    u = p - bpl;
    filterType = *p++;

    if (BL_UNLIKELY(filterType >= BL_PNG_FILTER_TYPE_COUNT))
      return blTraceError(BL_ERROR_INVALID_DATA);
  }

  #undef BL_PNG_PAETH
  #undef BL_PNG_SLL_ADDB_1X
  #undef BL_PNG_SLL_ADDB_2X

  return BL_SUCCESS;
}

} // {Png}
} // {bl}

#endif
