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

FetchSolidPart::FetchSolidPart(PipeCompiler* pc, BLInternalFormat format) noexcept
  : FetchPart(pc, FetchType::kSolid, format),
    _pixel("solid") {

  // Advancing has no cost.
  _partFlags |= PipePartFlags::kAdvanceXIsSimple;
  // Solid fetcher doesn't access memory, so masked access is always available.
  _partFlags |= PipePartFlags::kMaskedAccess;

  _maxPixels = kUnlimitedMaxPixels;
  _maxSimdWidthSupported = SimdWidth::k512;
  _pixel.setCount(PixelCount(1));
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
    case PixelType::kA8: {
      if (blTestFlag(flags, PixelFlags::kSA | PixelFlags::kPA | PixelFlags::kUA | PixelFlags::kUI) && !s.sa.isValid()) {
        s.sa = cc->newUInt32("solid.sa");
        pc->i_load_u8(s.sa, x86::ptr_8(pc->_fetchData, 3));
      }

      if (blTestFlag(flags, PixelFlags::kPA | PixelFlags::kUA | PixelFlags::kUI) && s.ua.empty()) {
        s.ua.init(pc->newVec("solid.ua"));
        pc->v_broadcast_u16(s.ua[0], s.sa);
      }
      break;
    }

    case PixelType::kRGBA32: {
      if (blTestFlag(flags, PixelFlags::kPC | PixelFlags::kUC | PixelFlags::kUA | PixelFlags::kUI) && s.pc.empty()) {
        s.pc.init(pc->newVec("solid.pc"));
        pc->v_broadcast_u32(s.pc[0], x86::ptr_32(pc->_fetchData));
      }
      break;
    }

    default:
      BL_NOT_REACHED();
  }

  pc->x_satisfy_solid(s, flags);
}

// BLPipeline::JIT::FetchSolidPart - Fetch
// =======================================

void FetchSolidPart::fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept {
  BL_ASSERT(_pixel.type() == p.type());
  blUnused(predicate);

  p.setCount(n);
  Pixel& s = _pixel;

  switch (p.type()) {
    case PixelType::kA8: {
      if (n == 1) {
        if (blTestFlag(flags, PixelFlags::kSA)) {
          initSolidFlags(PixelFlags::kSA);

          if (blTestFlag(flags, PixelFlags::kImmutable)) {
            if (blTestFlag(flags, PixelFlags::kSA)) {
              p.sa = s.sa;
            }
          }
          else {
            if (blTestFlag(flags, PixelFlags::kSA)) {
              p.sa = cc->newUInt32("%ssa", p.name());
              cc->mov(p.sa, s.sa);
            }
          }
        }
      }
      else {
        initSolidFlags(flags & (PixelFlags::kPA | PixelFlags::kUA | PixelFlags::kUI));

        SimdWidth paSimdWidth = pc->simdWidthOf(DataWidth::k8, n);
        SimdWidth uaSimdWidth = pc->simdWidthOf(DataWidth::k16, n);

        uint32_t paCount = pc->regCountOf(DataWidth::k8, n);
        uint32_t uaCount = pc->regCountOf(DataWidth::k16, n);

        if (blTestFlag(flags, PixelFlags::kImmutable)) {
          if (blTestFlag(flags, PixelFlags::kPA)) { p.pa = s.pa.cloneAs(paSimdWidth); }
          if (blTestFlag(flags, PixelFlags::kUA)) { p.ua = s.ua.cloneAs(uaSimdWidth); }
          if (blTestFlag(flags, PixelFlags::kUI)) { p.ui = s.ui.cloneAs(uaSimdWidth); }
        }
        else {
          if (blTestFlag(flags, PixelFlags::kPA)) {
            pc->newVecArray(p.pa, paCount, paSimdWidth, p.name(), "pa");
            pc->v_mov(p.pa, s.pa[0].cloneAs(p.pa[0]));
          }

          if (blTestFlag(flags, PixelFlags::kUA)) {
            pc->newVecArray(p.ua, uaCount, uaSimdWidth, p.name(), "ua");
            pc->v_mov(p.ua, s.ua[0].cloneAs(p.ua[0]));
          }

          if (blTestFlag(flags, PixelFlags::kUI)) {
            pc->newVecArray(p.ui, uaCount, uaSimdWidth, p.name(), "ui");
            pc->v_mov(p.ui, s.ui[0].cloneAs(p.ui[0]));
          }
        }
      }
      break;
    }

    case PixelType::kRGBA32: {
      initSolidFlags(flags & (PixelFlags::kPC | PixelFlags::kUC | PixelFlags::kUA | PixelFlags::kUI));

      SimdWidth pcWidth = pc->simdWidthOf(DataWidth::k32, n);
      SimdWidth ucWidth = pc->simdWidthOf(DataWidth::k64, n);

      uint32_t pcCount = pc->regCountOf(DataWidth::k32, n);
      uint32_t ucCount = pc->regCountOf(DataWidth::k64, n);

      if (blTestFlag(flags, PixelFlags::kImmutable)) {
        if (blTestFlag(flags, PixelFlags::kPC)) { p.pc = s.pc.cloneAs(pcWidth); }
        if (blTestFlag(flags, PixelFlags::kUC)) { p.uc = s.uc.cloneAs(ucWidth); }
        if (blTestFlag(flags, PixelFlags::kUA)) { p.ua = s.ua.cloneAs(ucWidth); }
        if (blTestFlag(flags, PixelFlags::kUI)) { p.ui = s.ui.cloneAs(ucWidth); }
      }
      else {
        if (blTestFlag(flags, PixelFlags::kPC)) {
          pc->newVecArray(p.pc, pcCount, pcWidth, p.name(), "pc");
          pc->v_mov(p.pc, s.pc[0].cloneAs(p.pc[0]));
        }

        if (blTestFlag(flags, PixelFlags::kUC)) {
          pc->newVecArray(p.uc, ucCount, ucWidth, p.name(), "uc");
          pc->v_mov(p.uc, s.uc[0].cloneAs(p.uc[0]));
        }

        if (blTestFlag(flags, PixelFlags::kUA)) {
          pc->newVecArray(p.ua, ucCount, ucWidth, p.name(), "ua");
          pc->v_mov(p.ua, s.ua[0].cloneAs(p.ua[0]));
        }

        if (blTestFlag(flags, PixelFlags::kUI)) {
          pc->newVecArray(p.ui, ucCount, ucWidth, p.name(), "ui");
          pc->v_mov(p.ui, s.ui[0].cloneAs(p.ui[0]));
        }
      }

      break;
    }

    default:
      BL_NOT_REACHED();
  }

  pc->x_satisfy_pixel(p, flags);
}

} // {JIT}
} // {BLPipeline}

#endif
