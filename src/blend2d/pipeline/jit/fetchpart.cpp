// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if BL_TARGET_ARCH_X86 && !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/fetchpart_p.h"
#include "../../pipeline/jit/pipecompiler_p.h"

namespace BLPipeline {
namespace JIT {

// BLPipeline::JIT::FetchPart - Construction & Destruction
// =======================================================

FetchPart::FetchPart(PipeCompiler* pc, FetchType fetchType, BLInternalFormat format) noexcept
  : PipePart(pc, PipePartType::kFetch),
    _fetchType(fetchType),
    _format(format),
    _bpp(uint8_t(blFormatInfo[size_t(format)].depth / 8u)),
    _hasRGB((blFormatInfo[size_t(format)].flags & BL_FORMAT_FLAG_RGB) != 0),
    _hasAlpha((blFormatInfo[size_t(format)].flags & BL_FORMAT_FLAG_ALPHA) != 0) {}

// BLPipeline::JIT::FetchPart - Init & Fini
// ========================================

void FetchPart::init(x86::Gp& x, x86::Gp& y, PixelType pixelType, uint32_t pixelGranularity) noexcept {
  _isRectFill = x.isValid();

  _pixelType = pixelType;
  _pixelGranularity = uint8_t(pixelGranularity);

  // Initialize alpha fetch information. The fetch would be A8 if either the
  // requested pixel is alpha-only or the source pixel format is alpha-only
  // (or both).
  _alphaFetch = _pixelType == PixelType::kA8 || _format == BLInternalFormat::kA8;
  _alphaOffset = blFormatInfo[size_t(_format)].aShift / 8;

  _initPart(x, y);
  _initGlobalHook(cc->cursor());
}

void FetchPart::fini() noexcept {
  _finiPart();
  _finiGlobalHook();

  _isRectFill = false;
  _pixelType = PixelType::kNone;
  _pixelGranularity = 0;
}

void FetchPart::_initPart(x86::Gp& x, x86::Gp& y) noexcept {
  blUnused(x, y);
}

void FetchPart::_finiPart() noexcept {}

// BLPipeline::JIT::FetchPart - Advance
// ====================================

// By default these do nothing, only used by `SolidFetch()` this way.
void FetchPart::advanceY() noexcept {
  // Nothing by default.
}

void FetchPart::startAtX(const x86::Gp& x) noexcept {
  // Nothing by default.
  blUnused(x);
}

void FetchPart::advanceX(const x86::Gp& x, const x86::Gp& diff) noexcept {
  // Nothing by default.
  blUnused(x, diff);
}

// BLPipeline::JIT::FetchPart - Fetch
// ==================================

void FetchPart::prefetch1() noexcept {
  // Nothing by default.
}

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

void FetchPart::_fetch2x4(Pixel& p, PixelFlags flags) noexcept {
  // Fallback to `fetch4()` by default.
  p.setCount(PixelCount(8));

  Pixel x("x", p.type());
  Pixel y("y", p.type());

  fetch(x, PixelCount(4), flags, pc->emptyPredicate());
  fetch(y, PixelCount(4), flags, pc->emptyPredicate());

  // Each invocation of fetch should provide a stable output.
  BL_ASSERT(x.isImmutable() == y.isImmutable());

  if (p.isRGBA32()) {
    if (!x.pc.empty()) {
      BL_ASSERT(!y.pc.empty());
      if (pc->simdWidth() >= SimdWidth::k256) {
        pc->newYmmArray(p.pc, 1, p.name(), "pc");
        cc->vinserti128(p.pc[0], x.pc[0].ymm(), y.pc[0], 1);
      }
      else {
        p.pc.init(x.pc[0], y.pc[0]);
        pc->rename(p.pc, "pc");
      }
    }

    if (!x.uc.empty()) {
      BL_ASSERT(!y.uc.empty());
      p.uc.init(x.uc[0], x.uc[1], y.uc[0], y.uc[1]);
      pc->rename(p.uc, "uc");
    }

    if (!x.ua.empty()) {
      BL_ASSERT(!y.ua.empty());
      p.ua.init(x.ua[0], x.ua[1], y.ua[0], y.ua[1]);
      pc->rename(p.uc, "ua");
    }

    if (!x.ui.empty()) {
      BL_ASSERT(!y.ui.empty());
      p.ui.init(x.ui[0], x.ui[1], y.ui[0], y.ui[1]);
      pc->rename(p.uc, "ui");
    }

    p.setImmutable(x.isImmutable());
  }
  else if (p.isA8()) {
    if (blTestFlag(flags, PixelFlags::kPA)) {
      p.pa.init(x.pa[0]);
      pc->rename(p.pa, "pa");
      pc->v_interleave_lo_i32(x.pa[0], x.pa[0], y.pa[0]);
    }

    if (blTestFlag(flags, PixelFlags::kUA)) {
      p.ua.init(x.ua[0]);
      pc->rename(p.ua, "ua");
      pc->v_interleave_lo_i64(x.ua[0], x.ua[0], y.ua[0]);
    }

    if (blTestFlag(flags, PixelFlags::kUI)) {
      p.ui.init(x.ui[0]);
      pc->rename(p.ui, "ui");
      pc->v_interleave_lo_i64(x.ui[0], x.ui[0], y.ui[0]);
    }

    p.setImmutable(x.isImmutable());
  }
}

} // {JIT}
} // {BLPipeline}

#endif
