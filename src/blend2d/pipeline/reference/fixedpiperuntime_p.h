// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_REFERENCE_FIXEDPIPERUNTIME_P_H_INCLUDED
#define BLEND2D_PIPELINE_REFERENCE_FIXEDPIPERUNTIME_P_H_INCLUDED

#include "../../pipeline/pipedefs_p.h"
#include "../../pipeline/piperuntime_p.h"
#include "../../support/wrap_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_reference
//! \{

namespace BLPipeline {

//! Static pipline runtime.
//!
//! Static runtime is a runtime without JIT capability.
class PipeStaticRuntime : public PipeRuntime {
public:
  PipeStaticRuntime() noexcept;
  ~PipeStaticRuntime() noexcept;

  static BLWrap<PipeStaticRuntime> _global;
};

} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_REFERENCE_FIXEDPIPERUNTIME_P_H_INCLUDED
