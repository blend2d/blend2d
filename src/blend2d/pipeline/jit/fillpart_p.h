// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_FILLPART_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_FILLPART_P_H_INCLUDED

#include "../../pipeline/jit/pipepart_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace BLPipeline {
namespace JIT {

//! Pipeline fill part.
class FillPart : public PipePart {
public:
  enum : uint32_t {
    kIndexDstPart = 0,
    kIndexCompOpPart = 1
  };

  //! Fill type.
  FillType _fillType;
  //! True if this a pure rectangle fill (either axis-aligned or fractional).
  bool _isRectFill;

  FillPart(PipeCompiler* pc, FillType fillType, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept;

  BL_INLINE FetchPixelPtrPart* dstPart() const noexcept {
    return reinterpret_cast<FetchPixelPtrPart*>(_children[kIndexDstPart]);
  }

  BL_INLINE void setDstPart(FetchPixelPtrPart* part) noexcept {
    _children[kIndexDstPart] = reinterpret_cast<PipePart*>(part);
  }

  BL_INLINE CompOpPart* compOpPart() const noexcept {
    return reinterpret_cast<CompOpPart*>(_children[kIndexCompOpPart]);
  }

  BL_INLINE void setCompOpPart(FetchPixelPtrPart* part) noexcept {
    _children[kIndexCompOpPart] = reinterpret_cast<PipePart*>(part);
  }

  //! Returns fill type, see `BLPipeFillType`.
  BL_INLINE FillType fillType() const noexcept { return _fillType; }
  //! Tests whether the fill type matches `fillType`.
  BL_INLINE bool isFillType(FillType fillType) const noexcept { return _fillType == fillType; }

  //! Tests whether fill-type is a pure rectangular fill.
  //!
  //! Rectangle fills have some properties that can be exploited by other parts. For example if a fill is rectangular
  //! the pipeline may recalculate stride of source and destination pointers to address the width. There are currently
  //! many optimizations that individual parts do.
  BL_INLINE bool isRectFill() const noexcept { return _isRectFill; }
  BL_INLINE bool isAnalyticFill() const noexcept { return _fillType == FillType::kAnalytic; }

  //! Compiles the fill part.
  virtual void compile() noexcept = 0;
};

class FillBoxAPart final : public FillPart {
public:
  FillBoxAPart(PipeCompiler* pc, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept;

  void compile() noexcept override;
};

class FillBoxUPart final : public FillPart {
public:
  FillBoxUPart(PipeCompiler* pc, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept;

  void compile() noexcept override;
};

class FillMaskPart final : public FillPart {
public:
  FillMaskPart(PipeCompiler* pc, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept;

  void compile() noexcept override;
  void disadvanceDstPtr(const x86::Gp& dstPtr, const x86::Gp& x, int dstBpp) noexcept;
};

class FillAnalyticPart final : public FillPart {
public:
  FillAnalyticPart(PipeCompiler* pc, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept;

  void compile() noexcept override;

  void accumulateCoverages(const x86::Vec& cov) noexcept;
  void normalizeCoverages(const x86::Vec& cov) noexcept;

  //! Calculates masks for 4 pixels - this works for both NonZero and EvenOdd fill rules.
  void calcMasksFromCells(const x86::Vec& dst, const x86::Vec& cov, const x86::Vec& fillRuleMask, const x86::Vec& globalAlpha) noexcept;

  //! Emits the following:
  //!
  //! ```
  //! dstPtr -= x * dstBpp;
  //! cellPtr -= x * 4;
  //! ```
  void disadvanceDstPtrAndCellPtr(const x86::Gp& dstPtr, const x86::Gp& cellPtr, const x86::Gp& x, uint32_t dstBpp) noexcept;
};

} // {JIT}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_FILLPART_P_H_INCLUDED
