// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_COMPOP_P_H
#define BLEND2D_COMPOP_P_H

#include "./api-internal_p.h"
#include "./context.h"
#include "./format_p.h"
#include "./pipedefs_p.h"
#include "./tables_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [Constants]
// ============================================================================

//! Additional composition operators used internally.
enum BLCompOpInternal : uint32_t {
  //! Invert destination alpha (alpha formats only).
  BL_COMP_OP_INTERNAL_ALPHA_INV = BL_COMP_OP_COUNT,
  //! Count of all composition operators including internal ones..
  BL_COMP_OP_INTERNAL_COUNT
};

//! Simplification of a composition operator that leads to SOLID fill instead.
enum BLCompOpSolidId : uint32_t {
  //! Source pixels are used.
  //!
  //! \note This value must be zero as it's usually combined with rendering
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
//! second reason is about decreasing the number of possible pipeline signatures
//! the rendering context may require. For example by using "SRC-COPY" operator
//! instead of "CLEAR" operator the rendering engine basically eliminated a
//! possible compilation of "CLEAR" operator that would perform exactly the same
//! as "SRC-COPY".
struct BLCompOpSimplifyInfo {
  //! Alternative composition operator, destination format, source format, and
  //! solid-id information packed into 16 bits.
  uint16_t data;

  // Pipeline signature construction helpers.
  enum SignatureShift : uint32_t {
    kCompOpShift = blBitShiftOf(BL_PIPE_SIGNATURE_COMP_OP),
    kDstFmtShift = blBitShiftOf(BL_PIPE_SIGNATURE_DST_FORMAT),
    kSrcFmtShift = blBitShiftOf(BL_PIPE_SIGNATURE_SRC_FORMAT),
    kSolidIdShift = 14
  };

  //! Returns all bits that form the signature (CompOp, DstFormat SrcFormat).
  BL_INLINE constexpr uint32_t signatureBits() const noexcept { return data & 0x3FFFu; }
  //! Returns `BLPipeSignature` configured to have the same bits set as `signatureBits()`.
  BL_INLINE constexpr BLPipeSignature signature() const noexcept { return BLPipeSignature(signatureBits()); }
  //! Returns solid-id information regarding this simplification.
  BL_INLINE constexpr uint32_t solidId() const noexcept { return data >> kSolidIdShift; }

  //! Returns `BLCompOpSimplifyInfo` from decomposed arguments.
  static BL_INLINE constexpr BLCompOpSimplifyInfo make(uint32_t compOp, uint32_t d, uint32_t s, uint32_t solidId) noexcept {
    return BLCompOpSimplifyInfo {
      uint16_t((compOp << kCompOpShift) | (d << kDstFmtShift) | (s << kSrcFmtShift) | (solidId << kSolidIdShift))
    };
  }

  //! Returns `BLCompOpSimplifyInfo` sentinel containing the correct value of
  //! DST_COPY (NOP) operator. All other variations of DST_COPY are invalid.
  static BL_INLINE constexpr BLCompOpSimplifyInfo dstCopy() noexcept {
    return make(BL_COMP_OP_DST_COPY, 0, 0, BL_COMP_OP_SOLID_ID_TRANSPARENT);
  }
};

// Initially we have used a single table, however, some older compilers would
// reach template instantiation depth limit (as the table is not small), so the
// implementation was changed to this instead to make sure this won't happen.
enum : uint32_t { BL_COMP_OP_SIMPLIFY_RECORD_SIZE = BL_COMP_OP_INTERNAL_COUNT * BL_FORMAT_RESERVED_COUNT };
typedef BLLookupTable<BLCompOpSimplifyInfo, BL_COMP_OP_SIMPLIFY_RECORD_SIZE> BLCompOpSimplifyInfoRecordSet;

struct BLCompOpSimplifyInfoTable { BLCompOpSimplifyInfoRecordSet data[BL_FORMAT_COUNT]; };
BL_HIDDEN extern const BLCompOpSimplifyInfoTable blCompOpSimplifyInfoTable;

static BL_INLINE const BLCompOpSimplifyInfo* blCompOpSimplifyInfoArrayOf(uint32_t compOp, uint32_t dstFormat) noexcept {
  return &blCompOpSimplifyInfoTable.data[dstFormat][compOp * BL_FORMAT_RESERVED_COUNT];
}

static BL_INLINE const BLCompOpSimplifyInfo& blCompOpSimplifyInfo(uint32_t compOp, uint32_t dstFormat, uint32_t srcFormat) noexcept {
  return blCompOpSimplifyInfoArrayOf(compOp, dstFormat)[srcFormat];
}

//! \}
//! \endcond

#endif // BLEND2D_COMPOP_P_H
