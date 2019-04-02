// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "../blapi-build_p.h"
#include "../pipegen/blfetchpart_p.h"
#include "../pipegen/blpipecompiler_p.h"

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::FetchPart - Construction / Destruction]
// ============================================================================

FetchPart::FetchPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept
  : PipePart(pc, kTypeFetch),
    _fetchType(fetchType),
    _fetchPayload(fetchPayload),
    _format(uint8_t(format)),
    _bpp(uint8_t(blFormatInfo[format].depth / 8u)),
    _maxPixels(1),
    _pixelGranularity(0),
    _hasRGB((blFormatInfo[format].flags & BL_FORMAT_FLAG_RGB) != 0),
    _hasAlpha((blFormatInfo[format].flags & BL_FORMAT_FLAG_ALPHA) != 0),
    _isRectFill(false),
    _isComplexFetch(false) {}

// ============================================================================
// [BLPipeGen::FetchPart - Init / Fini]
// ============================================================================

void FetchPart::init(x86::Gp& x, x86::Gp& y, uint32_t pixelGranularity) noexcept {
  _isRectFill = x.isValid();
  _pixelGranularity = uint8_t(pixelGranularity);

  _initPart(x, y);
  _initGlobalHook(cc->cursor());
}

void FetchPart::fini() noexcept {
  _finiPart();
  _finiGlobalHook();

  _isRectFill = false;
  _pixelGranularity = 0;
}

void FetchPart::_initPart(x86::Gp& x, x86::Gp& y) noexcept {}
void FetchPart::_finiPart() noexcept {}

// ============================================================================
// [BLPipeGen::FetchPart - Advance]
// ============================================================================

// By default these do nothing, only used by `SolidFetch()` this way.
void FetchPart::advanceY() noexcept {}
void FetchPart::startAtX(x86::Gp& x) noexcept {}
void FetchPart::advanceX(x86::Gp& x, x86::Gp& diff) noexcept {}

// ============================================================================
// [BLPipeGen::FetchPart - Fetch]
// ============================================================================

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

void FetchPart::fetch8(PixelARGB& p, uint32_t flags) noexcept {
  // Fallback to `fetch4()` by default.
  PixelARGB x, y;

  fetch4(x, flags);
  fetch4(y, flags);

  if (flags & PixelARGB::kPC ) { p.pc.init(x.pc[0], y.pc[0]); }
  if (flags & PixelARGB::kUC ) { p.uc.init(x.uc[0], x.uc[1], y.uc[0], y.uc[1]); }
  if (flags & PixelARGB::kUA ) { p.ua.init(x.ua[0], x.ua[1], y.ua[0], y.ua[1]); }
  if (flags & PixelARGB::kUIA) { p.uia.init(x.uia[0], x.uia[1], y.uia[0], y.uia[1]); }

  p.immutable = x.immutable;
}

} // {BLPipeGen}
