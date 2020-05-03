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

#ifndef BLEND2D_PIPEGEN_FETCHPIXELPTRPART_P_H_INCLUDED
#define BLEND2D_PIPEGEN_FETCHPIXELPTRPART_P_H_INCLUDED

#include "../pipegen/fetchpart_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_pipegen
//! \{

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::FetchPixelPtrPart]
// ============================================================================

//! Pipeline fetch pixel-pointer part.
class FetchPixelPtrPart : public FetchPart {
public:
  BL_NONCOPYABLE(FetchPixelPtrPart)

  //! Pixel pointer.
  x86::Gp _ptr;
  //! Pixel pointer alignment (updated by FillPart|CompOpPart).
  uint8_t _ptrAlignment;

  FetchPixelPtrPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept;

  //! Initializes the pixel pointer to `p`.
  inline void initPtr(const x86::Gp& p) noexcept { _ptr = p; }
  //! Returns the pixel-pointer.
  inline x86::Gp& ptr() noexcept { return _ptr; }

  //! Returns the pixel-pointer alignment.
  inline uint32_t ptrAlignment() const noexcept { return _ptrAlignment; }
  //! Sets the pixel-pointer alignment.
  inline void setPtrAlignment(uint32_t alignment) noexcept { _ptrAlignment = uint8_t(alignment); }

  void fetch1(Pixel& p, uint32_t flags) noexcept override;
  void fetch4(Pixel& p, uint32_t flags) noexcept override;
  void fetch8(Pixel& p, uint32_t flags) noexcept override;
};

} // {BLPipeGen}

//! \}
//! \endcond

#endif // BLEND2D_PIPEGEN_FETCHPIXELPTRPART_P_H_INCLUDED
