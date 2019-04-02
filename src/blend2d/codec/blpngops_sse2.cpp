// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "../blapi-build_p.h"
#ifdef BL_TARGET_OPT_SSE2

#include "../blsimd_p.h"
#include "../codec/blpngops_p.h"

// ============================================================================
// [BLPngOps [InverseFilter@SSE2]]
// ============================================================================

BLResult BL_CDECL blPngInverseFilter_SSE2(uint8_t* p, uint32_t bpp, uint32_t bpl, uint32_t h) noexcept {
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
  filterType = blPngFirstRowFilterReplacement(filterType);

  #define BL_PNG_PAETH(DST, A, B, C)                                          \
    do {                                                                      \
      I128 MinAB = vmini16(A, B);                                             \
      I128 MaxAB = vmaxi16(A, B);                                             \
      I128 DivAB = vmulhu16(vsubi16(MaxAB, MinAB), rcp3);                     \
                                                                              \
      MinAB = vsubi16(MinAB, C);                                              \
      MaxAB = vsubi16(MaxAB, C);                                              \
                                                                              \
      DST = vaddi16(C  , vandnot_a(vsrai16<15>(vaddi16(DivAB, MinAB)), MaxAB)); \
      DST = vaddi16(DST, vandnot_a(vsrai16<15>(vsubi16(DivAB, MaxAB)), MinAB)); \
    } while (0)

  #define BL_PNG_SLL_ADDB_1X(P0, T0, SHIFT)                                   \
    do {                                                                      \
      T0 = vslli128b<SHIFT>(P0);                                              \
      P0 = vaddi8(P0, T0);                                                    \
    } while (0)

  #define BL_PNG_SLL_ADDB_2X(P0, T0, P1, T1, SHIFT)                           \
    do {                                                                      \
      T0 = vslli128b<SHIFT>(P0);                                              \
      T1 = vslli128b<SHIFT>(P1);                                              \
      P0 = vaddi8(P0, T0);                                                    \
      P1 = vaddi8(P1, T1);                                                    \
    } while (0)

  for (;;) {
    uint32_t i;

    switch (filterType) {
      // ----------------------------------------------------------------------
      // [None]
      // ----------------------------------------------------------------------

      case BL_PNG_FILTER_TYPE_NONE:
        p += bpl;
        break;

      // ----------------------------------------------------------------------
      // [Sub]
      // ----------------------------------------------------------------------

      // This is one of the easiest filters to parallelize. Although it looks
      // like the data dependency is too high, it's simply additions, which are
      // really easy to parallelize. The following formula:
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
      // The size of the register doesn't matter here. The Y0' dependency has
      // been omitted to make the flow cleaner, however, it can be added to Y1
      // before processing or it can be shifted to the first cell so the first
      // addition would be performed against [Y0', Y1, Y2, Y3].

      case BL_PNG_FILTER_TYPE_SUB: {
        i = bpl - bpp;

        if (i >= 32) {
          // Align to 16-BYTE boundary.
          uint32_t j = uint32_t(blAlignUpDiff(uintptr_t(p + bpp), 16));
          for (i -= j; j != 0; j--, p++)
            p[bpp] = blPngSumFilter(p[bpp], p[0]);

          if (bpp == 1) {
            I128 p0, p1, p2, p3;
            I128 t0, t2;

            // Process 64 BYTEs at a time.
            p0 = vcvtu32i128(p[0]);
            while (i >= 64) {
              p0 = vaddi8(p0, *reinterpret_cast<I128*>(p + 1));
              p1 = vloadi128a(p + 17);
              p2 = vloadi128a(p + 33);
              p3 = vloadi128a(p + 49);

              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t2, 1);
              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t2, 2);
              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t2, 4);
              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t2, 8);
              vstorei128a(p + 1, p0);

              p0 = vsrli128b<15>(p0);
              t2 = vsrli128b<15>(p2);
              p1 = vaddi8(p1, p0);
              p3 = vaddi8(p3, t2);

              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t2, 1);
              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t2, 2);
              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t2, 4);
              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t2, 8);
              vstorei128a(p + 17, p1);

              p1 = vunpackhi8(p1, p1);
              p1 = vunpackhi16(p1, p1);
              p1 = vswizi32<3, 3, 3, 3>(p1);

              p2 = vaddi8(p2, p1);
              p3 = vaddi8(p3, p1);

              vstorei128a(p + 33, p2);
              vstorei128a(p + 49, p3);
              p0 = vsrli128b<15>(p3);

              p += 64;
              i -= 64;
            }

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              p0 = vaddi8(p0, *reinterpret_cast<I128*>(p + 1));

              BL_PNG_SLL_ADDB_1X(p0, t0, 1);
              BL_PNG_SLL_ADDB_1X(p0, t0, 2);
              BL_PNG_SLL_ADDB_1X(p0, t0, 4);
              BL_PNG_SLL_ADDB_1X(p0, t0, 8);

              vstorei128a(p + 1, p0);
              p0 = vsrli128b<15>(p0);

              p += 16;
              i -= 16;
            }
          }
          else if (bpp == 2) {
            I128 p0, p1, p2, p3;
            I128 t0, t2;

            // Process 64 BYTEs at a time.
            p0 = vcvtu32i128(blMemReadU16a(p));
            while (i >= 64) {
              p0 = vaddi8(p0, *reinterpret_cast<I128*>(p + 2));
              p1 = vloadi128a(p + 18);
              p2 = vloadi128a(p + 34);
              p3 = vloadi128a(p + 50);

              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t2, 2);
              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t2, 4);
              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t2, 8);
              vstorei128a(p + 2, p0);

              p0 = vsrli128b<14>(p0);
              t2 = vsrli128b<14>(p2);
              p1 = vaddi8(p1, p0);
              p3 = vaddi8(p3, t2);

              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t2, 2);
              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t2, 4);
              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t2, 8);
              vstorei128a(p + 18, p1);

              p1 = vunpackhi16(p1, p1);
              p1 = vswizi32<3, 3, 3, 3>(p1);

              p2 = vaddi8(p2, p1);
              p3 = vaddi8(p3, p1);

              vstorei128a(p + 34, p2);
              vstorei128a(p + 50, p3);
              p0 = vsrli128b<14>(p3);

              p += 64;
              i -= 64;
            }

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              p0 = vaddi8(p0, *reinterpret_cast<I128*>(p + 2));
              BL_PNG_SLL_ADDB_1X(p0, t0, 2);
              BL_PNG_SLL_ADDB_1X(p0, t0, 4);
              BL_PNG_SLL_ADDB_1X(p0, t0, 8);

              vstorei128a(p + 2, p0);
              p0 = vsrli128b<14>(p0);

              p += 16;
              i -= 16;
            }
          }
          else if (bpp == 3) {
            I128 p0, p1, p2, p3;
            I128 t0, t2;
            I128 ext3b = vseti128i32(0x01000001);

            // Process 64 BYTEs at a time.
            p0 = vcvtu32i128(blMemReadU32u(p) & 0x00FFFFFFu);
            while (i >= 64) {
              p0 = vaddi8(p0, *reinterpret_cast<I128*>(p + 3));
              p1 = vloadi128a(p + 19);
              p2 = vloadi128a(p + 35);

              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t2, 3);
              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t2, 6);
              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t2, 12);

              p3 = vloadi128a(p + 51);
              t0 = vsrli128b<13>(p0);
              t2 = vsrli128b<13>(p2);

              p1 = vaddi8(p1, t0);
              p3 = vaddi8(p3, t2);

              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t2, 3);
              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t2, 6);
              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t2, 12);
              vstorei128a(p + 3, p0);

              p0 = vswizi32<3, 3, 3, 3>(p1);
              p0 = vsrli32<8>(p0);
              p0 = _mm_mul_epu32(p0, ext3b);

              p0 = vswizli16<0, 2, 1, 0>(p0);
              p0 = vswizhi16<1, 0, 2, 1>(p0);

              vstorei128a(p + 19, p1);
              p2 = vaddi8(p2, p0);
              p0 = vswizi32<1, 3, 2, 1>(p0);

              vstorei128a(p + 35, p2);
              p0 = vaddi8(p0, p3);

              vstorei128a(p + 51, p0);
              p0 = vsrli128b<13>(p0);

              p += 64;
              i -= 64;
            }

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              p0 = vaddi8(p0, *reinterpret_cast<I128*>(p + 3));

              BL_PNG_SLL_ADDB_1X(p0, t0, 3);
              BL_PNG_SLL_ADDB_1X(p0, t0, 6);
              BL_PNG_SLL_ADDB_1X(p0, t0, 12);

              vstorei128a(p + 3, p0);
              p0 = vsrli128b<13>(p0);

              p += 16;
              i -= 16;
            }
          }
          else if (bpp == 4) {
            I128 p0, p1, p2, p3;
            I128 t0, t1, t2;

            // Process 64 BYTEs at a time.
            p0 = vcvtu32i128(blMemReadU32a(p));
            while (i >= 64) {
              p0 = vaddi8(p0, *reinterpret_cast<I128*>(p + 4));
              p1 = vloadi128a(p + 20);
              p2 = vloadi128a(p + 36);
              p3 = vloadi128a(p + 52);

              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t1, 4);
              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t1, 8);
              vstorei128a(p + 4, p0);

              p0 = vsrli128b<12>(p0);
              t2 = vsrli128b<12>(p2);

              p1 = vaddi8(p1, p0);
              p3 = vaddi8(p3, t2);

              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t1, 4);
              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t1, 8);

              p0 = vswizi32<3, 3, 3, 3>(p1);
              vstorei128a(p + 20, p1);

              p2 = vaddi8(p2, p0);
              p0 = vaddi8(p0, p3);

              vstorei128a(p + 36, p2);
              vstorei128a(p + 52, p0);
              p0 = vsrli128b<12>(p0);

              p += 64;
              i -= 64;
            }

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              p0 = vaddi8(p0, *reinterpret_cast<I128*>(p + 4));

              BL_PNG_SLL_ADDB_1X(p0, t0, 4);
              BL_PNG_SLL_ADDB_1X(p0, t0, 8);
              vstorei128a(p + 4, p0);
              p0 = vsrli128b<12>(p0);

              p += 16;
              i -= 16;
            }
          }
          else if (bpp == 6) {
            I128 p0, p1, p2, p3;
            I128 t0, t1;

            p0 = vloadi128_64(p);
            p0 = vslli64<16>(p0);
            p0 = vsrli64<16>(p0);

            // Process 64 BYTEs at a time.
            while (i >= 64) {
              p0 = vaddi8(p0, *reinterpret_cast<I128*>(p + 6));
              p1 = vloadi128a(p + 22);
              p2 = vloadi128a(p + 38);

              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t1, 6);
              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t1, 12);

              p3 = vloadi128a(p + 54);
              vstorei128a(p + 6, p0);

              p0 = vsrli128b<10>(p0);
              t1 = vsrli128b<10>(p2);

              p1 = vaddi8(p1, p0);
              p3 = vaddi8(p3, t1);

              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t1, 6);
              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t1, 12);
              p0 = vduphi64(p1);

              p0 = vswizli16<1, 3, 2, 1>(p0);
              p0 = vswizhi16<2, 1, 3, 2>(p0);

              vstorei128a(p + 22, p1);
              p2 = vaddi8(p2, p0);
              p0 = vswizi32<1, 3, 2, 1>(p0);

              vstorei128a(p + 38, p2);
              p0 = vaddi8(p0, p3);

              vstorei128a(p + 54, p0);
              p0 = vsrli128b<10>(p0);

              p += 64;
              i -= 64;
            }

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              p0 = vaddi8(p0, *reinterpret_cast<I128*>(p + 6));

              BL_PNG_SLL_ADDB_1X(p0, t0, 6);
              BL_PNG_SLL_ADDB_1X(p0, t0, 12);

              vstorei128a(p + 6, p0);
              p0 = vsrli128b<10>(p0);

              p += 16;
              i -= 16;
            }
          }
          else if (bpp == 8) {
            I128 p0, p1, p2, p3;
            I128 t0, t1, t2;

            // Process 64 BYTEs at a time.
            p0 = vloadi128_64(p);
            while (i >= 64) {
              p0 = vaddi8(p0, *reinterpret_cast<I128*>(p + 8));
              p1 = vloadi128a(p + 24);
              p2 = vloadi128a(p + 40);
              p3 = vloadi128a(p + 56);

              BL_PNG_SLL_ADDB_2X(p0, t0, p2, t1, 8);
              vstorei128a(p + 8, p0);

              p0 = vsrli128b<8>(p0);
              t2 = vduphi64(p2);
              p1 = vaddi8(p1, p0);

              BL_PNG_SLL_ADDB_2X(p1, t0, p3, t1, 8);
              p0 = vduphi64(p1);
              p3 = vaddi8(p3, t2);
              vstorei128a(p + 24, p1);

              p2 = vaddi8(p2, p0);
              p0 = vaddi8(p0, p3);

              vstorei128a(p + 40, p2);
              vstorei128a(p + 56, p0);
              p0 = vsrli128b<8>(p0);

              p += 64;
              i -= 64;
            }

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              p0 = vaddi8(p0, *reinterpret_cast<I128*>(p + 8));
              BL_PNG_SLL_ADDB_1X(p0, t0, 8);

              vstorei128a(p + 8, p0);
              p0 = vsrli128b<8>(p0);

              p += 16;
              i -= 16;
            }
          }
        }

        for (; i != 0; i--, p++)
          p[bpp] = blPngSumFilter(p[bpp], p[0]);

        p += bpp;
        break;
      }

      // ----------------------------------------------------------------------
      // [Up]
      // ----------------------------------------------------------------------

      // This is actually the easiest filter that doesn't require any kind of
      // specialization for a particular BPP. Even C++ compiler like GCC is
      // able to parallelize a naive implementation. However, MSVC compiler does
      // not parallelize the naive implementation so the SSE2 implementation
      // provided greatly boosted the performance on Windows.
      //
      //     +-----------+-----------+-----------+-----------+
      //     |    Y1     |    Y2     |    Y3     |    Y4     |
      //     +-----------+-----------+-----------+-----------+
      //                           PADDB
      //     +-----------+-----------+-----------+-----------+
      //     |    U1     |    U2     |    U3     |    U4     | ----+
      //     +-----------+-----------+-----------+-----------+     |
      //                                                           |
      //     +-----------+-----------+-----------+-----------+     |
      //     |   Y1+U1   |   Y2+U2   |   Y3+U3   |   Y4+U4   | <---+
      //     +-----------+-----------+-----------+-----------+

      case BL_PNG_FILTER_TYPE_UP: {
        i = bpl;

        if (i >= 24) {
          // Align to 16-BYTE boundary.
          uint32_t j = uint32_t(blAlignUpDiff(uintptr_t(p), 16));
          for (i -= j; j != 0; j--, p++, u++)
            p[0] = blPngSumFilter(p[0], u[0]);

          // Process 64 BYTEs at a time.
          while (i >= 64) {
            I128 u0 = vloadi128u(u);
            I128 u1 = vloadi128u(u + 16);

            I128 p0 = vloadi128a(p);
            I128 p1 = vloadi128a(p + 16);

            I128 u2 = vloadi128u(u + 32);
            I128 u3 = vloadi128u(u + 48);

            p0 = vaddi8(p0, u0);
            p1 = vaddi8(p1, u1);

            I128 p2 = vloadi128a(p + 32);
            I128 p3 = vloadi128a(p + 48);

            p2 = vaddi8(p2, u2);
            p3 = vaddi8(p3, u3);

            vstorei128a(p     , p0);
            vstorei128a(p + 16, p1);
            vstorei128a(p + 32, p2);
            vstorei128a(p + 48, p3);

            p += 64;
            u += 64;
            i -= 64;
          }

          // Process 8 BYTEs at a time.
          while (i >= 8) {
            I128 u0 = vloadi128_64(u);
            I128 p0 = vloadi128_64(p);

            p0 = vaddi8(p0, u0);
            vstorei64(p, p0);

            p += 8;
            u += 8;
            i -= 8;
          }
        }

        for (; i != 0; i--, p++, u++)
          p[0] = blPngSumFilter(p[0], u[0]);
        break;
      }

      // ----------------------------------------------------------------------
      // [Avg]
      // ----------------------------------------------------------------------

      // This filter is extremely difficult for low BPP values as there is
      // a huge sequential data dependency, I didn't succeeded to solve it.
      // 1-3 BPP implementations are pretty bad and I would like to hear about
      // a way to improve those. The implementation for 4 BPP and more is
      // pretty good, as these is less data dependency between individual bytes.
      //
      // Sequental Approach:
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
        for (i = 0; i < bpp; i++)
          p[i] = blPngSumFilter(p[i], u[i] >> 1);

        i = bpl - bpp;
        u += bpp;

        if (i >= 32) {
          // Align to 16-BYTE boundary.
          uint32_t j = uint32_t(blAlignUpDiff(uintptr_t(p + bpp), 16));
          I128 zero = vzeroi128();

          for (i -= j; j != 0; j--, p++, u++)
            p[bpp] = blPngSumFilter(p[bpp], blPngAvgFilter(p[0], u[0]));

          if (bpp == 1) {
            // This is one of the most difficult AVG filters. 1-BPP has a huge
            // sequential dependency, which is nearly impossible to parallelize.
            // The code below is the best I could have written, it's a mixture
            // of C++ and SIMD. Maybe using a pure C would be even better than
            // this code, but, I tried to take advantage of 8 BYTE fetches at
            // least. Unrolling the loop any further doesn't lead to an
            // improvement.
            //
            // I know that the code looks terrible, but it's a bit faster than
            // a pure specialized C++ implementation I used to have before.
            uint32_t t0 = p[0];
            uint32_t t1;

            // Process 8 BYTEs at a time.
            while (i >= 8) {
              I128 p0 = vloadi128_64(p + 1);
              I128 u0 = vloadi128_64(u);

              p0 = vunpackli8(p0, zero);
              u0 = vunpackli8(u0, zero);

              p0 = vslli16<1>(p0);
              p0 = vaddi16(p0, u0);

              t1 = vcvti128u32(p0);
              p0 = vsrli128b<4>(p0);
              t0 = ((t0 + t1) >> 1) & 0xFF; t1 >>= 16;
              p[1] = uint8_t(t0);

              t0 = ((t0 + t1) >> 1) & 0xFF;
              t1 = vcvti128u32(p0);
              p0 = vsrli128b<4>(p0);
              p[2] = uint8_t(t0);

              t0 = ((t0 + t1) >> 1) & 0xFF; t1 >>= 16;
              p[3] = uint8_t(t0);

              t0 = ((t0 + t1) >> 1) & 0xFF;
              t1 = vcvti128u32(p0);
              p0 = vsrli128b<4>(p0);
              p[4] = uint8_t(t0);

              t0 = ((t0 + t1) >> 1) & 0xFF; t1 >>= 16;
              p[5] = uint8_t(t0);

              t0 = ((t0 + t1) >> 1) & 0xFF;
              t1 = vcvti128u32(p0);
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
            I128 m00FF = vseti128i32(0x00FF00FF);
            I128 m01FF = vseti128i32(0x01FF01FF);
            I128 t1 = vunpackli8(vcvtu32i128(blMemReadU32a(p)), zero);

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              I128 p0, p1;
              I128 u0, u1;

              p0 = vloadi128a(p + 4);
              u0 = vloadi128u(u);

              p1 = p0;                       // HI | Move Ln
              p0 = vunpackli8(p0, zero);     // LO | Unpack Ln

              u1 = u0;                       // HI | Move Up
              p0 = vslli16<1>(p0);           // LO | << 1

              u0 = vunpackli8(u0, zero);     // LO | Unpack Up
              p0 = vaddi16(p0, t1);          // LO | Add Last

              p1 = vunpackhi8(p1, zero);     // HI | Unpack Ln
              p0 = vaddi16(p0, u0);          // LO | Add Up
              p0 = vand(p0, m01FF);          // LO | & 0x01FE

              u1 = vunpackhi8(u1, zero);     // HI | Unpack Up
              t1 = vslli128b<8>(p0);         // LO | Get Last
              p0 = vslli16<1>(p0);           // LO | << 1

              p1 = vslli16<1>(p1);           // HI | << 1
              p0 = vaddi16(p0, t1);          // LO | Add Last
              p0 = vsrli16<2>(p0);           // LO | >> 2

              p1 = vaddi16(p1, u1);          // HI | Add Up
              p0 = vand(p0, m00FF);          // LO | & 0x00FF
              t1 = vsrli128b<8>(p0);         // LO | Get Last

              p1 = vaddi16(p1, t1);          // HI | Add Last
              p1 = vand(p1, m01FF);          // HI | & 0x01FE

              t1 = vslli128b<8>(p1);         // HI | Get Last
              p1 = vslli16<1>(p1);           // HI | << 1

              t1 = vaddi16(t1, p1);          // HI | Add Last
              t1 = vsrli16<2>(t1);           // HI | >> 2
              t1 = vand(t1, m00FF);          // HI | & 0x00FF

              p0 = vpackzzwb(p0, t1);
              t1 = vsrli128b<8>(t1);         // HI | Get Last
              vstorei128a(p + 4, p0);

              p += 16;
              u += 16;
              i -= 16;
            }
          }
          else if (bpp == 6) {
            I128 t1 = vloadi128_64(p);

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              I128 p0, p1, p2;
              I128 u0, u1, u2;

              u0 = vloadi128u(u);
              t1 = vunpackli8(t1, zero);
              p0 = vloadi128a(p + 6);

              p1 = vsrli128b<6>(p0);         // P1 | Extract
              u1 = vsrli128b<6>(u0);         // P1 | Extract

              p2 = vsrli128b<12>(p0);        // P2 | Extract
              u2 = vsrli128b<12>(u0);        // P2 | Extract

              p0 = vunpackli8(p0, zero);     // P0 | Unpack
              u0 = vunpackli8(u0, zero);     // P0 | Unpack

              p1 = vunpackli8(p1, zero);     // P1 | Unpack
              u1 = vunpackli8(u1, zero);     // P1 | Unpack

              p2 = vunpackli8(p2, zero);     // P2 | Unpack
              u2 = vunpackli8(u2, zero);     // P2 | Unpack

              u0 = vaddi16(u0, t1);          // P0 | Add Last
              u0 = vsrli16<1>(u0);           // P0 | >> 1
              p0 = vaddi8(p0, u0);           // P0 | Add (Up+Last)/2

              u1 = vaddi16(u1, p0);          // P1 | Add P0
              u1 = vsrli16<1>(u1);           // P1 | >> 1
              p1 = vaddi8(p1, u1);           // P1 | Add (Up+Last)/2

              u2 = vaddi16(u2, p1);          // P2 | Add P1
              u2 = vsrli16<1>(u2);           // P2 | >> 1
              p2 = vaddi8(p2, u2);           // P2 | Add (Up+Last)/2

              p0 = vslli128b<4>(p0);
              p0 = vpackzzwb(p0, p1);
              p0 = vslli128b<2>(p0);
              p0 = vsrli128b<4>(p0);

              p2 = vpackzzwb(p2, p2);
              p2 = vslli128b<12>(p2);
              p0 = vor(p0, p2);

              vstorei128a(p + 6, p0);
              t1 = vsrli128b<10>(p0);

              p += 16;
              u += 16;
              i -= 16;
            }
          }
          else if (bpp == 8) {
            // Process 16 BYTEs at a time.
            I128 t1 = vunpackli8(vloadi128_64(p), zero);

            while (i >= 16) {
              I128 p0, p1;
              I128 u0, u1;

              u0 = vloadi128u(u);
              p0 = vloadi128a(p + 8);

              u1 = u0;                       // HI | Move Up
              p1 = p0;                       // HI | Move Ln
              u0 = vunpackli8(u0, zero);     // LO | Unpack Up
              p0 = vunpackli8(p0, zero);     // LO | Unpack Ln

              u0 = vaddi16(u0, t1);          // LO | Add Last
              p1 = vunpackhi8(p1, zero);     // HI | Unpack Ln
              u0 = vsrli16<1>(u0);           // LO | >> 1
              u1 = vunpackhi8(u1, zero);     // HI | Unpack Up

              p0 = vaddi8(p0, u0);           // LO | Add (Up+Last)/2
              u1 = vaddi16(u1, p0);          // HI | Add LO
              u1 = vsrli16<1>(u1);           // HI | >> 1
              p1 = vaddi8(p1, u1);           // HI | Add (Up+LO)/2

              p0 = vpackzzwb(p0, p1);
              t1 = p1;                       // HI | Get Last
              vstorei128a(p + 8, p0);

              p += 16;
              u += 16;
              i -= 16;
            }
          }
        }

        for (; i != 0; i--, p++, u++)
          p[bpp] = blPngSumFilter(p[bpp], blPngAvgFilter(p[0], u[0]));

        p += bpp;
        break;
      }

      // ----------------------------------------------------------------------
      // [Paeth]
      // ----------------------------------------------------------------------

      case BL_PNG_FILTER_TYPE_PAETH: {
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
            p[i] = blPngSumFilter(p[i], u[i]);

          i = bpl - bpp;

          if (i >= 32) {
            // Align to 16-BYTE boundary.
            uint32_t j = uint32_t(blAlignUpDiff(uintptr_t(p + bpp), 16));

            I128 zero = vzeroi128();
            I128 rcp3 = vseti128i16(0xAB << 7);

            for (i -= j; j != 0; j--, p++, u++)
              p[bpp] = blPngSumFilter(p[bpp], blPngPaethFilter(p[0], u[bpp], u[0]));

            // TODO: [PNG] Not complete.
            /*
            if (bpp == 2) {
            }
            */
            if (bpp == 3) {
              I128 pz = vunpackli8(vcvtu32i128(blMemReadU32u(p) & 0x00FFFFFFu), zero);
              I128 uz = vunpackli8(vcvtu32i128(blMemReadU32u(u) & 0x00FFFFFFu), zero);
              I128 mask = vseti128i32(0, 0, 0x0000FFFF, 0xFFFFFFFF);

              // Process 8 BYTEs at a time.
              while (i >= 8) {
                I128 p0, p1;
                I128 u0, u1;

                u0 = vloadi128_64(u + 3);
                p0 = vloadi128_64(p + 3);

                u0 = vunpackli8(u0, zero);
                p0 = vunpackli8(p0, zero);
                u1 = vsrli128b<6>(u0);

                BL_PNG_PAETH(uz, pz, u0, uz);
                uz = vand(uz, mask);
                p0 = vaddi8(p0, uz);

                BL_PNG_PAETH(uz, p0, u1, u0);
                uz = vand(uz, mask);
                uz = vslli128b<6>(uz);
                p0 = vaddi8(p0, uz);

                p1 = vsrli128b<6>(p0);
                u0 = vsrli128b<6>(u1);

                BL_PNG_PAETH(u0, p1, u0, u1);
                u0 = vslli128b<12>(u0);

                p0 = vaddi8(p0, u0);
                pz = vsrli128b<10>(p0);
                uz = vsrli128b<4>(u1);

                p0 = vpackzzwb(p0, p0);
                vstorei64(p + 3, p0);

                p += 8;
                u += 8;
                i -= 8;
              }
            }
            else if (bpp == 4) {
              I128 pz = vunpackli8(_mm_cvtsi32_si128(blMemReadU32a(p)), zero);
              I128 uz = vunpackli8(_mm_cvtsi32_si128(blMemReadU32u(u)), zero);
              I128 mask = vseti128i32(0, 0, 0xFFFFFFFF, 0xFFFFFFFF);

              // Process 16 BYTEs at a time.
              while (i >= 16) {
                I128 p0, p1;
                I128 u0, u1;

                p0 = vloadi128a(p + 4);
                u0 = vloadi128u(u + 4);

                p1 = vunpackhi8(p0, zero);
                p0 = vunpackli8(p0, zero);
                u1 = vunpackhi8(u0, zero);
                u0 = vunpackli8(u0, zero);

                BL_PNG_PAETH(uz, pz, u0, uz);
                uz = vand(uz, mask);
                p0 = vaddi8(p0, uz);
                uz = vswapi64(u0);

                BL_PNG_PAETH(u0, p0, uz, u0);
                u0 = vslli128b<8>(u0);
                p0 = vaddi8(p0, u0);
                pz = vsrli128b<8>(p0);

                BL_PNG_PAETH(uz, pz, u1, uz);
                uz = vand(uz, mask);
                p1 = vaddi8(p1, uz);
                uz = vswapi64(u1);

                BL_PNG_PAETH(u1, p1, uz, u1);
                u1 = vslli128b<8>(u1);
                p1 = vaddi8(p1, u1);
                pz = vsrli128b<8>(p1);

                p0 = vpackzzwb(p0, p1);
                vstorei128a(p + 4, p0);

                p += 16;
                u += 16;
                i -= 16;
              }
            }
            else if (bpp == 6) {
              I128 pz = vunpackli8(vloadi128_64(p), zero);
              I128 uz = vunpackli8(vloadi128_64(u), zero);

              // Process 16 BYTEs at a time.
              while (i >= 16) {
                I128 p0, p1, p2;
                I128 u0, u1, u2;

                p0 = vloadi128a(p + 6);
                u0 = vloadi128u(u + 6);

                p1 = vsrli128b<6>(p0);
                p0 = vunpackli8(p0, zero);
                u1 = vsrli128b<6>(u0);
                u0 = vunpackli8(u0, zero);

                BL_PNG_PAETH(uz, pz, u0, uz);
                p0 = vaddi8(p0, uz);
                p2 = vsrli128b<6>(p1);
                u2 = vsrli128b<6>(u1);
                p1 = vunpackli8(p1, zero);
                u1 = vunpackli8(u1, zero);

                BL_PNG_PAETH(u0, p0, u1, u0);
                p1 = vaddi8(p1, u0);
                p2 = vunpackli8(p2, zero);
                u2 = vunpackli8(u2, zero);

                BL_PNG_PAETH(u0, p1, u2, u1);
                p2 = vaddi8(p2, u0);

                p0 = vslli128b<4>(p0);
                p0 = vpackzzwb(p0, p1);
                p0 = vslli128b<2>(p0);
                p0 = vsrli128b<4>(p0);

                p2 = vdupli64(p2);
                u2 = vdupli64(u2);

                pz = vswizi32<3, 3, 1, 0>(vunpackhi32(p1, p2));
                uz = vswizi32<3, 3, 1, 0>(vunpackhi32(u1, u2));

                p2 = vpackzzwb(p2, p2);
                p2 = vslli128b<12>(p2);

                p0 = vor(p0, p2);
                vstorei128a(p + 6, p0);

                p += 16;
                u += 16;
                i -= 16;
              }
            }
            else if (bpp == 8) {
              I128 pz = vunpackli8(vloadi128_64(p), zero);
              I128 uz = vunpackli8(vloadi128_64(u), zero);

              // Process 16 BYTEs at a time.
              while (i >= 16) {
                I128 p0, p1;
                I128 u0, u1;

                p0 = vloadi128a(p + 8);
                u0 = vloadi128u(u + 8);

                p1 = vunpackhi8(p0, zero);
                p0 = vunpackli8(p0, zero);
                u1 = vunpackhi8(u0, zero);
                u0 = vunpackli8(u0, zero);

                BL_PNG_PAETH(uz, pz, u0, uz);
                p0 = vaddi8(p0, uz);

                BL_PNG_PAETH(pz, p0, u1, u0);
                pz = vaddi8(pz, p1);
                uz = u1;

                p0 = vpackzzwb(p0, pz);
                vstorei128a(p + 8, p0);

                p += 16;
                u += 16;
                i -= 16;
              }
            }
          }

          for (; i != 0; i--, p++, u++)
            p[bpp] = blPngSumFilter(p[bpp], blPngPaethFilter(p[0], u[bpp], u[0]));

          p += bpp;
        }
        break;
      }

      case BL_PNG_FILTER_TYPE_AVG0: {
        for (i = bpl - bpp; i != 0; i--, p++)
          p[bpp] = blPngSumFilter(p[bpp], p[0] >> 1);

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

#endif
