// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_PIPERUNTIME_P_H_INCLUDED
#define BLEND2D_PIPELINE_PIPERUNTIME_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../simd_p.h"
#include "../pipeline/pipedefs_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace BLPipeline {

struct PipeRuntime;
struct PipeProvider;
struct PipeLookupCache;

enum class PipeRuntimeType : uint8_t {
  //! Static runtime that doesn't use JIT (can be either a reference reference in pure C++ or a SIMD optimized one).
  kStatic = 0,
  //! JIT runtime, which provides dynamic pipeline construction.
  kJIT = 1
};

enum class PipeRuntimeFlags : uint8_t {
  kNone = 0,
  kIsolated = 0x01
};

BL_DEFINE_ENUM_FLAGS(PipeRuntimeFlags)

//! This is a base class used by either `PipeDynamicRuntime` or `PipeStaticRuntime`. The purpose of this class is to
//! create an interface that is used by the rendering context so it doesn't have to know which kind of pipelines it uses.
struct PipeRuntime {
  //! Type of the runtime, see `PipeRuntimeType`.
  PipeRuntimeType _runtimeType;
  //! Runtime flags.
  PipeRuntimeFlags _runtimeFlags;
  //! Size of this runtime in bytes.
  uint16_t _runtimeSize;

  //! Runtime destructor.
  void (BL_CDECL* _destroy)(PipeRuntime* self) BL_NOEXCEPT;

  //! Functions exposed by the runtime that are copied to `PipeProvider` to make them local in the rendering context.
  //! It seems hacky, but this removes one extra indirection that would be needed if they were virtual.
  struct Funcs {
    BLResult (BL_CDECL* test)(PipeRuntime* self, uint32_t signature, DispatchData* out, PipeLookupCache* cache) BL_NOEXCEPT;
    BLResult (BL_CDECL* get)(PipeRuntime* self, uint32_t signature, DispatchData* out, PipeLookupCache* cache) BL_NOEXCEPT;
  } _funcs;

  BL_INLINE PipeRuntimeType runtimeType() const noexcept { return _runtimeType; }
  BL_INLINE PipeRuntimeFlags runtimeFlags() const noexcept { return _runtimeFlags; }
  BL_INLINE uint32_t runtimeSize() const noexcept { return _runtimeSize; }

  BL_INLINE void destroy() noexcept { _destroy(this); }
};

//! Pipeline provider.
struct PipeProvider {
  PipeRuntime* _runtime;
  PipeRuntime::Funcs _funcs;

  BL_INLINE PipeProvider() noexcept
    : _runtime(nullptr),
      _funcs {} {}

  BL_INLINE bool isInitialized() const noexcept {
    return _runtime != nullptr;
  }

  BL_INLINE void init(PipeRuntime* runtime) noexcept {
    _runtime = runtime;
    _funcs = runtime->_funcs;
  }

  BL_INLINE void reset() noexcept {
    memset(this, 0, sizeof(*this));
  }

  BL_INLINE PipeRuntime* runtime() const noexcept {
    return _runtime;
  }

  BL_INLINE BLResult test(uint32_t signature, DispatchData* out, PipeLookupCache* cache) const noexcept {
    return _funcs.test(_runtime, signature, out, cache);
  }

  BL_INLINE BLResult get(uint32_t signature, DispatchData* out, PipeLookupCache* cache) const noexcept {
    return _funcs.get(_runtime, signature, out, cache);
  }
};

//! Pipe lookup cache is a local cache used by the rendering engine to store `N` recently used pipelines so it doesn't
//! have to use `PipeProvider` that that has a considerably higher overhead.
struct alignas(16) PipeLookupCache {
  // Number of cached pipelines, must be multiply of 4.
#if BL_TARGET_ARCH_X86
  static const uint32_t N = 16; // SSE2 friendly option.
#else
  static const uint32_t N = 8;
#endif

  struct IndexMatch {
    size_t _index;

    BL_INLINE bool isValid() const noexcept { return _index < N; }
    BL_INLINE size_t index() const noexcept { return _index; }
  };

  struct BitMatch {
    uint32_t _bits;

    BL_INLINE bool isValid() const noexcept { return _bits != 0; }
    BL_INLINE size_t index() const noexcept { return BLIntOps::ctz(_bits); }
  };

  //! Array of signatures for the lookup, uninitialized signatures are zero.
  uint32_t _signatures[N];
  //! Index where the next signature will be written (incremental, wraps to zero).
  size_t _currentIndex;
  //! Array of functions matching signatures stored in `_signatures` array.
  DispatchData _dispatchData[N];

  BL_INLINE void reset() {
    memset(_signatures, 0, sizeof(_signatures));
    _currentIndex = 0;
  }

#if defined(BL_TARGET_OPT_SSE2)
  typedef BitMatch MatchType;
  BL_INLINE MatchType match(uint32_t signature) const noexcept {
    BL_STATIC_ASSERT(N == 4 || N == 8 || N == 16);
    using namespace SIMD;

    Vec128I vSign = v_fill_i128_u32(signature);

    switch (N) {
      case 4: {
        Vec128I vec0 = v_cmp_eq_i32(v_loada_i128(_signatures + 0), vSign);
        return BitMatch { uint32_t(_mm_movemask_ps(v_cast<Vec128F>(vec0))) };
      }

      case 8: {
        Vec128I vec0 = v_cmp_eq_i32(v_loada_i128(_signatures + 0), vSign);
        Vec128I vec1 = v_cmp_eq_i32(v_loada_i128(_signatures + 4), vSign);
        Vec128I vecm = v_packs_i16_i8(v_packs_i32_i16(vec0, vec1));
        return BitMatch { uint32_t(_mm_movemask_epi8(vecm)) };
      }

      case 16: {
        Vec128I vec0 = v_cmp_eq_i32(v_loada_i128(_signatures + 0), vSign);
        Vec128I vec1 = v_cmp_eq_i32(v_loada_i128(_signatures + 4), vSign);
        Vec128I vec2 = v_cmp_eq_i32(v_loada_i128(_signatures + 8), vSign);
        Vec128I vec3 = v_cmp_eq_i32(v_loada_i128(_signatures + 12), vSign);
        Vec128I vecm = v_packs_i16_i8(v_packs_i32_i16(vec0, vec1), v_packs_i32_i16(vec2, vec3));
        return BitMatch { uint32_t(_mm_movemask_epi8(vecm)) };
      }

      default:
        return BitMatch { 0 };
    }
  }
#else
  typedef IndexMatch MatchType;
  BL_INLINE MatchType match(uint32_t signature) const noexcept {
    size_t i;
    for (i = 0; i < N; i++)
      if (_signatures[i] == signature)
        break;
    return IndexMatch { i };
  }
#endif

  template<typename MatchT>
  BL_INLINE const DispatchData& matchToData(const MatchT& match) const noexcept {
    BL_ASSERT(match.isValid());
    return _dispatchData[match.index()];
  }

  BL_INLINE void store(uint32_t signature, const DispatchData* dispatchData) noexcept {
    BL_ASSERT(signature != 0);
    _signatures[_currentIndex] = signature;
    _dispatchData[_currentIndex] = *dispatchData;
    _currentIndex = (_currentIndex + 1) % N;
  }
};

} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_PIPERUNTIME_P_H_INCLUDED
