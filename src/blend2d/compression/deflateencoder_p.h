// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_COMPRESSION_DEFLATEENCODER_P_H_INCLUDED
#define BLEND2D_COMPRESSION_DEFLATEENCODER_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../array.h"
#include "../compression/deflatedefs_p.h"

//! \cond INTERNAL

namespace bl {
namespace Compression {
namespace Deflate {

struct EncoderImpl;

static constexpr uint32_t kMaxCompressionLevel = 12;

class Encoder {
public:
  EncoderImpl* impl;

  BL_INLINE Encoder() noexcept : impl(nullptr) {}
  BL_INLINE ~Encoder() noexcept { reset(); }

  BL_INLINE bool isInitialized() const noexcept { return impl != nullptr; }

  BLResult init(FormatType format, uint32_t compressionLevel) noexcept;
  void reset() noexcept;

  size_t minimumOutputBufferSize(size_t inputSize) const noexcept;
  size_t compressTo(uint8_t* output, size_t outputSize, const uint8_t* input, size_t inputSize) noexcept;
  BLResult compress(BLArray<uint8_t>& dst, BLModifyOp modifyOp, BLDataView input) noexcept;
};

} // {Deflate}
} // {Compression}
} // {bl}

//! \endcond

#endif // BLEND2D_COMPRESSION_DEFLATEENCODER_P_H_INCLUDED
