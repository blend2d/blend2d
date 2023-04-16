// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_FETCHPART_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_FETCHPART_P_H_INCLUDED

#include "../../pipeline/jit/pipepart_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace BLPipeline {
namespace JIT {

//! Pipeline fetch part.
class FetchPart : public PipePart {
public:
  enum : uint32_t {
    kUnlimitedMaxPixels = 64
  };

  //! Fetch type.
  FetchType _fetchType;

  //! Source pixel format.
  BLInternalFormat _format = BLInternalFormat::kNone;
  //! Source bytes-per-pixel (only required by pattern fetcher).
  uint8_t _bpp = 0;
  //! Maximum pixel step that the fetcher can fetch at a time (0=unlimited).
  uint8_t _maxPixels = 1;
  //! Pixel type.
  PixelType _pixelType = PixelType::kNone;
  //! Pixel granularity passed to init().
  uint8_t _pixelGranularity = 0;

  //! True if the fetching should happen in alpha mode (no RGB).
  uint8_t _alphaFetch = 0;
  //! Alpha channel [memory] offset, only used when `_alphaFetch` is true
  uint8_t _alphaOffset = 0;

  //! Fetcher is in a rectangle fill mode, set and cleared by `init...()`.
  bool _isRectFill = false;
  //! If the fetch-type is complex (used to limit the maximum number of pixels).
  bool _isComplexFetch = false;

  //! If the fetched pixels contain RGB channels.
  bool _hasRGB = false;
  //! If the fetched pixels contain alpha channel.
  bool _hasAlpha = false;

  FetchPart(PipeCompiler* pc, FetchType fetchType, BLInternalFormat format) noexcept;

  //! Returns the fetch type.
  BL_INLINE FetchType fetchType() const noexcept { return _fetchType; }

  //! Tests whether the fetch-type equals `value`.
  BL_INLINE bool isFetchType(FetchType value) const noexcept { return _fetchType == value; }
  //! Tests whether the fetch-type is between `first..last`, inclusive.
  BL_INLINE bool isFetchType(FetchType first, FetchType last) const noexcept { return _fetchType >= first && _fetchType <= last; }

  //! Tests whether the fetch-type is solid.
  BL_INLINE bool isSolid() const noexcept { return isFetchType(FetchType::kSolid); }

  //! Tests whether the fetch-type is gradient.
  BL_INLINE bool isGradient() const noexcept { return isFetchType(FetchType::kGradientAnyFirst, FetchType::kGradientAnyLast); }
  //! Tests whether the fetch-type is linear gradient.
  BL_INLINE bool isLinearGradient() const noexcept { return isFetchType(FetchType::kGradientLinearFirst, FetchType::kGradientLinearLast); }
  //! Tests whether the fetch-type is radial gradient.
  BL_INLINE bool isRadialGradient() const noexcept { return isFetchType(FetchType::kGradientRadialFirst, FetchType::kGradientRadialLast); }
  //! Tests whether the fetch-type is conical gradient.
  BL_INLINE bool isConicalGradient() const noexcept { return isFetchType(FetchType::kGradientConicalFirst, FetchType::kGradientConicalLast); }

  //! Tests whether the fetch-type is pattern.
  BL_INLINE bool isPattern() const noexcept { return isFetchType(FetchType::kPatternAnyFirst, FetchType::kPatternAnyLast); }
  //! Tests whether the fetch is the destination (special type).
  BL_INLINE bool isPixelPtr() const noexcept { return isFetchType(FetchType::kPixelPtr); }

  //! Returns source pixel format.
  BL_INLINE BLInternalFormat format() const noexcept { return _format; }
  //! Returns source pixel format information.
  BL_INLINE BLFormatInfo formatInfo() const noexcept { return blFormatInfo[size_t(_format)]; }

  //! Returns source bytes-per-pixel (only used when `isPattern()` is true).
  BL_INLINE uint32_t bpp() const noexcept { return _bpp; }

  //! Returns the maximum pixels the fetch part can fetch at a time.
  BL_INLINE uint32_t maxPixels() const noexcept { return _maxPixels; }

  //! Tests whether the fetched pixels contain RGB channels.
  BL_INLINE bool hasRGB() const noexcept { return _hasRGB; }
  //! Tests whether the fetched pixels contain Alpha channel.
  BL_INLINE bool hasAlpha() const noexcept { return _hasAlpha; }

  //! Tests whether the fetching should happen in alpha-only mode.
  BL_INLINE bool isAlphaFetch() const noexcept { return _alphaFetch != 0; }
  //! Returns a byte offset of alpha channel (if provided), used when fetching alpha from memory.
  BL_INLINE uint32_t alphaOffset() const noexcept { return _alphaOffset; }

  //! Tests whether the fetch is currently initialized for a rectangular fill.
  BL_INLINE bool isRectFill() const noexcept { return _isRectFill; }
  //! Returns the pixel granularity passed to `FetchPath::init()`.
  BL_INLINE uint32_t pixelGranularity() const noexcept { return _pixelGranularity; }

  BL_INLINE bool isComplexFetch() const noexcept { return _isComplexFetch; }
  BL_INLINE void setComplexFetch(bool value) noexcept { _isComplexFetch = value; }

  void init(x86::Gp& x, x86::Gp& y, PixelType pixelType, uint32_t pixelGranularity) noexcept;
  void fini() noexcept;

  virtual void _initPart(x86::Gp& x, x86::Gp& y) noexcept;
  virtual void _finiPart() noexcept;

  //! Advances the current y coordinate by one pixel.
  virtual void advanceY() noexcept;

  //! Initializes the current horizontal cursor of the current scanline to `x`.
  //!
  //! \note This initializer is generally called once per scanline to setup the current position by initializing it
  //! to `x`. The position is then advanced automatically by pixel fetchers and by `advanceX()`, which is used when
  //! there is a gap in the scanline that has to be skipped.
  virtual void startAtX(const x86::Gp& x) noexcept;

  //! Advances the current x coordinate by `diff` pixels. The final x position after advance will be `x`. The fetcher
  //! can decide whether to use `x`, `diff`, or both.
  virtual void advanceX(const x86::Gp& x, const x86::Gp& diff) noexcept;

  //! Called before `fetch()` with `n == 1`.
  virtual void prefetch1() noexcept;

  //! Called as a prolog before fetching multiple fixels at once. This must be called before any loop that would call
  //! `fetch()` with `n` greater than 1 unless the fetcher is in a vector mode because of `pixelGranularity`.
  virtual void enterN() noexcept;

  //! Called as an epilog after fetching multiple fixels at once. This must be called after a loop that uses `fetch()`
  //! with `n` greater than 1 unless the fetcher is in a vector mode because of `pixelGranularity`.
  virtual void leaveN() noexcept;

  //! Called before a loop that calls `fetch()` with `n` greater than 1. In some cases there will be some instructions
  //! placed between `prefetch()` and `fetch()`, which means that if the fetcher requires an expensive operation that
  //! has greater latency then it would be better to place that code into the prefetch area.
  virtual void prefetchN() noexcept;

  //! Cancels the effect of `prefetchN()` and also automatic prefetch that happens inside `fetch()` with `n` greater than
  //! 1. Must be called after a loop that calls `fetch()` to fetch multiple pixels, or immediately after `prefetchN()` if
  //! no loop would be entered, but prefetchN() was already used.
  virtual void postfetchN() noexcept;

  //! Fetches N pixels to `p` and advances by N.
  virtual void fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept = 0;

  //! Fetches 8 pixels by calling fetch() twice to fetch 4 pixels each time.
  void _fetch2x4(Pixel& p, PixelFlags flags) noexcept;
};

} // {JIT}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_FETCHPART_P_H_INCLUDED
