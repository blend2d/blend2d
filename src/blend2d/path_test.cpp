// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_test_p.h"
#if defined(BL_TEST)

#include "object_p.h"
#include "path_p.h"

// bl::Path - Tests
// ================

namespace bl {
namespace Tests {

UNIT(path_allocation_strategy, BL_TEST_GROUP_GEOMETRY_CONTAINERS) {
  BLPath p;
  size_t kNumItems = 1000000;
  size_t capacity = p.capacity();

  for (size_t i = 0; i < kNumItems; i++) {
    if (i == 0)
      p.moveTo(0, 0);
    else
      p.moveTo(double(i), double(i));

    if (capacity != p.capacity()) {
      size_t implSize = PathInternal::implSizeFromCapacity(p.capacity()).value();
      INFO("Capacity increased from %zu to %zu [ImplSize=%zu]\n", capacity, p.capacity(), implSize);

      capacity = p.capacity();
    }
  }
}

} // {Tests}
} // {bl}

#endif // BL_TEST
