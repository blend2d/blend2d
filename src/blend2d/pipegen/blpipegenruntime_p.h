// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_PIPEGEN_BLPIPEGENRUNTIME_P_H
#define BLEND2D_PIPEGEN_BLPIPEGENRUNTIME_P_H

#include "../blpiperuntime_p.h"
#include "../blthreading_p.h"
#include "../blzoneallocator_p.h"
#include "../pipegen/blpipegencore_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_pipegen
//! \{

// ============================================================================
// [BLPipeFunctionCache]
// ============================================================================

//! Function cache.
//!
//! NOTE: No locking is preformed implicitly, it's user's responsibility to
//! ensure only one thread is accessing `BLPipeFunctionCache` at the sime time.
class BLPipeFunctionCache {
public:
  enum { kHeightLimit = 64 };

  struct Node {
    //! Function signature.
    uint32_t _signature;
    //! Horizontal level used for tree balancing.
    uint32_t _level;
    //! Function pointer.
    void* _func;
    //! Left and right nodes.
    Node* _link[2];
  };

  Node* _root;
  BLZoneAllocator _zone;

  BLPipeFunctionCache() noexcept;
  ~BLPipeFunctionCache() noexcept;

  inline void* get(uint32_t signature) const noexcept {
    Node* node = _root;
    while (node) {
      uint32_t nodeSignature = node->_signature;
      if (nodeSignature == signature) {
        void* func = node->_func;
        BL_ASSUME(func != nullptr);
        return func;
      }
      node = node->_link[nodeSignature < signature];
    }
    return nullptr;
  }

  BLResult put(uint32_t signature, void* func) noexcept;
};

// ============================================================================
// [BLPipeGenRuntime]
// ============================================================================

class BLPipeGenRuntime : public BLPipeRuntime {
public:
  //! JIT runtime (stores JIT functions).
  asmjit::JitRuntime _jitRuntime;
  //! Read/write lock used to read/write function cache.
  BLRWLock _rwLock;
  //! Function cache (caches JIT functions).
  BLPipeFunctionCache _functionCache;
  //! Count of cached pipelines.
  size_t _pipelineCount;

  //! CPU features to use (either detected or restricted by the user).
  asmjit::BaseFeatures _cpuFeatures;
  //! Maximum pixels at a time, 0 if no limit (debug).
  uint32_t _maxPixels;

  //! Whether to turn on asmjit's logging feature.
  bool _enableLogger;
  //! Whether to emit correct stack frames to make debugging easier. Disabled
  //! by default, because it consumes one GP register, which is always useful.
  bool _emitStackFrames;

  #ifndef ASMJIT_DISABLE_LOGGING
  asmjit::FileLogger _logger;
  #endif

  explicit BLPipeGenRuntime(uint32_t runtimeFlags = 0) noexcept;
  ~BLPipeGenRuntime() noexcept;

  //! Restricts CPU features not provided in the given mask. This function
  //! is only used by isolated runtimes to setup the runtime. It should never
  //! be used after the runtime is in use.
  void _restrictFeatures(uint32_t mask) noexcept;

  BL_INLINE uint32_t maxPixels() const noexcept { return _maxPixels; }
  BL_INLINE void setMaxPixelStep(uint32_t value) noexcept { _maxPixels = value; }

  BLPipeFillFunc _compileFillFunc(uint32_t signature) noexcept;

  static BLWrap<BLPipeGenRuntime> _global;
};

//! \}
//! \endcond

#endif // BLEND2D_PIPEGEN_BLPIPEGENRUNTIME_P_H
