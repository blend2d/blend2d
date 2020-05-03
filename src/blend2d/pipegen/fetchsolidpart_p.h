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

#ifndef BLEND2D_PIPEGEN_FETCHSOLIDPART_P_H_INCLUDED
#define BLEND2D_PIPEGEN_FETCHSOLIDPART_P_H_INCLUDED

#include "../pipegen/fetchpart_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_pipegen
//! \{

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::FetchSolidPart]
// ============================================================================

//! Pipeline solid-fetch part.
class FetchSolidPart : public FetchPart {
public:
  BL_NONCOPYABLE(FetchSolidPart)

  //! Source pixel, expanded to the whole register if necessary.
  Pixel _pixel;

  FetchSolidPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept;

  void _initPart(x86::Gp& x, x86::Gp& y) noexcept override;
  void _finiPart() noexcept override;

  //! Injects code at the beginning of the pipeline that is required to prepare
  //! the requested variables that will be used by a special compositor that can
  //! composite the destination with solid pixels. Multiple calls to `prepareSolid()`
  //! are allowed and this feature is used to setup variables required by various
  //! parts of the pipeline.
  //!
  //! \note Initialization means code injection, calling `prepareSolid()` will
  //! not emit any code at the current position, it will instead inject code to
  //! the position saved by `init()`.
  void initSolidFlags(uint32_t flags) noexcept;

  void fetch1(Pixel& p, uint32_t flags) noexcept override;
  void fetch4(Pixel& p, uint32_t flags) noexcept override;
};

} // {BLPipeGen}

//! \}
//! \endcond

#endif // BLEND2D_PIPEGEN_FETCHSOLIDPART_P_H_INCLUDED
