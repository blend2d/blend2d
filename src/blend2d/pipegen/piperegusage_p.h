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

#ifndef BLEND2D_PIPEGEN_PIPEREGUSAGE_P_H_INCLUDED
#define BLEND2D_PIPEGEN_PIPEREGUSAGE_P_H_INCLUDED

#include "../pipegen/pipegencore_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_pipegen
//! \{

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::PipeRegUsage]
// ============================================================================

//! Registers that are used/reserved by a PipePart.
struct PipeRegUsage {
  uint32_t _data[kNumVirtGroups];

  BL_INLINE void reset() noexcept {
    for (uint32_t i = 0; i < kNumVirtGroups; i++)
      _data[i] = 0;
  }

  BL_INLINE uint32_t& operator[](uint32_t kind) noexcept {
    ASMJIT_ASSERT(kind < kNumVirtGroups);
    return _data[kind];
  }

  BL_INLINE const uint32_t& operator[](uint32_t kind) const noexcept {
    ASMJIT_ASSERT(kind < kNumVirtGroups);
    return _data[kind];
  }

  BL_INLINE void set(const PipeRegUsage& other) noexcept {
    for (uint32_t i = 0; i < kNumVirtGroups; i++)
      _data[i] = other._data[i];
  }

  BL_INLINE void add(const PipeRegUsage& other) noexcept {
    for (uint32_t i = 0; i < kNumVirtGroups; i++)
      _data[i] += other._data[i];
  }

  BL_INLINE void max(const PipeRegUsage& other) noexcept {
    for (uint32_t i = 0; i < kNumVirtGroups; i++)
      _data[i] = blMax(_data[i], other._data[i]);
  }
};

} // {BLPipeGen}

//! \}
//! \endcond

#endif // BLEND2D_PIPEGEN_PIPEREGUSAGE_P_H_INCLUDED
