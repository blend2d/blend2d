// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_FETCHUTILSCOVERAGE_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_FETCHUTILSCOVERAGE_P_H_INCLUDED

#include "../../pipeline/jit/pipecompiler_p.h"
#include "../../pipeline/jit/pipeprimitives_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace bl {
namespace Pipeline {
namespace JIT {
namespace FetchUtils {

//! Provides unpacked global alpha mask; can be used by \ref FillPart and \ref CompOpPart as a global alpha abstraction.
class GlobalAlpha {
public:
  //! \name Members
  //! \{

  //! Pipeline compiler.
  PipeCompiler* _pc = nullptr;
  //! Node where to emit additional code in case `sm` is not initialized, but required.
  asmjit::BaseNode* _hook = nullptr;

  //! Global alpha as scalar (only used by scalar alpha-only processing operations).
  Gp _sm;
  //! Unpacked global alpha as vector.
  Vec _vm;

  //! \}

  //! \name Initialization
  //! \{

  BL_INLINE void initFromMem(PipeCompiler* pc, const Mem& mem, PixelCoverageFormat coverageFormat) noexcept {
    _pc = pc;
    _vm = pc->newVec("ga.vec");

    if (coverageFormat == PixelCoverageFormat::kPacked)
      _pc->v_broadcast_u8(_vm, mem);
    else
      _pc->v_broadcast_u16(_vm, mem);

    _hook = pc->cc->cursor();
  }

  BL_INLINE void initFromVec(PipeCompiler* pc, const Vec& vm) noexcept {
    _pc = pc;
    _hook = pc->cc->cursor();
    _vm = vm;
  }

  //! Returns whether global alpha is initialized and should be applied
  BL_INLINE_NODEBUG bool isInitialized() const noexcept { return _hook != nullptr; }

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE_NODEBUG const Vec& vm() const noexcept { return _vm; }

  BL_NOINLINE const Gp& sm() noexcept {
    if (_vm.isValid() && !_sm.isValid()) {
      ScopedInjector injector(_pc->cc, &_hook);
      _sm = _pc->newGp32("ga.sm");

#if defined(BL_JIT_ARCH_A64)
      _pc->s_extract_u8(_sm, _vm, 0u);
#else
      _pc->s_extract_u16(_sm, _vm, 0u);
#endif
    }

    return _sm;
  }

  //! \}
};

} // {FetchUtils}
} // {JIT}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_FETCHUTILSCOVERAGE_P_H_INCLUDED
