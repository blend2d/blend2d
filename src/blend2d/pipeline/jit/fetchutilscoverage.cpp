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

// bl::Pipeline::JIT - GlobalAlpha
// ===============================

void GlobalAlpha::_initInternal(PipeCompiler* pc) noexcept {
  _pc = pc;
  _pc->cc->comment("[[Global Alpha]]");
  _hook = pc->cc->cursor();
}

void GlobalAlpha::initFromMem(PipeCompiler* pc, const Mem& mem) noexcept {
  // Can only be initialized once.
  BL_ASSERT(!isInitialized());

  _initInternal(pc);
  _mem = mem;
}

void GlobalAlpha::initFromScalar(PipeCompiler* pc, const Gp& sa) noexcept {
  // Can only be initialized once.
  BL_ASSERT(!isInitialized());

  _initInternal(pc);
  _sa = sa;
}

void GlobalAlpha::initFromPacked(PipeCompiler* pc, const Vec& pa) noexcept {
  // Can only be initialized once.
  BL_ASSERT(!isInitialized());

  _initInternal(pc);
  _pa = pa;
}

void GlobalAlpha::initFromUnpacked(PipeCompiler* pc, const Vec& ua) noexcept {
  // Can only be initialized once.
  BL_ASSERT(!isInitialized());

  _initInternal(pc);
  _ua = ua;
}

const Gp& GlobalAlpha::sa() noexcept {
  BL_ASSERT(isInitialized());

  if (!_sa.isValid()) {
    ScopedInjector injector(_pc->cc, &_hook);
    _sa = _pc->newGp32("ga.sa");

    if (_ua.isValid()) {
      _pc->s_extract_u16(_sa, _ua, 0u);
    }
    else if (_pa.isValid()) {
      _pc->s_extract_u8(_sa, _ua, 0u);
    }
    else {
      _pc->load_u8(_sa, _mem);
    }
  }

  return _sa;
}

const Vec& GlobalAlpha::pa() noexcept {
  BL_ASSERT(isInitialized());

  if (!_pa.isValid()) {
    ScopedInjector injector(_pc->cc, &_hook);
    _pa = _pc->newVec("ga.pa");

    if (_ua.isValid()) {
      _pc->v_packs_i16_u8(_pa, _ua, _ua);
    }
    else if (_sa.isValid()) {
      _pc->v_broadcast_u8z(_pa, _sa);
    }
    else {
      _pc->v_broadcast_u8(_pa, _mem);
    }
  }

  return _pa;
}

const Vec& GlobalAlpha::ua() noexcept {
  BL_ASSERT(isInitialized());

  if (!_ua.isValid()) {
    ScopedInjector injector(_pc->cc, &_hook);
    _ua = _pc->newVec("ga.ua");

    if (_pa.isValid()) {
      _pc->v_cvt_u8_lo_to_u16(_ua, _pa);
    }
    else if (_sa.isValid()) {
      _pc->v_broadcast_u16z(_ua, _sa);
    }
    else {
      _pc->v_broadcast_u16(_ua, _mem);
    }
  }

  return _ua;
}

} // {JIT}
} // {Pipeline}
} // {bl}

#endif // !BL_BUILD_NO_JIT
