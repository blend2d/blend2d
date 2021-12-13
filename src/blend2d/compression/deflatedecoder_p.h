// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_COMPRESSION_DEFLATEDECODER_P_H_INCLUDED
#define BLEND2D_COMPRESSION_DEFLATEDECODER_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../array.h"

//! \cond INTERNAL

namespace BLCompression {
namespace Deflate {

//! Callback that is used to read a chunk of data to be consumed by the decoder. It was introduced for PNG support,
//! which can divide the data stream into multiple `"IDAT"` chunks, thus the stream is not continuous.
//!
//! The logic has been simplified in a way that `ReadFunc` reads the first and all consecutive chunks. There is no
//! other way to be consumed by the decoder.
typedef bool (BL_CDECL* ReadFunc)(void* readCtx, const uint8_t** pData, const uint8_t** pEnd) BL_NOEXCEPT;

//! Deflate data retrieved by `ReadFunc` into `dst` buffer.
BLResult deflate(BLArray<uint8_t>& dst, void* readCtx, ReadFunc readFunc, bool hasHeader) noexcept;

} // {Deflate}
} // {BLCompression}

//! \endcond

#endif // BLEND2D_COMPRESSION_DEFLATEDECODER_P_H_INCLUDED
