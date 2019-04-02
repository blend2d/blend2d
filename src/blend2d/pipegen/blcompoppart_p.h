// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_PIPEGEN_BLCOMPOPPART_P_H
#define BLEND2D_PIPEGEN_BLCOMPOPPART_P_H

#include "../pipegen/blfetchpart_p.h"
#include "../pipegen/blpipepart_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_pipegen
//! \{

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::CompOpPart]
// ============================================================================

//! Pipeline combine part.
class CompOpPart : public PipePart {
public:
  BL_NONCOPYABLE(CompOpPart)

  enum : uint32_t {
    kIndexDstPart = 0,
    kIndexSrcPart = 1
  };

  //! Composition operator.
  uint32_t _compOp;
  //! The current span mode.
  uint8_t _cMaskLoopType;
  //! Maximum pixels the compositor can handle at a time.
  uint8_t _maxPixels;
  //! Pixel granularity.
  uint8_t _pixelGranularity;
  //! Minimum alignment required to process `_maxPixels`.
  uint8_t _minAlignment;

  //! Whether the destination format has an alpha channel.
  uint8_t _hasDa : 1;
  //! Whether the source format has an alpha channel.
  uint8_t _hasSa : 1;

  //! A hook that is used by the current loop.
  asmjit::BaseNode* _cMaskLoopHook;
  //! Optimized solid pixel for operators that allow it.
  SolidPixelARGB _solidOpt;
  //! Pre-processed solid pixel for TypeA operators that always use `vMaskProc?()`.
  PixelARGB _solidPre;
  //! Partial fetch that happened at the end of the scanline (border case).
  PixelARGB _pixPartial;
  //! Const mask.
  BLWrap<PipeCMask> _mask;

  CompOpPart(PipeCompiler* pc, uint32_t compOp, FetchPart* dstPart, FetchPart* srcPart) noexcept;

  BL_INLINE FetchPart* dstPart() const noexcept { return reinterpret_cast<FetchPart*>(_children[kIndexDstPart]); }
  BL_INLINE FetchPart* srcPart() const noexcept { return reinterpret_cast<FetchPart*>(_children[kIndexSrcPart]); }

  //! Get composition operator id, see `BLCompOp`.
  BL_INLINE uint32_t compOp() const noexcept { return _compOp; }
  //! Get composition operator flags, see `BLCompOpFlags`.
  BL_INLINE uint32_t compOpFlags() const noexcept { return blCompOpInfo[_compOp].flags; }

  //! Get whether the destination pixel format has an alpha channel.
  BL_INLINE bool hasDa() const noexcept { return _hasSa != 0; }
  //! Get whether the source pixel format has an alpha channel.
  BL_INLINE bool hasSa() const noexcept { return _hasDa != 0; }

  //! Get the current loop mode.
  BL_INLINE uint32_t cMaskLoopType() const noexcept { return _cMaskLoopType; }
  //! Get whether the current loop is fully opaque (no mask).
  BL_INLINE bool isLoopOpaque() const noexcept { return _cMaskLoopType == kCMaskLoopTypeOpaque; }
  //! Get whether the current loop is `CMask` (constant mask).
  BL_INLINE bool isLoopCMask() const noexcept { return _cMaskLoopType == kCMaskLoopTypeMask; }

  //! Get the maximum pixels the composite part can handle at a time.
  //!
  //! NOTE: This value is configured in a way that it's always one if the fetch
  //! part doesn't support more. This makes it easy to use in loop compilers.
  //! In other words, the value doesn't describe the real implementation of the
  //! composite part.
  BL_INLINE uint32_t maxPixels() const noexcept { return _maxPixels; }
  //! Get the maximum pixels the children of this part can handle.
  BL_INLINE uint32_t maxPixelsOfChildren() const noexcept { return blMin(dstPart()->maxPixels(), srcPart()->maxPixels()); }

  //! Get pixel granularity passed to `init()`, otherwise the result should be zero.
  BL_INLINE uint32_t pixelGranularity() const noexcept { return _pixelGranularity; }
  //! Get the minimum destination alignment required to the maximum number of pixels `_maxPixels`.
  BL_INLINE uint32_t minAlignment() const noexcept { return _minAlignment; }

  BL_INLINE bool isUsingSolidPre() const noexcept { return !_solidPre.pc.empty() || !_solidPre.uc.empty(); }

  void init(x86::Gp& x, x86::Gp& y, uint32_t pixelGranularity) noexcept;
  void fini() noexcept;

  //! Get whether the opaque fill should be optimized and placed into a separate
  //! loop.
  bool shouldOptimizeOpaqueFill() const noexcept;

  //! Get whether the compositor should emit a specialized loop that contains
  //! an inlined version of `memcpy()` or `memset()`.
  bool shouldMemcpyOrMemsetOpaqueFill() const noexcept;

  void startAtX(x86::Gp& x) noexcept;
  void advanceX(x86::Gp& x, x86::Gp& diff) noexcept;
  void advanceY() noexcept;

  // These are just wrappers that call these on both source & destination parts.
  void prefetch1() noexcept;
  void enterN() noexcept;
  void leaveN() noexcept;
  void prefetchN() noexcept;
  void postfetchN() noexcept;

  void dstFetch32(PixelARGB& out, uint32_t flags, uint32_t n) noexcept;
  void srcFetch32(PixelARGB& out, uint32_t flags, uint32_t n) noexcept;

  BL_INLINE bool isInPartialMode() const noexcept {
    return !_pixPartial.pc.empty();
  }

  void enterPartialMode(uint32_t partialFlags = 0) noexcept;
  void exitPartialMode() noexcept;
  void nextPartialPixel() noexcept;

  void cMaskInit(x86::Gp& m) noexcept;
  void cMaskInit(const x86::Mem& pMsk) noexcept;
  void cMaskFini() noexcept;

  void cMaskGenericLoop(x86::Gp& i) noexcept;
  void cMaskGranularLoop(x86::Gp& i) noexcept;
  void cMaskMemcpyOrMemsetLoop(x86::Gp& i) noexcept;

  void _cMaskLoopInit(uint32_t loopType) noexcept;
  void _cMaskLoopFini() noexcept;

  void cMaskInitXmm(x86::Vec& m) noexcept;
  void cMaskFiniXmm() noexcept;

  void cMaskGenericLoopXmm(x86::Gp& i) noexcept;
  void cMaskGranularLoopXmm(x86::Gp& i) noexcept;

  void cMaskProc32Xmm1(PixelARGB& out, uint32_t flags) noexcept;
  void cMaskProc32Xmm4(PixelARGB& out, uint32_t flags) noexcept;
  void cMaskProc32Xmm8(PixelARGB& out, uint32_t flags) noexcept;
  void cMaskProc32XmmV(PixelARGB& out, uint32_t flags, uint32_t n) noexcept;

  void vMaskProc(PixelARGB& out, uint32_t flags, x86::Gp& m) noexcept;

  void vMaskProc32Xmm1(PixelARGB& out, uint32_t flags, VecArray& mv, bool mImmutable) noexcept;
  void vMaskProc32Xmm4(PixelARGB& out, uint32_t flags, VecArray& mv, bool mImmutable) noexcept;
  void vMaskProc32XmmV(PixelARGB& out, uint32_t flags, VecArray& mv, uint32_t n, bool mImmutable) noexcept;

  void vMaskProc32InvertMask(VecArray& mi, VecArray& mv) noexcept;
  void vMaskProc32InvertDone(VecArray& mi, bool mImmutable) noexcept;
};

} // {BLPipeGen}

//! \}
//! \endcond

#endif // BLEND2D_PIPEGEN_BLCOMPOPPART_P_H
