// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_PIPEGENRUNTIME_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_PIPEGENRUNTIME_P_H_INCLUDED

#include "../../support/arenaallocator_p.h"
#include "../../support/arenahashmap_p.h"
#include "../../support/wrap_p.h"
#include "../../threading/mutex_p.h"
#include "../../pipeline/piperuntime_p.h"
#include "../../pipeline/jit/pipegencore_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace BLPipeline {
namespace JIT {

//! PipeGen function cache.
//!
//! \note No locking is preformed implicitly as `BLPipeGenRuntime` synchronizes the access on its own.
class FunctionCache {
public:
  struct FuncNode : public BLArenaHashMapNode {
    //! Function pointer.
    void* _func;

    BL_INLINE FuncNode(uint32_t signature, void* func) noexcept
      : BLArenaHashMapNode(signature) {
      _customData = signature;
      _func = func;
    }

    BL_INLINE void* func() const noexcept { return _func; }
    BL_INLINE uint32_t signature() const noexcept { return _customData; }
  };

  struct FuncMatcher {
    uint32_t _signature;

    BL_INLINE explicit FuncMatcher(uint32_t signature) noexcept
      : _signature(signature) {}

    BL_INLINE uint32_t hashCode() const noexcept { return _signature; }
    BL_INLINE bool matches(const FuncNode* node) const noexcept { return node->signature() == _signature; }
  };

  BLArenaAllocator _allocator;
  BLArenaHashMap<FuncNode> _funcMap;

  FunctionCache() noexcept;
  ~FunctionCache() noexcept;

  BL_INLINE void* get(uint32_t signature) const noexcept {
    const FuncNode* node = _funcMap.get(FuncMatcher(signature));
    return node ? node->func() : nullptr;
  }

  BLResult put(uint32_t signature, void* func) noexcept;
};

//! JIT pipeline runtime.
class PipeDynamicRuntime : public PipeRuntime {
public:
  //! JIT runtime (stores JIT functions).
  asmjit::JitRuntime _jitRuntime;
  //! Read/write lock used to read/write function cache.
  BLSharedMutex _mutex;
  //! Function cache (caches JIT functions).
  FunctionCache _functionCache;
  //! Count of cached pipelines.
  std::atomic<size_t> _pipelineCount;

  //! CPU features to use (either detected or restricted by the user).
  asmjit::CpuFeatures _cpuFeatures;
  //! Optimization flags.
  PipeOptFlags _optFlags;
  //! Maximum pixels at a time, 0 if no limit (debug).
  uint32_t _maxPixels;

  //! Whether to turn on asmjit's logging feature.
  bool _loggerEnabled;
  //! Whether to emit correct stack frames to make debugging easier. Disabled by default, because it consumes
  //! one GP register, which is always useful.
  bool _emitStackFrames;

  explicit PipeDynamicRuntime(PipeRuntimeFlags runtimeFlags = PipeRuntimeFlags::kNone) noexcept;
  ~PipeDynamicRuntime() noexcept;

  void _initCpuInfo(const asmjit::CpuInfo& cpuInfo) noexcept;

  //! Restricts CPU features not provided in the given mask. This function is only used by isolated runtimes
  //! to setup the runtime. It should never be used after the runtime is in use.
  void _restrictFeatures(uint32_t mask) noexcept;

  BL_INLINE uint32_t maxPixels() const noexcept { return _maxPixels; }
  BL_INLINE void setMaxPixelStep(uint32_t value) noexcept { _maxPixels = value; }
  BL_INLINE void setLoggerEnabled(bool value) noexcept { _loggerEnabled = value; }

  FillFunc _compileFillFunc(uint32_t signature) noexcept;

  static BLWrap<PipeDynamicRuntime> _global;
};

} // {JIT}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_PIPEGENRUNTIME_P_H_INCLUDED
