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

#include "../api-build_p.h"
#if BL_TARGET_ARCH_X86 && !defined(BL_BUILD_NO_JIT)

#include "../pipegen/compoppart_p.h"
#include "../pipegen/fetchpart_p.h"
#include "../pipegen/fetchsolidpart_p.h"
#include "../pipegen/fillpart_p.h"
#include "../pipegen/pipecompiler_p.h"
#include "../pipegen/pipegenruntime_p.h"

// ============================================================================
// [Globals]
// ============================================================================

BLWrap<BLPipeGenRuntime> BLPipeGenRuntime::_global;

// ============================================================================
// [BLPipeFunctionCache]
// ============================================================================

BLPipeFunctionCache::BLPipeFunctionCache() noexcept
  : _zone(4096 - BLZoneAllocator::kBlockOverhead),
    _funcMap() {}
BLPipeFunctionCache::~BLPipeFunctionCache() noexcept {}

BLResult BLPipeFunctionCache::put(uint32_t signature, void* func) noexcept {
  FuncEntry* newNode = _zone.newT<FuncEntry>(signature, func);
  if (BL_UNLIKELY(!newNode))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  _funcMap.insert(newNode);
  return BL_SUCCESS;
}

// ============================================================================
// [BLPipeGenErrorHandler]
// ============================================================================

//! JIT error handler that implements `asmjit::ErrorHandler` interface.
class BLPipeGenErrorHandler : public asmjit::ErrorHandler {
public:
  asmjit::Error _err;

  BLPipeGenErrorHandler() noexcept : _err(asmjit::kErrorOk) {}
  virtual ~BLPipeGenErrorHandler() noexcept {}

  void handleError(asmjit::Error err, const char* message, asmjit::BaseEmitter* origin) override {
    blUnused(origin);
    _err = err;
    blRuntimeMessageFmt("BLPipeGen assembling error: %s\n", message);
  }
};

// ============================================================================
// [BLPipeGenRuntime]
// ============================================================================

static void BL_CDECL blPipeGenRuntimeDestroy(BLPipeRuntime* self_) noexcept {
  BLPipeGenRuntime* self = static_cast<BLPipeGenRuntime*>(self_);
  self->~BLPipeGenRuntime();
}

static BLPipeFillFunc BL_CDECL blPipeGenRuntimeGet(BLPipeRuntime* self_, uint32_t signature, BLPipeLookupCache* cache) noexcept {
  BLPipeGenRuntime* self = static_cast<BLPipeGenRuntime*>(self_);
  BLPipeFillFunc func = self->_mutex.protectShared([&] { return (BLPipeFillFunc)self->_functionCache.get(signature); });

  if (!func) {
    func = self->_compileFillFunc(signature);
    if (BL_UNLIKELY(!func))
      return nullptr;

    BLResult result = self->_mutex.protect([&] { return self->_functionCache.put(signature, (void*)func); });
    if (BL_UNLIKELY(result != BL_SUCCESS)) {
      self->_jitRuntime.release(func);
      return nullptr;
    }

    self->_pipelineCount++;
  }

  if (cache)
    cache->store<BLPipeFillFunc>(signature, func);
  return func;
}

static BLPipeFillFunc BL_CDECL blPipeGenRuntimeTest(BLPipeRuntime* self_, uint32_t signature, BLPipeLookupCache* cache) noexcept {
  blUnused(cache);

  BLPipeGenRuntime* self = static_cast<BLPipeGenRuntime*>(self_);
  BLPipeFillFunc func = self->_mutex.protectShared([&] { return (BLPipeFillFunc)self->_functionCache.get(signature); });

  return func;
}

BLPipeGenRuntime::BLPipeGenRuntime(uint32_t runtimeFlags) noexcept
  : _jitRuntime(),
    _functionCache(),
    _pipelineCount(0),
    _cpuFeatures(asmjit::CpuInfo::host().features()),
    _maxPixels(0),
    _enableLogger(false),
    _emitStackFrames(false) {

  // Setup the `BLPipeRuntime` base.
  _runtimeType = uint8_t(BL_PIPE_RUNTIME_TYPE_PIPEGEN);
  _reserved = 0;
  _runtimeSize = uint16_t(sizeof(BLPipeGenRuntime));
  _runtimeFlags = runtimeFlags;

  // BLPipeGenRuntime destructor - callable from other places.
  _destroy = blPipeGenRuntimeDestroy;

  // BLPipeGenRuntime interface - used by the rendering context and `BLPipeProvider`.
  _funcs.get = blPipeGenRuntimeGet;
  _funcs.test = blPipeGenRuntimeTest;

  #ifndef ASMJIT_NO_LOGGING
  const uint32_t kFormatFlags = asmjit::FormatOptions::kFlagRegCasts    |
                                asmjit::FormatOptions::kFlagAnnotations |
                                asmjit::FormatOptions::kFlagMachineCode ;
  _logger.setFile(stderr);
  _logger.addFlags(kFormatFlags);
  #endif
}
BLPipeGenRuntime::~BLPipeGenRuntime() noexcept {}

void BLPipeGenRuntime::_restrictFeatures(uint32_t mask) noexcept {
  #if BL_TARGET_ARCH_X86
  if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_AVX2)) {
    _cpuFeatures.remove(asmjit::x86::Features::kAVX2);
    if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_AVX)) {
      _cpuFeatures.remove(asmjit::x86::Features::kAVX);
      if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_SSE4_2)) {
        _cpuFeatures.remove(asmjit::x86::Features::kSSE4_2);
        if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_SSE4_1)) {
          _cpuFeatures.remove(asmjit::x86::Features::kSSE4_1);
          if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_SSSE3)) {
            _cpuFeatures.remove(asmjit::x86::Features::kSSSE3);
            if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_SSE3)) {
              _cpuFeatures.remove(asmjit::x86::Features::kSSE3);
            }
          }
        }
      }
    }
  }
  #endif
}

BLPipeFillFunc BLPipeGenRuntime::_compileFillFunc(uint32_t signature) noexcept {
  using namespace BLPipeGen;

  BLPipeSignature sig(signature);
  BL_ASSERT(sig.compOp() != BL_COMP_OP_CLEAR);    // Always simplified to SRC_COPY.
  BL_ASSERT(sig.compOp() != BL_COMP_OP_DST_COPY); // Should never pass through the rendering context.

  BLPipeGenErrorHandler eh;
  asmjit::CodeHolder code;

  code.init(_jitRuntime.environment());
  code.setErrorHandler(&eh);

  #ifndef ASMJIT_NO_LOGGING
  if (_enableLogger)
    code.setLogger(&_logger);
  #endif

  asmjit::x86::Compiler cc(&code);
  cc.addEncodingOptions(
    asmjit::BaseEmitter::kEncodingOptionOptimizeForSize |
    asmjit::BaseEmitter::kEncodingOptionOptimizedAlign);

  #ifndef ASMJIT_NO_LOGGING
  if (_enableLogger) {
    cc.commentf("Signature 0x%08X DstFmt=%d SrcFmt=%d CompOp=%d FillType=%d FetchType=%d FetchPayload=%d",
      sig.value,
      sig.dstFormat(),
      sig.srcFormat(),
      sig.compOp(),
      sig.fillType(),
      sig.fetchType(),
      sig.fetchPayload());
  }
  #endif

  // Construct the pipeline and compile it.
  {
    PipeCompiler pc(&cc, _cpuFeatures.as<asmjit::x86::Features>());

    // TODO: Limit MaxPixels.
    FetchPart* dstPart = pc.newFetchPart(BL_PIPE_FETCH_TYPE_PIXEL_PTR, 0, sig.dstFormat());
    FetchPart* srcPart = pc.newFetchPart(sig.fetchType(), sig.fetchPayload(), sig.srcFormat());

    CompOpPart* compOpPart = pc.newCompOpPart(sig.compOp(), dstPart, srcPart);
    FillPart* fillPart = pc.newFillPart(sig.fillType(), dstPart, compOpPart);

    pc.beginFunction();

    if (_emitStackFrames)
      pc._funcNode->frame().addAttributes(asmjit::FuncFrame::kAttrHasPreservedFP);

    pc.initPipeline(fillPart);
    fillPart->compile();
    pc.endFunction();
  }

  if (eh._err)
    return nullptr;

  if (cc.finalize() != asmjit::kErrorOk)
    return nullptr;

  #ifndef ASMJIT_NO_LOGGING
  if (_enableLogger)
    _logger.logf("[Pipeline size: %u bytes]\n\n", uint32_t(code.codeSize()));
  #endif

  BLPipeFillFunc func = nullptr;
  _jitRuntime.add(&func, &code);
  return func;
}

// ============================================================================
// [BLPipeGenRuntime - Runtime]
// ============================================================================

static void BL_CDECL blPipeGenOnResourceInfo(BLRuntimeContext* rt, BLRuntimeResourceInfo* resourceInfo) noexcept {
  blUnused(rt);

  BLPipeGenRuntime& pipeGenRuntime = BLPipeGenRuntime::_global();
  asmjit::JitAllocator::Statistics pipeStats = pipeGenRuntime._jitRuntime.allocator()->statistics();

  resourceInfo->vmUsed += pipeStats.usedSize();
  resourceInfo->vmReserved += pipeStats.reservedSize();
  resourceInfo->vmOverhead += pipeStats.overheadSize();
  resourceInfo->vmBlockCount += pipeStats.blockCount();
  resourceInfo->dynamicPipelineCount += pipeGenRuntime._pipelineCount.load();
}

static void BL_CDECL blPipeGenOnShutdown(BLRuntimeContext* rt) noexcept {
  blUnused(rt);
  BLPipeGenRuntime::_global.destroy();
}

void blPipeGenOnInit(BLRuntimeContext* rt) noexcept {
  BLPipeGenRuntime::_global.init();

  rt->shutdownHandlers.add(blPipeGenOnShutdown);
  rt->resourceInfoHandlers.add(blPipeGenOnResourceInfo);
}

#endif
