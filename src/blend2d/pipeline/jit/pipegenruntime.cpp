// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if BL_TARGET_ARCH_X86 && !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/compoppart_p.h"
#include "../../pipeline/jit/fetchpart_p.h"
#include "../../pipeline/jit/fetchsolidpart_p.h"
#include "../../pipeline/jit/fillpart_p.h"
#include "../../pipeline/jit/pipecompiler_p.h"
#include "../../pipeline/jit/pipegenruntime_p.h"
#include "../../support/wrap_p.h"

namespace BLPipeline {
namespace JIT {

// BLPipeline::JIT::Runtime - Globals
// ==================================

BLWrap<PipeDynamicRuntime> PipeDynamicRuntime::_global;

// BLPipeline::JIT::Runtime - FunctionCache
// ========================================

FunctionCache::FunctionCache() noexcept
  : _allocator(4096 - BLArenaAllocator::kBlockOverhead),
    _funcMap(&_allocator) {}
FunctionCache::~FunctionCache() noexcept {}

BLResult FunctionCache::put(uint32_t signature, void* func) noexcept {
  FuncNode* node = _funcMap.get(FuncMatcher(signature));
  if (node)
    return blTraceError(BL_ERROR_ALREADY_EXISTS);

  node = _allocator.newT<FuncNode>(signature, func);
  if (BL_UNLIKELY(!node))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  _funcMap.insert(node);
  return BL_SUCCESS;
}

// BLPipeline::JIT::Runtime - Compiler Error Handler
// =================================================

//! JIT error handler that implements `asmjit::ErrorHandler` interface.
class CompilerErrorHandler : public asmjit::ErrorHandler {
public:
  asmjit::Error _err;

  CompilerErrorHandler() noexcept : _err(asmjit::kErrorOk) {}
  virtual ~CompilerErrorHandler() noexcept {}

  void handleError(asmjit::Error err, const char* message, asmjit::BaseEmitter* origin) override {
    blUnused(origin);
    _err = err;
    blRuntimeMessageFmt("BLPipeline assembling error: %s\n", message);
  }
};

// BLPipeline::JIT::Runtime - Dynamic Runtime Implementation
// =========================================================

static void BL_CDECL blPipeGenRuntimeDestroy(PipeRuntime* self_) noexcept {
  PipeDynamicRuntime* self = static_cast<PipeDynamicRuntime*>(self_);
  self->~PipeDynamicRuntime();
}

static BLResult BL_CDECL blPipeGenRuntimeTest(PipeRuntime* self_, uint32_t signature, DispatchData* out, PipeLookupCache* cache) noexcept {
  blUnused(cache);

  PipeDynamicRuntime* self = static_cast<PipeDynamicRuntime*>(self_);
  FillFunc fillFunc = self->_mutex.protectShared([&] { return (FillFunc)self->_functionCache.get(signature); });

  // NOTE: This is not traced by blTraceError() as this case is expected.
  if (!fillFunc)
    return BL_ERROR_NO_ENTRY;

  out->init(fillFunc);
  cache->store(signature, out);

  return BL_SUCCESS;
}

static BLResult BL_CDECL blPipeGenRuntimeGet(PipeRuntime* self_, uint32_t signature, DispatchData* out, PipeLookupCache* cache) noexcept {
  PipeDynamicRuntime* self = static_cast<PipeDynamicRuntime*>(self_);
  FillFunc fillFunc = self->_mutex.protectShared([&] { return (FillFunc)self->_functionCache.get(signature); });

  if (!fillFunc) {
    fillFunc = self->_compileFillFunc(signature);
    if (BL_UNLIKELY(!fillFunc))
      return blTraceError(BL_ERROR_INVALID_STATE);

    BLResult result = self->_mutex.protect([&] { return self->_functionCache.put(signature, (void*)fillFunc); });
    if (result == BL_SUCCESS) {
      self->_pipelineCount++;
    }
    else {
      self->_jitRuntime.release(fillFunc);
      if (result != BL_ERROR_ALREADY_EXISTS) {
        return result;
      }
      else {
        // NOTE: There is a slight chance that some other thread registered the pipeline meanwhile
        // it was being compiled. In that case we drop the one we have just compiled and use the
        // one that is already in the function cache.
        fillFunc = self->_mutex.protectShared([&] { return (FillFunc)self->_functionCache.get(signature); });

        // It must be there...
        if (!fillFunc)
          return blTraceError(BL_ERROR_INVALID_STATE);
      }
    }
  }

  out->init(fillFunc);
  cache->store(signature, out);

  return BL_SUCCESS;
}

PipeDynamicRuntime::PipeDynamicRuntime(PipeRuntimeFlags runtimeFlags) noexcept
  : _jitRuntime(),
    _functionCache(),
    _pipelineCount(0),
    _cpuFeatures(asmjit::CpuInfo::host().features()),
    _maxPixels(0),
    _loggerEnabled(false),
    _emitStackFrames(false) {

  // Setup the `PipeRuntime` base.
  _runtimeType = PipeRuntimeType::kJIT;
  _runtimeFlags = runtimeFlags;
  _runtimeSize = uint16_t(sizeof(PipeDynamicRuntime));

  // PipeDynamicRuntime destructor - callable from other places.
  _destroy = blPipeGenRuntimeDestroy;

  // PipeDynamicRuntime interface - used by the rendering context and `PipeProvider`.
  _funcs.test = blPipeGenRuntimeTest;
  _funcs.get = blPipeGenRuntimeGet;

#ifndef ASMJIT_NO_LOGGING
  const asmjit::FormatFlags kFormatFlags =
    asmjit::FormatFlags::kRegCasts    |
    asmjit::FormatFlags::kMachineCode ;
  _logger.setFile(stderr);
  _logger.addFlags(kFormatFlags);
#endif
}
PipeDynamicRuntime::~PipeDynamicRuntime() noexcept {}

void PipeDynamicRuntime::_restrictFeatures(uint32_t mask) noexcept {
#if BL_TARGET_ARCH_X86
  if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_AVX2)) {
    _cpuFeatures.remove(asmjit::CpuFeatures::X86::kAVX2);
    if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_AVX)) {
      _cpuFeatures.remove(asmjit::CpuFeatures::X86::kAVX);
      if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_SSE4_2)) {
        _cpuFeatures.remove(asmjit::CpuFeatures::X86::kSSE4_2);
        if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_SSE4_1)) {
          _cpuFeatures.remove(asmjit::CpuFeatures::X86::kSSE4_1);
          if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_SSSE3)) {
            _cpuFeatures.remove(asmjit::CpuFeatures::X86::kSSSE3);
            if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_SSE3)) {
              _cpuFeatures.remove(asmjit::CpuFeatures::X86::kSSE3);
            }
          }
        }
      }
    }
  }
#endif
}

FillFunc PipeDynamicRuntime::_compileFillFunc(uint32_t signature) noexcept {
  Signature sig(signature);
  BL_ASSERT(sig.compOp() != BL_COMP_OP_CLEAR);    // Always simplified to SRC_COPY.
  BL_ASSERT(sig.compOp() != BL_COMP_OP_DST_COPY); // Should never pass through the rendering context.

  CompilerErrorHandler eh;
  asmjit::CodeHolder code;

  code.init(_jitRuntime.environment());
  code.setErrorHandler(&eh);

#ifndef ASMJIT_NO_LOGGING
  if (_loggerEnabled) {
    code.setLogger(&_logger);
  }
#endif

  asmjit::x86::Compiler cc(&code);
  cc.addEncodingOptions(
    asmjit::EncodingOptions::kOptimizeForSize |
    asmjit::EncodingOptions::kOptimizedAlign);

#ifndef ASMJIT_NO_LOGGING
  if (_loggerEnabled) {
    cc.addDiagnosticOptions(
      asmjit::DiagnosticOptions::kRAAnnotate |
      asmjit::DiagnosticOptions::kRADebugAll);
    cc.commentf("Signature 0x%08X DstFmt=%d SrcFmt=%d CompOp=%d FillType=%d FetchType=%d",
      sig.value,
      sig.dstFormat(),
      sig.srcFormat(),
      sig.compOp(),
      sig.fillType(),
      sig.fetchType());
  }
#endif

  // Construct the pipeline and compile it.
  {
    PipeCompiler pc(&cc, _cpuFeatures);

    // TODO: Limit MaxPixels.
    FetchPart* dstPart = pc.newFetchPart(FetchType::kPixelPtr, sig.dstFormat());
    FetchPart* srcPart = pc.newFetchPart(sig.fetchType(), sig.srcFormat());

    CompOpPart* compOpPart = pc.newCompOpPart(sig.compOp(), dstPart, srcPart);
    FillPart* fillPart = pc.newFillPart(sig.fillType(), dstPart, compOpPart);

    pc.beginFunction();

    if (_emitStackFrames)
      pc._funcNode->frame().addAttributes(asmjit::FuncAttributes::kHasPreservedFP);

    pc.initPipeline(fillPart);
    fillPart->compile();
    pc.endFunction();
  }

  if (eh._err)
    return nullptr;

  if (cc.finalize() != asmjit::kErrorOk)
    return nullptr;

#ifndef ASMJIT_NO_LOGGING
  if (_loggerEnabled)
    _logger.logf("[Pipeline size: %u bytes]\n\n", uint32_t(code.codeSize()));
#endif

  FillFunc func = nullptr;
  _jitRuntime.add(&func, &code);
  return func;
}

} // {JIT}
} // {BLPipeline}

// BLPipeline::JIT::Runtime - Runtime Registration
// ===============================================

static void BL_CDECL blDynamicPipeRtResourceInfo(BLRuntimeContext* rt, BLRuntimeResourceInfo* resourceInfo) noexcept {
  blUnused(rt);

  BLPipeline::JIT::PipeDynamicRuntime& pipeGenRuntime = BLPipeline::JIT::PipeDynamicRuntime::_global();
  asmjit::JitAllocator::Statistics pipeStats = pipeGenRuntime._jitRuntime.allocator()->statistics();

  resourceInfo->vmUsed += pipeStats.usedSize();
  resourceInfo->vmReserved += pipeStats.reservedSize();
  resourceInfo->vmOverhead += pipeStats.overheadSize();
  resourceInfo->vmBlockCount += pipeStats.blockCount();
  resourceInfo->dynamicPipelineCount += pipeGenRuntime._pipelineCount.load();
}

static void BL_CDECL blDynamicPipeRtShutdown(BLRuntimeContext* rt) noexcept {
  blUnused(rt);
  BLPipeline::JIT::PipeDynamicRuntime::_global.destroy();
}

void blDynamicPipelineRtInit(BLRuntimeContext* rt) noexcept {
  BLPipeline::JIT::PipeDynamicRuntime::_global.init();

  rt->shutdownHandlers.add(blDynamicPipeRtShutdown);
  rt->resourceInfoHandlers.add(blDynamicPipeRtResourceInfo);
}

#endif
