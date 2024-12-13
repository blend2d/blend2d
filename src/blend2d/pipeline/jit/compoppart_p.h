// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_COMPOPPART_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_COMPOPPART_P_H_INCLUDED

#include "../../compopinfo_p.h"
#include "../../pipeline/jit/fetchpart_p.h"
#include "../../pipeline/jit/fetchutilscoverage_p.h"
#include "../../pipeline/jit/pipepart_p.h"
#include "../../pipeline/jit/pipeprimitives_p.h"
#include "../../support/wrap_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace bl {
namespace Pipeline {
namespace JIT {

//! Pipeline combine part.
class CompOpPart : public PipePart {
public:
  static constexpr uint32_t kIndexDstPart = 0;
  static constexpr uint32_t kIndexSrcPart = 1;

  //! \name Members
  //! \{

  //! Composition operator.
  CompOpExt _compOp{};
  //! Pixel type of the composition.
  PixelType _pixelType = PixelType::kNone;
  //! The current span mode.
  CMaskLoopType _cMaskLoopType = CMaskLoopType::kNone;
  //! Pixel coverage format expected by the compositor.
  PixelCoverageFormat _coverageFormat = PixelCoverageFormat::kNone;
  //! Maximum pixels the compositor can handle at a time.
  uint8_t _maxPixels = 0;
  //! Pixel granularity.
  PixelCount _pixelGranularity {};
  //! Minimum alignment required to process `_maxPixels`.
  Alignment _minAlignment {1};

  uint8_t _isInPartialMode : 1;
  //! Whether the destination format has an alpha component.
  uint8_t _hasDa : 1;
  //! Whether the source format has an alpha component.
  uint8_t _hasSa : 1;

  //! A hook that is used by the current loop.
  asmjit::BaseNode* _cMaskLoopHook = nullptr;
  //! Optimized solid pixel for operators that allow it.
  SolidPixel _solidOpt;
  //! Pre-processed solid pixel for TypeA operators that always use `vMaskProc?()`.
  Pixel _solidPre {};
  //! Partial fetch that happened at the end of the scanline (border case).
  Pixel _partialPixel {};
  //! Const mask.
  Wrap<PipeCMask> _mask;

  //! \}

  //! \name Construction & Destruction
  //! \{

  CompOpPart(PipeCompiler* pc, CompOpExt compOp, FetchPart* dstPart, FetchPart* srcPart) noexcept;

  //! \}

  //! \name Children
  //! \{

  BL_INLINE_NODEBUG FetchPart* dstPart() const noexcept { return reinterpret_cast<FetchPart*>(_children[kIndexDstPart]); }
  BL_INLINE_NODEBUG FetchPart* srcPart() const noexcept { return reinterpret_cast<FetchPart*>(_children[kIndexSrcPart]); }

  //! \}

  //! \name Accessors
  //! \{

  //! Returns the composition operator id, see \ref BLCompOp.
  BL_INLINE_NODEBUG CompOpExt compOp() const noexcept { return _compOp; }

  BL_INLINE_NODEBUG bool isSrcCopy() const noexcept { return _compOp == CompOpExt::kSrcCopy; }
  BL_INLINE_NODEBUG bool isSrcOver() const noexcept { return _compOp == CompOpExt::kSrcOver; }
  BL_INLINE_NODEBUG bool isSrcIn() const noexcept { return _compOp == CompOpExt::kSrcIn; }
  BL_INLINE_NODEBUG bool isSrcOut() const noexcept { return _compOp == CompOpExt::kSrcOut; }
  BL_INLINE_NODEBUG bool isSrcAtop() const noexcept { return _compOp == CompOpExt::kSrcAtop; }
  BL_INLINE_NODEBUG bool isDstCopy() const noexcept { return _compOp == CompOpExt::kDstCopy; }
  BL_INLINE_NODEBUG bool isDstOver() const noexcept { return _compOp == CompOpExt::kDstOver; }
  BL_INLINE_NODEBUG bool isDstIn() const noexcept { return _compOp == CompOpExt::kDstIn; }
  BL_INLINE_NODEBUG bool isDstOut() const noexcept { return _compOp == CompOpExt::kDstOut; }
  BL_INLINE_NODEBUG bool isDstAtop() const noexcept { return _compOp == CompOpExt::kDstAtop; }
  BL_INLINE_NODEBUG bool isXor() const noexcept { return _compOp == CompOpExt::kXor; }
  BL_INLINE_NODEBUG bool isPlus() const noexcept { return _compOp == CompOpExt::kPlus; }
  BL_INLINE_NODEBUG bool isMinus() const noexcept { return _compOp == CompOpExt::kMinus; }
  BL_INLINE_NODEBUG bool isModulate() const noexcept { return _compOp == CompOpExt::kModulate; }
  BL_INLINE_NODEBUG bool isMultiply() const noexcept { return _compOp == CompOpExt::kMultiply; }
  BL_INLINE_NODEBUG bool isScreen() const noexcept { return _compOp == CompOpExt::kScreen; }
  BL_INLINE_NODEBUG bool isOverlay() const noexcept { return _compOp == CompOpExt::kOverlay; }
  BL_INLINE_NODEBUG bool isDarken() const noexcept { return _compOp == CompOpExt::kDarken; }
  BL_INLINE_NODEBUG bool isLighten() const noexcept { return _compOp == CompOpExt::kLighten; }
  BL_INLINE_NODEBUG bool isColorDodge() const noexcept { return _compOp == CompOpExt::kColorDodge; }
  BL_INLINE_NODEBUG bool isColorBurn() const noexcept { return _compOp == CompOpExt::kColorBurn; }
  BL_INLINE_NODEBUG bool isLinearBurn() const noexcept { return _compOp == CompOpExt::kLinearBurn; }
  BL_INLINE_NODEBUG bool isLinearLight() const noexcept { return _compOp == CompOpExt::kLinearLight; }
  BL_INLINE_NODEBUG bool isPinLight() const noexcept { return _compOp == CompOpExt::kPinLight; }
  BL_INLINE_NODEBUG bool isHardLight() const noexcept { return _compOp == CompOpExt::kHardLight; }
  BL_INLINE_NODEBUG bool isSoftLight() const noexcept { return _compOp == CompOpExt::kSoftLight; }
  BL_INLINE_NODEBUG bool isDifference() const noexcept { return _compOp == CompOpExt::kDifference; }
  BL_INLINE_NODEBUG bool isExclusion() const noexcept { return _compOp == CompOpExt::kExclusion; }

  BL_INLINE_NODEBUG bool isAlphaInv() const noexcept { return _compOp == CompOpExt::kAlphaInv; }

  //! Returns the composition operator flags.
  BL_INLINE_NODEBUG CompOpFlags compOpFlags() const noexcept { return compOpInfoTable[size_t(_compOp)].flags(); }
  //! Returns a pixel coverage format, which must be honored when calling the composition API.
  BL_INLINE_NODEBUG PixelCoverageFormat coverageFormat() const noexcept { return _coverageFormat; }

  //! Tests whether the destination pixel format has an alpha component.
  BL_INLINE_NODEBUG bool hasDa() const noexcept { return _hasDa != 0; }
  //! Tests whether the source pixel format has an alpha component.
  BL_INLINE_NODEBUG bool hasSa() const noexcept { return _hasSa != 0; }

  BL_INLINE_NODEBUG PixelType pixelType() const noexcept { return _pixelType; }
  BL_INLINE_NODEBUG bool isA8Pixel() const noexcept { return _pixelType == PixelType::kA8; }
  BL_INLINE_NODEBUG bool isRGBA32Pixel() const noexcept { return _pixelType == PixelType::kRGBA32; }

  //! Returns the current loop mode.
  BL_INLINE_NODEBUG CMaskLoopType cMaskLoopType() const noexcept { return _cMaskLoopType; }
  //! Tests whether the current loop is fully opaque (no mask).
  BL_INLINE_NODEBUG bool isLoopOpaque() const noexcept { return _cMaskLoopType == CMaskLoopType::kOpaque; }
  //! Tests whether the current loop is `CMask` (constant mask).
  BL_INLINE_NODEBUG bool isLoopCMask() const noexcept { return _cMaskLoopType == CMaskLoopType::kVariant; }

  //! Returns the maximum pixels the composite part can handle at a time.
  //!
  //! \note This value is configured in a way that it's always one if the fetch part doesn't support more. This makes
  //! it easy to use it in loop compilers. In other words, the value doesn't describe the real implementation of the
  //! composite part.
  BL_INLINE_NODEBUG uint32_t maxPixels() const noexcept { return _maxPixels; }
  //! Returns the maximum pixels the children of this part can handle.
  BL_INLINE_NODEBUG uint32_t maxPixelsOfChildren() const noexcept { return blMin(dstPart()->maxPixels(), srcPart()->maxPixels()); }

  BL_INLINE void setMaxPixels(uint32_t maxPixels) noexcept {
    BL_ASSERT(maxPixels <= 0xFF);
    _maxPixels = uint8_t(maxPixels);
  }

  //! Returns pixel granularity passed to `init()`, otherwise the result should be zero.
  BL_INLINE_NODEBUG PixelCount pixelGranularity() const noexcept { return _pixelGranularity; }
  //! Returns the minimum destination alignment required to the maximum number of pixels `_maxPixels`.
  BL_INLINE_NODEBUG Alignment minAlignment() const noexcept { return _minAlignment; }

  BL_INLINE_NODEBUG bool isUsingSolidPre() const noexcept { return !_solidPre.pc.empty() || !_solidPre.uc.empty(); }
  BL_INLINE_NODEBUG bool isInPartialMode() const noexcept { return _isInPartialMode != 0; }

  //! \}

  void preparePart() noexcept override;

  //! \name Initialization & Finalization
  //! \{

  void init(const PipeFunction& fn, Gp& x, Gp& y, uint32_t pixelGranularity) noexcept;
  void fini() noexcept;

  //! \}

  //! Tests whether the opaque fill should be optimized and placed into a separate loop. This means that if this
  //! function returns true two composition loops would be generated by the filler.
  bool shouldOptimizeOpaqueFill() const noexcept;

  //! Tests whether the compositor should emit a specialized loop that contains an inlined version of `memcpy()`
  //! or `memset()`.
  bool shouldJustCopyOpaqueFill() const noexcept;

  void startAtX(const Gp& x) noexcept;
  void advanceX(const Gp& x, const Gp& diff) noexcept;
  void advanceY() noexcept;

  // These are just wrappers that call these on both source & destination parts.
  void enterN() noexcept;
  void leaveN() noexcept;
  void prefetchN() noexcept;
  void postfetchN() noexcept;

  void dstFetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept;
  void srcFetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept;

  void enterPartialMode(PixelFlags partialFlags = PixelFlags::kNone) noexcept;
  void exitPartialMode() noexcept;
  void nextPartialPixel() noexcept;

  void cMaskInit(const Mem& mem) noexcept;
  void cMaskInit(const Gp& sm_, const Vec& vm_) noexcept;
  void cMaskInitOpaque() noexcept;
  void cMaskFini() noexcept;

  void _cMaskLoopInit(CMaskLoopType loopType) noexcept;
  void _cMaskLoopFini() noexcept;

  void cMaskGenericLoop(Gp& i) noexcept;
  void cMaskGenericLoopVec(Gp& i) noexcept;

  void cMaskGranularLoop(Gp& i) noexcept;
  void cMaskGranularLoopVec(Gp& i) noexcept;

  void cMaskMemcpyOrMemsetLoop(Gp& i) noexcept;

  void cMaskProcStoreAdvance(const Gp& dPtr, PixelCount n, Alignment alignment = Alignment(1)) noexcept;
  void cMaskProcStoreAdvance(const Gp& dPtr, PixelCount n, Alignment alignment, PixelPredicate& predicate) noexcept;

  void vMaskGenericLoop(Gp& i, const Gp& dPtr, const Gp& mPtr, GlobalAlpha* ga, const Label& done) noexcept;
  void vMaskGenericStep(const Gp& dPtr, PixelCount n, const Gp& mPtr, GlobalAlpha* ga) noexcept;
  void vMaskGenericStep(const Gp& dPtr, PixelCount n, const Gp& mPtr, GlobalAlpha* ga, PixelPredicate& predicate) noexcept;

  void vMaskProcStoreAdvance(const Gp& dPtr, PixelCount n, const VecArray& vm, PixelCoverageFlags coverageFlags, Alignment alignment = Alignment(1)) noexcept;
  void vMaskProcStoreAdvance(const Gp& dPtr, PixelCount n, const VecArray& vm, PixelCoverageFlags coverageFlags, Alignment alignment, PixelPredicate& predicate) noexcept;

  void vMaskProc(Pixel& out, PixelFlags flags, Gp& msk, PixelCoverageFlags coverageFlags) noexcept;

  void cMaskInitA8(const Gp& sm_, const Vec& vm_) noexcept;
  void cMaskFiniA8() noexcept;

  void cMaskProcA8Gp(Pixel& out, PixelFlags flags) noexcept;
  void vMaskProcA8Gp(Pixel& out, PixelFlags flags, const Gp& msk, PixelCoverageFlags coverageFlags) noexcept;

  void cMaskProcA8Vec(Pixel& out, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept;
  void vMaskProcA8Vec(Pixel& out, PixelCount n, PixelFlags flags, const VecArray& vm, PixelCoverageFlags coverageFlags, PixelPredicate& predicate) noexcept;

  void cMaskInitRGBA32(const Vec& vm) noexcept;
  void cMaskFiniRGBA32() noexcept;

  void cMaskProcRGBA32Vec(Pixel& out, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept;
  void vMaskProcRGBA32Vec(Pixel& out, PixelCount n, PixelFlags flags, const VecArray& vm, PixelCoverageFlags coverageFlags, PixelPredicate& predicate) noexcept;

  void vMaskProcRGBA32InvertMask(VecArray& vn, const VecArray& vm, PixelCoverageFlags coverageFlags) noexcept;
  void vMaskProcRGBA32InvertDone(VecArray& vn, const VecArray& vm, PixelCoverageFlags coverageFlags) noexcept;
};

} // {JIT}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_COMPOPPART_P_H_INCLUDED
