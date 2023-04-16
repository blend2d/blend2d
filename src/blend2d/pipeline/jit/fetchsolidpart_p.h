// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_FETCHSOLIDPART_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_FETCHSOLIDPART_P_H_INCLUDED

#include "../../pipeline/jit/fetchpart_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace BLPipeline {
namespace JIT {

//! Pipeline solid-fetch part.
class FetchSolidPart : public FetchPart {
public:
  //! Source pixel, expanded to the whole register if necessary.
  Pixel _pixel;

  FetchSolidPart(PipeCompiler* pc, BLInternalFormat format) noexcept;

  void preparePart() noexcept override;

  void _initPart(x86::Gp& x, x86::Gp& y) noexcept override;
  void _finiPart() noexcept override;

  //! Injects code at the beginning of the pipeline that is required to prepare the requested variables that will
  //! be used by a special compositor that can composite the destination with solid pixels. Multiple calls to
  //! `prepareSolid()` are allowed and this feature is used to setup variables required by various parts of the
  //! pipeline.
  //!
  //! \note Initialization means code injection, calling `prepareSolid()` will not emit any code at the current
  //! position, it will instead inject code to the position saved by `init()`.
  void initSolidFlags(PixelFlags flags) noexcept;

  void fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept override;
};

} // {JIT}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_FETCHSOLIDPART_P_H_INCLUDED
