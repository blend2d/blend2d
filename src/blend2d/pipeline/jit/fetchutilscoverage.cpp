// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/fetchutilscoverage_p.h"

namespace bl {
namespace Pipeline {
namespace JIT {
namespace FetchUtils {

// bl::Pipeline::JIT::FetchUtils - Init & Pass Vec Coverage
// ========================================================

// TODO: REMOVE_THIS_ADD_THIS_TO_FILLPART

static uint32_t calculateCoverageByteCount(PixelCount pixelCount, PixelType pixelType, PixelCoverageFormat coverageFormat) noexcept {
  DataWidth dataWidth = DataWidth::k8;

  switch (coverageFormat) {
    case PixelCoverageFormat::kPacked:
      dataWidth = DataWidth::k8;
      break;

    case PixelCoverageFormat::kUnpacked:
      dataWidth = DataWidth::k16;
      break;

    default:
      BL_NOT_REACHED();
  }

  uint32_t count = pixelCount.value();
  switch (pixelType) {
    case PixelType::kA8:
      break;

    case PixelType::kRGBA32:
      count *= 4u;
      break;

    default:
      BL_NOT_REACHED();
  }

  return (1u << uint32_t(dataWidth)) * count;
}

void initVecCoverage(
  PipeCompiler* pc,
  VecArray& dst,
  PixelCount maxPixelCount,
  SimdWidth maxSimdWidth,
  PixelType pixelType,
  PixelCoverageFormat coverageFormat) noexcept {

  uint32_t coverageByteCount = calculateCoverageByteCount(maxPixelCount, pixelType, coverageFormat);
  SimdWidth simdWidth = SimdWidthUtils::simdWidthForByteCount(maxSimdWidth, coverageByteCount);
  uint32_t vecCount = SimdWidthUtils::vecCountForByteCount(simdWidth, coverageByteCount);

  pc->newVecArray(dst, vecCount, simdWidth, "vm");
}

void passVecCoverage(
  VecArray& dst,
  const VecArray& src,
  PixelCount pixelCount,
  PixelType pixelType,
  PixelCoverageFormat coverageFormat) noexcept {

  uint32_t coverageByteCount = calculateCoverageByteCount(pixelCount, pixelType, coverageFormat);
  SimdWidth simdWidth = SimdWidthUtils::simdWidthForByteCount(SimdWidthUtils::simdWidthOf(src[0]), coverageByteCount);
  uint32_t vecCount = SimdWidthUtils::vecCountForByteCount(simdWidth, coverageByteCount);

  // We can use at most what was given to us, or less in case that the current
  // `pixelCount` is less than `maxPixelCount` passed to `initVecCoverage()`.
  BL_ASSERT(vecCount <= src.size());

  dst._size = vecCount;
  for (uint32_t i = 0; i < vecCount; i++) {
    dst.v[i].reset();
    dst.v[i].as<asmjit::BaseReg>().setSignatureAndId(SimdWidthUtils::signatureOf(simdWidth), src.v[i].id());
  }
}

} // {FetchUtils}
} // {JIT}
} // {Pipeline}
} // {bl}

#endif // !BL_BUILD_NO_JIT
