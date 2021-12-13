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

namespace BLCompression {
namespace Deflate {

struct EncoderImpl;

class Encoder {
public:
  EncoderImpl* impl;

  BL_INLINE Encoder() noexcept : impl(nullptr) {}
  BL_INLINE ~Encoder() noexcept { reset(); }

  BL_INLINE bool isInitialized() const noexcept { return impl != nullptr; }

  BLResult init(uint32_t format, uint32_t compressionLevel) noexcept;
  void reset() noexcept;

  size_t minimumOutputBufferSize(size_t inputSize) const noexcept;
  size_t compress(void* output, size_t outputSize, const void* input, size_t inputSize) noexcept;
};

} // {Deflate}
} // {BLCompression}

//! \endcond

#endif // BLEND2D_COMPRESSION_DEFLATEENCODER_P_H_INCLUDED
