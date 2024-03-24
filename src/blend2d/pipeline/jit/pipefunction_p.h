// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_PIPEFUNCTION_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_PIPEFUNCTION_P_H_INCLUDED

#include "../../pipeline/jit/jitbase_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace bl {
namespace Pipeline {
namespace JIT {

class PipeCompiler;
class PipePart;

class PipeFunction {
public:
  BL_NONCOPYABLE(PipeFunction)

  //! \name Members
  //! \{

  //! Holds `ctxData` argument.
  Gp _ctxData;
  //! Holds `fillData` argument.
  Gp _fillData;
  //! Holds `fetchData` argument.
  Gp _fetchData;

  //! \}

  //! \name Construction & Destruction
  //! \{

  PipeFunction() noexcept;

  //! \}

  //! \name Accessors
  //! \{

  //! Returns `ctxData` arguments of the pipeline function.
  BL_INLINE_NODEBUG const Gp& ctxData() const noexcept { return _ctxData; }
  //! Returns `fillData` arguments of the pipeline function.
  BL_INLINE_NODEBUG const Gp& fillData() const noexcept { return _fillData; }
  //! Returns `fetchData` arguments of the pipeline function.
  BL_INLINE_NODEBUG const Gp& fetchData() const noexcept { return _fetchData; }

  //! \}

  //! \name Interface
  //! \{

  //! Prepares all parts of the pipeline and configures the pipeline compiler for agreed simd width.
  void prepare(PipeCompiler& pc, PipePart* root) noexcept;

  void beginFunction(PipeCompiler& pc) noexcept;
  void endFunction(PipeCompiler& pc) noexcept;

  //! \}
};

} // {JIT}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_PIPEFUNCTION_P_H_INCLUDED
