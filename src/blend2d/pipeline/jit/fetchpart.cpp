// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/fetchpart_p.h"
#include "../../pipeline/jit/pipecompiler_p.h"

namespace bl {
namespace Pipeline {
namespace JIT {

// bl::Pipeline::JIT::FetchPart - Construction & Destruction
// =========================================================

FetchPart::FetchPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept
  : PipePart(pc, PipePartType::kFetch),
    _fetchType(fetchType),
    _fetchInfo(format),
    _bpp(uint8_t(blFormatInfo[size_t(format)].depth / 8u)) {}

// bl::Pipeline::JIT::FetchPart - Init & Fini
// ==========================================

void FetchPart::init(const PipeFunction& fn, Gp& x, Gp& y, PixelType pixelType, uint32_t pixelGranularity) noexcept {
  addPartFlags(x.isValid() ? PipePartFlags::kRectFill : PipePartFlags::kNone);

  _pixelType = pixelType;
  _pixelGranularity = uint8_t(pixelGranularity);

  // Initialize alpha fetch information. The fetch would be A8 if either the requested
  // pixel is alpha-only or the source pixel format is alpha-only (or both).
  _alphaFetch = _pixelType == PixelType::kA8 || format() == FormatExt::kA8;

  _initPart(fn, x, y);
  _initGlobalHook(cc->cursor());
}

void FetchPart::fini() noexcept {
  removePartFlags(PipePartFlags::kRectFill);

  _finiPart();
  _finiGlobalHook();

  _pixelType = PixelType::kNone;
  _pixelGranularity = 0;
}

void FetchPart::_initPart(const PipeFunction& fn, Gp& x, Gp& y) noexcept {
  blUnused(fn, x, y);
}

void FetchPart::_finiPart() noexcept {}

// bl::Pipeline::JIT::FetchPart - Advance
// ======================================

// By default these do nothing, only used by `SolidFetch()` this way.
void FetchPart::advanceY() noexcept {
  // Nothing by default.
}

void FetchPart::startAtX(const Gp& x) noexcept {
  // Nothing by default.
  blUnused(x);
}

void FetchPart::advanceX(const Gp& x, const Gp& diff) noexcept {
  // Nothing by default.
  blUnused(x, diff);
}

// bl::Pipeline::JIT::FetchPart - Fetch
// ====================================

void FetchPart::enterN() noexcept {
  // Nothing by default.
}

void FetchPart::leaveN() noexcept {
  // Nothing by default.
}

void FetchPart::prefetchN() noexcept {
  // Nothing by default.
}

void FetchPart::postfetchN() noexcept {
  // Nothing by default.
}

// [[pure virtual]]
void FetchPart::fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept {
  blUnused(p, n, flags, predicate);
  BL_NOT_REACHED();
}

} // {JIT}
} // {Pipeline}
} // {bl}

#endif // !BL_BUILD_NO_JIT
