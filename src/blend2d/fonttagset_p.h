// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FONTTAGSET_P_H_INCLUDED
#define BLEND2D_FONTTAGSET_P_H_INCLUDED

#include "api-internal_p.h"
#include "array.h"
#include "fonttagdataids_p.h"
#include "support/algorithm_p.h"
#include "support/bitops_p.h"
#include "support/fixedbitarray_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {
namespace FontTagData {

BL_HIDDEN BLResult finalizeTagArray(BLArray<BLTag>& tags) noexcept;

BL_HIDDEN BLResult flattenTagSetTo(BLArray<BLTag>& dst,
  const BLTag* knownIdToTagTable,
  const BLBitWord* knownTagData, size_t knownTagDataSize, size_t knownTagCount,
  const BLTag* unknownTagData, size_t unknownTagCount) noexcept;

//! A set of known and unknown OpenType tags that can be used to build an array of tags regarding a single feature.
//! It optimizes the case for adding known tags (tags that have a corresponding ID in Blend2D tag database) over tags
//! that are not known (such tags are non-standard and could be totally unsupported by Blend2D anyway).
template<size_t kKnownTagCount>
class TagSet {
public:
  BLArray<BLTag> unknownTags;
  FixedBitArray<BLBitWord, kKnownTagCount> knownTags {};
  size_t knownTagCount {};

  BL_INLINE bool _hasTag(BLTag tag, uint32_t id) const noexcept {
    if (id != kInvalidId)
      return knownTags.bitAt(id);

    size_t index = lowerBound(unknownTags.data(), unknownTags.size(), id);
    return index < unknownTags.size() && unknownTags[index] == tag;
  }

  BL_INLINE BLResult _addTag(BLTag tag, uint32_t id) noexcept {
    if (id != kInvalidId)
      return _addKnownTagId(id);
    else
      return _addUnknownTag(tag);
  }

  BL_INLINE BLResult _addKnownTagId(uint32_t id) noexcept {
    BL_ASSERT(id < kKnownTagCount);

    knownTagCount += size_t(!knownTags.bitAt(id));
    knownTags.setAt(id);
    return BL_SUCCESS;
  }

  BL_INLINE BLResult _addUnknownTag(BLTag tag) noexcept {
    return unknownTags.append(tag);
  }

  // Called when it's known that no more tags will be added.
  BL_INLINE BLResult finalize() noexcept {
    return finalizeTagArray(unknownTags);
  }

  BL_NOINLINE BLResult flattenTo(BLArray<BLTag>& dst, const BLTag* idToTagTable) noexcept {
    const BLTag* unknownTagData = unknownTags.data();
    size_t unknownTagCount = unknownTags.size();
    size_t tagCount = knownTagCount + unknownTagCount;

    BLTag* dstData = nullptr;
    BL_PROPAGATE(dst.modifyOp(BL_MODIFY_OP_ASSIGN_FIT, tagCount, &dstData));

    size_t dstDataIndex = 0;
    size_t unknownTagIndex = 0;
    ParametrizedBitOps<BitOrder::kLSB, BLBitWord>::BitVectorIterator it(knownTags.data, knownTags.sizeInWords());

    while (it.hasNext()) {
      uint32_t tagId = it.next();
      BLTag knownTag = idToTagTable[tagId];

      while (unknownTagIndex < unknownTagCount && unknownTagData[unknownTagIndex] < knownTag)
        dstData[dstDataIndex++] = unknownTagData[unknownTagIndex++];

      dstData[dstDataIndex++] = knownTag;
    }

    while (unknownTagIndex < unknownTagCount)
      dstData[dstDataIndex++] = unknownTagData[unknownTagIndex++];

    return BL_SUCCESS;
  }
};

class ScriptTagSet : public TagSet<kScriptIdCount> {
public:
  BL_INLINE bool hasTag(BLTag tag) const noexcept {
    return _hasTag(tag, scriptTagToId(tag));
  }

  BL_INLINE BLResult addTag(BLTag tag) noexcept {
    return _addTag(tag, scriptTagToId(tag));
  }

  BL_INLINE BLResult flattenTo(BLArray<BLTag>& dst) const noexcept {
    return flattenTagSetTo(dst, scriptIdToTagTable, knownTags.data, knownTags.sizeInWords(), knownTagCount, unknownTags.data(), unknownTags.size());
  }
};

class LanguageTagSet : public TagSet<kLanguageIdCount> {
public:
  BL_INLINE bool hasTag(BLTag tag) const noexcept {
    return _hasTag(tag, languageTagToId(tag));
  }

  BL_INLINE BLResult addTag(BLTag tag) noexcept {
    return _addTag(tag, languageTagToId(tag));
  }

  BL_INLINE BLResult flattenTo(BLArray<BLTag>& dst) const noexcept {
    return flattenTagSetTo(dst, languageIdToTagTable, knownTags.data, knownTags.sizeInWords(), knownTagCount, unknownTags.data(), unknownTags.size());
  }
};

class FeatureTagSet : public TagSet<kFeatureIdCount> {
public:
  BL_INLINE bool hasTag(BLTag tag) const noexcept {
    return _hasTag(tag, featureTagToId(tag));
  }

  BL_INLINE BLResult addTag(BLTag tag) noexcept {
    return _addTag(tag, featureTagToId(tag));
  }

  BL_INLINE BLResult flattenTo(BLArray<BLTag>& dst) const noexcept {
    return flattenTagSetTo(dst, featureIdToTagTable, knownTags.data, knownTags.sizeInWords(), knownTagCount, unknownTags.data(), unknownTags.size());
  }
};

class BaselineTagSet : public TagSet<kBaselineIdCount> {
public:
  BL_INLINE bool hasTag(BLTag tag) const noexcept {
    return _hasTag(tag, baselineTagToId(tag));
  }

  BL_INLINE BLResult addTag(BLTag tag) noexcept {
    return _addTag(tag, baselineTagToId(tag));
  }

  BL_INLINE BLResult flattenTo(BLArray<BLTag>& dst) const noexcept {
    return flattenTagSetTo(dst, baselineIdToTagTable, knownTags.data, knownTags.sizeInWords(), knownTagCount, unknownTags.data(), unknownTags.size());
  }
};

class VariationTagSet : public TagSet<kVariationIdCount> {
public:
  BL_INLINE bool hasTag(BLTag tag) const noexcept {
    return _hasTag(tag, variationTagToId(tag));
  }

  BL_INLINE BLResult addTag(BLTag tag) noexcept {
    return _addTag(tag, variationTagToId(tag));
  }

  BL_INLINE BLResult flattenTo(BLArray<BLTag>& dst) const noexcept {
    return flattenTagSetTo(dst, variationIdToTagTable, knownTags.data, knownTags.sizeInWords(), knownTagCount, unknownTags.data(), unknownTags.size());
  }
};

} // {FontTagData}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_FONTTAGSET_P_H_INCLUDED
