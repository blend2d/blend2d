// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_BITSET_P_H
#define BLEND2D_BITSET_P_H

#include "api-internal_p.h"
#include "bitset.h"
#include "support/algorithm_p.h"
#include "support/bitops_p.h"
#include "support/intops_p.h"
#include "support/memops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! \name BLBitSet - Types
//! \{

typedef BLParametrizedBitOps<BLBitOrder::kMSB, uint32_t> BLBitSetOps;

//! \}

namespace BLBitSetPrivate {

//! \name BLBitSet - Constants
//! \{

static constexpr uint32_t kInvalidIndex = BL_BIT_SET_INVALID_INDEX;
static constexpr uint32_t kSegmentWordCount = BL_BIT_SET_SEGMENT_WORD_COUNT;

//! \}

//! \name BLBitSet - Utility Functions
//! \{

//! SSO BitSet is empty when all 3 words are zero.
//!
//! This check handles both dense SSO data and SSO ranges.
static BL_INLINE bool isSSOEmpty(const BLBitSetCore* self) noexcept { return (self->_d.u64_data[0] | self->_d.u32_data[2]) == 0u; }

static BL_INLINE uint32_t getSSORangeStart(const BLBitSetCore* self) noexcept { return self->_d.u32_data[0]; }
static BL_INLINE uint32_t getSSORangeEnd(const BLBitSetCore* self) noexcept { return self->_d.u32_data[1]; }
static BL_INLINE uint32_t getSSOWordIndex(const BLBitSetCore* self) noexcept { return self->_d.info.bits & BLBitSet::kSSOIndexMask; }

//! \}

//! \name BLBitSet - Memory Management
//! \{

static BL_INLINE BLBitSetImpl* getImpl(const BLBitSetCore* self) noexcept {
  return static_cast<BLBitSetImpl*>(self->_d.impl);
}

static BL_INLINE BLResult freeImpl(BLBitSetImpl* impl, BLObjectInfo info) noexcept {
  return blObjectImplFreeInline(impl, info);
}

//! \}

} // {BLBitSetPrivate}

//! \}
//! \endcond

#endif // BLEND2D_BITSET_P_H
