// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_PIPEGEN_BLFETCHPART_P_H
#define BLEND2D_PIPEGEN_BLFETCHPART_P_H

#include "../pipegen/blpipepart_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_pipegen
//! \{

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::FetchPart]
// ============================================================================

//! Pipeline fetch part.
class FetchPart : public PipePart {
public:
  BL_NONCOPYABLE(FetchPart)

  enum : uint32_t {
    kUnlimitedMaxPixels = 64
  };

  //! Fetch type.
  uint32_t _fetchType;
  //! Fetch extra (different meaning for each fetch type).
  uint32_t _fetchPayload;

  //! Source pixel format, see `PixelFormat::Id`.
  uint8_t _format;
  //! Source bytes-per-pixel (only required by pattern fetcher).
  uint8_t _bpp;
  //! Maximum pixel step that the fetcher can fetch at a time (0=unlimited).
  uint8_t _maxPixels;
  //! Pixel granularity passed to init().
  uint8_t _pixelGranularity;

  //! If the fetched pixels contain RGB channels.
  bool _hasRGB;
  //! If the fetched pixels contain alpha channel.
  bool _hasAlpha;

  //! Fetcher is in a rectangle fill mode, set and cleared by `init...()`.
  bool _isRectFill;
  //! If the fetch-type is complex (used to limit the maximum number of pixels).
  bool _isComplexFetch;

  FetchPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept;

  //! Get fetch type.
  BL_INLINE uint32_t fetchType() const noexcept { return _fetchType; }

  //! Get whether the fetch-type equals `ft`.
  BL_INLINE bool isFetchType(uint32_t ft) const noexcept { return _fetchType == ft; }
  //! Get whether the fetch-type is between `first..last`, inclusive.
  BL_INLINE bool isFetchType(uint32_t first, uint32_t last) const noexcept { return _fetchType >= first && _fetchType <= last; }

  //! Get whether the fetch-type is solid.
  BL_INLINE bool isSolid() const noexcept { return isFetchType(BL_PIPE_FETCH_TYPE_SOLID); }

  //! Get whether the fetch-type is gradient.
  BL_INLINE bool isGradient() const noexcept { return isFetchType(BL_PIPE_FETCH_TYPE_GRADIENT_ANY_FIRST, BL_PIPE_FETCH_TYPE_GRADIENT_ANY_LAST); }
  //! Get whether the fetch-type is linear gradient.
  BL_INLINE bool isLinearGradient() const noexcept { return isFetchType(BL_PIPE_FETCH_TYPE_GRADIENT_LINEAR_FIRST, BL_PIPE_FETCH_TYPE_GRADIENT_LINEAR_LAST); }
  //! Get whether the fetch-type is radial gradient.
  BL_INLINE bool isRadialGradient() const noexcept { return isFetchType(BL_PIPE_FETCH_TYPE_GRADIENT_RADIAL_FIRST, BL_PIPE_FETCH_TYPE_GRADIENT_RADIAL_LAST); }
  //! Get whether the fetch-type is conical gradient.
  BL_INLINE bool isConicalGradient() const noexcept { return isFetchType(BL_PIPE_FETCH_TYPE_GRADIENT_CONICAL_FIRST, BL_PIPE_FETCH_TYPE_GRADIENT_CONICAL_LAST); }

  //! Get whether the fetch-type is pattern.
  BL_INLINE bool isPattern() const noexcept { return isFetchType(BL_PIPE_FETCH_TYPE_PATTERN_ANY_FIRST, BL_PIPE_FETCH_TYPE_PATTERN_ANY_LAST); }
  //! Get whether the fetch is the destination (special type).
  BL_INLINE bool isPixelPtr() const noexcept { return isFetchType(BL_PIPE_FETCH_TYPE_PIXEL_PTR); }

  //! Get source pixel format.
  BL_INLINE uint32_t format() const noexcept { return _format; }
  //! Get source pixel format information.
  BL_INLINE BLFormatInfo formatInfo() const noexcept { return blFormatInfo[_format]; }

  //! Get source bytes-per-pixel (only used when `isPattern()` is true).
  BL_INLINE uint32_t bpp() const noexcept { return _bpp; }

  //! Get the maximum pixels the fetch part can fetch at a time.
  BL_INLINE uint32_t maxPixels() const noexcept { return _maxPixels; }

  //! Get whether the fetched pixels contain RGB channels.
  BL_INLINE bool hasRGB() const noexcept { return _hasRGB; }
  //! Get whether the fetched pixels contain Alpha channel.
  BL_INLINE bool hasAlpha() const noexcept { return _hasAlpha; }

  //! Get whether the fetch is currently initialized for a rectangular fill.
  BL_INLINE bool isRectFill() const noexcept { return _isRectFill; }
  //! Get pixel granularity passed to `FetchPath::init()`.
  BL_INLINE uint32_t pixelGranularity() const noexcept { return _pixelGranularity; }

  BL_INLINE bool isComplexFetch() const noexcept { return _isComplexFetch; }
  BL_INLINE void setComplexFetch(bool value) noexcept { _isComplexFetch = value; }

  void init(x86::Gp& x, x86::Gp& y, uint32_t pixelGranularity) noexcept;
  void fini() noexcept;

  virtual void _initPart(x86::Gp& x, x86::Gp& y) noexcept;
  virtual void _finiPart() noexcept;

  //! Advance the current y coordinate by one pixel.
  virtual void advanceY() noexcept;

  //! Initialize the current horizontal cursor of the current scanline to `x`.
  //!
  //! NOTE: This initializer is generally called once per scanline to setup the
  //! current position by initializing it to `x`. The position is then advanced
  //! automatically by pixel fetchers and by `advanceX()`, which is used when
  //! there is a gap in the scanline that has to be skipped.
  virtual void startAtX(x86::Gp& x) noexcept;

  //! Advance the current x coordinate by `diff` pixels, the final x position
  //! after advance will be `x`. The fetcher can decide whether to use `x`,
  //! `diff`, or both.
  virtual void advanceX(x86::Gp& x, x86::Gp& diff) noexcept;

  //! Must be called before `fetch1()`.
  virtual void prefetch1() noexcept;

  //! Load 1 pixel to XMM register(s) in `p` and advance by 1.
  virtual void fetch1(PixelARGB& p, uint32_t flags) noexcept = 0;

  //! Called as a prolog before fetching multiple fixels at once. This must be
  //! called before any loop that would call `fetch4()` or `fetch8()` unless the
  //! fetcher is in a vector mode because of `pixelGranularity`.
  virtual void enterN() noexcept;

  //! Called as an epilog after fetching multiple fixels at once. This must be
  //! called after a loop that uses `fetch4()` or `fetch8()` unless the fetcher
  //! is in a vector mode because of `pixelGranularity`.
  virtual void leaveN() noexcept;

  //! Must be called before a loop that calls `fetch4()` or `fetch8()`. In some
  //! cases there will be some instructions placed between `prefetch` and `fetch`,
  //! which means that if the fetcher requires an expensive operation that has
  //! greater latency then it would be better to place that code into the prefetch
  //! area.
  virtual void prefetchN() noexcept;

  //! Cancels the effect of `prefetchN()` and also automatic prefetch that happens
  //! inside `fetch4()` or `fetch8()`. Must be called after a loop that calls
  //! `fetch4()`, `fetch8()`, or immediately after `prefetchN()` if no loop has
  //! been entered.
  virtual void postfetchN() noexcept;

  //! Fetch 4 pixels to XMM register(s) in `p` and advance by 4.
  virtual void fetch4(PixelARGB& p, uint32_t flags) noexcept = 0;

  //! Fetch 8 pixels to XMM register(s) in `p` and advance by 8.
  //!
  //! NOTE: The default implementation uses `fetch4()` twice.
  virtual void fetch8(PixelARGB& p, uint32_t flags) noexcept;
};

} // {BLPipeGen}

//! \}
//! \endcond

#endif // BLEND2D_PIPEGEN_BLFETCHPART_P_H
