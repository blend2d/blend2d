// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_PIPE_P_H
#define BLEND2D_PIPE_P_H

#include "./api-internal_p.h"
#include "./pipedefs_p.h"
#include "./simd_p.h"

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
//! to create an interface that is used by the rendering context so it doesn't
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
//! that has a considerably higher overhead.
struct BLPipeLookupCache {
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
    BL_INLINE size_t index() const noexcept { return blBitCtz(_bits); }
  };

  //! Array of signatures for the lookup, uninitialized signatures are zero.
  uint32_t _signatures[N];
  //! Array of functions matching signatures stored in `_signatures` array.
  void* _funcs[N];
  //! Index where the next signature will be written (incremental, wraps to zero).
  size_t _currentIndex;

  BL_INLINE void reset() { memset(this, 0, sizeof(*this)); }

#if defined(BL_TARGET_OPT_SSE2)
  typedef BitMatch MatchType;
  BL_INLINE MatchType match(uint32_t signature) const noexcept {
    static_assert(N == 4 || N == 8 || N == 16, "This code is written for N == 4|8|16");
    using namespace SIMD;

    I128 vSign = vseti128u32(signature);

    switch (N) {
      case 4: {
        I128 vec0 = vcmpeqi32(vloadi128u(_signatures + 0), vSign);
        return BitMatch { uint32_t(_mm_movemask_ps(vcast<F128>(vec0))) };
      }

      case 8: {
        I128 vec0 = vcmpeqi32(vloadi128u(_signatures + 0), vSign);
        I128 vec1 = vcmpeqi32(vloadi128u(_signatures + 4), vSign);
        I128 vecm = vpacki16i8(vpacki32i16(vec0, vec1));
        return BitMatch { uint32_t(_mm_movemask_epi8(vecm)) };
      }

      case 16: {
        I128 vec0 = vcmpeqi32(vloadi128u(_signatures + 0), vSign);
        I128 vec1 = vcmpeqi32(vloadi128u(_signatures + 4), vSign);
        I128 vec2 = vcmpeqi32(vloadi128u(_signatures + 8), vSign);
        I128 vec3 = vcmpeqi32(vloadi128u(_signatures + 12), vSign);
        I128 vecm = vpacki16i8(vpacki32i16(vec0, vec1), vpacki32i16(vec2, vec3));
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

  BL_INLINE void _store(uint32_t signature, void* func) noexcept {
    _signatures[_currentIndex] = signature;
    _funcs[_currentIndex] = func;
    _currentIndex = (_currentIndex + 1) % N;
  }

  template<typename Func, typename MatchT>
  BL_INLINE Func getMatch(const MatchT& match) const noexcept {
    BL_ASSERT(match.isValid());
    return (Func)_funcs[match.index()];
  }

  template<typename Func>
  BL_INLINE Func lookup(uint32_t signature) const noexcept {
    BL_ASSERT(signature != 0);
    MatchType m = match(signature);
    return (Func)(m.isValid() ? _funcs[m.index()] : nullptr);
  }

  template<typename Func>
  BL_INLINE void store(uint32_t signature, Func func) noexcept {
    BL_ASSERT(signature != 0);
    BL_ASSERT((void*)func != nullptr);
    _store(signature, (void*)func);
  }
};

//! \}
//! \endcond

#endif // BLEND2D_PIPE_P_H
