// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#include "../api-build_p.h"
#if BL_TARGET_ARCH_X86 && !defined(BL_BUILD_NO_JIT)

#include "../pipegen/compoppart_p.h"
#include "../pipegen/fetchsolidpart_p.h"
#include "../pipegen/pipecompiler_p.h"

namespace BLPipeGen {

#define REL_SOLID(FIELD) BL_OFFSET_OF(BLPipeFetchData::Solid, FIELD)

// ============================================================================
// [BLPipeGen::FetchSolidPart - Construction / Destruction]
// ============================================================================

FetchSolidPart::FetchSolidPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept
  : FetchPart(pc, fetchType, fetchPayload, format) {

  _maxPixels = kUnlimitedMaxPixels;
  _maxSimdWidthSupported = 16;

  _pixel.reset();
  _pixel.setCount(1);
}

// ============================================================================
// [BLPipeGen::FetchSolidPart - Init / Fini]
// ============================================================================

void FetchSolidPart::_initPart(x86::Gp& x, x86::Gp& y) noexcept {
  if (_pixel.type() != _pixelType) {
    _pixel.setType(_pixelType);
  }
  else {
    // The type should never change after it's been assigned.
    BL_ASSERT(_pixel.type() == _pixelType);
  }

  BL_UNUSED(x);
  BL_UNUSED(y);
}

void FetchSolidPart::_finiPart() noexcept {}

// ============================================================================
// [BLPipeGen::FetchSolidPart - InitSolidFlags]
// ============================================================================

void FetchSolidPart::initSolidFlags(uint32_t flags) noexcept {
  ScopedInjector injector(cc, &_globalHook);
  Pixel& s = _pixel;

  switch (s.type()) {
    case Pixel::kTypeRGBA:
      if ((flags & (Pixel::kPC | Pixel::kUC | Pixel::kUA | Pixel::kUIA)) && s.pc.empty()) {
        s.pc.init(cc->newXmm("pixel.pc"));
        x86::Vec& pix = s.pc[0];
        pc->v_broadcast_u32(pix, x86::ptr_32(pc->_fetchData));
      }
      break;

    case Pixel::kTypeAlpha:
      if ((flags & (Pixel::kSA | Pixel::kPA | Pixel::kUA | Pixel::kUIA)) && !s.sa.isValid()) {
        s.sa = cc->newUInt32("pixel.sa");
        pc->load8(s.sa, x86::ptr_8(pc->_fetchData, 3));
      }

      if (flags & (Pixel::kPA | Pixel::kUA | Pixel::kUIA) && s.ua.empty()) {
        s.ua.init(cc->newXmm("pixel.ua"));
        pc->v_broadcast_u16(s.ua[0], s.sa);
      }
      break;
  }

  pc->xSatisfySolid(s, flags);
}

// ============================================================================
// [BLPipeGen::FetchSolidPart - Fetch]
// ============================================================================

void FetchSolidPart::fetch1(Pixel& p, uint32_t flags) noexcept {
  BL_ASSERT(_pixel.type() == p.type());

  p.setCount(1);
  if (p.isRGBA()) {
    if (flags & Pixel::kAny) {
      initSolidFlags(flags & Pixel::kAny);
      Pixel& s = _pixel;

      if (flags & Pixel::kImmutable) {
        if (flags & Pixel::kPC ) { p.pc.init(s.pc); }
        if (flags & Pixel::kUC ) { p.uc.init(s.uc); }
        if (flags & Pixel::kUA ) { p.ua.init(s.ua); }
        if (flags & Pixel::kUIA) { p.uia.init(s.uia); }
      }
      else {
        if (flags & Pixel::kPC) {
          p.pc.init(cc->newXmm("p.pc0"));
          pc->v_mov(p.pc[0], s.pc[0]);
        }

        if (flags & Pixel::kUC) {
          p.uc.init(cc->newXmm("p.uc0"));
          pc->v_mov(p.uc[0], s.uc[0]);
        }

        if (flags & Pixel::kUA) {
          p.ua.init(cc->newXmm("p.ua0"));
          pc->v_mov(p.ua[0], s.ua[0]);
        }

        if (flags & Pixel::kUIA) {
          p.uia.init(cc->newXmm("p.uia0"));
          pc->v_mov(p.uia[0], s.uia[0]);
        }
      }
    }
  }
  else if (p.isAlpha()) {
    if (flags & Pixel::kSA) {
      initSolidFlags(Pixel::kSA);
      Pixel& s = _pixel;

      if (flags & Pixel::kImmutable) {
        if (flags & Pixel::kSA ) { p.sa = s.sa; }
      }
      else {
        if (flags & Pixel::kSA) {
          p.sa = cc->newUInt32("p.sa");
          cc->mov(p.sa, s.sa);
        }
      }
    }
  }

  pc->xSatisfyPixel(p, flags);
}

void FetchSolidPart::fetch4(Pixel& p, uint32_t flags) noexcept {
  BL_ASSERT(_pixel.type() == p.type());

  p.setCount(4);
  if (p.isRGBA()) {
    initSolidFlags(flags & (Pixel::kPC | Pixel::kUC | Pixel::kUA | Pixel::kUIA));
    Pixel& s = _pixel;

    uint32_t pCount = 1;
    uint32_t uCount = 2;

    if (flags & Pixel::kImmutable) {
      if (flags & Pixel::kPC) { p.pc.init(s.pc); }
      if (flags & Pixel::kUC) { p.uc.init(s.uc); }
      if (flags & Pixel::kUA) { p.ua.init(s.ua); }
      if (flags & Pixel::kUIA) { p.uia.init(s.uia); }
    }
    else {
      if (flags & Pixel::kPC) {
        pc->newXmmArray(p.pc, pCount, "p.pc");
        pc->v_mov(p.pc, s.pc[0]);
      }

      if (flags & Pixel::kUC) {
        pc->newXmmArray(p.uc, uCount, "p.uc");
        pc->v_mov(p.uc, s.uc[0]);
      }

      if (flags & Pixel::kUA) {
        pc->newXmmArray(p.ua, uCount, "p.ua");
        pc->v_mov(p.ua, s.ua[0]);
      }

      if (flags & Pixel::kUIA) {
        pc->newXmmArray(p.uia, uCount, "p.uia");
        pc->v_mov(p.uia, s.uia[0]);
      }
    }
  }
  else if (p.isAlpha()) {
    initSolidFlags(flags & (Pixel::kPA | Pixel::kUA | Pixel::kUIA));
    Pixel& s = _pixel;

    uint32_t pCount = 1;
    uint32_t uCount = 1;

    if (flags & Pixel::kImmutable) {
      if (flags & Pixel::kPA) { p.pa.init(s.pa); }
      if (flags & Pixel::kUA) { p.ua.init(s.ua); }
      if (flags & Pixel::kUIA) { p.uia.init(s.uia); }
    }
    else {
      if (flags & Pixel::kPA) {
        pc->newXmmArray(p.pa, pCount, "p.pa");
        pc->v_mov(p.pa[0], s.pa[0]);
      }

      if (flags & Pixel::kUA) {
        pc->newXmmArray(p.ua, uCount, "p.ua");
        pc->v_mov(p.ua, s.ua[0]);
      }

      if (flags & Pixel::kUIA) {
        pc->newXmmArray(p.uia, uCount, "p.uia");
        pc->v_mov(p.uia, s.uia[0]);
      }
    }
  }

  pc->xSatisfyPixel(p, flags);
}

} // {BLPipeGen}

#endif
