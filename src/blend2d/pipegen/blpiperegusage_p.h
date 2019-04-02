// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_PIPEGEN_BLPIPEREGUSAGE_P_H
#define BLEND2D_PIPEGEN_BLPIPEREGUSAGE_P_H

#include "../pipegen/blpipegencore_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_pipegen
//! \{

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::PipeRegUsage]
// ============================================================================

//! \ingroup blend2d_pipegen
//!
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

#endif // BLEND2D_PIPEGEN_BLPIPEREGUSAGE_P_H
