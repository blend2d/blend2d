// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_FETCHUTILSPIXELGATHER_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_FETCHUTILSPIXELGATHER_P_H_INCLUDED

#include "../../format.h"
#include "../../pipeline/jit/pipecompiler_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace bl {
namespace Pipeline {
namespace JIT {

// Interleave callback is used to interleave a sequence of code into pixel fetching sequence. There are two scenarios in
// general:
//
//   - Fetching is performed by scalar loads and shuffles to form the destination pixel. In this case individual fetches
//     can be interleaved with another code to hide the latency of reading from memory and shuffling.
//   - Fetching is performed by hardware (vpgatherxx). In this case the interleave code is inserted after gather to hide
//     its latency (i.e. to not immediately depend on gathered content).
typedef void (*InterleaveCallback)(uint32_t step, void* data) BL_NOEXCEPT;

static void dummyInterleaveCallback(uint32_t step, void* data) noexcept { blUnused(step, data); }

namespace FetchUtils {

enum class IndexLayout : uint32_t {
  //! Consecutive unsigned 16-bit indexes.
  kUInt16,
  //! Consecutive unsigned 32-bit indexes.
  kUInt32,
  //! Unsigned 16-bit indexes in lo 32-bit words (or, even 16-bit indexes).
  kUInt32Lo16,
  //! Unsigned 16-bit indexes in hi 32-bit words (or, odd 16-bit indexes).
  kUInt32Hi16
};

//! Index extractor makes it easy to extract indexes from SIMD registers. We have learned the hard way that the best
//! way of extracting indexes is to use stack instead of dedicated instructions like PEXTRW/PEXTRD. The problem of such
//! instructions is that they have high latency on many older micro-architectures. Newer architectures decreased the
//! latency, but even 2-3 cycles is worse than fetching the index from stack.
class IndexExtractor {
public:
  BL_NONCOPYABLE(IndexExtractor)

  enum Type : uint32_t {
    kTypeNone,
    kTypeInt16,
    kTypeUInt16,
    kTypeInt32,
    kTypeUInt32,
    kTypeCount
  };

  //! \name Members
  //! \{

  PipeCompiler* _pc;
  Vec _vec;
  Mem _mem;
  uint32_t _type;
  uint16_t _indexSize;
  uint16_t _memSize;

  //! \}

  //! Creates a zero-initialized `IndexExtractor`.
  //!
  //! You must call `begin()` to make it usable.
  explicit IndexExtractor(PipeCompiler* pc) noexcept;

  //! Begins index extraction from a SIMD register `vec`.
  void begin(uint32_t type, const Vec& vec) noexcept;
  //! Begins index extraction from memory.
  void begin(uint32_t type, const Mem& mem, uint32_t memSize) noexcept;
  //! Extracts the given `index` into the destination register `dst`.
  void extract(const Gp& dst, uint32_t index) noexcept;
};

//! Context that is used to fetch more than 1 pixel - used by SIMD fetchers that fetch 2, 4, 8, 16, or 32 pixels per
//! a single loop iteration.
class FetchContext {
public:
  BL_NONCOPYABLE(FetchContext)

  enum class FetchMode : uint8_t {
    //! Uninitialized.
    kNone = 0,

    //! Fetching packed A8 pixels from A8 buffer.
    kA8FromA8_PA,
    //! Fetching unpacked A8 pixels from A8 buffer.
    kA8FromA8_UA,
    //! Fetching packed A8 pixels from RGBA32 buffer.
    kA8FromRGBA32_PA,
    //! Fetching unpacked A8 pixels from RGBA32 buffer.
    kA8FromRGBA32_UA,

    //! Fetching packed RGBA32 pixels from A8 buffer.
    kRGBA32FromA8_PC,
    //! Fetching unpacked RGBA32 pixels from A8 buffer.
    kRGBA32FromA8_UC,

    //! Fetching packed RGBA32 pixels from RGBA32 buffer.
    kRGBA32FromRGBA32_PC,
    //! Fetching unpacked RGBA32 pixels from RGBA32 buffer.
    kRGBA32FromRGBA32_UC,

    //! Fetching packed RGBA64 pixels from RGBA64 buffer.
    kRGBA64FromRGBA64_PC
  };

  //! Widening operation that will be used to widen vectors (128-bit -> 256-bit and 256-bit -> 512-bit).
  enum class WideningOp {
    //! No operation (widening is disabled or not supported due to architecture constraints).
    kNone = 0,
    //! Interleave two vectors into a larger vector.
    kInterleave = 1,
    //! Unpack vector (convert from packed pixel format to unpacked).
    kUnpack = 2,
    //! Unpack 2 128-bit vectors.
    kUnpack2x = 3,
    //! Repeats a vector - repeats DCBA to DDCCBBAA.
    kRepeat = 4,
    //! Repeats a vector of 8 packed A8 pixels into 8 unpacked RGBA32 pixels (AVX-512 only).
    kRepeat8xA8ToRGBA32_UC_AVX512 = 5
  };

  //! \name Members
  //! \{

  //! Pipeline compiler.
  PipeCompiler* _pc {};
  //! Target pixel.
  Pixel* _pixel {};

  //! Required fetch flags, possibly enhanced to make the fetching logic simpler.
  PixelFlags _fetchFlags {};
  //! Pixel fetch information.
  PixelFetchInfo _fetchInfo {};
  //! The index of the pixel to be fetched next, starts at 0.
  uint32_t _pixelIndex {};
  //! The current vector index where the pixel will be fetched, incremented by `_vecStep`.
  uint32_t _vecIndex {};
  //! The step `_vecIndex` is incremented by when a vector lane reaches its end.
  uint32_t _vecStep {};
  //! The current lane index where the next pixel will be fetched.
  uint32_t _laneIndex {};
  //! Number of lanes to fetch pixels to - when `_laneIndex` reaches `_laneCount` it's zeroed and `_vecIndex`
  //! incremented by `_vecStep`.
  uint32_t _laneCount {};
  //! Fetch mode - selects the approach used to fetch pixels.
  FetchMode _fetchMode {};

  //! Fetch is predicated and never contains all the pixels (the fetcher can avoid checking the last pixel).
  GatherMode _gatherMode {};
  //! Number of 128-bit vectors to fetch into.
  uint8_t _p128Count {};
  //! Fetch was completed, `end()` was already called.
  bool _fetchDone = false;

  //! Temporary vector register that can be used as a load destination before inserting to any of _p128[].
  Vec _pTmp[2];

  //! Temporaries used by the fetcher.
  //!
  //! The maximum vector width of pTmp is 128-bits, because in general we cannot do element-wise insertion to wider
  //! vectors. This is consistent on all backends at the moment as AArch64 only has 128-bit vectors and X86_64 only
  //! allows insertion to 128-bit low part of a vector register, clearing the rest of it.
  Vec _p128[8];

#if defined(BL_JIT_ARCH_X86)
  //! Accumulator used by Alpha only fetches.
  //!
  //! On many targets this may be faster than using SIMD for 8-bit or 16-bit inserts.
  Gp _aAcc;

  //! Current index in `_aAcc` register.
  uint32_t _aAccIndex {};

  //! 256-bit wide vectors that can be used by pipelines taking advantage of AVX2 and AVX-512 extensions.
  VecArray _p256;
  //! 512-bit wide vectors that can be used by pipelines taking advantage of AVX-512 extensions.
  VecArray _p512;

  //! Widening operation used to widen 128-bit vectors to 256-bit vectors.
  WideningOp _widening256Op {};
  //! Widening operation used to widen 256-bit vectors to 512-bit vectors.
  WideningOp _widening512Op {};
#endif // BL_JIT_ARCH_X86

  //! \}

  inline FetchContext(PipeCompiler* pc, Pixel* pixel, PixelCount n, PixelFlags flags, PixelFetchInfo fInfo, GatherMode mode = GatherMode::kFetchAll) noexcept
    : _pc(pc),
      _pixel(pixel),
      _fetchFlags(flags),
      _fetchInfo(fInfo),
      _gatherMode(mode),
      _fetchDone(false) { _init(n); }

  BL_INLINE_NODEBUG FormatExt fetchFormat() const noexcept { return _fetchInfo.format(); }
  BL_INLINE_NODEBUG FetchMode fetchMode() const noexcept { return _fetchMode; }
  BL_INLINE_NODEBUG GatherMode gatherMode() const noexcept { return _gatherMode; }

  void _init(PixelCount n) noexcept;
  void _initFetchMode() noexcept;
  void _initFetchRegs() noexcept;
  void _initTargetPixel() noexcept;

  void fetchPixel(const Mem& src) noexcept;
  void _fetchAll(const Mem& src, uint32_t srcShift, IndexExtractor& extractor, const uint8_t* indexes, InterleaveCallback cb, void* cbData) noexcept;
  void _doneVec(uint32_t index) noexcept;

  // Fetches all pixels and allows to interleave the fetch sequence with a lambda function `interleaveFunc`.
  template<class InterleaveFunc>
  void fetchAll(const Mem& src, uint32_t srcShift, IndexExtractor& extractor, const uint8_t* indexes, InterleaveFunc&& interleaveFunc) noexcept {
    _fetchAll(src, srcShift, extractor, indexes, [](uint32_t step, void* data) noexcept {
      (*static_cast<const InterleaveFunc*>(data))(step);
    }, (void*)&interleaveFunc);
  }

  void end() noexcept;
};

void gatherPixels(PipeCompiler* pc, Pixel& p, PixelCount n, PixelFlags flags, PixelFetchInfo fInfo, const Mem& src, const Vec& idx, uint32_t shift, IndexLayout indexLayout, GatherMode mode, InterleaveCallback cb, void* cbData) noexcept;

template<class InterleaveFunc>
static void gatherPixels(PipeCompiler* pc, Pixel& p, PixelCount n, PixelFlags flags, PixelFetchInfo fInfo, const Mem& src, const Vec& idx, uint32_t shift, IndexLayout indexLayout, GatherMode mode, InterleaveFunc&& interleaveFunc) noexcept {
  gatherPixels(pc, p, n, flags, fInfo, src, idx, shift, indexLayout, mode, [](uint32_t step, void* data) noexcept {
    (*static_cast<const InterleaveFunc*>(data))(step);
  }, (void*)&interleaveFunc);
}

} // {FetchUtils}
} // {JIT}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_FETCHUTILSPIXELGATHER_P_H_INCLUDED
