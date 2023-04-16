// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_COMPOP_P_H_INCLUDED
#define BLEND2D_COMPOP_P_H_INCLUDED

#include "api-internal_p.h"
#include "context.h"
#include "format_p.h"
#include "tables_p.h"
#include "pipeline/pipedefs_p.h"
#include "support/bitops_p.h"

//! \cond INTERNAL

//! \addtogroup blend2d_internal
//! \{

//! Additional composition operators used internally.
enum BLCompOpInternal : uint32_t {
  //! Invert destination alpha (alpha formats only).
  BL_COMP_OP_INTERNAL_ALPHA_INV = BL_COMP_OP_MAX_VALUE + 1,
  //! Count of all composition operators including internal ones..
  BL_COMP_OP_INTERNAL_COUNT
};

//! Simplification of a composition operator that leads to SOLID fill instead.
enum class BLCompOpSolidId : uint32_t {
  //! Source pixels are used.
  //!
  //! \note This value must be zero as it's usually combined with rendering context flags and then used for decision
  //! making about the whole command.
  kNone = 0,
  //! Source pixels are always treated as transparent zero (all 0).
  kTransparent = 1,
  //! Source pixels are always treated as opaque black (R|G|B=0 A=1).
  kOpaqueBlack = 2,
  //! Source pixels are always treated as opaque white (R|G|B=1 A=1).
  kOpaqueWhite = 3
};

//! Composition operator flags that can be retrieved through BLCompOpInfo[] table.
enum class BLCompOpFlags : uint32_t {
  kNone = 0,

  //! TypeA operator - "D*(1-M) + Op(D, S)*M" == "Op(D, S * M)".
  kTypeA = 0x00000001u,
  //! TypeB operator - "D*(1-M) + Op(D, S)*M" == "Op(D, S*M) + D*(1-M)".
  kTypeB = 0x00000002u,
  //! TypeC operator - cannot be simplified.
  kTypeC = 0x00000004u,

  //! Non-separable operator.
  kNonSeparable = 0x00000008u,

  //! Uses `Dc` (destination color or luminance channel).
  kDc = 0x00000010u,
  //! Uses `Da` (destination alpha channel).
  kDa = 0x00000020u,
  //! Uses both `Dc` and `Da`.
  kDc_Da = 0x00000030u,

  //! Uses `Sc` (source color or luminance channel).
  kSc = 0x00000040u,
  //! Uses `Sa` (source alpha channel).
  kSa = 0x00000080u,
  //! Uses both `Sc` and `Sa`,
  kSc_Sa = 0x000000C0u,

  //! Destination is never changed (NOP).
  kNop = 0x00000800u,
  //! Destination is changed only if `Da != 0`.
  kNopIfDaEq0 = 0x00001000u,
  //! Destination is changed only if `Da != 1`.
  kNopIfDaEq1 = 0x00002000u,
  //! Destination is changed only if `Sa != 0`.
  kNopIfSaEq0 = 0x00004000u,
  //! Destination is changed only if `Sa != 1`.
  kNopIfSaEq1 = 0x00008000u
};
BL_DEFINE_ENUM_FLAGS(BLCompOpFlags)

//! Information about a composition operator.
struct BLCompOpInfo {
  uint16_t _flags;

  BL_INLINE BLCompOpFlags flags() const noexcept { return (BLCompOpFlags)_flags; }
};

//! Provides flags for each composition operator.
BL_HIDDEN extern const BLLookupTable<BLCompOpInfo, BL_COMP_OP_INTERNAL_COUNT> blCompOpInfo;

//! Information that can be used to simplify a "Dst CompOp Src" into a simpler composition operator with a possible
//! format conversion and arbitrary source to solid conversion. This is used by the rendering engine to simplify every
//! composition operator before it considers which pipeline to use.
//!
//! There are two reasons for simplification - the first is performance and the second reason is about decreasing the
//! number of possible pipeline signatures the rendering context may require. For example by using "SRC-COPY" operator
//! instead of "CLEAR" operator the rendering engine basically eliminated a possible compilation of "CLEAR" operator
//! that would perform exactly the same as "SRC-COPY".
struct BLCompOpSimplifyInfo {
  //! Alternative composition operator, destination format, source format, and solid-id information packed into 16 bits.
  uint16_t data;

  // Data shift specify where the value is stored in `data`.
  enum DataShift : uint32_t {
    kCompOpShift = BLIntOps::bitShiftOf(BLPipeline::Signature::kMaskCompOp),
    kDstFmtShift = BLIntOps::bitShiftOf(BLPipeline::Signature::kMaskDstFormat),
    kSrcFmtShift = BLIntOps::bitShiftOf(BLPipeline::Signature::kMaskSrcFormat),
    kSolidIdShift = 14
  };

  //! Returns all bits that form the signature (CompOp, DstFormat SrcFormat).
  BL_INLINE constexpr uint32_t signatureBits() const noexcept { return data & 0x3FFFu; }
  //! Returns `Signature` configured to have the same bits set as `signatureBits()`.
  BL_INLINE constexpr BLPipeline::Signature signature() const noexcept { return BLPipeline::Signature(signatureBits()); }
  //! Returns solid-id information regarding this simplification.
  BL_INLINE constexpr BLCompOpSolidId solidId() const noexcept { return (BLCompOpSolidId)(data >> kSolidIdShift); }

  //! Returns `BLCompOpSimplifyInfo` from decomposed arguments.
  static BL_INLINE constexpr BLCompOpSimplifyInfo make(uint32_t compOp, BLInternalFormat d, BLInternalFormat s, BLCompOpSolidId solidId) noexcept {
    return BLCompOpSimplifyInfo {
      uint16_t((compOp << kCompOpShift) |
               (uint32_t(d) << kDstFmtShift) |
               (uint32_t(s) << kSrcFmtShift) |
               (uint32_t(solidId) << kSolidIdShift))
    };
  }

  //! Returns `BLCompOpSimplifyInfo` sentinel containing the only correct value of DST_COPY (NOP) operator. All other
  //! variations of DST_COPY are invalid.
  static BL_INLINE constexpr BLCompOpSimplifyInfo dstCopy() noexcept {
    return make(BL_COMP_OP_DST_COPY, BLInternalFormat::kNone, BLInternalFormat::kNone, BLCompOpSolidId::kTransparent);
  }
};

// Initially we have used a single table, however, some older compilers would reach template instantiation depth limit
// (as the table is not small), so the implementation was changed to this instead to make sure this won't happen.
enum : uint32_t {
  BL_COMP_OP_SIMPLIFY_RECORD_SIZE = uint32_t(BL_COMP_OP_INTERNAL_COUNT) * (uint32_t(BLInternalFormat::kMaxReserved) + 1)
};
typedef BLLookupTable<BLCompOpSimplifyInfo, BL_COMP_OP_SIMPLIFY_RECORD_SIZE> BLCompOpSimplifyInfoRecordSet;

struct BLCompOpSimplifyInfoTable { BLCompOpSimplifyInfoRecordSet data[BL_FORMAT_MAX_VALUE + 1u]; };
BL_HIDDEN extern const BLCompOpSimplifyInfoTable blCompOpSimplifyInfoTable;

static BL_INLINE const BLCompOpSimplifyInfo* blCompOpSimplifyInfoArrayOf(uint32_t compOp, BLInternalFormat dstFormat) noexcept {
  return &blCompOpSimplifyInfoTable.data[size_t(dstFormat)][size_t(compOp) * (size_t(BLInternalFormat::kMaxReserved) + 1)];
}

static BL_INLINE const BLCompOpSimplifyInfo& blCompOpSimplifyInfo(uint32_t compOp, BLInternalFormat dstFormat, BLInternalFormat srcFormat) noexcept {
  return blCompOpSimplifyInfoArrayOf(compOp, dstFormat)[size_t(srcFormat)];
}

//! \}
//! \endcond

#endif // BLEND2D_COMPOP_P_H_INCLUDED
