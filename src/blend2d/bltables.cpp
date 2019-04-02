// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./bltables_p.h"

// ============================================================================
// [BLBitCountOfByteTable]
// ============================================================================

struct BLBitCountOfByteTableGen {
  static constexpr uint8_t value(size_t i) noexcept {
    return uint8_t( ((i >> 0) & 1) + ((i >> 1) & 1) + ((i >> 2) & 1) + ((i >> 3) & 1) +
                    ((i >> 4) & 1) + ((i >> 5) & 1) + ((i >> 6) & 1) + ((i >> 7) & 1) );
  }
};

static constexpr const BLLookupTable<uint8_t, 256> blBitCountOfByteTable_
  = blLookupTable<uint8_t, 256, BLBitCountOfByteTableGen>();

const BLLookupTable<uint8_t, 256> blBitCountOfByteTable = blBitCountOfByteTable_;

// ============================================================================
// [BLModuloTable]
// ============================================================================

#define INV()  {{ 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0   , 0   , 0   , 0   , 0   , 0   , 0    }}
#define DEF(N) {{ 1%N, 2%N, 3%N, 4%N, 5%N, 6%N, 7%N, 8%N, 9%N, 10%N, 11%N, 12%N, 13%N, 14%N, 15%N, 16%N }}

const BLModuloTable blModuloTable[18] = {
  INV(  ), DEF( 1), DEF( 2), DEF( 3),
  DEF( 4), DEF( 5), DEF( 6), DEF( 7),
  DEF( 8), DEF( 9), DEF(10), DEF(11),
  DEF(12), DEF(13), DEF(14), DEF(15),
  DEF(16), DEF(17)
};

#undef DEF
#undef INV

// ============================================================================
// [BLCommonTable]
// ============================================================================

struct BLDiv24BitReciprocalGen {
  // Helper to calculate the reciprocal - C++11 doesn't allow variables...
  static constexpr uint32_t value(size_t t, double d) noexcept {
    return uint32_t(d) + uint32_t(d - double(uint32_t(d)) > 0.0);
  }

  //! Calculates the reciprocal for `BLCommonTable::div25bit` table.
  static constexpr uint32_t value(size_t i) noexcept {
    return value(i, i ? double(0xFF0000) / double(i) : 0.0);
  }
};

#define REPEAT_1X(...) __VA_ARGS__
#define REPEAT_2X(...) __VA_ARGS__, __VA_ARGS__
#define REPEAT_4X(...) __VA_ARGS__, __VA_ARGS__, __VA_ARGS__, __VA_ARGS__
#define FLOAT_4X(A, B, C, D) float(A), float(B), float(C), float(D)

static constexpr const BLCommonTable blCommonTable_ = {
  // --------------------------------------------------------------------------
  // [I128 Constants]
  // --------------------------------------------------------------------------

  { REPEAT_2X(0x0000000000000000u) },
  { REPEAT_2X(0x007F007F007F007Fu) },
  { REPEAT_2X(0x0080008000800080u) },
  { REPEAT_2X(0x00FF00FF00FF00FFu) },
  { REPEAT_2X(0x0100010001000100u) },
  { REPEAT_2X(0x0101010101010101u) },
  { REPEAT_2X(0x01FF01FF01FF01FFu) },
  { REPEAT_2X(0x0200020002000200u) },
  { REPEAT_2X(0x8000800080008000u) },
  { REPEAT_2X(0xFFFFFFFFFFFFFFFFu) },

  { REPEAT_2X(0x000000FF000000FFu) },
  { REPEAT_2X(0x0000010000000100u) },
  { REPEAT_2X(0x000001FF000001FFu) },
  { REPEAT_2X(0x0000020000000200u) },
  { REPEAT_2X(0x0000FFFF0000FFFFu) },
  { REPEAT_2X(0x0002000000020000u) },
  { REPEAT_2X(0x00FFFFFF00FFFFFFu) },
  { REPEAT_2X(0xFF000000FF000000u) },
  { REPEAT_2X(0xFFFF0000FFFF0000u) },

  { REPEAT_2X(0x000000FF00FF00FFu) },
  { REPEAT_2X(0x0000010001000100u) },
  { REPEAT_2X(0x0000080000000800u) },
  { REPEAT_2X(0x0000FFFFFFFFFFFFu) },
  { REPEAT_2X(0x00FF000000000000u) },
  { REPEAT_2X(0x0100000000000000u) },
  { REPEAT_2X(0x0101010100000000u) },
  { REPEAT_2X(0xFFFF000000000000u) },
  { REPEAT_2X(0xFFFFFFFF00000000u) },

  { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0 },        // i128_FFFFFFFF_FFFFFFFF_FFFFFFFF_0.

  { 0, 1, 2, 3 },                                      // xmm_u32_0_1_2_3.
  { REPEAT_4X(4) },                                    // xmm_u32_4.

  // --------------------------------------------------------------------------
  // [F128 Constants]
  // --------------------------------------------------------------------------

  { REPEAT_2X(0x8000000080000000) },
  { REPEAT_2X(0x7FFFFFFF7FFFFFFF) },
  { REPEAT_2X(0xFFFFFFFF7FFFFFFF) },
  { REPEAT_2X(0x7FFFFFFFFFFFFFFF) },
  { REPEAT_4X(8388608.0f)         },
  { REPEAT_4X(12582912.0f)        },

  { REPEAT_4X(1.0f)               }, // f128_1.
  { REPEAT_4X(4.0f)               }, // f128_4.
  { REPEAT_4X(255.0f)             }, // f128_255.
  { REPEAT_4X(1e-3f)              }, // f128_1e_m3.
  { REPEAT_4X(1e-20f)             }, // f128_1e_m20.
  { REPEAT_4X(1.0f / 255.0f)      }, // f128_1div255.
  { 0.0f  , 1.0f  , 2.0f  , 3.0f  }, // f128_3_2_1_0.

  // --------------------------------------------------------------------------
  // [D128 Constants]
  // --------------------------------------------------------------------------

  { REPEAT_1X(0x8000000000000000u, 0x8000000000000000u) },
  { REPEAT_1X(0x7FFFFFFFFFFFFFFFu, 0x7FFFFFFFFFFFFFFFu) },
  { REPEAT_1X(0x7FFFFFFFFFFFFFFFu, 0xFFFFFFFFFFFFFFFFu) },
  { REPEAT_1X(0xFFFFFFFFFFFFFFFFu, 0x7FFFFFFFFFFFFFFFu) },
  { REPEAT_2X(4503599627370496.0) },
  { REPEAT_2X(6755399441055744.0) },

  { REPEAT_2X(1.0)                }, // d128_1.
  { REPEAT_2X(1e-20)              }, // d128_1e_m20.
  { REPEAT_2X(4.0)                }, // d128_4.
  { REPEAT_2X(-1.0)               }, // d128_m1.

  // --------------------------------------------------------------------------
  // [PSHUFB Constants]
  // --------------------------------------------------------------------------

  #define Z 0x80 // PSHUFB zeroing.
  { 0 , 4 , 8 , 12, 0 , 4 , 8 , 12, 0 , 4 , 8 , 12, 0 , 4 , 8 , 12 }, // i128_pshufb_u32_to_u8_lo
  { 0 , 1 , 4 , 5 , 8 , 9 , 12, 13, 0 , 1 , 4 , 5 , 8 , 9 , 12, 13 }, // i128_pshufb_u32_to_u16_lo
  { 3 , Z , 3 , Z , 3 , Z , 3 , Z , 7 , Z , 7 , Z , 7 , Z , 7 , Z  }, // i128_pshufb_packed_argb32_2x_lo_to_unpacked_a8
  { 11, Z , 11, Z , 11, Z , 11, Z , 15, Z , 15, Z , 15, Z , 15, Z  }, // i128_pshufb_packed_argb32_2x_lo_to_unpacked_a8
  #undef Z

  // dummy to align the constants that follow.
  { 0 },

  // --------------------------------------------------------------------------
  // [I256 Constants]
  // --------------------------------------------------------------------------

  { REPEAT_4X(0x007F007F007F007Fu) },
  { REPEAT_4X(0x0080008000800080u) },
  { REPEAT_4X(0x00FF00FF00FF00FFu) },
  { REPEAT_4X(0x0100010001000100u) },
  { REPEAT_4X(0x0101010101010101u) },
  { REPEAT_4X(0x01FF01FF01FF01FFu) },
  { REPEAT_4X(0x0200020002000200u) },
  { REPEAT_4X(0x8000800080008000u) },
  { REPEAT_4X(0xFFFFFFFFFFFFFFFFu) },

  // --------------------------------------------------------------------------
  // [XMM Gradients]
  // --------------------------------------------------------------------------

  // CONICAL GRADIENT:
  //
  // Polynomial to approximate `atan(x) * N / 2PI`:
  //   `x * (Q0 + x^2 * (Q1 + x^2 * (Q2 + x^2 * Q3)))`
  //
  // The following numbers were obtained by `lolremez` - minmax tool for approx.:
  //
  // Atan is an odd function, so we take advantage of it (see lolremez docs):
  //   1. E=|atan(x) * N / 2PI - P(x)                  | <- subs. `P(x)` by `x*Q(x^2))`
  //   2. E=|atan(x) * N / 2PI - x*Q(x^2)              | <- subs. `x^2` by `y`
  //   3. E=|atan(sqrt(y)) * N / 2PI - sqrt(y) * Q(y)  | <- eliminate `y` from Q side - div by `y`
  //   4. E=|atan(sqrt(y)) * N / (2PI * sqrt(y)) - Q(y)|
  //
  // LolRemez C++ code:
  //   real f(real const& x) {
  //     real y = sqrt(x);
  //     return atan(y) * real(N) / (real(2) * real::R_PI * y);
  //   }
  //   real g(real const& x) {
  //     return re(sqrt(x));
  //   }
  //   int main(int argc, char **argv) {
  //     RemezSolver<3, real> solver;
  //     solver.Run("1e-1000", 1, f, g, 40);
  //     return 0;
  //   }
  {
    #define REC(N, Q0, Q1, Q2, Q3) {    \
      { FLOAT_4X(N  , N  , N  , N  ) }, \
      { FLOAT_4X(N/2, N/2, N/2, N/2) }, \
      { FLOAT_4X(N/4, N/4, N/4, N/4) }, \
      { FLOAT_4X(N/2, N  , N/2, N  ) }, \
      { FLOAT_4X(Q0 , Q0 , Q0 , Q0 ) }, \
      { FLOAT_4X(Q1 , Q1 , Q1 , Q1 ) }, \
      { FLOAT_4X(Q2 , Q2 , Q2 , Q2 ) }, \
      { FLOAT_4X(Q3 , Q3 , Q3 , Q3 ) }  \
    }
    REC(256 , 4.071421038552e+1, -1.311160794048e+1, 6.017670215625   , -1.623253505085   ),
    REC(512 , 8.142842077104e+1, -2.622321588095e+1, 1.203534043125e+1, -3.246507010170   ),
    REC(1024, 1.628568415421e+2, -5.244643176191e+1, 2.407068086250e+1, -6.493014020340   ),
    REC(2048, 3.257136830841e+2, -1.048928635238e+2, 4.814136172500e+1, -1.298602804068e+1),
    REC(4096, 6.514273661683e+2, -2.097857270476e+2, 9.628272344999e+1, -2.597205608136e+1)
    #undef REC
  },

  // --------------------------------------------------------------------------
  // [Div24Bit]
  // --------------------------------------------------------------------------

  blLookupTable<uint32_t, 256, BLDiv24BitReciprocalGen>()
};

// NOTE: We must go through `blCommonTable_` as it's the only way to convince
// MSVC to emit constexpr. If this step is missing it will emit initialization
// code for this const data, which is exactly what we don't want. Also, we cannot
// just add `constexpr` to the real `blCommonTable` as MSVC would complain about
// different storage type.
const BLCommonTable blCommonTable = blCommonTable_;
