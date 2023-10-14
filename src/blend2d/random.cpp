// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "random_p.h"
#include "simd/simd_p.h"

// bl::Random - API - Reset
// ========================

BL_API_IMPL BLResult blRandomReset(BLRandom* self, uint64_t seed) noexcept {
  bl::RandomInternal::resetSeed(self, seed);
  return BL_SUCCESS;
}

// bl::Random - API - Next
// =======================

BL_API_IMPL double blRandomNextDouble(BLRandom* self) noexcept {
  return bl::RandomInternal::nextDouble(self);
}

BL_API_IMPL uint32_t blRandomNextUInt32(BLRandom* self) noexcept {
  return bl::RandomInternal::nextUInt32(self);
}

BL_API_IMPL uint64_t blRandomNextUInt64(BLRandom* self) noexcept {
  return bl::RandomInternal::nextUInt64(self);
}
