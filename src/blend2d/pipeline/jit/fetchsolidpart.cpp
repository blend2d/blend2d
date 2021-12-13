// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if BL_TARGET_ARCH_X86 && !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/compoppart_p.h"
#include "../../pipeline/jit/fetchsolidpart_p.h"
#include "../../pipeline/jit/pipecompiler_p.h"

namespace BLPipeline {
namespace JIT {

#define REL_SOLID(FIELD) BL_OFFSET_OF(FetchData::Solid, FIELD)

// BLPipeline::JIT::FetchSolidPart - Construction & Destruction
// ============================================================

FetchSolidPart::FetchSolidPart(PipeCompiler* pc, uint32_t format) noexcept
  : FetchPart(pc, FetchType::kSolid, format) {

  _maxPixels = kUnlimitedMaxPixels;
  /*
  _maxSimdWidthSupported = SimdWidth::k256;
  */

  _pixel.reset();
  _pixel.setCount(1);
}

// BLPipeline::JIT::FetchSolidPart - Prepare
// =========================================

void FetchSolidPart::preparePart() noexcept {}

// BLPipeline::JIT::FetchSolidPart - Init & Fini
// =============================================

void FetchSolidPart::_initPart(x86::Gp& x, x86::Gp& y) noexcept {
  blUnused(x, y);
  if (_pixel.type() != _pixelType) {
    _pixel.setType(_pixelType);
  }
  else {
    // The type should never change after it's been assigned.
    BL_ASSERT(_pixel.type() == _pixelType);
  }
}

void FetchSolidPart::_finiPart() noexcept {}

// BLPipeline::JIT::FetchSolidPart - InitSolidFlags
// ================================================

void FetchSolidPart::initSolidFlags(PixelFlags flags) noexcept {
  ScopedInjector injector(cc, &_globalHook);
  Pixel& s = _pixel;

  switch (s.type()) {
    case PixelType::kRGBA:
      if (blTestFlag(flags, PixelFlags::kPC | PixelFlags::kUC | PixelFlags::kUA | PixelFlags::kUIA) && s.pc.empty()) {
        s.pc.init(pc->newVec("pixel.pc"));
        pc->v_broadcast_u32(s.pc[0], x86::ptr_32(pc->_fetchData));
      }
      break;

    case PixelType::kAlpha:
      if (blTestFlag(flags, PixelFlags::kSA | PixelFlags::kPA | PixelFlags::kUA | PixelFlags::kUIA) && !s.sa.isValid()) {
        s.sa = cc->newUInt32("pixel.sa");
        pc->load8(s.sa, x86::ptr_8(pc->_fetchData, 3));
      }

      if (blTestFlag(flags, PixelFlags::kPA | PixelFlags::kUA | PixelFlags::kUIA) && s.ua.empty()) {
        s.ua.init(pc->newVec("pixel.ua"));
        pc->v_broadcast_u16(s.ua[0], s.sa);
      }
      break;
  }

  pc->xSatisfySolid(s, flags);
}

// BLPipeline::JIT::FetchSolidPart - Fetch
// =======================================

void FetchSolidPart::fetch1(Pixel& p, PixelFlags flags) noexcept {
  BL_ASSERT(_pixel.type() == p.type());

  p.setCount(1);
  if (p.isRGBA()) {
    if (blTestFlag(flags, PixelFlags::kAny)) {
      initSolidFlags(flags & PixelFlags::kAny);
      Pixel& s = _pixel;

      if (blTestFlag(flags, PixelFlags::kImmutable)) {
        if (blTestFlag(flags, PixelFlags::kPC )) { p.pc.init(s.pc); }
        if (blTestFlag(flags, PixelFlags::kUC )) { p.uc.init(s.uc); }
        if (blTestFlag(flags, PixelFlags::kUA )) { p.ua.init(s.ua); }
        if (blTestFlag(flags, PixelFlags::kUIA)) { p.uia.init(s.uia); }
      }
      else {
        if (blTestFlag(flags, PixelFlags::kPC)) {
          p.pc.init(pc->newVec("p.pc0"));
          pc->v_mov(p.pc[0], s.pc[0]);
        }

        if (blTestFlag(flags, PixelFlags::kUC)) {
          p.uc.init(pc->newVec("p.uc0"));
          pc->v_mov(p.uc[0], s.uc[0]);
        }

        if (blTestFlag(flags, PixelFlags::kUA)) {
          p.ua.init(pc->newVec("p.ua0"));
          pc->v_mov(p.ua[0], s.ua[0]);
        }

        if (blTestFlag(flags, PixelFlags::kUIA)) {
          p.uia.init(pc->newVec("p.uia0"));
          pc->v_mov(p.uia[0], s.uia[0]);
        }
      }
    }
  }
  else if (p.isAlpha()) {
    if (blTestFlag(flags, PixelFlags::kSA)) {
      initSolidFlags(PixelFlags::kSA);
      Pixel& s = _pixel;

      if (blTestFlag(flags, PixelFlags::kImmutable)) {
        if (blTestFlag(flags, PixelFlags::kSA)) {
          p.sa = s.sa;
        }
      }
      else {
        if (blTestFlag(flags, PixelFlags::kSA)) {
          p.sa = cc->newUInt32("p.sa");
          cc->mov(p.sa, s.sa);
        }
      }
    }
  }

  pc->xSatisfyPixel(p, flags);
}

void FetchSolidPart::fetch4(Pixel& p, PixelFlags flags) noexcept {
  BL_ASSERT(_pixel.type() == p.type());

  p.setCount(4);
  if (p.isRGBA()) {
    initSolidFlags(flags & (PixelFlags::kPC | PixelFlags::kUC | PixelFlags::kUA | PixelFlags::kUIA));
    Pixel& s = _pixel;

    uint32_t pCount = 1;
    uint32_t uCount = 2;

    if (blTestFlag(flags, PixelFlags::kImmutable)) {
      if (blTestFlag(flags, PixelFlags::kPC)) { p.pc.init(s.pc); }
      if (blTestFlag(flags, PixelFlags::kUC)) { p.uc.init(s.uc); }
      if (blTestFlag(flags, PixelFlags::kUA)) { p.ua.init(s.ua); }
      if (blTestFlag(flags, PixelFlags::kUIA)) { p.uia.init(s.uia); }
    }
    else {
      if (blTestFlag(flags, PixelFlags::kPC)) {
        pc->newVecArray(p.pc, pCount, "p.pc");
        pc->v_mov(p.pc, s.pc[0]);
      }

      if (blTestFlag(flags, PixelFlags::kUC)) {
        pc->newVecArray(p.uc, uCount, "p.uc");
        pc->v_mov(p.uc, s.uc[0]);
      }

      if (blTestFlag(flags, PixelFlags::kUA)) {
        pc->newVecArray(p.ua, uCount, "p.ua");
        pc->v_mov(p.ua, s.ua[0]);
      }

      if (blTestFlag(flags, PixelFlags::kUIA)) {
        pc->newVecArray(p.uia, uCount, "p.uia");
        pc->v_mov(p.uia, s.uia[0]);
      }
    }
  }
  else if (p.isAlpha()) {
    initSolidFlags(flags & (PixelFlags::kPA | PixelFlags::kUA | PixelFlags::kUIA));
    Pixel& s = _pixel;

    uint32_t pCount = 1;
    uint32_t uCount = 1;

    if (blTestFlag(flags, PixelFlags::kImmutable)) {
      if (blTestFlag(flags, PixelFlags::kPA)) { p.pa.init(s.pa); }
      if (blTestFlag(flags, PixelFlags::kUA)) { p.ua.init(s.ua); }
      if (blTestFlag(flags, PixelFlags::kUIA)) { p.uia.init(s.uia); }
    }
    else {
      if (blTestFlag(flags, PixelFlags::kPA)) {
        pc->newVecArray(p.pa, pCount, "p.pa");
        pc->v_mov(p.pa[0], s.pa[0]);
      }

      if (blTestFlag(flags, PixelFlags::kUA)) {
        pc->newVecArray(p.ua, uCount, "p.ua");
        pc->v_mov(p.ua, s.ua[0]);
      }

      if (blTestFlag(flags, PixelFlags::kUIA)) {
        pc->newVecArray(p.uia, uCount, "p.uia");
        pc->v_mov(p.uia, s.uia[0]);
      }
    }
  }

  pc->xSatisfyPixel(p, flags);
}

} // {JIT}
} // {BLPipeline}

#endif
