// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_FILLPART_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_FILLPART_P_H_INCLUDED

#include "../../pipeline/jit/pipecompiler_p.h"
#include "../../pipeline/jit/pipepart_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace bl {
namespace Pipeline {
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

  FillPart(PipeCompiler* pc, FillType fillType, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept;

  BL_INLINE_NODEBUG FetchPixelPtrPart* dstPart() const noexcept {
    return reinterpret_cast<FetchPixelPtrPart*>(_children[kIndexDstPart]);
  }

  BL_INLINE_NODEBUG void setDstPart(FetchPixelPtrPart* part) noexcept {
    _children[kIndexDstPart] = reinterpret_cast<PipePart*>(part);
  }

  BL_INLINE_NODEBUG CompOpPart* compOpPart() const noexcept {
    return reinterpret_cast<CompOpPart*>(_children[kIndexCompOpPart]);
  }

  BL_INLINE_NODEBUG void setCompOpPart(FetchPixelPtrPart* part) noexcept {
    _children[kIndexCompOpPart] = reinterpret_cast<PipePart*>(part);
  }

  //! Returns fill type, see \ref FillType.
  BL_INLINE_NODEBUG FillType fillType() const noexcept { return _fillType; }
  //! Tests whether the fill type matches `fillType`.
  BL_INLINE_NODEBUG bool isFillType(FillType fillType) const noexcept { return _fillType == fillType; }

  //! Tests whether fill-type is a pure rectangular fill.
  //!
  //! Rectangle fills have some properties that can be exploited by other parts. For example if a fill is rectangular
  //! the pipeline may recalculate stride of source and destination pointers to address the width. There are currently
  //! many optimizations that individual parts do.
  BL_INLINE_NODEBUG bool isAnalyticFill() const noexcept { return _fillType == FillType::kAnalytic; }

  //! Compiles the fill part.
  virtual void compile(const PipeFunction& fn) noexcept;
};

class FillBoxAPart final : public FillPart {
public:
  FillBoxAPart(PipeCompiler* pc, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept;

  void compile(const PipeFunction& fn) noexcept override;
};

class FillMaskPart final : public FillPart {
public:
  FillMaskPart(PipeCompiler* pc, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept;

  void compile(const PipeFunction& fn) noexcept override;
  void deadvanceDstPtr(const Gp& dstPtr, const Gp& x, int dstBpp) noexcept;
};

class FillAnalyticPart final : public FillPart {
public:
  FillAnalyticPart(PipeCompiler* pc, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept;

  void compile(const PipeFunction& fn) noexcept override;

  BL_INLINE_NODEBUG VecWidth vecWidth() const noexcept { return blMin(pc->vecWidth(), VecWidth::k256); }

  inline void countZeros(const Gp& dst, const Gp& src) noexcept {
    OpcodeRR op = BitOrder::kPrivate == BitOrder::kLSB ? OpcodeRR::kCTZ : OpcodeRR::kCLZ;
    pc->emit_2i(op, dst, src);
  }

  inline void shiftMask(const Gp& dst, const Gp& src1, const Gp& src2) noexcept {
    OpcodeRRR op = BitOrder::kPrivate == BitOrder::kLSB ? OpcodeRRR::kSll : OpcodeRRR::kSrl;
    pc->emit_3i(op, dst, src1, src2);
  }

  void accumulateCoverages(const Vec& acc) noexcept;
  void normalizeCoverages(const Vec& acc) noexcept;

  //! Calculates masks for 4 pixels - this works for both NonZero and EvenOdd fill rules.
  void calcMasksFromCells(const Vec& msk, const Vec& acc, const Vec& fillRuleMask, const Vec& globalAlpha) noexcept;

  //! Expands the calculated mask in a way so it can be used by the compositor.
  void expandMask(const VecArray& msk, PixelCount pixelCount) noexcept;

  //! Emits the following:
  //!
  //! ```
  //! dstPtr -= x * dstBpp;
  //! cellPtr -= x * 4;
  //! ```
  void deadvanceDstPtrAndCellPtr(const Gp& dstPtr, const Gp& cellPtr, const Gp& x, uint32_t dstBpp) noexcept;
};

} // {JIT}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_FILLPART_P_H_INCLUDED
