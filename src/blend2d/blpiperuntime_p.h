// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLPIPE_P_H
#define BLEND2D_BLPIPE_P_H

#include "./blapi-internal_p.h"
#include "./blpipedefs_p.h"
#include "./blsimd_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [Forward Declarations]
// ============================================================================

struct BLPipeRuntime;
struct BLPipeProvider;
struct BLPipeLookupCache;

// ============================================================================
// [Constants]
// ============================================================================

enum BLPipeRuntimeType : uint32_t {
  //! Fixed runtime that doesn't use JIT (can be either reference or optimized).
  BL_PIPE_RUNTIME_TYPE_FIXED = 0,
  //! Runtime that uses PipeGen - JIT optimized pipelines.
  BL_PIPE_RUNTIME_TYPE_PIPEGEN = 1,

  //! Count of pipeline runtime types.
  BL_PIPE_RUNTIME_TYPE_COUNT
};

enum BLPipeRuntimeFlags : uint32_t {
  BL_PIPE_RUNTIME_FLAG_ISOLATED = 0x00000001
};

// ============================================================================
// [BLPipeRuntime]
// ============================================================================

//! This is a base class used by either `BLPipeGenRuntime` (for dynamic piplines)
//! or `BLFixedPipeRuntime` for static pipelines. The purpose of this class is
//! to create interface that is used by the rendering context so it doesn't
//! have to know which kind of pipelines it uses.
struct BLPipeRuntime {
  //! Type of the runtime, see `BLPipeRuntimeType`.
  uint8_t _runtimeType;
  //! Reserved for future use.
  uint8_t _reserved;
  //! Size of this runtime in bytes.
  uint16_t _runtimeSize;
  //! Runtime flags, see `BLPipeRuntimeFlags`.
  uint32_t _runtimeFlags;

  //! Runtime destructor.
  void (BL_CDECL* _destroy)(BLPipeRuntime* self) BL_NOEXCEPT;

  //! Functions exposed by the runtime that are copied to `BLPipeProvider` to
  //! make them local in the rendering context. It seems hacky, but this removes
  //! one extra indirection that would be needed if they were virtual.
  struct Funcs {
    BLPipeFillFunc (BL_CDECL* get)(BLPipeRuntime* self, uint32_t signature, BLPipeLookupCache* cache) BL_NOEXCEPT;
    BLPipeFillFunc (BL_CDECL* test)(BLPipeRuntime* self, uint32_t signature, BLPipeLookupCache* cache) BL_NOEXCEPT;
  } _funcs;

  BL_INLINE uint32_t runtimeType() const noexcept { return _runtimeType; }
  BL_INLINE uint32_t runtimeFlags() const noexcept { return _runtimeFlags; }
  BL_INLINE uint32_t runtimeSize() const noexcept { return _runtimeSize; }

  BL_INLINE void destroy() noexcept { _destroy(this); }
};

// ============================================================================
// [BLPipeProvider]
// ============================================================================

struct BLPipeProvider {
  BLPipeRuntime* _runtime;
  BLPipeRuntime::Funcs _funcs;

  BL_INLINE BLPipeProvider() noexcept
    : _runtime(nullptr),
      _funcs {} {}

  BL_INLINE bool isInitialized() const noexcept {
    return _runtime != nullptr;
  }

  BL_INLINE void init(BLPipeRuntime* runtime) noexcept {
    _runtime = runtime;
    _funcs = runtime->_funcs;
  }

  BL_INLINE void reset() noexcept {
    memset(this, 0, sizeof(*this));
  }

  BL_INLINE BLPipeRuntime* runtime() const noexcept { return _runtime; }
  BL_INLINE BLPipeFillFunc get(uint32_t signature, BLPipeLookupCache* cache = nullptr) const noexcept { return _funcs.get(_runtime, signature, cache); }
  BL_INLINE BLPipeFillFunc test(uint32_t signature, BLPipeLookupCache* cache = nullptr) const noexcept { return _funcs.test(_runtime, signature, cache); }
};

// ============================================================================
// [BLPipeLookupCache]
// ============================================================================

//! Pipe lookup cache is a local cache used by the rendering engine to store
//! `N` recently used pipelines so it doesn't have to use `BLPipeProvider` that
//! may would call `BLPipeRuntime` to query (or compile) the required pipeline.
struct BLPipeLookupCache {
  //! Number of cached pipelines, must be multiply of 4.
  enum : uint32_t { N = 8 };

  //! Array of signatures for the lookup, uninitialized signatures should be zero.
  uint32_t _signs[N];
  //! Array of functions matching signatures from `_signs` array. There is one
  //! extra function at the end that must always be `nullptr` and is returned
  //! when a signature isn't matched.
  void* _funcs[N + 1];
  //! Index where a next signature will be written (incremental, wraps to zero).
  size_t _currentIndex;

  BL_INLINE void reset() { memset(this, 0, sizeof(*this)); }

  BL_INLINE void* _lookup(uint32_t signature) const noexcept {
  #if defined(BL_TARGET_OPT_SSE2) && 0
    using namespace SIMD;
    static_assert(N == 8, "This code is written for N == 8");

    I128 vSign = vseti128u32(signature);
    I128 v0123 = vcmpeqi32(vloadi128u(_signs + 0), vSign);
    I128 v4567 = vcmpeqi32(vloadi128u(_signs + 4), vSign);

    uint32_t m0123 = uint32_t(_mm_movemask_ps(vcast<F128>(v0123)));
    uint32_t m4567 = uint32_t(_mm_movemask_ps(vcast<F128>(v4567)) << 4);

    uint32_t i = blBitCtz(m0123 + m4567 + (1u << N));
    return _funcs[i];
  #else
    size_t i;
    for (i = 0; i < N; i++)
      if (_signs[i] == signature)
        break;
    return _funcs[i];
  #endif
  }

  BL_INLINE void _store(uint32_t signature, void* func) noexcept {
    _signs[_currentIndex] = signature;
    _funcs[_currentIndex] = func;

    if (++_currentIndex >= N)
      _currentIndex = 0;
  }

  template<typename Func>
  BL_INLINE Func lookup(uint32_t signature) const noexcept { return (Func)_lookup(signature); }

  template<typename Func>
  BL_INLINE void store(uint32_t signature, Func func) noexcept { _store(signature, (void*)func); }
};

//! \}
//! \endcond

#endif // BLEND2D_BLPIPE_P_H
