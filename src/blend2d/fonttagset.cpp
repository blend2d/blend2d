// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "fonttagset_p.h"
#include "support/algorithm_p.h"

namespace bl {
namespace FontTagData {

BLResult finalizeTagArray(BLArray<BLTag>& tags) noexcept {
  size_t size = tags.size();
  if (size > 1) {
    BLTag* data = nullptr;
    BL_PROPAGATE(tags.makeMutable(&data));

    // Sort and deduplicate afterwards.
    quickSort(data, size);

    size_t j = 1;
    BLTag prevTag = data[0];

    for (size_t i = 1; i < size; i++) {
      BLTag currentTag = data[i];
      if (currentTag == prevTag)
        continue;

      data[j++] = currentTag;
      prevTag = currentTag;
    }

    size = j;
    tags.resize(size, BLTag(0));
  }

  return tags.shrink();
}

BLResult flattenTagSetTo(BLArray<BLTag>& dst,
  const BLTag* knownIdToTagTable,
  const BLBitWord* knownTagData, size_t knownTagDataSize, size_t knownTagCount,
  const BLTag* unknownTagData, size_t unknownTagCount) noexcept {

  size_t tagCount = knownTagCount + unknownTagCount;

  BLTag* dstData = nullptr;
  BL_PROPAGATE(dst.modifyOp(BL_MODIFY_OP_ASSIGN_FIT, tagCount, &dstData));

  size_t dstDataIndex = 0;
  size_t unknownTagIndex = 0;
  ParametrizedBitOps<BitOrder::kLSB, BLBitWord>::BitVectorIterator it(knownTagData, knownTagDataSize);

  while (it.hasNext()) {
    uint32_t tagId = uint32_t(it.next());
    BLTag knownTag = knownIdToTagTable[tagId];

    while (unknownTagIndex < unknownTagCount && unknownTagData[unknownTagIndex] < knownTag)
      dstData[dstDataIndex++] = unknownTagData[unknownTagIndex++];

    dstData[dstDataIndex++] = knownTag;
  }

  while (unknownTagIndex < unknownTagCount)
    dstData[dstDataIndex++] = unknownTagData[unknownTagIndex++];

  return BL_SUCCESS;
}

} // {FontTagData}
} // {bl}
