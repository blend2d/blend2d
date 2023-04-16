// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_TABLES_P_H_INCLUDED
#define BLEND2D_TABLES_P_H_INCLUDED

#include "support/lookuptable_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! Table, which provides bit count for 8-bit quantities.
BL_HIDDEN extern const BLLookupTable<uint8_t, 256> blBitCountByteTable;

//! Table that contains precomputed `{1..16} % N`.
struct alignas(16) BLModuloTable {
  uint8_t x1_16[16];
};

BL_HIDDEN extern const BLModuloTable blModuloTable[18];

template<typename T>
struct alignas(8) BLVecConst64 {
  T data[8 / sizeof(T)];
};

template<typename T>
struct alignas(16) BLVecConst128 {
  T data[16 / sizeof(T)];

  template<typename VecType>
  BL_INLINE const VecType& as() const noexcept { return *reinterpret_cast<const VecType*>(data); }
};

template<typename T>
struct alignas(32) BLVecConst256 {
  T data[32 / sizeof(T)];

  template<typename VecType>
  BL_INLINE const VecType& as() const noexcept { return *reinterpret_cast<const VecType*>(data); }
};

#if BL_TARGET_ARCH_X86
template<typename T>
using BLVecConstNative = BLVecConst256<T>;
#else
template<typename T>
using BLVecConstNative = BLVecConst128<T>;
#endif

#define REPEAT_2X(...) __VA_ARGS__, __VA_ARGS__
#define REPEAT_4X(...) REPEAT_2X(__VA_ARGS__), REPEAT_2X(__VA_ARGS__)
#define REPEAT_8X(...) REPEAT_4X(__VA_ARGS__), REPEAT_4X(__VA_ARGS__)
#define REPEAT_16X(...) REPEAT_8X(__VA_ARGS__), REPEAT_8X(__VA_ARGS__)

#if BL_TARGET_ARCH_X86
#define REPEAT_32B(...) REPEAT_8X(__VA_ARGS__)
#define REPEAT_64B(...) REPEAT_4X(__VA_ARGS__)
#define REPEAT_128B(...) REPEAT_2X(__VA_ARGS__)
#else
#define REPEAT_32B(...) REPEAT_4X(__VA_ARGS__)
#define REPEAT_64B(...) REPEAT_2X(__VA_ARGS__)
#define REPEAT_128B(...) __VA_ARGS__
#endif

//! Common table that contains constants used across Blend2D library, but most importantly in pipelines (either static
//! or dynamic. The advantage of this table is that it contains all constants that SIMD code (or also a generic code)
//! requires so only one register (pointer) is required to address all of them in either static or generated pipelines.
struct alignas(32) BLCommonTable {
  //! \name 128-bit Constants
  //!
  //! These constants are only used by 128-bit SIMD code paths and are limited to 256 bytes as if the displacement
  //! is greater than -128+127 range it's encoded with 4-byte displacement anyway in X86/X64 assembly.
  //! \{

  BLVecConst128<uint64_t> i128_0000000000000000 {{ REPEAT_2X(0x0000000000000000u) }};
  BLVecConst128<uint64_t> i128_0080008000800080 {{ REPEAT_2X(0x0080008000800080u) }};
  BLVecConst128<uint64_t> i128_0101010101010101 {{ REPEAT_2X(0x0101010101010101u) }};
  BLVecConst128<uint64_t> i128_FF000000FF000000 {{ REPEAT_2X(0xFF000000FF000000u) }};

  //! \}

  //! \name 128-bit and 256-bit Constants
  //!
  //! These constants are shared between 128-bit and 256-bit code paths.

  BLVecConstNative<uint64_t> i_0000000000000000 {{ REPEAT_64B(0x0000000000000000u) }};
  BLVecConstNative<uint64_t> i_3030303030303030 {{ REPEAT_64B(0x3030303030303030u) }};
  BLVecConstNative<uint64_t> i_0F0F0F0F0F0F0F0F {{ REPEAT_64B(0x0F0F0F0F0F0F0F0Fu) }};
  BLVecConstNative<uint64_t> i_8080808080808080 {{ REPEAT_64B(0x8080808080808080u) }};
  BLVecConstNative<uint64_t> i_FFFFFFFFFFFFFFFF {{ REPEAT_64B(0xFFFFFFFFFFFFFFFFu) }};

  BLVecConstNative<uint64_t> i_007F007F007F007F {{ REPEAT_64B(0x007F007F007F007Fu) }};
  BLVecConstNative<uint64_t> i_0080008000800080 {{ REPEAT_64B(0x0080008000800080u) }};
  BLVecConstNative<uint64_t> i_00FF00FF00FF00FF {{ REPEAT_64B(0x00FF00FF00FF00FFu) }};
  BLVecConstNative<uint64_t> i_0100010001000100 {{ REPEAT_64B(0x0100010001000100u) }};
  BLVecConstNative<uint64_t> i_0101010101010101 {{ REPEAT_64B(0x0101010101010101u) }};
  BLVecConstNative<uint64_t> i_01FF01FF01FF01FF {{ REPEAT_64B(0x01FF01FF01FF01FFu) }};
  BLVecConstNative<uint64_t> i_0200020002000200 {{ REPEAT_64B(0x0200020002000200u) }};
  BLVecConstNative<uint64_t> i_8000800080008000 {{ REPEAT_64B(0x8000800080008000u) }};

  BLVecConstNative<uint64_t> i_000000FF000000FF {{ REPEAT_64B(0x000000FF000000FFu) }};
  BLVecConstNative<uint64_t> i_0000010000000100 {{ REPEAT_64B(0x0000010000000100u) }};
  BLVecConstNative<uint64_t> i_000001FF000001FF {{ REPEAT_64B(0x000001FF000001FFu) }};
  BLVecConstNative<uint64_t> i_0000020000000200 {{ REPEAT_64B(0x0000020000000200u) }};
  BLVecConstNative<uint64_t> i_0000FFFF0000FFFF {{ REPEAT_64B(0x0000FFFF0000FFFFu) }};
  BLVecConstNative<uint64_t> i_0002000000020000 {{ REPEAT_64B(0x0002000000020000u) }}; // 256 << 9
  BLVecConstNative<uint64_t> i_00FFFFFF00FFFFFF {{ REPEAT_64B(0x00FFFFFF00FFFFFFu) }};
  BLVecConstNative<uint64_t> i_0101000001010000 {{ REPEAT_64B(0x0101000001010000u) }};
  BLVecConstNative<uint64_t> i_FF000000FF000000 {{ REPEAT_64B(0xFF000000FF000000u) }};
  BLVecConstNative<uint64_t> i_FFFF0000FFFF0000 {{ REPEAT_64B(0xFFFF0000FFFF0000u) }};

  BLVecConstNative<uint64_t> i_000000FF00FF00FF {{ REPEAT_64B(0x000000FF00FF00FFu) }};
  BLVecConstNative<uint64_t> i_0000010001000100 {{ REPEAT_64B(0x0000010001000100u) }};
  BLVecConstNative<uint64_t> i_0000080000000800 {{ REPEAT_64B(0x0000080000000800u) }};
  BLVecConstNative<uint64_t> i_0000800000008000 {{ REPEAT_64B(0x0000800000008000u) }};
  BLVecConstNative<uint64_t> i_0000FFFFFFFFFFFF {{ REPEAT_64B(0x0000FFFFFFFFFFFFu) }};
  BLVecConstNative<uint64_t> i_00FF000000000000 {{ REPEAT_64B(0x00FF000000000000u) }};
  BLVecConstNative<uint64_t> i_0100000000000000 {{ REPEAT_64B(0x0100000000000000u) }};
  BLVecConstNative<uint64_t> i_0101010100000000 {{ REPEAT_64B(0x0101010100000000u) }};
  BLVecConstNative<uint64_t> i_FFFF000000000000 {{ REPEAT_64B(0xFFFF000000000000u) }};
  BLVecConstNative<uint64_t> i_FFFFFFFF00000000 {{ REPEAT_64B(0xFFFFFFFF00000000u) }};

  BLVecConstNative<uint32_t> i_FFFFFFFF_FFFFFFFF_FFFFFFFF_0 {{ REPEAT_128B(0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0) }};

  BLVecConstNative<uint32_t> u32_0_1_2_3 {{ REPEAT_128B(0, 1, 2, 3) }};
  BLVecConstNative<uint32_t> u32_4_4_4_4 {{ REPEAT_128B(4, 4, 4, 4) }};

  // Mask of all `float` bits containing a sign.
  BLVecConstNative<uint32_t> f32_sgn {{ REPEAT_32B(0x80000000u) }};
  // Mask of all `float` bits without a sign.
  BLVecConstNative<uint32_t> f32_abs {{ REPEAT_32B(0x7FFFFFFFu) }};
  // Mask of all LO `float` bits without a sign.
  BLVecConstNative<uint32_t> f32_abs_lo {{ REPEAT_128B(0x7FFFFFFFu, 0xFFFFFFFFu, 0x7FFFFFFFu, 0xFFFFFFFFu) }};
  // Mask of all HI `float` bits without a sign.
  BLVecConstNative<uint32_t> f32_abs_hi {{ REPEAT_128B(0xFFFFFFFFu, 0x7FFFFFFFu, 0xFFFFFFFFu, 0x7FFFFFFFu) }};
  // Maximum float value to round (8388608).
  BLVecConstNative<float> f32_round_max {{ REPEAT_32B(8388608.0f) }};
  // Magic float used by round (12582912).
  BLVecConstNative<float> f32_round_magic {{ REPEAT_32B(12582912.0f) }};

  // Vector of `1.0f`.
  BLVecConstNative<float> f32_1 {{ REPEAT_32B(1.0f) }};
  // Vector of `4.0f`.
  BLVecConstNative<float> f32_4 {{ REPEAT_32B(4.0f) }};
  // Vector of `255.0f`.
  BLVecConstNative<float> f32_255 {{ REPEAT_32B(255.0f) }};
  // Vector of `1e-3`.
  BLVecConstNative<float> f32_1e_m3 {{ REPEAT_32B(1e-3f) }};
  // Vector of `1e-20`.
  BLVecConstNative<float> f32_1e_m20 {{ REPEAT_32B(1e-20f) }};
  // Vector of `1.0f / 255.0f`.
  BLVecConstNative<float> f32_1div255 {{ REPEAT_32B(1.0f / 255.0f) }};
  // Vector of `[3f, 2f, 1f, 0f]`.
  BLVecConstNative<float> f32_0_1_2_3 {{ REPEAT_128B(0.0f, 1.0f, 2.0f, 3.0f) }};

  // Mask of all `double` bits containing a sign.
  BLVecConstNative<uint64_t> f64_sgn {{ REPEAT_64B(0x8000000000000000u) }};
  // Mask of all `double` bits without a sign.
  BLVecConstNative<uint64_t> f64_abs {{ REPEAT_64B(0x7FFFFFFFFFFFFFFFu) }};
  // Mask of LO `double` bits without a sign.
  BLVecConstNative<uint64_t> f64_abs_lo {{ REPEAT_128B(0x7FFFFFFFFFFFFFFFu, 0xFFFFFFFFFFFFFFFFu) }};
  // Mask of HI `double` bits without a sign.
  BLVecConstNative<uint64_t> f64_abs_hi {{ REPEAT_128B(0xFFFFFFFFFFFFFFFFu, 0x7FFFFFFFFFFFFFFFu) }};
  // Maximum double value to round (4503599627370496).
  BLVecConstNative<double> f64_round_max {{ REPEAT_64B(4503599627370496.0) }};
  // Magic double used by round (6755399441055744).
  BLVecConstNative<double> f64_round_magic {{ REPEAT_64B(6755399441055744.0) }};

  // Vector of `1.0`.
  BLVecConstNative<double> f64_1 {{ REPEAT_64B(1.0) }};
  // Vector of `1e-20`.
  BLVecConstNative<double> f64_1e_m20 {{ REPEAT_64B(1e-20) }};
  // Vector of `4.0`.
  BLVecConstNative<double> f64_4 {{ REPEAT_64B(4.0) }};
  // Vector of `-1.0`.
  BLVecConstNative<double> f64_m1 {{ REPEAT_64B(-1.0) }};

  //! \}

  //! \name 128-bit and 256-bit [V]PSHUFB Predicates (X86 specific)
  //! \{

#if BL_TARGET_ARCH_X86

  BLVecConstNative<uint64_t> pshufb_xxxxxxxxxxxx3210_to_3333222211110000 {{ REPEAT_128B(0x0101010100000000u, 0x0303030302020202u) }};
  BLVecConstNative<uint64_t> pshufb_xxxxxxxx1xxx0xxx_to_z1z1z1z1z0z0z0z0 {{ REPEAT_128B(0xFF03FF03FF03FF03u, 0xFF07FF07FF07FF07u) }};
  BLVecConstNative<uint64_t> pshufb_xxxxxxx1xxxxxxx0_to_zzzzzzzz11110000 {{ REPEAT_128B(0x0808080800000000u, 0xFFFFFFFFFFFFFFFFu) }};
  BLVecConstNative<uint64_t> pshufb_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0 {{ REPEAT_128B(0xFF00FF00FF00FF00u, 0xFF08FF08FF08FF08u) }};
  BLVecConstNative<uint64_t> pshufb_xxx3xxx2xxx1xxx0_to_3210321032103210 {{ REPEAT_128B(0x0C0804000C080400u, 0x0C0804000C080400u) }};
  BLVecConstNative<uint64_t> pshufb_xxx3xxx2xxx1xxx0_to_3333222211110000 {{ REPEAT_128B(0x0404040400000000u, 0x0C0C0C0C08080808u) }};
  BLVecConstNative<uint64_t> pshufb_xx76xx54xx32xx10_to_7654321076543210 {{ REPEAT_128B(0x0D0C090805040100u, 0x0D0C090805040100u) }};
  BLVecConstNative<uint64_t> pshufb_1xxx0xxxxxxxxxxx_to_z1z1z1z1z0z0z0z0 {{ REPEAT_128B(0xFF0BFF0BFF0BFF0Bu, 0xFF0FFF0FFF0FFF0Fu) }};
  BLVecConstNative<uint64_t> pshufb_3xxx2xxx1xxx0xxx_to_zzzzzzzzzzzz3210 {{ REPEAT_128B(0xFFFFFFFFFFFFFFFFu, 0xFFFFFFFF0F0B0703u) }};
  BLVecConstNative<uint64_t> pshufb_32xxxxxx10xxxxxx_to_3232323210101010 {{ REPEAT_128B(0x0706070607060706u, 0x0F0E0F0E0F0E0F0Eu) }};
  BLVecConstNative<uint64_t> pshufb_76543210xxxxxxxx_to_z7z6z5z4z3z2z1z0 {{ REPEAT_128B(0xFF0BFF0AFF09FF08u, 0xFF0FFF0EFF0DFF0Cu) }};

#endif

  //! \}

  //! \name Load / Store Masks for VPMASKMOV Instruction (X86 specific)
  //! \{

#if BL_TARGET_ARCH_X86

  // 16-bit masks for use with AVX-512 {k} registers.
  //
  // NOTE: It's better to calculate the mask if the number of elements is greater than 16.

  uint16_t k_msk16_data[16 + 16 + 16 + 16 + 1] = {
    0x0000u,
    0x0001u, 0x0003u, 0x0007u, 0x000Fu,
    0x001Fu, 0x003Fu, 0x007Fu, 0x00FFu,
    0x01FFu, 0x03FFu, 0x07FFu, 0x0FFFu,
    0x1FFFu, 0x3FFFu, 0x7FFFu, 0xFFFFu,
    0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu,
    0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu,
    0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu,
    0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu,
    0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu,
    0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu,
    0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu,
    0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu,
    0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu,
    0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu,
    0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu,
    0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu
  };

  // NOTE: Use VPMOVSXBD to extend BYTEs to DWORDs or VPMOVSXBQ to extend BYTEs to QWORDs to
  // extend the mask into the correct data size. VPMOVSX?? is not an expensive instruction.

  BLVecConst64<uint64_t> loadstore_msk8_data[32 + 8 + 32 + 1] = {
    {{ 0x0000000000000000u }}, // [0]
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }}, // [8]
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }}, // [16]
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }},
    {{ 0x0000000000000000u }}, // [24]
    {{ 0x0000000000000000u }}, //
    {{ 0x0000000000000000u }}, //
    {{ 0x0000000000000000u }}, //
    {{ 0x0000000000000000u }}, //
    {{ 0x0000000000000000u }}, //
    {{ 0x0000000000000000u }}, //
    {{ 0x0000000000000000u }}, //
    {{ 0x0000000000000000u }}, // [32]
    {{ 0x00000000000000FFu }}, //
    {{ 0x000000000000FFFFu }}, //
    {{ 0x0000000000FFFFFFu }}, //
    {{ 0x00000000FFFFFFFFu }}, //
    {{ 0x000000FFFFFFFFFFu }}, //
    {{ 0x0000FFFFFFFFFFFFu }}, //
    {{ 0x00FFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, // [40]
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, // [48]
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, // [56]
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, // [64]
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}, //
    {{ 0xFFFFFFFFFFFFFFFFu }}  // [72]
  };

  BL_INLINE const BLVecConst64<uint64_t>* loadstore16_lo8_msk8() const noexcept { return loadstore_msk8_data + 32; }
  BL_INLINE const BLVecConst64<uint64_t>* loadstore16_hi8_msk8() const noexcept { return loadstore_msk8_data + 24; }

#endif

  //! \}

  //! \name Conical Gradient Tables
  //! \{

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
  // ```
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
  // ```
  enum TableId {
    kTable256   = 0,
    kTable512   = 1,
    kTable1024  = 2,
    kTable2048  = 3,
    kTable4096  = 4,
    kTableCount = 5
  };

  struct Conical {
    float n_div_1[4];
    float n_div_2[4];
    float n_div_4[4];
    float n_extra[4];

    // Polynomial to approximate `atan(x) * N / 2PI`:
    //   `x * (Q0 + x*x * (Q1 + x*x * (Q2 + x*x * Q3)))`
    // Where:
    //   `x >= 0 && x <= 1`
    float q0[4];
    float q1[4];
    float q2[4];
    float q3[4];
  } xmm_f_con[kTableCount] = {
    #define ROW(N, Q0, Q1, Q2, Q3) {    \
      { float(N  ), float(N  ), float(N  ), float(N  ) }, \
      { float(N/2), float(N/2), float(N/2), float(N/2) }, \
      { float(N/4), float(N/4), float(N/4), float(N/4) }, \
      { float(N/2), float(N  ), float(N/2), float(N  ) }, \
      { float(Q0 ), float(Q0 ), float(Q0 ), float(Q0 ) }, \
      { float(Q1 ), float(Q1 ), float(Q1 ), float(Q1 ) }, \
      { float(Q2 ), float(Q2 ), float(Q2 ), float(Q2 ) }, \
      { float(Q3 ), float(Q3 ), float(Q3 ), float(Q3 ) }  \
    }
    ROW(256 , 4.071421038552e+1, -1.311160794048e+1, 6.017670215625   , -1.623253505085   ),
    ROW(512 , 8.142842077104e+1, -2.622321588095e+1, 1.203534043125e+1, -3.246507010170   ),
    ROW(1024, 1.628568415421e+2, -5.244643176191e+1, 2.407068086250e+1, -6.493014020340   ),
    ROW(2048, 3.257136830841e+2, -1.048928635238e+2, 4.814136172500e+1, -1.298602804068e+1),
    ROW(4096, 6.514273661683e+2, -2.097857270476e+2, 9.628272344999e+1, -2.597205608136e+1)
    #undef ROW
  };

  //! \}

  //! \name Unpremultiply Tables
  //! \{

  //! Table, which can be used to turn integer division into multiplication and shift that is used by PRGB to ARGB
  //! (unpremultiply) pixel conversion. It supports division by 0 (multiplies by zero) up to 255 using 24 bits of
  //! precision. The multiplied product has to be shifted to the right by 16 bits to receive the final result.
  //!
  //! The unpremultiply function:
  //!
  //!   `if (b) ? (a * 255) / b : 0`
  //!
  //! can be rewritten as
  //!
  //!   `(a * unpremultiplyRcp[b] + 0x8000u) >> 16`
  uint32_t unpremultiplyRcp[256] = {
    // Could be also generated by:
    //
    // struct BLUnpremultiplyTableGen {
    //   //! Calculates the reciprocal for `BLCommonTable::unpremultiply` table.
    //   static constexpr uint32_t value(size_t i) noexcept {
    //     return i ? uint32_t(0xFF00FF / i) : uint32_t(0);
    //   }
    // };
    0x00000000u, 0x00FF00FFu, 0x007F807Fu, 0x00550055u, 0x003FC03Fu, 0x00330033u, 0x002A802Au, 0x00246DDBu,
    0x001FE01Fu, 0x001C5571u, 0x00198019u, 0x00172EA2u, 0x00154015u, 0x00139D9Du, 0x001236EDu, 0x00110011u,
    0x000FF00Fu, 0x000F000Fu, 0x000E2AB8u, 0x000D6BD7u, 0x000CC00Cu, 0x000C249Eu, 0x000B9751u, 0x000B164Du,
    0x000AA00Au, 0x000A333Du, 0x0009CECEu, 0x000971D0u, 0x00091B76u, 0x0008CB11u, 0x00088008u, 0x000839D6u,
    0x0007F807u, 0x0007BA36u, 0x00078007u, 0x0007492Bu, 0x0007155Cu, 0x0006E459u, 0x0006B5EBu, 0x000689DFu,
    0x00066006u, 0x00063838u, 0x0006124Fu, 0x0005EE29u, 0x0005CBA8u, 0x0005AAB0u, 0x00058B26u, 0x00056CF5u,
    0x00055005u, 0x00053443u, 0x0005199Eu, 0x00050005u, 0x0004E767u, 0x0004CFB7u, 0x0004B8E8u, 0x0004A2EDu,
    0x00048DBBu, 0x00047947u, 0x00046588u, 0x00045275u, 0x00044004u, 0x00042E2Eu, 0x00041CEBu, 0x00040C34u,
    0x0003FC03u, 0x0003EC52u, 0x0003DD1Bu, 0x0003CE57u, 0x0003C003u, 0x0003B219u, 0x0003A495u, 0x00039773u,
    0x00038AAEu, 0x00037E42u, 0x0003722Cu, 0x00036669u, 0x00035AF5u, 0x00034FCEu, 0x000344EFu, 0x00033A57u,
    0x00033003u, 0x000325F0u, 0x00031C1Cu, 0x00031284u, 0x00030927u, 0x00030003u, 0x0002F714u, 0x0002EE5Bu,
    0x0002E5D4u, 0x0002DD7Eu, 0x0002D558u, 0x0002CD5Fu, 0x0002C593u, 0x0002BDF2u, 0x0002B67Au, 0x0002AF2Bu,
    0x0002A802u, 0x0002A0FFu, 0x00029A21u, 0x00029367u, 0x00028CCFu, 0x00028658u, 0x00028002u, 0x000279CBu,
    0x000273B3u, 0x00026DB9u, 0x000267DBu, 0x0002621Au, 0x00025C74u, 0x000256E8u, 0x00025176u, 0x00024C1Du,
    0x000246DDu, 0x000241B5u, 0x00023CA3u, 0x000237A9u, 0x000232C4u, 0x00022DF5u, 0x0002293Au, 0x00022494u,
    0x00022002u, 0x00021B83u, 0x00021717u, 0x000212BDu, 0x00020E75u, 0x00020A3Fu, 0x0002061Au, 0x00020206u,
    0x0001FE01u, 0x0001FA0Du, 0x0001F629u, 0x0001F254u, 0x0001EE8Du, 0x0001EAD5u, 0x0001E72Bu, 0x0001E390u,
    0x0001E001u, 0x0001DC80u, 0x0001D90Cu, 0x0001D5A5u, 0x0001D24Au, 0x0001CEFCu, 0x0001CBB9u, 0x0001C882u,
    0x0001C557u, 0x0001C236u, 0x0001BF21u, 0x0001BC16u, 0x0001B916u, 0x0001B620u, 0x0001B334u, 0x0001B053u,
    0x0001AD7Au, 0x0001AAACu, 0x0001A7E7u, 0x0001A52Au, 0x0001A277u, 0x00019FCDu, 0x00019D2Bu, 0x00019A92u,
    0x00019801u, 0x00019578u, 0x000192F8u, 0x0001907Fu, 0x00018E0Eu, 0x00018BA4u, 0x00018942u, 0x000186E7u,
    0x00018493u, 0x00018247u, 0x00018001u, 0x00017DC2u, 0x00017B8Au, 0x00017958u, 0x0001772Du, 0x00017508u,
    0x000172EAu, 0x000170D1u, 0x00016EBFu, 0x00016CB2u, 0x00016AACu, 0x000168ABu, 0x000166AFu, 0x000164BAu,
    0x000162C9u, 0x000160DEu, 0x00015EF9u, 0x00015D18u, 0x00015B3Du, 0x00015966u, 0x00015795u, 0x000155C9u,
    0x00015401u, 0x0001523Eu, 0x0001507Fu, 0x00014EC6u, 0x00014D10u, 0x00014B60u, 0x000149B3u, 0x0001480Bu,
    0x00014667u, 0x000144C7u, 0x0001432Cu, 0x00014194u, 0x00014001u, 0x00013E71u, 0x00013CE5u, 0x00013B5Du,
    0x000139D9u, 0x00013859u, 0x000136DCu, 0x00013563u, 0x000133EDu, 0x0001327Bu, 0x0001310Du, 0x00012FA1u,
    0x00012E3Au, 0x00012CD5u, 0x00012B74u, 0x00012A16u, 0x000128BBu, 0x00012763u, 0x0001260Eu, 0x000124BDu,
    0x0001236Eu, 0x00012223u, 0x000120DAu, 0x00011F94u, 0x00011E51u, 0x00011D11u, 0x00011BD4u, 0x00011A9Au,
    0x00011962u, 0x0001182Du, 0x000116FAu, 0x000115CAu, 0x0001149Du, 0x00011372u, 0x0001124Au, 0x00011124u,
    0x00011001u, 0x00010EE0u, 0x00010DC1u, 0x00010CA5u, 0x00010B8Bu, 0x00010A73u, 0x0001095Eu, 0x0001084Bu,
    0x0001073Au, 0x0001062Cu, 0x0001051Fu, 0x00010415u, 0x0001030Du, 0x00010207u, 0x00010103u, 0x00010001u
  };

#if BL_TARGET_ARCH_X86

  // These tables are designed to take advantage of PMADDWD instruction (or its VPMADDWD AVX variant) so we only use
  // them on X86 targets. The approach with `unpremultiplyRcp[]` table is not possible in baseline SSE2 and it's not
  // so good even on SSE4.1 capable hardware that has PMULLD instruction, which has double the latency compared to
  // other multiplication instructions including PMADDWD.
  //
  // The downside of this approach is that we need two extra tables, as the multiplication is not precise enough.
  // The first table `unpremultiplyPmaddwdRcp` is used with PMADDWD instruction, and the second table is used to
  // round the result correctly.
  uint32_t unpremultiplyPmaddwdRcp[256] = {
    0x00000000u, 0x7E0067D0u, 0x3E0077D0u, 0x2A002555u, 0x1E007AAAu, 0x18006333u, 0x140052AAu, 0x12000FFFu,
    0x0E007CCCu, 0x0E000B6Du, 0x0C003199u, 0x0A0065FFu, 0x0A0028E3u, 0x080073FFu, 0x08004745u, 0x08002111u,
    0x06007E38u, 0x060060F0u, 0x060045B6u, 0x06002D89u, 0x0600186Bu, 0x060004B4u, 0x040072FFu, 0x040062D2u,
    0x0400542Cu, 0x0400468Bu, 0x040039FFu, 0x04002E50u, 0x0400237Au, 0x04001969u, 0x04001088u, 0x04000745u,
    0x02007F0Fu, 0x02007755u, 0x02007078u, 0x02006936u, 0x020062C2u, 0x02005C92u, 0x020056C4u, 0x02005143u,
    0x02004C1Fu, 0x0200470Au, 0x0200425Au, 0x02003DC7u, 0x0200397Bu, 0x02003574u, 0x02003169u, 0x02002DA1u,
    0x02002A0Bu, 0x0200268Bu, 0x02002345u, 0x02002050u, 0x02001CEFu, 0x020019FFu, 0x02001728u, 0x02001464u,
    0x020011BAu, 0x02000F2Du, 0x02000CB4u, 0x02000A4Fu, 0x02000823u, 0x020005C7u, 0x020003A2u, 0x02000189u,
    0x00007F83u, 0x00007D8Fu, 0x00007BAAu, 0x000079CBu, 0x0000781Eu, 0x00007646u, 0x0000749Bu, 0x000072EEu,
    0x0000715Au, 0x00006FFFu, 0x00006E49u, 0x00006CD8u, 0x00006B62u, 0x000069FFu, 0x000068A1u, 0x0000674Bu,
    0x00006606u, 0x000064BFu, 0x00006385u, 0x00006250u, 0x00006128u, 0x00006030u, 0x00005EE3u, 0x00005DCCu,
    0x00005CBDu, 0x00005BB1u, 0x00005ABAu, 0x000059ACu, 0x000058B4u, 0x000057BFu, 0x000056D0u, 0x000055E7u,
    0x00005503u, 0x00005421u, 0x00005345u, 0x0000526Eu, 0x0000519Du, 0x000050CCu, 0x00005028u, 0x00004F39u,
    0x00004E77u, 0x00004DBCu, 0x00004CFCu, 0x00004C44u, 0x00004B90u, 0x00004ADDu, 0x00004A32u, 0x00004984u,
    0x000048DDu, 0x00004837u, 0x00004796u, 0x000046F6u, 0x00004659u, 0x000045BFu, 0x00004529u, 0x00004497u,
    0x00004408u, 0x00004370u, 0x000042E3u, 0x00004258u, 0x000041CFu, 0x00004149u, 0x000040C4u, 0x00004041u,
    0x00003FC0u, 0x00003F42u, 0x00003EC7u, 0x00003E4Au, 0x00003DD2u, 0x00003D5Au, 0x00003CE2u, 0x00003C75u,
    0x00003C07u, 0x00003B90u, 0x00003B23u, 0x00003AB4u, 0x00003A4Bu, 0x000039DFu, 0x00003977u, 0x00003911u,
    0x000038ACu, 0x00003847u, 0x000037E5u, 0x00003783u, 0x00003723u, 0x000036C4u, 0x0000366Cu, 0x0000360Au,
    0x000035B0u, 0x0000355Eu, 0x000034FDu, 0x000034A6u, 0x0000344Fu, 0x000033FFu, 0x000033A6u, 0x00003352u,
    0x00003301u, 0x000032AFu, 0x0000325Fu, 0x00003210u, 0x000031C2u, 0x00003176u, 0x00003128u, 0x000030DDu,
    0x00003093u, 0x00003049u, 0x00003018u, 0x00002FB8u, 0x00002F72u, 0x00002F2Bu, 0x00002EE6u, 0x00002EA1u,
    0x00002E5Eu, 0x00002E1Au, 0x00002DD8u, 0x00002D96u, 0x00002D59u, 0x00002D17u, 0x00002CD6u, 0x00002C97u,
    0x00002C59u, 0x00002C1Cu, 0x00002BDFu, 0x00002BA4u, 0x00002B68u, 0x00002B2Du, 0x00002AF3u, 0x00002AB9u,
    0x00002A80u, 0x00002A49u, 0x00002A10u, 0x000029DAu, 0x000029A2u, 0x0000296Cu, 0x00002937u, 0x00002901u,
    0x000028CEu, 0x00002899u, 0x00002866u, 0x00002832u, 0x0000280Au, 0x000027CEu, 0x0000279Eu, 0x0000276Cu,
    0x0000273Cu, 0x0000270Bu, 0x000026DEu, 0x000026ACu, 0x0000267Eu, 0x0000264Fu, 0x00002622u, 0x000025F4u,
    0x000025C7u, 0x0000259Au, 0x0000256Fu, 0x00002543u, 0x00002518u, 0x000024EDu, 0x000024C2u, 0x0000249Au,
    0x0000246Eu, 0x00002445u, 0x0000241Cu, 0x000023F4u, 0x000023CAu, 0x000023A4u, 0x0000237Bu, 0x00002353u,
    0x00002333u, 0x00002306u, 0x000022DFu, 0x000022B9u, 0x00002294u, 0x0000226Eu, 0x0000224Bu, 0x00002227u,
    0x00002202u, 0x000021DCu, 0x000021B8u, 0x00002195u, 0x00002174u, 0x0000214Fu, 0x0000212Cu, 0x0000210Au,
    0x000020E7u, 0x000020C5u, 0x000020A4u, 0x00002083u, 0x00002062u, 0x00002041u, 0x00002040u, 0x00002010u
  };

  uint32_t unpremultiplyPmaddwdRnd[256] = {
    0x0000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x0F8Du, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1100u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x0FE2u, 0x1000u, 0x1000u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x0FC6u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1008u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x0FB4u, 0x1000u, 0x1000u, 0x1000u,
    0x0FB0u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u,
    0x1004u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u,
    0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x1000u,
    0x1000u, 0x1000u, 0x0FC2u, 0x1000u, 0x1000u, 0x1000u, 0x0F6Eu, 0x0FB4u,
    0x0FA0u, 0x1000u, 0x1000u, 0x1000u, 0x0FD4u, 0x1000u, 0x1000u, 0x1000u,
    0x1014u, 0x1000u, 0x0FBDu, 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x0F00u,
    0x0FE0u, 0x1000u, 0x0FA4u, 0x0F3Cu, 0x1014u, 0x0F24u, 0x1000u, 0x1000u,
    0x0CE4u, 0x0FA6u, 0x1023u, 0x1000u, 0x0FC8u, 0x1000u, 0x1000u, 0x0EC8u,
    0x1000u, 0x1000u, 0x1008u, 0x0FADu, 0x0EB8u, 0x0F95u, 0x1000u, 0x0F92u,
    0x101Cu, 0x1019u, 0x1000u, 0x0FA9u, 0x0FECu, 0x1000u, 0x0040u, 0x1000u
  };

#endif

  //! \}

  //! \name Epilog
  //! \{

  //! Dummy constant to have something at the end.
  uint8_t epilog[32] {};

  //! \}
};

#undef REPEAT_128B
#undef REPEAT_64B
#undef REPEAT_32B

#undef REPEAT_16X
#undef REPEAT_8X
#undef REPEAT_4X
#undef REPEAT_2X

BL_HIDDEN extern const BLCommonTable blCommonTable;

//! \}
//! \endcond

#endif // BLEND2D_TABLES_P_H_INCLUDED
