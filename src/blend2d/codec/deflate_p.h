// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef BLEND2D_CODEC_DEFLATE_P_H_INCLUDED
#define BLEND2D_CODEC_DEFLATE_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../array.h"

//! \cond INTERNAL

// ============================================================================
// [BLDeflate]
// ============================================================================

struct Deflate {
  //! Callback that is used to read a chunk of data to be consumed by the
  //! decoder. It was introduced for PNG support, which can divide the data
  //! stream into multiple `"IDAT"` chunks, thus the stream is not continuous.
  //!
  //! The logic has been simplified in a way that `ReadFunc` reads the first
  //! and all consecutive chunks. There is no other way to be consumed by the
  //! decoder.
  typedef bool (BL_CDECL* ReadFunc)(void* readCtx, const uint8_t** pData, const uint8_t** pEnd) BL_NOEXCEPT;

  //! Deflate data retrieved by `ReadFunc` into `dst` buffer.
  static BLResult deflate(BLArray<uint8_t>& dst, void* readCtx, ReadFunc readFunc, bool hasHeader) noexcept;
};

//! \endcond

#endif // BLEND2D_CODEC_DEFLATE_P_H_INCLUDED
