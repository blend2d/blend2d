// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "./api-build_p.h"
#include "./tables_p.h"

#ifdef BL_TEST
#include "./math_p.h"
#include "./support_p.h"
#endif

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
  { REPEAT_2X(0x0101000001010000u) },
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

  #if BL_TARGET_ARCH_X86
  #define Z 0x80 // PSHUFB zeroing.

  { 0 , 4 , 8 , 12, 0 , 4 , 8 , 12, 0 , 4 , 8 , 12, 0 , 4 , 8 , 12 }, // i128_pshufb_u32_to_u8_lo
  { 0 , 1 , 4 , 5 , 8 , 9 , 12, 13, 0 , 1 , 4 , 5 , 8 , 9 , 12, 13 }, // i128_pshufb_u32_to_u16_lo

  { 3 , 7 , 11, 15, Z , Z , Z , Z , Z , Z , Z , Z , Z , Z , Z , Z  }, // i128_pshufb_argb32_to_a8_packed
  { 3 , Z , 3 , Z , 3 , Z , 3 , Z , 7 , Z , 7 , Z , 7 , Z , 7 , Z  }, // i128_pshufb_packed_argb32_2x_lo_to_unpacked_a8
  { 11, Z , 11, Z , 11, Z , 11, Z , 15, Z , 15, Z , 15, Z , 15, Z  }, // i128_pshufb_packed_argb32_2x_lo_to_unpacked_a8

  #undef Z
  #endif

  // --------------------------------------------------------------------------
  // [Alignment]
  // --------------------------------------------------------------------------

  // dummy to align 256-bit constants that follow.
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
  // [I256 Load/Store Masks]
  // --------------------------------------------------------------------------

  // m256_load_store_32
  {
    {{ 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u }},
    {{ 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u }},
    {{ 0xFFFFFFFFu, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u }},
    {{ 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u }},
    {{ 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u }},
    {{ 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u }},
    {{ 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x00000000u, 0x00000000u }},
    {{ 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x00000000u }},
    {{ 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu }}
  },

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
  // [Unpremultiply]
  // --------------------------------------------------------------------------

  // Could be also generated by:
  //
  // struct BLUnpremultiplyTableGen {
  //   //! Calculates the reciprocal for `BLCommonTable::unpremultiply` table.
  //   static constexpr uint32_t value(size_t i) noexcept {
  //     return i ? uint32_t(0xFF00FF / i) : uint32_t(0);
  //   }
  // };
  {
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
  },

  // Unpremultiply tables designed for 'PMADDWD'.
  #if BL_TARGET_ARCH_X86

  {
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
  },

  {
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
  },

  #endif

  // --------------------------------------------------------------------------
  // [Epilog]
  // --------------------------------------------------------------------------

  { 0 }
};

// NOTE: We must go through `blCommonTable_` as it's the only way to convince
// MSVC to emit constexpr. If this step is missing it will emit initialization
// code for this const data, which is exactly what we don't want. Also, we cannot
// just add `constexpr` to the real `blCommonTable` as MSVC would complain about
// different storage type.
const BLCommonTable blCommonTable = blCommonTable_;

// ============================================================================
// [BLCommonTable - Unit Tests]
// ============================================================================

#ifdef BL_TEST
static BL_INLINE uint32_t unpremultiplyAsFloatOp(uint32_t c, uint32_t a) noexcept {
  float cf = float(c);
  float af = blMax(float(a), 0.0001f);

  return uint32_t(blRoundToInt((cf / af) * 255.0f));
}

UNIT(tables, -10) {
  // Make sure that the 256-bit constants are properly aligned.
  INFO("Testing 'blCommonTable' alignment");
  EXPECT(blIsAligned(&blCommonTable, 32));
  EXPECT(blIsAligned(&blCommonTable.i256_007F007F007F007F, 32));

  INFO("Testing 'blCommonTable.unpremultiplyRcp' correctness");
  {
    for (uint32_t a = 0; a < 256; a++) {
      for (uint32_t c = 0; c <= a; c++) {
        uint32_t u0 = (c * blCommonTable.unpremultiplyRcp[a] + 0x8000u) >> 16;
        uint32_t u1 = unpremultiplyAsFloatOp(c, a);
        EXPECT(u0 == u1, "Value[0x%02X] != Expected[0x%02X] [C=%u, A=%u]", u0, u1, c, a);
      }
    }
  }

  #if BL_TARGET_ARCH_X86
  INFO("Testing 'blCommonTable.unpremultiplyPmaddwd[Rcp|Rnd]' correctness");
  {
    for (uint32_t a = 0; a < 256; a++) {
      for (uint32_t c = 0; c <= a; c++) {
        uint32_t c0 = c;
        uint32_t c1 = c << 6;

        uint32_t r0 = blCommonTable.unpremultiplyPmaddwdRcp[a] & 0xFFFF;
        uint32_t r1 = blCommonTable.unpremultiplyPmaddwdRcp[a] >> 16;
        uint32_t rnd = blCommonTable.unpremultiplyPmaddwdRnd[a];

        uint32_t u0 = (c0 * r0 + c1 * r1 + rnd) >> 13;
        uint32_t u1 = unpremultiplyAsFloatOp(c, a);

        EXPECT(u0 == u1, "Value[0x%02X] != Expected[0x%02X] [C=%u, A=%u]", u0, u1, c, a);
      }
    }
  }
  #endif
}
#endif
