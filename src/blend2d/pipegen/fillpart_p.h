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

#ifndef BLEND2D_PIPEGEN_FILLPART_P_H_INCLUDED
#define BLEND2D_PIPEGEN_FILLPART_P_H_INCLUDED

#include "../pipegen/pipepart_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_pipegen
//! \{

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::FillPart]
// ============================================================================

//! Pipeline fill part.
class FillPart : public PipePart {
public:
  BL_NONCOPYABLE(FillPart)

  enum : uint32_t {
    kIndexDstPart = 0,
    kIndexCompOpPart = 1
  };

  //! Fill type.
  uint8_t _fillType;
  //! True if this a pure rectangle fill (either axis-aligned or fractional).
  bool _isRectFill;

  FillPart(PipeCompiler* pc, uint32_t fillType, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept;

  inline FetchPixelPtrPart* dstPart() const noexcept {
    return reinterpret_cast<FetchPixelPtrPart*>(_children[kIndexDstPart]);
  }

  inline void setDstPart(FetchPixelPtrPart* part) noexcept {
    _children[kIndexDstPart] = reinterpret_cast<PipePart*>(part);
  }

  inline CompOpPart* compOpPart() const noexcept {
    return reinterpret_cast<CompOpPart*>(_children[kIndexCompOpPart]);
  }

  inline void setCompOpPart(FetchPixelPtrPart* part) noexcept {
    _children[kIndexCompOpPart] = reinterpret_cast<PipePart*>(part);
  }

  //! Returns fill type, see `BLPipeFillType`.
  inline uint32_t fillType() const noexcept { return _fillType; }
  //! Tests whether the fill type matches `fillType`.
  inline bool isFillType(uint32_t fillType) const noexcept { return _fillType == fillType; }

  //! Tests whether fill-type is a pure rectangular fill (aligned or fractional).
  //!
  //! Rectangle fills have some properties that can be exploited by other parts.
  inline bool isRectFill() const noexcept { return _isRectFill; }
  inline bool isAnalyticFill() const noexcept { return _fillType == BL_PIPE_FILL_TYPE_ANALYTIC; }

  //! Compiles the fill part.
  virtual void compile() noexcept = 0;
};

// ============================================================================
// [BLPipeGen::FillBoxAPart]
// ============================================================================

class FillBoxAPart final : public FillPart {
public:
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  FillBoxAPart(PipeCompiler* pc, uint32_t fillType, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept;

  // --------------------------------------------------------------------------
  // [Compile]
  // --------------------------------------------------------------------------

  void compile() noexcept override;
};

// ============================================================================
// [BLPipeGen::FillBoxUPart]
// ============================================================================

class FillBoxUPart final : public FillPart {
public:
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  FillBoxUPart(PipeCompiler* pc, uint32_t fillType, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept;

  // --------------------------------------------------------------------------
  // [Compile]
  // --------------------------------------------------------------------------

  void compile() noexcept override;
};

// ============================================================================
// [BLPipeGen::FillAnalyticPart]
// ============================================================================

class FillAnalyticPart final : public FillPart {
public:
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  FillAnalyticPart(PipeCompiler* pc, uint32_t fillType, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept;

  // --------------------------------------------------------------------------
  // [Compile]
  // --------------------------------------------------------------------------

  void compile() noexcept override;

  //! Adds covers held by `val` to the accumulator `acc`.
  void accumulateCells(const x86::Vec& acc, const x86::Vec& val) noexcept;

  //! Calculates masks for 4 pixels - this works for both NonZero and EvenOdd
  //! fill rules.
  void calcMasksFromCells(const x86::Vec& dst, const x86::Vec& src, const x86::Vec& fillRuleMask, const x86::Vec& globalAlpha, bool unpack) noexcept;

  //! Emits the following:
  //!
  //! ```
  //! dstPtr -= x * dstBpp;
  //! cellPtr -= x * 4;
  //! ```
  void disadvanceDstPtrAndCellPtr(const x86::Gp& dstPtr, const x86::Gp& cellPtr, const x86::Gp& x, int dstBpp) noexcept;
};

} // {BLPipeGen}

//! \}
//! \endcond

#endif // BLEND2D_PIPEGEN_FILLPART_P_H_INCLUDED
