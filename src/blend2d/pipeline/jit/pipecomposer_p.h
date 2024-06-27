// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_PIPECOMPOSER_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_PIPECOMPOSER_P_H_INCLUDED

#include "../../pipeline/jit/jitbase_p.h"
#include "../../pipeline/jit/pipeprimitives_p.h"
#include "../../pipeline/jit/pipepart_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace bl {
namespace Pipeline {
namespace JIT {

//! Pipeline composer.
//!
//! The purpose of pipeline composer is to create pipeline parts and to compose them together. This functionality
//! was initially part of \ref PipeCompiler, but it was moved out to make pipeline compiler only focus on compiling
//! and composer only focus on combining multiple parts together.
class PipeComposer {
public:
  BL_NONCOPYABLE(PipeComposer)

  //! \name Members
  //! \{

  PipeCompiler& _pc;
  asmjit::Zone& _zone;

  //! \}

  //! \name Construction & Destruction
  //! \{

  explicit PipeComposer(PipeCompiler& pc) noexcept;

  //! \}

  //! \name Pipeline Parts Construction
  //! \{

  template<typename T>
  BL_INLINE T* newPartT() noexcept {
    void* p = _zone.alloc(sizeof(T), 8);
    if (BL_UNLIKELY(!p))
      return nullptr;
    return new(BLInternal::PlacementNew{p}) T(&_pc);
  }

  template<typename T, typename... Args>
  BL_INLINE T* newPartT(Args&&... args) noexcept {
    void* p = _zone.alloc(sizeof(T), 8);
    if (BL_UNLIKELY(!p))
      return nullptr;
    return new(BLInternal::PlacementNew{p}) T(&_pc, BLInternal::forward<Args>(args)...);
  }

  FillPart* newFillPart(FillType fillType, FetchPart* dstPart, CompOpPart* compOpPart) noexcept;
  FetchPart* newFetchPart(FetchType fetchType, FormatExt format) noexcept;
  CompOpPart* newCompOpPart(CompOpExt compOp, FetchPart* dstPart, FetchPart* srcPart) noexcept;

  //! \}
};

} // {JIT}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_PIPECOMPOSER_P_H_INCLUDED
