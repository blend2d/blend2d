// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_COMPOPPART_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_COMPOPPART_P_H_INCLUDED

#include "../../pipeline/jit/fetchpart_p.h"
#include "../../pipeline/jit/pipepart_p.h"
#include "../../support/wrap_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace BLPipeline {
namespace JIT {

//! Pipeline combine part.
class CompOpPart : public PipePart {
public:
  enum : uint32_t {
    kIndexDstPart = 0,
    kIndexSrcPart = 1
  };

  //! Composition operator.
  uint32_t _compOp {};
  //! Pixel type of the composition.
  PixelType _pixelType = PixelType::kNone;
  //! The current span mode.
  CMaskLoopType _cMaskLoopType = CMaskLoopType::kNone;
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
  BLWrap<PipeCMask> _mask;

  CompOpPart(PipeCompiler* pc, uint32_t compOp, FetchPart* dstPart, FetchPart* srcPart) noexcept;

  BL_INLINE FetchPart* dstPart() const noexcept { return reinterpret_cast<FetchPart*>(_children[kIndexDstPart]); }
  BL_INLINE FetchPart* srcPart() const noexcept { return reinterpret_cast<FetchPart*>(_children[kIndexSrcPart]); }

  //! Returns the composition operator id, see `BLCompOp`.
  BL_INLINE uint32_t compOp() const noexcept { return _compOp; }

  BL_INLINE bool isSrcCopy() const noexcept { return _compOp == BL_COMP_OP_SRC_COPY; }
  BL_INLINE bool isSrcOver() const noexcept { return _compOp == BL_COMP_OP_SRC_OVER; }
  BL_INLINE bool isSrcIn() const noexcept { return _compOp == BL_COMP_OP_SRC_IN; }
  BL_INLINE bool isSrcOut() const noexcept { return _compOp == BL_COMP_OP_SRC_OUT; }
  BL_INLINE bool isSrcAtop() const noexcept { return _compOp == BL_COMP_OP_SRC_ATOP; }

  BL_INLINE bool isDstCopy() const noexcept { return _compOp == BL_COMP_OP_DST_COPY; }
  BL_INLINE bool isDstOver() const noexcept { return _compOp == BL_COMP_OP_DST_OVER; }
  BL_INLINE bool isDstIn() const noexcept { return _compOp == BL_COMP_OP_DST_IN; }
  BL_INLINE bool isDstOut() const noexcept { return _compOp == BL_COMP_OP_DST_OUT; }
  BL_INLINE bool isDstAtop() const noexcept { return _compOp == BL_COMP_OP_DST_ATOP; }

  BL_INLINE bool isXor() const noexcept { return _compOp == BL_COMP_OP_XOR; }
  BL_INLINE bool isPlus() const noexcept { return _compOp == BL_COMP_OP_PLUS; }
  BL_INLINE bool isMinus() const noexcept { return _compOp == BL_COMP_OP_MINUS; }
  BL_INLINE bool isModulate() const noexcept { return _compOp == BL_COMP_OP_MODULATE; }
  BL_INLINE bool isMultiply() const noexcept { return _compOp == BL_COMP_OP_MULTIPLY; }
  BL_INLINE bool isScreen() const noexcept { return _compOp == BL_COMP_OP_SCREEN; }
  BL_INLINE bool isOverlay() const noexcept { return _compOp == BL_COMP_OP_OVERLAY; }
  BL_INLINE bool isDarken() const noexcept { return _compOp == BL_COMP_OP_DARKEN; }
  BL_INLINE bool isLighten() const noexcept { return _compOp == BL_COMP_OP_LIGHTEN; }
  BL_INLINE bool isColorDodge() const noexcept { return _compOp == BL_COMP_OP_COLOR_DODGE; }
  BL_INLINE bool isColorBurn() const noexcept { return _compOp == BL_COMP_OP_COLOR_BURN; }
  BL_INLINE bool isLinearBurn() const noexcept { return _compOp == BL_COMP_OP_LINEAR_BURN; }
  BL_INLINE bool isLinearLight() const noexcept { return _compOp == BL_COMP_OP_LINEAR_LIGHT; }
  BL_INLINE bool isPinLight() const noexcept { return _compOp == BL_COMP_OP_PIN_LIGHT; }
  BL_INLINE bool isHardLight() const noexcept { return _compOp == BL_COMP_OP_HARD_LIGHT; }
  BL_INLINE bool isSoftLight() const noexcept { return _compOp == BL_COMP_OP_SOFT_LIGHT; }
  BL_INLINE bool isDifference() const noexcept { return _compOp == BL_COMP_OP_DIFFERENCE; }
  BL_INLINE bool isExclusion() const noexcept { return _compOp == BL_COMP_OP_EXCLUSION; }


  //! Returns the composition operator flags, see `BLCompOpFlags`.
  BL_INLINE BLCompOpFlags compOpFlags() const noexcept { return blCompOpInfo[_compOp].flags(); }

  //! Tests whether the destination pixel format has an alpha component.
  BL_INLINE bool hasDa() const noexcept { return _hasDa != 0; }
  //! Tests whether the source pixel format has an alpha component.
  BL_INLINE bool hasSa() const noexcept { return _hasSa != 0; }

  BL_INLINE PixelType pixelType() const noexcept { return _pixelType; }
  BL_INLINE bool isA8Pixel() const noexcept { return _pixelType == PixelType::kA8; }
  BL_INLINE bool isRGBA32Pixel() const noexcept { return _pixelType == PixelType::kRGBA32; }

  //! Returns the current loop mode.
  BL_INLINE CMaskLoopType cMaskLoopType() const noexcept { return _cMaskLoopType; }
  //! Tests whether the current loop is fully opaque (no mask).
  BL_INLINE bool isLoopOpaque() const noexcept { return _cMaskLoopType == CMaskLoopType::kOpaque; }
  //! Tests whether the current loop is `CMask` (constant mask).
  BL_INLINE bool isLoopCMask() const noexcept { return _cMaskLoopType == CMaskLoopType::kVariant; }

  //! Returns the maximum pixels the composite part can handle at a time.
  //!
  //! \note This value is configured in a way that it's always one if the fetch
  //! part doesn't support more. This makes it easy to use it in loop compilers.
  //! In other words, the value doesn't describe the real implementation of the
  //! composite part.
  BL_INLINE uint32_t maxPixels() const noexcept { return _maxPixels; }
  //! Returns the maximum pixels the children of this part can handle.
  BL_INLINE uint32_t maxPixelsOfChildren() const noexcept { return blMin(dstPart()->maxPixels(), srcPart()->maxPixels()); }

  BL_INLINE void setMaxPixels(uint32_t maxPixels) noexcept {
    BL_ASSERT(maxPixels <= 0xFF);
    _maxPixels = uint8_t(maxPixels);
  }

  //! Returns pixel granularity passed to `init()`, otherwise the result should be zero.
  BL_INLINE PixelCount pixelGranularity() const noexcept { return _pixelGranularity; }
  //! Returns the minimum destination alignment required to the maximum number of pixels `_maxPixels`.
  BL_INLINE Alignment minAlignment() const noexcept { return _minAlignment; }

  BL_INLINE bool isUsingSolidPre() const noexcept { return !_solidPre.pc.empty() || !_solidPre.uc.empty(); }

  void preparePart() noexcept override;

  void init(x86::Gp& x, x86::Gp& y, uint32_t pixelGranularity) noexcept;
  void fini() noexcept;

  //! Tests whether the opaque fill should be optimized and placed into a separate
  //! loop. This means that if this function returns true two composition loops
  //! would be generated by the filler.
  bool shouldOptimizeOpaqueFill() const noexcept;

  //! Tests whether the compositor should emit a specialized loop that contains
  //! an inlined version of `memcpy()` or `memset()`.
  bool shouldJustCopyOpaqueFill() const noexcept;

  void startAtX(const x86::Gp& x) noexcept;
  void advanceX(const x86::Gp& x, const x86::Gp& diff) noexcept;
  void advanceY() noexcept;

  // These are just wrappers that call these on both source & destination parts.
  void prefetch1() noexcept;
  void enterN() noexcept;
  void leaveN() noexcept;
  void prefetchN() noexcept;
  void postfetchN() noexcept;

  void dstFetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept;
  void srcFetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept;

  BL_INLINE bool isInPartialMode() const noexcept { return _isInPartialMode != 0; }

  void enterPartialMode(PixelFlags partialFlags = PixelFlags::kNone) noexcept;
  void exitPartialMode() noexcept;
  void nextPartialPixel() noexcept;

  void cMaskInit(const x86::Mem& mem) noexcept;
  void cMaskInit(const x86::Gp& sm_, const x86::Vec& vm_) noexcept;
  void cMaskInitOpaque() noexcept;
  void cMaskFini() noexcept;

  void _cMaskLoopInit(CMaskLoopType loopType) noexcept;
  void _cMaskLoopFini() noexcept;

  void cMaskGenericLoop(x86::Gp& i) noexcept;
  void cMaskGenericLoopVec(x86::Gp& i) noexcept;

  void cMaskGranularLoop(x86::Gp& i) noexcept;
  void cMaskGranularLoopVec(x86::Gp& i) noexcept;

  void cMaskMemcpyOrMemsetLoop(x86::Gp& i) noexcept;

  void cMaskProcStoreAdvance(const x86::Gp& dPtr, PixelCount n, Alignment alignment = Alignment(1)) noexcept;
  void cMaskProcStoreAdvance(const x86::Gp& dPtr, PixelCount n, Alignment alignment, PixelPredicate& predicate) noexcept;

  void vMaskGenericLoop(x86::Gp& i, const x86::Gp& dPtr, const x86::Gp& mPtr, GlobalAlpha& ga, const Label& done) noexcept;
  void vMaskGenericStep(const x86::Gp& dPtr, PixelCount n, const x86::Gp& mPtr, const x86::Reg& ga) noexcept;

  void vMaskProcStoreAdvance(const x86::Gp& dPtr, PixelCount n, VecArray& vm, bool vmImmutable, Alignment alignment = Alignment(1)) noexcept;
  void vMaskProcStoreAdvance(const x86::Gp& dPtr, PixelCount n, VecArray& vm, bool vmImmutable, Alignment alignment, PixelPredicate& predicate) noexcept;

  void vMaskProc(Pixel& out, PixelFlags flags, x86::Gp& msk, bool mImmutable) noexcept;

  void cMaskInitA8(const x86::Gp& sm_, const x86::Vec& vm_) noexcept;
  void cMaskFiniA8() noexcept;

  void cMaskProcA8Gp(Pixel& out, PixelFlags flags) noexcept;
  void vMaskProcA8Gp(Pixel& out, PixelFlags flags, x86::Gp& msk, bool mImmutable) noexcept;

  void cMaskProcA8Vec(Pixel& out, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept;
  void vMaskProcA8Vec(Pixel& out, PixelCount n, PixelFlags flags, VecArray& vm, bool mImmutable, PixelPredicate& predicate) noexcept;

  void cMaskInitRGBA32(const x86::Vec& vm) noexcept;
  void cMaskFiniRGBA32() noexcept;

  void cMaskProcRGBA32Vec(Pixel& out, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept;
  void vMaskProcRGBA32Vec(Pixel& out, PixelCount n, PixelFlags flags, VecArray& vm, bool mImmutable, PixelPredicate& predicate) noexcept;
  void vMaskProcRGBA32InvertMask(VecArray& vn, VecArray& vm) noexcept;
  void vMaskProcRGBA32InvertDone(VecArray& vn, bool mImmutable) noexcept;
};

} // {JIT}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_COMPOPPART_P_H_INCLUDED
