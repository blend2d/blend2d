// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_PIPERUNTIME_P_H_INCLUDED
#define BLEND2D_PIPELINE_PIPERUNTIME_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../pipeline/pipedefs_p.h"
#include "../simd/simd_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {
namespace Pipeline {

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
  //! Type of the runtime, see \ref PipeRuntimeType.
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

  BL_INLINE_NODEBUG PipeRuntimeType runtimeType() const noexcept { return _runtimeType; }
  BL_INLINE_NODEBUG PipeRuntimeFlags runtimeFlags() const noexcept { return _runtimeFlags; }
  BL_INLINE_NODEBUG uint32_t runtimeSize() const noexcept { return _runtimeSize; }

  BL_INLINE_NODEBUG void destroy() noexcept { _destroy(this); }
};

//! Pipeline provider.
struct PipeProvider {
  PipeRuntime* _runtime;
  PipeRuntime::Funcs _funcs;

  BL_INLINE_NODEBUG PipeProvider() noexcept
    : _runtime(nullptr),
      _funcs {} {}

  BL_INLINE_NODEBUG bool isInitialized() const noexcept {
    return _runtime != nullptr;
  }

  BL_INLINE_NODEBUG void init(PipeRuntime* runtime) noexcept {
    _runtime = runtime;
    _funcs = runtime->_funcs;
  }

  BL_INLINE_NODEBUG void reset() noexcept {
    memset(this, 0, sizeof(*this));
  }

  BL_INLINE_NODEBUG PipeRuntime* runtime() const noexcept {
    return _runtime;
  }

  BL_INLINE_NODEBUG BLResult test(uint32_t signature, DispatchData* out, PipeLookupCache* cache) const noexcept {
    return _funcs.test(_runtime, signature, out, cache);
  }

  BL_INLINE_NODEBUG BLResult get(uint32_t signature, DispatchData* out, PipeLookupCache* cache) const noexcept {
    return _funcs.get(_runtime, signature, out, cache);
  }
};

//! Pipe lookup cache is a local cache used by the rendering engine to store `N` recently used pipelines so it doesn't
//! have to use `PipeProvider` that that has a considerably higher overhead.
struct alignas(16) PipeLookupCache {
  // Number of cached pipelines, must be multiply of 4.
#if BL_TARGET_ARCH_X86
  enum : uint32_t { N = 16 }; // SSE2 friendly option.
#else
  enum : uint32_t { N = 8 };
#endif

  struct IndexMatch {
    size_t _index;

    BL_INLINE_NODEBUG bool isValid() const noexcept { return _index < N; }
    BL_INLINE_NODEBUG size_t index() const noexcept { return _index; }
  };

  struct BitMatch {
    uint32_t _bits;

    BL_INLINE_NODEBUG bool isValid() const noexcept { return _bits != 0; }
    BL_INLINE_NODEBUG size_t index() const noexcept { return IntOps::ctz(_bits); }
  };

  //! Array of signatures for the lookup, uninitialized signatures are zero.
  uint32_t _signatures[N];
  //! Index where the next signature will be written (incremental, wraps to zero).
  size_t _currentIndex;
  //! Array of functions matching signatures stored in `_signatures` array.
  DispatchData _dispatchData[N];

  BL_INLINE_NODEBUG void reset() {
    memset(_signatures, 0, sizeof(_signatures));
    _currentIndex = 0;
  }

  BL_INLINE const DispatchData& dispatchData(size_t index) const noexcept {
    return _dispatchData[index];
  }

  BL_INLINE void store(uint32_t signature, const DispatchData* dispatchData) noexcept {
    BL_ASSERT(signature != 0);
    _signatures[_currentIndex] = signature;
    _dispatchData[_currentIndex] = *dispatchData;
    _currentIndex = (_currentIndex + 1) % N;
  }
};

namespace {

#if defined(BL_SIMD_FEATURE_ARRAY_LOOKUP)
static BL_INLINE SIMD::ArrayLookupResult<PipeLookupCache::N> cacheLookup(const PipeLookupCache& cache, uint32_t signature) noexcept {
  return SIMD::array_lookup_u32_eq_aligned16<PipeLookupCache::N>(cache._signatures, signature);
}
#else
struct IndexMatch {
  size_t _index;

  BL_INLINE_NODEBUG bool matched() const noexcept { return _index < PipeLookupCache::N; }
  BL_INLINE_NODEBUG size_t index() const noexcept { return _index; }
};

static BL_INLINE IndexMatch cacheLookup(const PipeLookupCache& cache, uint32_t signature) noexcept {
  size_t i;
  for (i = 0; i < size_t(PipeLookupCache::N); i++)
    if (cache._signatures[i] == signature)
      break;
  return IndexMatch{i};
}
#endif

} // {anonymous}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_PIPERUNTIME_P_H_INCLUDED
