// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_FETCHUTILSINLINELOOPS_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_FETCHUTILSINLINELOOPS_P_H_INCLUDED

#include "../../pipeline/jit/pipecompiler_p.h"
#include "../../pipeline/jit/pipeprimitives_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace bl {
namespace Pipeline {
namespace JIT {
namespace FetchUtils {

// bl::Pipeline::Jit::FetchUtils -- FillSpan & FillRect Loops
// ==========================================================

void inlineFillSpanLoop(
  PipeCompiler* pc,
  Gp dst,
  Vec src,
  Gp i,
  uint32_t mainLoopSize, uint32_t itemSize, uint32_t itemGranularity) noexcept;

void inlineFillRectLoop(
  PipeCompiler* pc,
  Gp dstPtr,
  Gp stride,
  Gp w,
  Gp h,
  Vec src,
  uint32_t itemSize,
  const Label& end) noexcept;

// bl::Pipeline::Jit::FetchUtils -- CopySpan & CopyRect Loops
// ==========================================================

void inlineCopySpanLoop(
  PipeCompiler* pc,
  Gp dst,
  Gp src,
  Gp i,
  uint32_t mainLoopSize, uint32_t itemSize, uint32_t itemGranularity, FormatExt format) noexcept;

} // {FetchUtils}
} // {JIT}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_FETCHUTILSINLINELOOPS_P_H_INCLUDED
