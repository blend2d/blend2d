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

FetchPart::FetchPart(PipeCompiler* pc, FetchType fetchType, uint32_t format) noexcept
  : PipePart(pc, PipePartType::kFetch),
    _fetchType(fetchType),
    _format(uint8_t(format)),
    _bpp(uint8_t(blFormatInfo[format].depth / 8u)),
    _hasRGB((blFormatInfo[format].flags & BL_FORMAT_FLAG_RGB) != 0),
    _hasAlpha((blFormatInfo[format].flags & BL_FORMAT_FLAG_ALPHA) != 0) {}

// BLPipeline::JIT::FetchPart - Init & Fini
// ========================================

void FetchPart::init(x86::Gp& x, x86::Gp& y, PixelType pixelType, uint32_t pixelGranularity) noexcept {
  _isRectFill = x.isValid();

  _pixelType = pixelType;
  _pixelGranularity = uint8_t(pixelGranularity);

  // Initialize alpha fetch information. The fetch would be A8 if either the
  // requested pixel is alpha-only or the source pixel format is alpha-only
  // (or both).
  _alphaFetch = _pixelType == PixelType::kAlpha || _format == BL_FORMAT_A8;
  _alphaOffset = blFormatInfo[_format].aShift / 8;

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

void FetchPart::startAtX(x86::Gp& x) noexcept {
  // Nothing by default.
  blUnused(x);
}

void FetchPart::advanceX(x86::Gp& x, x86::Gp& diff) noexcept {
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

void FetchPart::fetch8(Pixel& p, PixelFlags flags) noexcept {
  // Fallback to `fetch4()` by default.
  p.setCount(8);

  Pixel x(p.type());
  Pixel y(p.type());

  fetch4(x, flags);
  fetch4(y, flags);

  // Each invocation of fetch should provide a stable output.
  BL_ASSERT(x.isImmutable() == y.isImmutable());

  if (p.isRGBA()) {
    if (blTestFlag(flags, PixelFlags::kPC)) {
      p.pc.init(x.pc[0], y.pc[0]);
      pc->rename(p.pc, "pc");
    }

    if (blTestFlag(flags, PixelFlags::kUC)) {
      p.uc.init(x.uc[0], x.uc[1], y.uc[0], y.uc[1]);
      pc->rename(p.uc, "uc");
    }

    if (blTestFlag(flags, PixelFlags::kUA)) {
      p.ua.init(x.ua[0], x.ua[1], y.ua[0], y.ua[1]);
      pc->rename(p.uc, "ua");
    }

    if (blTestFlag(flags, PixelFlags::kUIA)) {
      p.uia.init(x.uia[0], x.uia[1], y.uia[0], y.uia[1]);
      pc->rename(p.uc, "uia");
    }

    p.setImmutable(x.isImmutable());
  }
  else if (p.isAlpha()) {
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

    if (blTestFlag(flags, PixelFlags::kUIA)) {
      p.uia.init(x.uia[0]);
      pc->rename(p.uia, "uia");
      pc->v_interleave_lo_i64(x.uia[0], x.uia[0], y.uia[0]);
    }

    p.setImmutable(x.isImmutable());
  }
}

} // {JIT}
} // {BLPipeline}

#endif
