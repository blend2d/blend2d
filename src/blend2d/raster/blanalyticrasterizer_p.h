// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_RASTER_BLANALYTICRASTERIZER_P_H
#define BLEND2D_RASTER_BLANALYTICRASTERIZER_P_H

#include "../blsupport_p.h"
#include "../raster/blrasterdefs_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

// ============================================================================
// [BLAnalyticCellStorage]
// ============================================================================

//! Stores cells and bit-vectors used by analytic rasterizer.
struct BLAnalyticCellStorage {
  //! BitWord pointer at top-left corner.
  BLBitWord* bitPtrTop;
  //! BitWord stride [in bytes].
  size_t bitStride;

  //! Cell pointer at top-left corner.
  uint32_t* cellPtrTop;
  //! Cell stride [in bytes].
  size_t cellStride;

  BL_INLINE void init(BLBitWord* bitPtrTop, size_t bitStride, uint32_t* cellPtrTop, size_t cellStride) noexcept {
    this->bitPtrTop = bitPtrTop;
    this->bitStride = bitStride;
    this->cellPtrTop = cellPtrTop;
    this->cellStride = cellStride;
  }

  BL_INLINE void reset() noexcept {
    this->bitPtrTop = nullptr;
    this->bitStride = 0;
    this->cellPtrTop = nullptr;
    this->cellStride = 0;
  }
};

// ============================================================================
// [BLAnalyticRasterizerUtils]
// ============================================================================

//! Analytic rasterizer utilities.
namespace BLAnalyticRasterizerUtils {
  //! Apply a sign-mask to `x`.
  //!
  //! A sign mask must have all bits either zero (no change) or ones (inverts
  //! the sign).
  template<typename X, typename Y>
  constexpr X applySignMask(const X& x, const Y& mask) noexcept {
    typedef typename std::make_unsigned<X>::type U;
    return X((U(x) ^ U(mask)) - U(mask));
  }

  //! Branchless implementation of the following code:
  //!
  //! ```
  //! iter -= step;
  //! if (iter < 0) {
  //!   acc++;
  //!   iter += correction;
  //! }
  //! ```
  template<typename AccT, typename IterT>
  static BL_INLINE void accErrStep(AccT& acc, IterT& iter, const IterT& step, const IterT& correction) noexcept {
    static_assert(sizeof(AccT) == sizeof(IterT),
                  "Accumulator and iterator must have the same size");

    constexpr uint32_t kSizeOfTInBits = blBitSizeOf<IterT>();
    typedef typename std::make_signed<IterT>::type S;

    iter -= step;

    // Contains all ones if the iterator has underflown (requires correction).
    IterT mask = IterT(S(iter) >> (kSizeOfTInBits - 1));

    acc -= AccT(mask);         // if (iter < 0) acc++;
    iter += mask & correction; // if (iter < 0) iter += correction;
  }
}

// ============================================================================
// [BLAnalyticRasterizerState]
// ============================================================================

//! Analytic rasterizer state.
//!
//! This state can be used to temporarily terminate rasterization. It's used in
//! case that the context uses banding (large inputs) or asyncronous rendering
//! possibly combined with multithreading.
struct BLAnalyticRasterizerState {
  enum Flags : uint32_t {
    //! This flag is always set by `BLAnalyticRasterizer::prepare()`, however,
    //! it can be ignored completely if the line is not horizontally oriented.
    kFlagInitialScanline = 0x00000001u,

    //! Flag set if the line is strictly vertical (dy == 0) or if it fits into
    //! a single cell. These are two special cases handled differently.
    kFlagVertOrSingle    = 0x00000002u,

    //! Set if the line is rasterized from right to left.
    kFlagRightToLeft     = 0x00000004u
  };

  int _ex0, _ey0, _ex1, _ey1;
  int _fx0, _fy0, _fx1, _fy1;

  int _xErr, _yErr;
  int _xDlt, _yDlt;
  int _xRem, _yRem;
  int _xLift, _yLift;

  int _dx, _dy;
  int _savedFy1;
  uint32_t _flags;
};

// ============================================================================
// [BLAnalyticRasterizer]
// ============================================================================

//! Analytic rasterizer.
//!
//! This rasterizer is designed to provide some customization through `OPTIONS`.
//! It's well suited for both small and large art having any number of input
//! vertices. The algorithm is based on AGG rasterizer, but was improved to
//! always render from top to bottom (to support banding) and to use dense cell
//! representation instead of cell spans or any other sparse cell representation.
//!
//! To mark cells that are non-zero (and have to be processed by the compositor)
//! it uses a fixed bit vectors per each scanline where 1 bit represents N cells
//! (and thus N target pixels). This has a huge advantage as the compositor can
//! skip pixels in hundreds by just checking the bit vector without having to
//! process cells that are zero.
//!
//! Since the rasterizer requires dense cell buffer and expects this buffer to
//! be zero initialized, the compositor should zero all cells and bits it
//! processes so the buffer is ready for another rasterization. Blend2D does
//! this implicitly, so this is just a note for anyone wondering how this works.
struct BLAnalyticRasterizer : public BLAnalyticRasterizerState {
  enum Options : uint32_t {
    // Other features the rasterizer supports.
    kOptionBandOffset     = 0x0004u, // Take BandOffset into consideration.
    kOptionBandingMode    = 0x0008u, // Rasterizer uses banding technique.
    kOptionEasyBitStride  = 0x0010u  // BitStride is equal to sizeof(BLBitWord).
  };

  //! BitWords and Cells, initialized by `init()`, never modified.
  BLAnalyticCellStorage _cellStorage;
  //! Sign mask.
  uint32_t _signMask;
  //! Height of a rendering band (number of scanlines).
  uint32_t _bandHeight;
  //! Offset to the first scanline in the current band.
  uint32_t _bandOffset;
  //! End of the current band (_bandOffset + _bandHeight - 1).
  uint32_t _bandEnd;

  // --------------------------------------------------------------------------
  // [Init]
  // --------------------------------------------------------------------------

  BL_INLINE void init(BLBitWord* bitPtrTop, size_t bitStride, uint32_t* cellPtrTop, size_t cellStride, uint32_t bandOffset, uint32_t bandHeight) noexcept {
    // Reset most members so the compiler doesn't think some of them are used
    // uninitialized in case we save state of a vertical only line, etc...
    //
    // We don't reset coords & dx/dy as they are always properly set by `prepare()`.
    _xErr = 0;
    _yErr = 0;
    _xDlt = 0;
    _yDlt = 0;
    _xRem = 0;
    _yRem = 0;
    _xLift = 0;
    _yLift = 0;
    _flags = 0;

    // BLAnalyticRasterizer members.
    _cellStorage.init(bitPtrTop, bitStride, cellPtrTop, cellStride);
    _signMask = 0;
    _bandHeight = bandHeight;
    _bandOffset = bandOffset;
    _bandEnd = bandOffset + bandHeight - 1;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  BL_INLINE BLBitWord* bitPtrTop() const noexcept {
    return _cellStorage.bitPtrTop;
  }

  //! Returns the current `bitStride`.
  //!
  //! This function returns `sizeof(BLBitWord)` in case we are generating an
  //! optimized rasterizer for small-art where the number of bits that
  //! represent pixels including padding doesn't exceed a single BitWord.
  template<uint32_t OPTIONS>
  BL_INLINE size_t bitStride() const noexcept {
    if (OPTIONS & kOptionEasyBitStride)
      return sizeof(BLBitWord);
    else
      return _cellStorage.bitStride;
  }

  BL_INLINE uint32_t* cellPtrTop() const noexcept { return _cellStorage.cellPtrTop; }
  BL_INLINE size_t cellStride() const noexcept { return _cellStorage.cellStride; }

  BL_INLINE uint32_t signMask() const noexcept { return _signMask; }
  BL_INLINE void setSignMask(uint32_t signMask) noexcept { _signMask = signMask; }
  BL_INLINE void setSignMaskFromBit(uint32_t signBit) noexcept { _signMask = blNegate(signBit); }

  // --------------------------------------------------------------------------
  // [Save / Restore]
  // --------------------------------------------------------------------------

  BL_INLINE void save(BLAnalyticRasterizerState& state) const noexcept {
    state = *static_cast<const BLAnalyticRasterizerState*>(this);
  }

  BL_INLINE void restore(BLAnalyticRasterizerState& state) noexcept {
    *static_cast<BLAnalyticRasterizerState*>(this) = state;
  }

  // --------------------------------------------------------------------------
  // [Prepare]
  // --------------------------------------------------------------------------

  BL_INLINE bool prepare(int x0, int y0, int x1, int y1) noexcept {
    // Line should be already reversed in case it has a negative sign.
    BL_ASSERT(y0 <= y1);

    // Should not happen regularly, but in some edge cases this can happen in
    // cases where a curve was flatenned into line sergments that don't change
    // vertically or produced by `BLEdgeBuilderFromSource` that doesn't eliminate
    // strictly horizontal edges.
    if (y0 == y1)
      return false;

    _dx = x1 - x0;
    _dy = y1 - y0;
    _flags = kFlagInitialScanline;

    if (_dx < 0) {
      _flags |= kFlagRightToLeft;
      _dx = -_dx;
    }

    _ex0 = (  x0 >> BL_PIPE_A8_SHIFT);
    _ey0 = (  y0 >> BL_PIPE_A8_SHIFT);
    _ex1 = (  x1 >> BL_PIPE_A8_SHIFT);
    _ey1 = (--y1 >> BL_PIPE_A8_SHIFT);

    _fx0 = (x0 & BL_PIPE_A8_MASK);
    _fy0 = (y0 & BL_PIPE_A8_MASK);
    _fx1 = (x1 & BL_PIPE_A8_MASK);
    _fy1 = (y1 & BL_PIPE_A8_MASK) + 1;

    _savedFy1 = _fy1;
    if (_ey0 != _ey1)
      _fy1 = BL_PIPE_A8_SCALE;

    if (_ex0 == _ex1 && (_ey0 == _ey1 || _dx == 0)) {
      _flags |= kFlagVertOrSingle;
      return true;
    }

    uint64_t xBase = uint64_t(uint32_t(_dx)) * BL_PIPE_A8_SCALE;
    uint64_t yBase = uint64_t(uint32_t(_dy)) * BL_PIPE_A8_SCALE;

    _xLift = int(xBase / unsigned(_dy));
    _xRem  = int(xBase % unsigned(_dy));

    _yLift = int(yBase / unsigned(_dx));
    _yRem  = int(yBase % unsigned(_dx));

    _xDlt = _dx;
    _yDlt = _dy;

    _xErr = (_dy >> 1) - 1;
    _yErr = (_dx >> 1) - 1;

    if (_ey0 != _ey1) {
      uint64_t p = uint64_t(BL_PIPE_A8_SCALE - _fy0) * _dx;
      _xDlt  = int(p / unsigned(_dy));
      _xErr -= int(p % unsigned(_dy));
    }

    if (_ex0 != _ex1) {
      uint64_t p = uint64_t((_flags & kFlagRightToLeft) ? _fx0 : int(BL_PIPE_A8_SCALE) - _fx0) * _dy;
      _yDlt  = int(p / unsigned(_dx));
      _yErr -= int(p % unsigned(_dx));
    }

    _yDlt += _fy0;
    return true;
  }

  // --------------------------------------------------------------------------
  // [Rasterize]
  // --------------------------------------------------------------------------

  template<uint32_t OPTIONS>
  BL_INLINE bool rasterize() noexcept {
    BL_ASSERT(uint32_t(_ey0) >= _bandOffset);

    using BLAnalyticRasterizerUtils::accErrStep;

    // Adjust `_ey1End` in case the line crosses the current band and banding is enabled.
    int _ey1End = _ey1;
    if (OPTIONS & kOptionBandingMode)
      _ey1End = blMin(_ey1End, int(_bandEnd));

    // Number of scanlines to rasterize excluding the first one.
    size_t i = unsigned(_ey1End) - unsigned(_ey0);
    uint32_t yOffset = unsigned(_ey0);

    if (OPTIONS & kOptionBandOffset)
      yOffset -= _bandOffset;

    BLBitWord* bitPtr = blOffsetPtr(bitPtrTop(), yOffset * bitStride<OPTIONS>());
    uint32_t* cellPtr = blOffsetPtr(cellPtrTop(), yOffset * cellStride());

    if (OPTIONS & kOptionBandingMode) {
      // Advance `_ey0` so it's valid for a next band if it crosses the current one.
      _ey0 += int(i) + 1;
    }

    const uint32_t fullCover = applySignMask(BL_PIPE_A8_SCALE);
    if (_flags & kFlagVertOrSingle) {
      // ....x....    .........
      // ....x....    .........
      // ....x.... or ....x....
      // ....x....    .........
      // ....x....    .........
      uint32_t cover;
      uint32_t area = uint32_t(_fx0) + uint32_t(_fx1);

      size_t bitIndex = unsigned(_ex0) / BL_PIPE_PIXELS_PER_ONE_BIT;
      BLBitWord bitMask = BLBitWord(0x1) << (bitIndex % blBitSizeOf<BLBitWord>());

      bitPtr += (bitIndex / blBitSizeOf<BLBitWord>());
      cellPtr += unsigned(_ex0);

      // First scanline or a line that occupies a single cell only. In case of banding
      // support this code can run multiple times, but it's safe as we adjust both
      // `_fy0` and `_fy1` accordingly.
      cover = applySignMask(uint32_t(_fy1 - _fy0));

      cellMerge(cellPtr, 0, cover, cover * area);
      bitPtr[0] |= bitMask;

      if (OPTIONS & kOptionBandingMode) {
        if (!i) {
          // Single cell line.
          if (_ey0 > _ey1)
            return true;

          // Border case: If the next scanline is end-of-line we must update
          // both `_fy0` and `_fy1` as we will only go through the same code
          // again.
          _fy0 = 0;
          _fy1 = _ey0 == _ey1 ? _savedFy1 : int(BL_PIPE_A8_SCALE);
          return false;
        }
      }
      else {
        // Single cell line.
        if (!i)
          return true;
      }

      // All scanlines between [_ey0:_ey1], exclusive.
      bitPtr = blOffsetPtr(bitPtr, bitStride<OPTIONS>());
      cellPtr = blOffsetPtr(cellPtr, cellStride());

      cover = fullCover;
      while (--i) {
        cellMerge(cellPtr, 0, cover, cover * area);
        cellPtr = blOffsetPtr(cellPtr, cellStride());

        bitPtr[0] |= bitMask;
        bitPtr = blOffsetPtr(bitPtr, bitStride<OPTIONS>());
      }

      if (OPTIONS & kOptionBandingMode) {
        if (_ey0 <= _ey1) {
          // Handle end-of-band case - renders the last scanline.
          cellMerge(cellPtr, 0, cover, cover * area);
          bitPtr[0] |= bitMask;

          // Border case: If the next scanline is end-of-line we must update
          // `_fy1` as we will only go through the initial cell next time.
          _fy0 = 0;
          _fy1 = _ey0 == _ey1 ? _savedFy1 : int(BL_PIPE_A8_SCALE);
          return false;
        }
      }

      // Special case - last scanline of the line.
      cover = applySignMask(uint32_t(_savedFy1));
      cellMerge(cellPtr, 0, cover, cover * area);
      bitPtr[0] |= bitMask;

      // Line ends within this band.
      return true;
    }
    else if (_dy >= _dx) {
      if (OPTIONS & kOptionBandingMode)
        i += (_ey0 <= _ey1);

      if (_flags & kFlagRightToLeft) {
        // ......x..
        // .....xx..
        // ....xx...
        // ...xx....
        // ...x.....
        for (;;) {
          // First and/or last scanline is a special-case that must consider `_fy0` and `_fy1`.
          // If this is a rasterizer that uses banding then this case will also be executed as
          // a start of each band, which is fine as it can handle all cases by design.
          uint32_t area = uint32_t(_fx0);
          uint32_t cov0, cov1;

          _fx0 -= _xDlt;
          if (_fx0 < 0) {
            _ex0--;
            _fx0 += BL_PIPE_A8_SCALE;
            _yDlt &= BL_PIPE_A8_MASK;

            if (!area) {
              area = BL_PIPE_A8_SCALE;
              accErrStep(_yDlt, _yErr, _yRem, _dx);
              _yDlt += _yLift;
              goto VertRightToLeftSingleFirstOrLast;
            }

            bitSet<OPTIONS>(bitPtr, unsigned(_ex0 + 0) / BL_PIPE_PIXELS_PER_ONE_BIT);
            bitSet<OPTIONS>(bitPtr, unsigned(_ex0 + 1) / BL_PIPE_PIXELS_PER_ONE_BIT);
            cov0 = applySignMask(uint32_t(_yDlt - _fy0));
            area = cov0 * area;
            cellMerge(cellPtr, _ex0 + 1, cov0, area);

            cov0 = applySignMask(uint32_t(_fy1 - _yDlt));
            area = cov0 * (uint32_t(_fx0) + BL_PIPE_A8_SCALE);
            cellMerge(cellPtr, _ex0, cov0, area);

            accErrStep(_yDlt, _yErr, _yRem, _dx);
            _yDlt += _yLift;
          }
          else {
VertRightToLeftSingleFirstOrLast:
            bitSet<OPTIONS>(bitPtr, unsigned(_ex0) / BL_PIPE_PIXELS_PER_ONE_BIT);
            cov0 = applySignMask(uint32_t(_fy1 - _fy0));
            area = cov0 * (area + uint32_t(_fx0));
            cellMerge(cellPtr, _ex0, cov0, area);
          }

          _fy0 = 0;
          bitPtr = blOffsetPtr(bitPtr, bitStride<OPTIONS>());
          cellPtr = blOffsetPtr(cellPtr, cellStride());

          if (!i) {
            if (OPTIONS & kOptionBandingMode) {
              if (_ey0 > _ey1)
                return true;

              _xDlt = _xLift;
              accErrStep(_xDlt, _xErr, _xRem, _dy);
              return false;
            }
            else {
              return true;
            }
          }

          // All scanlines between [_ey0:_ey1], exclusive.
          while (--i) {
            _xDlt = _xLift;
            accErrStep(_xDlt, _xErr, _xRem, _dy);

            area = uint32_t(_fx0);
            _fx0 -= _xDlt;

            if (_fx0 < 0) {
              _ex0--;
              _fx0 += BL_PIPE_A8_SCALE;
              _yDlt &= BL_PIPE_A8_MASK;

              if (!area) {
                area = BL_PIPE_A8_SCALE;
                accErrStep(_yDlt, _yErr, _yRem, _dx);
                _yDlt += _yLift;
                goto VertRightToLeftSingleInLoop;
              }

              bitSet<OPTIONS>(bitPtr, unsigned(_ex0 + 0) / BL_PIPE_PIXELS_PER_ONE_BIT);
              bitSet<OPTIONS>(bitPtr, unsigned(_ex0 + 1) / BL_PIPE_PIXELS_PER_ONE_BIT);
              bitPtr = blOffsetPtr(bitPtr, bitStride<OPTIONS>());

              cov1 = applySignMask(uint32_t(_yDlt));
              area = cov1 * area;
              cellAdd(cellPtr, _ex0 + 2, area);

              cov0 = fullCover - cov1;
              cov1 = blBitShl(cov1, 9) - area;
              area = cov0 * (uint32_t(_fx0) + BL_PIPE_A8_SCALE);

              cov0 = blBitShl(cov0, 9) - area;
              cov1 = cov1 + area;

              cellAdd(cellPtr, _ex0 + 0, cov0);
              cellAdd(cellPtr, _ex0 + 1, cov1);
              cellPtr = blOffsetPtr(cellPtr, cellStride());

              accErrStep(_yDlt, _yErr, _yRem, _dx);
              _yDlt += _yLift;
            }
            else {
VertRightToLeftSingleInLoop:
              bitSet<OPTIONS>(bitPtr, unsigned(_ex0) / BL_PIPE_PIXELS_PER_ONE_BIT);
              bitPtr = blOffsetPtr(bitPtr, bitStride<OPTIONS>());
              area = fullCover * (area + uint32_t(_fx0));

              cellMerge(cellPtr, _ex0, fullCover, area);
              cellPtr = blOffsetPtr(cellPtr, cellStride());
            }
          }

          if (OPTIONS & kOptionBandingMode) {
            if (_ey0 >= _ey1) {
              // Last scanline, we will do it either now or in the next band (border-case).
              _fy1 = _savedFy1;
              _xDlt = blBitShl(_ex0 - _ex1, BL_PIPE_A8_SHIFT) + _fx0 - _fx1;
              BL_ASSERT(_xDlt >= 0);

              // Border case, last scanline is the first scanline in the next band.
              if (_ey0 == _ey1)
                return false;
            }
            else {
              _xDlt = _xLift;
              accErrStep(_xDlt, _xErr, _xRem, _dy);
              return false;
            }
          }
          else {
            // Prepare the last scanline.
            _fy1 = _savedFy1;
            _xDlt = blBitShl(_ex0 - _ex1, BL_PIPE_A8_SHIFT) + _fx0 - _fx1;
            BL_ASSERT(_xDlt >= 0);
          }
        }
      }
      else {
        // ..x......
        // ..xx.....
        // ...xx....
        // ....xx...
        // .....x...
        for (;;) {
          // First and/or last scanline is a special-case that must consider `_fy0` and `_fy1`.
          // If this is a rasterizer that uses banding then this case will also be executed as
          // a start of each band, which is fine as it can handle all cases by design.
          uint32_t area = uint32_t(_fx0);
          uint32_t cov0, cov1;

          _fx0 += _xDlt;
          bitSet<OPTIONS>(bitPtr, unsigned(_ex0) / BL_PIPE_PIXELS_PER_ONE_BIT);

          if (_fx0 <= BL_PIPE_A8_SCALE) {
            cov0 = applySignMask(uint32_t(_fy1 - _fy0));
            area = cov0 * (area + uint32_t(_fx0));
            cellMerge(cellPtr, _ex0, cov0, area);

            if (_fx0 == BL_PIPE_A8_SCALE) {
              _ex0++;
              _fx0 = 0;
              _yDlt += _yLift;
              accErrStep(_yDlt, _yErr, _yRem, _dx);
            }
          }
          else {
            _ex0++;
            _fx0 &= BL_PIPE_A8_MASK;
            _yDlt &= BL_PIPE_A8_MASK;
            bitSet<OPTIONS>(bitPtr, unsigned(_ex0) / BL_PIPE_PIXELS_PER_ONE_BIT);

            cov0 = applySignMask(uint32_t(_yDlt - _fy0));
            area = cov0 * (area + BL_PIPE_A8_SCALE);
            cellMerge(cellPtr, _ex0 - 1, cov0, area);

            cov0 = applySignMask(uint32_t(_fy1 - _yDlt));
            area = cov0 * uint32_t(_fx0);
            cellMerge(cellPtr, _ex0, cov0, area);

            _yDlt += _yLift;
            accErrStep(_yDlt, _yErr, _yRem, _dx);
          }

          _fy0 = 0;
          bitPtr = blOffsetPtr(bitPtr, bitStride<OPTIONS>());
          cellPtr = blOffsetPtr(cellPtr, cellStride());

          if (!i) {
            if (OPTIONS & kOptionBandingMode) {
              if (_ey0 > _ey1)
                return true;
              _yDlt += _yLift;
              accErrStep(_yDlt, _yErr, _yRem, _dx);
              return false;
            }
            else {
              // If this was the only scanline (first and last at the same time) it would end here.
              return true;
            }
          }

          // All scanlines between [_ey0:_ey1], exclusive.
          while (--i) {
            _xDlt = _xLift;
            accErrStep(_xDlt, _xErr, _xRem, _dy);

            area = uint32_t(_fx0);
            _fx0 += _xDlt;
            bitSet<OPTIONS>(bitPtr, unsigned(_ex0) / BL_PIPE_PIXELS_PER_ONE_BIT);

            if (_fx0 <= BL_PIPE_A8_SCALE) {
              bitPtr = blOffsetPtr(bitPtr, bitStride<OPTIONS>());
              area = fullCover * (area + uint32_t(_fx0));

              cellMerge(cellPtr, _ex0, fullCover, area);
              cellPtr = blOffsetPtr(cellPtr, cellStride());

              if (_fx0 < BL_PIPE_A8_SCALE)
                continue;

              _ex0++;
              _fx0 = 0;
            }
            else {
              _fx0 &= BL_PIPE_A8_MASK;
              _yDlt &= BL_PIPE_A8_MASK;

              cov0 = applySignMask(uint32_t(_yDlt));
              cov1 = cov0 * (area + BL_PIPE_A8_SCALE);

              cov0 = blBitShl(cov0, 9) - cov1;
              cellAdd(cellPtr, _ex0 + 0, cov0);
              _ex0++;

              cov0 = applySignMask(BL_PIPE_A8_SCALE - uint32_t(_yDlt));
              area = cov0 * uint32_t(_fx0);

              cov0 = blBitShl(cov0, 9) - area + cov1;
              cellAdd(cellPtr, _ex0 + 0, cov0);
              cellAdd(cellPtr, _ex0 + 1, area);
              cellPtr = blOffsetPtr(cellPtr, cellStride());

              bitSet<OPTIONS>(bitPtr, unsigned(_ex0) / BL_PIPE_PIXELS_PER_ONE_BIT);
              bitPtr = blOffsetPtr(bitPtr, bitStride<OPTIONS>());
            }

            _yDlt += _yLift;
            accErrStep(_yDlt, _yErr, _yRem, _dx);
          }

          if (OPTIONS & kOptionBandingMode) {
            // Last scanline, we will do it either now or in the next band (border-case).
            if (_ey0 >= _ey1) {
              _fy1 = _savedFy1;
              _xDlt = ((_ex1 - _ex0) << BL_PIPE_A8_SHIFT) + _fx1 - _fx0;
              BL_ASSERT(_xDlt >= 0);

              // Border case, last scanline is the first scanline in the next band.
              if (_ey0 == _ey1)
                return false;
            }
            else {
              _xDlt = _xLift;
              accErrStep(_xDlt, _xErr, _xRem, _dy);
              return false;
            }
          }
          else {
            // Prepare the last scanline.
            _fy1 = _savedFy1;
            _xDlt = ((_ex1 - _ex0) << BL_PIPE_A8_SHIFT) + _fx1 - _fx0;
          }
        }
      }
    }
    else {
      // Since both first and last scanlines are special we set `i` to one and then repeatedly
      // to number of scanlines in middle, and then to `1` again for the last one. Since this
      // is a horizontally oriented line this overhead is fine and makes the rasterizer cleaner.
      size_t j = 1;
      int xLocal = (_ex0 << BL_PIPE_A8_SHIFT) + _fx0;

      uint32_t cover;
      uint32_t area;

      if (_flags & kFlagRightToLeft) {
        // .........
        // ......xxx
        // ..xxxxx..
        // xxx......
        // .........
        if (_flags & kFlagInitialScanline) {
          _flags &= ~kFlagInitialScanline;

          j = i;
          i = 1;

          if (_fx0 == 0) {
            _fx0 = BL_PIPE_A8_SCALE;
            _ex0--;

            _yDlt += _yLift;
            accErrStep(_yDlt, _yErr, _yRem, _dx);
          }

          cover = applySignMask(uint32_t(_yDlt - _fy0));
          BL_ASSERT(int32_t(cover) >= -int32_t(BL_PIPE_A8_SCALE) && int32_t(cover) <= int32_t(BL_PIPE_A8_SCALE));

          if (_fx0 - _xDlt < 0)
            goto HorzRightToLeftInside;

          xLocal -= _xDlt;
          bitSet<OPTIONS>(bitPtr, unsigned(_ex0) / BL_PIPE_PIXELS_PER_ONE_BIT);

          cover = applySignMask(uint32_t(_fy1 - _fy0));
          area = cover * uint32_t(_fx0 * 2 - _xDlt);
          cellMerge(cellPtr, _ex0, cover, area);

          if ((xLocal & BL_PIPE_A8_MASK) == 0) {
            _yDlt += _yLift;
            accErrStep(_yDlt, _yErr, _yRem, _dx);
          }

          bitPtr = blOffsetPtr(bitPtr, bitStride<OPTIONS>());
          cellPtr = blOffsetPtr(cellPtr, cellStride());
          i--;
        }

        for (;;) {
          while (i) {
            _xDlt = _xLift;
            accErrStep(_xDlt, _xErr, _xRem, _dy);

            _ex0 = xLocal >> BL_PIPE_A8_SHIFT;
            _fx0 = xLocal & BL_PIPE_A8_MASK;

            if (_fx0 == 0) {
              _fx0 = int(BL_PIPE_A8_SCALE);
              _ex0--;
            }

HorzRightToLeftSkip:
            _yDlt -= int(BL_PIPE_A8_SCALE);
            cover = applySignMask(uint32_t(_yDlt));
            BL_ASSERT(int32_t(cover) >= -int32_t(BL_PIPE_A8_SCALE) && int32_t(cover) <= int32_t(BL_PIPE_A8_SCALE));

HorzRightToLeftInside:
            xLocal -= _xDlt;
            {
              int exLocal = xLocal >> BL_PIPE_A8_SHIFT;
              int fxLocal = xLocal & BL_PIPE_A8_MASK;

              bitFill<OPTIONS>(bitPtr, unsigned(exLocal) / BL_PIPE_PIXELS_PER_ONE_BIT, unsigned(_ex0) / BL_PIPE_PIXELS_PER_ONE_BIT);
              area = cover * uint32_t(_fx0);

              while (_ex0 != exLocal) {
                cellMerge(cellPtr, _ex0, cover, area);

                cover = uint32_t(_yLift);
                accErrStep(cover, _yErr, _yRem, _dx);
                _yDlt += int32_t(cover);

                cover = applySignMask(cover);
                area = cover * BL_PIPE_A8_SCALE;

                _ex0--;
              }

              cover += applySignMask(uint32_t(_fy1 - _yDlt));
              area = cover * (uint32_t(fxLocal) + BL_PIPE_A8_SCALE);
              cellMerge(cellPtr, _ex0, cover, area);

              if (fxLocal == 0) {
                _yDlt += _yLift;
                accErrStep(_yDlt, _yErr, _yRem, _dx);
              }
            }

            bitPtr = blOffsetPtr(bitPtr, bitStride<OPTIONS>());
            cellPtr = blOffsetPtr(cellPtr, cellStride());

            i--;
          }

          _fy0 = 0;
          _fy1 = BL_PIPE_A8_SCALE;

          if (OPTIONS & kOptionBandingMode) {
            if (!j) {
              _ex0 = xLocal >> BL_PIPE_A8_SHIFT;
              _fx0 = xLocal & BL_PIPE_A8_MASK;
              return _ey0 > _ey1;
            }
          }
          else {
            if (!j)
              return true;
          }

          i = j - 1;
          j = 1;

          if (!i) {
            i = 1;
            j = 0;

            bool isLast = (OPTIONS & kOptionBandingMode) ? _ey0 > _ey1 : true;
            if (!isLast) continue;

            _xDlt = xLocal - ((_ex1 << BL_PIPE_A8_SHIFT) + _fx1);
            _fy1 = _savedFy1;

            _ex0 = xLocal >> BL_PIPE_A8_SHIFT;
            _fx0 = xLocal & BL_PIPE_A8_MASK;

            if (_fx0 == 0) {
              _fx0 = int(BL_PIPE_A8_SCALE);
              _ex0--;
            }

            if (_fx0 - _xDlt >= 0) {
              cover = applySignMask(uint32_t(_fy1));
              area = cover * uint32_t(_fx0 * 2 - _xDlt);

              cellMerge(cellPtr, _ex0, cover, area);
              bitSet<OPTIONS>(bitPtr, unsigned(_ex0) / BL_PIPE_PIXELS_PER_ONE_BIT);

              return true;
            }

            goto HorzRightToLeftSkip;
          }
        }
      }
      else {
        // .........
        // xxx......
        // ..xxxxx..
        // ......xxx
        // .........
        if (_flags & kFlagInitialScanline) {
          _flags &= ~kFlagInitialScanline;

          j = i;
          i = 1;

          cover = applySignMask(uint32_t(_yDlt - _fy0));
          if (_fx0 + _xDlt > int(BL_PIPE_A8_SCALE))
            goto HorzLeftToRightInside;

          xLocal += _xDlt;
          bitSet<OPTIONS>(bitPtr, unsigned(_ex0) / BL_PIPE_PIXELS_PER_ONE_BIT);

          // First scanline is only a single pixel, we deal with it here as it's a special case.
          cover = applySignMask(uint32_t(_fy1 - _fy0));
          area = cover * (uint32_t(_fx0) * 2 + uint32_t(_xDlt));
          cellMerge(cellPtr, _ex0, cover, area);

          if (_fx0 + _xDlt == int(BL_PIPE_A8_SCALE)) {
            _yDlt += _yLift;
            accErrStep(_yDlt, _yErr, _yRem, _dx);
          }

          bitPtr = blOffsetPtr(bitPtr, bitStride<OPTIONS>());
          cellPtr = blOffsetPtr(cellPtr, cellStride());
          i--;
        }

        for (;;) {
          while (i) {
            _xDlt = _xLift;
            accErrStep(_xDlt, _xErr, _xRem, _dy);
            _ex0 = xLocal >> BL_PIPE_A8_SHIFT;
            _fx0 = xLocal & BL_PIPE_A8_MASK;

HorzLeftToRightSkip:
            _yDlt -= int(BL_PIPE_A8_SCALE);
            cover = applySignMask(uint32_t(_yDlt));
            BL_ASSERT(int32_t(cover) >= -int32_t(BL_PIPE_A8_SCALE) && int32_t(cover) <= int32_t(BL_PIPE_A8_SCALE));

HorzLeftToRightInside:
            xLocal += _xDlt;
            {
              int exLocal = xLocal >> BL_PIPE_A8_SHIFT;
              int fxLocal = xLocal & BL_PIPE_A8_MASK;
              BL_ASSERT(_ex0 != exLocal);

              if (fxLocal == 0) {
                fxLocal = BL_PIPE_A8_SCALE;
                exLocal--;
              }

              bitFill<OPTIONS>(bitPtr, unsigned(_ex0) / BL_PIPE_PIXELS_PER_ONE_BIT, unsigned(exLocal) / BL_PIPE_PIXELS_PER_ONE_BIT);
              area = cover * (uint32_t(_fx0) + BL_PIPE_A8_SCALE);

              while (_ex0 != exLocal) {
                cellMerge(cellPtr, _ex0, cover, area);

                cover = uint32_t(_yLift);
                accErrStep(cover, _yErr, _yRem, _dx);
                _yDlt += int32_t(cover);

                cover = applySignMask(cover);
                area = cover * BL_PIPE_A8_SCALE;

                _ex0++;
              }

              cover += applySignMask(uint32_t(_fy1 - _yDlt));
              area = cover * uint32_t(fxLocal);
              cellMerge(cellPtr, _ex0, cover, area);

              if (fxLocal == BL_PIPE_A8_SCALE) {
                _yDlt += _yLift;
                accErrStep(_yDlt, _yErr, _yRem, _dx);
              }
            }

            bitPtr = blOffsetPtr(bitPtr, bitStride<OPTIONS>());
            cellPtr = blOffsetPtr(cellPtr, cellStride());

            i--;
          }

          _fy0 = 0;
          _fy1 = BL_PIPE_A8_SCALE;

          if (OPTIONS & kOptionBandingMode) {
            if (!j) {
              _ex0 = xLocal >> BL_PIPE_A8_SHIFT;
              _fx0 = xLocal & BL_PIPE_A8_MASK;
              return _ey0 > _ey1;
            }
          }
          else {
            if (!j)
              return true;
          }

          i = j - 1;
          j = 1;

          if (!i) {
            i = 1;
            j = 0;

            bool isLast = (OPTIONS & kOptionBandingMode) ? _ey0 > _ey1 : true;
            if (!isLast) continue;

            _xDlt = ((_ex1 << BL_PIPE_A8_SHIFT) + _fx1) - xLocal;
            _fy1 = _savedFy1;

            _ex0 = xLocal >> BL_PIPE_A8_SHIFT;
            _fx0 = xLocal & BL_PIPE_A8_MASK;

            if (_fx0 + _xDlt <= int(BL_PIPE_A8_SCALE)) {
              cover = applySignMask(uint32_t(_fy1));
              area = cover * (uint32_t(_fx0) * 2 + uint32_t(_xDlt));

              cellMerge(cellPtr, _ex0, cover, area);
              bitSet<OPTIONS>(bitPtr, unsigned(_ex0) / BL_PIPE_PIXELS_PER_ONE_BIT);

              return true;
            }

            goto HorzLeftToRightSkip;
          }
        }
      }
    }
  }

  // --------------------------------------------------------------------------
  // [Bit Helpers]
  // --------------------------------------------------------------------------

  //! Set bit `x` to 1 in a bit-vector starting at `bitPtr`.
  template<uint32_t OPTIONS, typename X>
  BL_INLINE void bitSet(BLBitWord* bitPtr, X x) const noexcept {
    typedef typename std::make_unsigned<X>::type U;

    if (OPTIONS & kOptionEasyBitStride)
      bitPtr[0] |= BLBitWord(1u) << U(x);
    else
      bitPtr[U(x) / blBitSizeOf<BLBitWord>()] |= BLBitWord(1u) << (U(x) % blBitSizeOf<BLBitWord>());
  }

  //! Fill bits between `first` and `last` (inclusive) in a bit-vector starting at `bitPtr`.
  template<uint32_t OPTIONS, typename X>
  BL_INLINE void bitFill(BLBitWord* bitPtr, X first, X last) const noexcept {
    typedef typename std::make_unsigned<X>::type U;

    BL_ASSERT(first <= last);

    if (OPTIONS & kOptionEasyBitStride) {
      BL_ASSERT(first < blBitSizeOf<BLBitWord>());
      BL_ASSERT(last < blBitSizeOf<BLBitWord>());

      bitPtr[0] |= (~BLBitWord(0) << U(first)) ^ (~BLBitWord(1) << U(last));
    }
    else {
      size_t idxCur = U(first) / blBitSizeOf<BLBitWord>();
      size_t idxEnd = U(last) / blBitSizeOf<BLBitWord>();

      BLBitWord mask = blBitOnes<BLBitWord>() << (U(first) % blBitSizeOf<BLBitWord>());
      if (idxCur != idxEnd) {
        bitPtr[idxCur] |= mask;
        mask |= blBitOnes<BLBitWord>();
        while (++idxCur != idxEnd)
          bitPtr[idxCur] = mask;
      }

      mask ^= (~BLBitWord(1)) << (U(last) % blBitSizeOf<BLBitWord>());
      bitPtr[idxCur] |= mask;
    }
  }

  // --------------------------------------------------------------------------
  // [Cell Helpers]
  // --------------------------------------------------------------------------

  template<typename T>
  BL_INLINE T applySignMask(T cover) const noexcept {
    return BLAnalyticRasterizerUtils::applySignMask(cover, _signMask);
  }

  template<typename X>
  BL_INLINE void cellAdd(uint32_t* cellPtr, X x, uint32_t value) const noexcept {
    BL_ASSERT(x >= 0);
    typedef typename std::make_unsigned<X>::type U;

    cellPtr[size_t(U(x))] += value;
  }

  template<typename X>
  BL_INLINE void cellMerge(uint32_t* cellPtr, X x, uint32_t cover, uint32_t area) const noexcept {
    BL_ASSERT(x >= 0);
    typedef typename std::make_unsigned<X>::type U;

    cellPtr[size_t(U(x)) + 0] += blBitShl(cover, 9) - area;
    cellPtr[size_t(U(x)) + 1] += area;
  }
};

//! \}
//! \endcond

#endif // BLEND2D_RASTER_BLANALYTICRASTERIZER_P_H
