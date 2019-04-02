// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLCOMPOP_P_H
#define BLEND2D_BLCOMPOP_P_H

#include "./blapi-internal_p.h"
#include "./blcontext.h"
#include "./blformat_p.h"
#include "./blpipe_p.h"
#include "./bltables_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [Constants]
// ============================================================================

//! Additional compositing operators used internally.
enum BLCompOpInternal : uint32_t {
  //! Set destination alpha to 1 (alpha formats only).
  BL_COMP_OP_INTERNAL_ALPHA_SET = BL_COMP_OP_COUNT,
  //! Invert destination alpha   (alpha formats only).
  BL_COMP_OP_INTERNAL_ALPHA_INV,
  //! Count of all compositing operators including internal ones..
  BL_COMP_OP_INTERNAL_COUNT
};

//! Simplification of a composition operator that leads to SOLID fill instead.
enum BLCompOpSolidId : uint32_t {
  //! Source pixels are used.
  //!
  //! NOTE: This value must be zero as it's usually combined with rendering
  //! context flags and then used for decision making about the whole command.
  BL_COMP_OP_SOLID_ID_NONE = 0,
  //! Source pixels are always treated as transparent zero (all 0).
  BL_COMP_OP_SOLID_ID_TRANSPARENT = 1,
  //! Source pixels are always treated as opaque black (R|G|B=0 A=1).
  BL_COMP_OP_SOLID_ID_OPAQUE_BLACK = 2,
  //! Source pixels are always treated as opaque white (R|G|B=1 A=1).
  BL_COMP_OP_SOLID_ID_OPAQUE_WHITE = 3
};

//! Composition operator flags that can be retrieved through BLCompOpInfo[] table.
enum BLCompOpFlags : uint32_t {
  //! TypeA operator - "D*(1-M) + Op(D, S)*M" == "Op(D, S * M)".
  BL_COMP_OP_FLAG_TYPE_A        = 0x00000001u,
  //! TypeB operator - "D*(1-M) + Op(D, S)*M" == "Op(D, S*M) + D*(1-M)".
  BL_COMP_OP_FLAG_TYPE_B        = 0x00000002u,
  //! TypeC operator - cannot be simplified.
  BL_COMP_OP_FLAG_TYPE_C        = 0x00000004u,

  //! Non-separable operator.
  BL_COMP_OP_FLAG_NON_SEPARABLE = 0x00000008u,

  //! Uses `Dc` (destination color or luminance channel).
  BL_COMP_OP_FLAG_DC            = 0x00000010u,
  //! Uses `Da` (destination alpha channel).
  BL_COMP_OP_FLAG_DA            = 0x00000020u,
  //! Uses both `Dc` and `Da`.
  BL_COMP_OP_FLAG_DC_DA         = 0x00000030u,

  //! Uses `Sc` (source color or luminance channel).
  BL_COMP_OP_FLAG_SC            = 0x00000040u,
  //! Uses `Sa` (source alpha channel).
  BL_COMP_OP_FLAG_SA            = 0x00000080u,
  //! Uses both `Sc` and `Sa`,
  BL_COMP_OP_FLAG_SC_SA         = 0x000000C0u,

  //! Destination is never changed (NOP).
  BL_COMP_OP_FLAG_NOP           = 0x00000800u,
  //! Destination is changed only if `Da != 0`.
  BL_COMP_OP_FLAG_NOP_IF_DA_0   = 0x00001000u,
  //! Destination is changed only if `Da != 1`.
  BL_COMP_OP_FLAG_NOP_IF_DA_1   = 0x00002000u,
  //! Destination is changed only if `Sa != 0`.
  BL_COMP_OP_FLAG_NOP_IF_SA_0   = 0x00004000u,
  //! Destination is changed only if `Sa != 1`.
  BL_COMP_OP_FLAG_NOP_IF_SA_1   = 0x00008000u
};

// ============================================================================
// [BLCompOpInfo]
// ============================================================================

//! Information about a composition operator.
struct BLCompOpInfo {
  uint32_t flags;
};

//! Provides flags for each composition operator.
BL_HIDDEN extern const BLLookupTable<BLCompOpInfo, BL_COMP_OP_INTERNAL_COUNT> blCompOpInfo;

// ============================================================================
// [BLCompOpSimplifyInfo]
// ============================================================================

//! Information that can be used to simplify a "Dst CompOp Src" into a simpler
//! composition operator with a possible format conversion and arbitrary source
//! to solid conversion. This is used by the rendering engine to simplify every
//! composition operator before it considers which pipeline to use.
//!
//! There are two reasons for simplification - the first is performance and the
//! second reason is to decrease the number of possible pipeline signatures the
//! rendering context may require. For example by using "SRC-COPY" operator
//! instead of "CLEAR" operator the rendering engine basically eliminated a
//! possible compilation of "CLEAR" operator that would perform exactly same as
//! "SRC-COPY".
struct BLCompOpSimplifyInfo {
  //! Alternative composition operator of the simplified operation.
  uint16_t altCompOp : 6;
  //! Source solid id, see `BLCompOpSolidId`.
  uint16_t srcSolidId : 2;
  //! Destination format of the simplified operation.
  uint16_t dstFormat : 4;
  //! Source format of the simplified operation.
  uint16_t srcFormat : 4;
};

enum : uint32_t {
  BL_COMP_OP_SIMPLIFY_INFO_SIZE = BL_FORMAT_COUNT *
                                  BL_COMP_OP_INTERNAL_COUNT *
                                  BL_FORMAT_RESERVED_COUNT
};

//! A lookup table that provides BLCompOpSimplifyInfo indexed as (DST_FMT x COMP_OP x SRC_FMT).
BL_HIDDEN extern const BLLookupTable<BLCompOpSimplifyInfo, BL_COMP_OP_SIMPLIFY_INFO_SIZE> blCompOpSimplifyInfoArray;

static BL_INLINE const BLCompOpSimplifyInfo* blCompOpSimplifyInfoArrayOf(uint32_t compOp, uint32_t dstFormat) noexcept {
  uint32_t index = (dstFormat * BL_COMP_OP_INTERNAL_COUNT + compOp) * BL_FORMAT_RESERVED_COUNT;
  return &blCompOpSimplifyInfoArray[index];
}

static BL_INLINE const BLCompOpSimplifyInfo& blCompOpSimplifyInfo(uint32_t compOp, uint32_t dstFormat, uint32_t srcFormat) noexcept {
  return blCompOpSimplifyInfoArrayOf(compOp, dstFormat)[srcFormat];
}

//! \}
//! \endcond

#endif // BLEND2D_BLCOMPOP_P_H
