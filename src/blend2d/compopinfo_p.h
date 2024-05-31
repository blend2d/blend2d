// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_COMPOPINFO_P_H_INCLUDED
#define BLEND2D_COMPOPINFO_P_H_INCLUDED

#include "api-internal_p.h"
#include "compop_p.h"
#include "pipeline/pipedefs_p.h"
#include "support/bitops_p.h"
#include "support/lookuptable_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

//! Information about a composition operator.
struct CompOpInfo {
  uint16_t _flags;

  BL_INLINE_NODEBUG CompOpFlags flags() const noexcept { return (CompOpFlags)_flags; }
};

//! Provides flags for each composition operator.
BL_HIDDEN extern const LookupTable<CompOpInfo, kCompOpExtCount> compOpInfoTable;

//! Information that can be used to simplify a "Dst CompOp Src" into a simpler composition operator with a possible
//! format conversion and arbitrary source to solid conversion. This is used by the rendering engine to simplify every
//! composition operator before it considers which pipeline to use.
//!
//! There are two reasons for simplification - the first is performance and the second reason is about decreasing the
//! number of possible pipeline signatures the rendering context may require. For example by using "SRC-COPY" operator
//! instead of "CLEAR" operator the rendering engine basically eliminated a possible compilation of "CLEAR" operator
//! that would perform exactly the same as "SRC-COPY".
struct CompOpSimplifyInfo {
  //! \name Constants
  //! \{

  // Data shift specify where the value is stored in `data`.
  static constexpr uint32_t kCompOpShift = IntOps::bitShiftOf(Pipeline::Signature::kMaskCompOp);
  static constexpr uint32_t kDstFmtShift = IntOps::bitShiftOf(Pipeline::Signature::kMaskDstFormat);
  static constexpr uint32_t kSrcFmtShift = IntOps::bitShiftOf(Pipeline::Signature::kMaskSrcFormat);
  static constexpr uint32_t kSolidIdShift = 16;

  //! \}

  //! \name Members
  //! \{

  //! Alternative composition operator, destination format, source format, and solid-id information packed into 32 bits.
  uint32_t data;

  //! \}

  //! \name Accessors
  //! \{

  //! Returns all bits that form the signature (CompOp, DstFormat SrcFormat).
  BL_INLINE_NODEBUG constexpr uint32_t signatureBits() const noexcept { return data & 0xFFFFu; }
  //! Returns `Signature` configured to have the same bits set as `signatureBits()`.
  BL_INLINE_NODEBUG constexpr Pipeline::Signature signature() const noexcept { return Pipeline::Signature{signatureBits()}; }

  //! Returns the simplified composition operator.
  BL_INLINE_NODEBUG constexpr CompOpExt compOp() const noexcept { return CompOpExt((data & Pipeline::Signature::kMaskCompOp) >> kCompOpShift); }
  //! Returns the simplified destination format.
  BL_INLINE_NODEBUG constexpr FormatExt dstFormat() const noexcept { return FormatExt((data & Pipeline::Signature::kMaskDstFormat) >> kDstFmtShift); }
  //! Returns the simplified source format.
  BL_INLINE_NODEBUG constexpr FormatExt srcFormat() const noexcept { return FormatExt((data & Pipeline::Signature::kMaskSrcFormat) >> kSrcFmtShift); }

  //! Returns solid-id information regarding this simplification.
  BL_INLINE_NODEBUG constexpr CompOpSolidId solidId() const noexcept { return CompOpSolidId(data >> kSolidIdShift); }

  //! \}

  //! \name Make
  //! \{

  //! Returns `CompOpSimplifyInfo` from decomposed arguments.
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo make(CompOpExt compOp, FormatExt d, FormatExt s, CompOpSolidId solidId) noexcept {
    return CompOpSimplifyInfo {
      uint32_t((uint32_t(compOp) << kCompOpShift) |
               (uint32_t(d) << kDstFmtShift) |
               (uint32_t(s) << kSrcFmtShift) |
               (uint32_t(solidId) << kSolidIdShift))
    };
  }

  //! Returns `CompOpSimplifyInfo` sentinel containing the only correct value of DST_COPY (NOP) operator. All other
  //! variations of DST_COPY are invalid.
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo dstCopy() noexcept {
    return make(CompOpExt::kDstCopy, FormatExt::kNone, FormatExt::kNone, CompOpSolidId::kAlwaysNop);
  }

  //! \}
};

// Initially we have used a single table, however, some older compilers would reach template instantiation depth limit
// (as the table is not small), so the implementation was changed to this instead to make sure this won't happen.
static constexpr uint32_t kCompOpSimplifyRecordSize = kCompOpExtCount * (uint32_t(FormatExt::kMaxReserved) + 1);
typedef LookupTable<CompOpSimplifyInfo, kCompOpSimplifyRecordSize> CompOpSimplifyInfoRecordSet;

struct CompOpSimplifyInfoTable { CompOpSimplifyInfoRecordSet data[BL_FORMAT_MAX_VALUE + 1u]; };
BL_HIDDEN extern const CompOpSimplifyInfoTable compOpSimplifyInfoTable;

static BL_INLINE const CompOpSimplifyInfo* compOpSimplifyInfoArrayOf(CompOpExt compOp, FormatExt dstFormat) noexcept {
  return &compOpSimplifyInfoTable.data[size_t(dstFormat)][size_t(compOp) * kFormatExtCount];
}

static BL_INLINE const CompOpSimplifyInfo& compOpSimplifyInfo(CompOpExt compOp, FormatExt dstFormat, FormatExt srcFormat) noexcept {
  return compOpSimplifyInfoArrayOf(compOp, dstFormat)[size_t(srcFormat)];
}

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_COMPOPINFO_P_H_INCLUDED
