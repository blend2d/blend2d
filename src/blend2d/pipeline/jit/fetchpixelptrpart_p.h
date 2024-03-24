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

namespace bl {
namespace Pipeline {
namespace JIT {

//! Pipeline fetch pixel-pointer part.
class FetchPixelPtrPart : public FetchPart {
public:
  //! Pixel pointer.
  Gp _ptr;
  //! Pixel pointer alignment (updated by FillPart|CompOpPart).
  Alignment _alignment{1};

  FetchPixelPtrPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept;

  //! Initializes the pixel pointer to `p`.
  BL_INLINE_NODEBUG void initPtr(const Gp& p) noexcept { _ptr = p; }
  //! Returns the pixel-pointer.
  BL_INLINE_NODEBUG Gp& ptr() noexcept { return _ptr; }

  //! Returns the pixel-pointer alignment.
  BL_INLINE_NODEBUG Alignment alignment() const noexcept { return _alignment; }
  //! Sets the pixel-pointer alignment to `alignment`.
  BL_INLINE_NODEBUG void setAlignment(Alignment alignment) noexcept { _alignment = alignment; }
  //! Resets the pixel-pointer alignment to 1 (no alignment)
  BL_INLINE_NODEBUG void resetAlignment() noexcept { _alignment = 1; }

  void fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept override;
};

} // {JIT}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_FETCHPIXELPTRPART_P_H_INCLUDED
