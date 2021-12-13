// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_FETCHPIXELPTRPART_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_FETCHPIXELPTRPART_P_H_INCLUDED

#include "../../pipeline/jit/fetchpart_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace BLPipeline {
namespace JIT {

//! Pipeline fetch pixel-pointer part.
class FetchPixelPtrPart : public FetchPart {
public:
  //! Pixel pointer.
  x86::Gp _ptr;
  //! Pixel pointer alignment (updated by FillPart|CompOpPart).
  uint8_t _ptrAlignment = 0;

  FetchPixelPtrPart(PipeCompiler* pc, FetchType fetchType, uint32_t format) noexcept;

  //! Initializes the pixel pointer to `p`.
  BL_INLINE void initPtr(const x86::Gp& p) noexcept { _ptr = p; }
  //! Returns the pixel-pointer.
  BL_INLINE x86::Gp& ptr() noexcept { return _ptr; }

  //! Returns the pixel-pointer alignment.
  BL_INLINE uint32_t ptrAlignment() const noexcept { return _ptrAlignment; }
  //! Sets the pixel-pointer alignment.
  BL_INLINE void setPtrAlignment(uint32_t alignment) noexcept { _ptrAlignment = uint8_t(alignment); }

  void fetch1(Pixel& p, PixelFlags flags) noexcept override;
  void fetch4(Pixel& p, PixelFlags flags) noexcept override;
  void fetch8(Pixel& p, PixelFlags flags) noexcept override;
};

} // {JIT}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_FETCHPIXELPTRPART_P_H_INCLUDED
