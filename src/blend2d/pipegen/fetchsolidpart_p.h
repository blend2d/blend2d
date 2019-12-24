// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_PIPEGEN_FETCHSOLIDPART_P_H
#define BLEND2D_PIPEGEN_FETCHSOLIDPART_P_H

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

#endif // BLEND2D_PIPEGEN_FETCHSOLIDPART_P_H
