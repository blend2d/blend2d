// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "../blapi-build_p.h"
#include "../pipegen/blcompoppart_p.h"
#include "../pipegen/blfetchpart_p.h"
#include "../pipegen/blfetchsolidpart_p.h"
#include "../pipegen/blfillpart_p.h"
#include "../pipegen/blpipecompiler_p.h"
#include "../pipegen/blpiperuntime_p.h"

namespace BLPipeGen {

BLWrap<PipeRuntime> PipeRuntime::_global;

// ============================================================================
// [BLPipeGen::FunctionCache]
// ============================================================================

//! Remove left horizontal links.
static inline FunctionCache::Node* FunctionCache_skewNode(FunctionCache::Node* node) noexcept {
  FunctionCache::Node* link = node->_link[0];
  uint32_t level = node->_level;

  if (level != 0 && link && link->_level == level) {
    node->_link[0] = link->_link[1];
    link->_link[1] = node;

    node = link;
  }

  return node;
}

//! Remove consecutive horizontal links.
static inline FunctionCache::Node* FunctionCache_splitNode(FunctionCache::Node* node) noexcept {
  FunctionCache::Node* link = node->_link[1];
  uint32_t level = node->_level;

  if (level != 0 && link && link->_link[1] && link->_link[1]->_level == level) {
    node->_link[1] = link->_link[0];
    link->_link[0] = node;

    node = link;
    node->_level++;
  }

  return node;
}

FunctionCache::FunctionCache() noexcept
  : _root(nullptr),
    _zone(4096 - BLZoneAllocator::kBlockOverhead) {}
FunctionCache::~FunctionCache() noexcept {}

BLResult FunctionCache::put(uint32_t signature, void* func) noexcept {
  Node* node = _root;
  Node* stack[kHeightLimit];

  Node* newNode = static_cast<Node*>(_zone.alloc(sizeof(Node)));
  newNode->_signature = signature;
  newNode->_level = 0;
  newNode->_func = func;
  newNode->_link[0] = nullptr;
  newNode->_link[1] = nullptr;

  if (BL_UNLIKELY(!newNode))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  if (node == nullptr) {
    _root = newNode;
    return BL_SUCCESS;
  }

  unsigned int top = 0;
  unsigned int dir;

  // Find a spot and save the stack.
  for (;;) {
    stack[top++] = node;
    dir = node->_signature < signature;
    if (node->_link[dir] == nullptr)
      break;
    node = node->_link[dir];
  }

  // Link and rebalance.
  node->_link[dir] = newNode;

  while (top > 0) {
    // Which child?
    node = stack[--top];

    if (top != 0)
      dir = stack[top - 1]->_link[1] == node;

    node = FunctionCache_skewNode(node);
    node = FunctionCache_splitNode(node);

    // Fix the parent.
    if (top != 0)
      stack[top - 1]->_link[dir] = node;
    else
      _root = node;
  }

  return BL_SUCCESS;
}
// ============================================================================
// [BLPipeGen::PipeErrorHandler]
// ============================================================================

//! JIT error handler that implements `asmjit::ErrorHandler` interface.
class PipeErrorHandler : public asmjit::ErrorHandler {
public:
  asmjit::Error _err;

  PipeErrorHandler() noexcept : _err(asmjit::kErrorOk) {}
  virtual ~PipeErrorHandler() noexcept {}

  void handleError(asmjit::Error err, const char* message, asmjit::BaseEmitter* origin) override {
    BL_UNUSED(origin);

    _err = err;
    blRuntimeMessageFmt("BLPipeGen assembling error: %s\n", message);
  }
};

// ============================================================================
// [BLPipeGen::PipeRuntime]
// ============================================================================

PipeRuntime::PipeRuntime() noexcept
  : _runtime(),
    _cache(),
    _pipelineCount(0),
    _cpuFeatures(asmjit::CpuInfo::host().features()),
    _maxPixels(0),
    _enableLogger(false),
    _emitStackFrames(false) {
  #ifndef ASMJIT_DISABLE_LOGGING
  const uint32_t kFormatFlags = asmjit::FormatOptions::kFlagRegCasts      |
                                asmjit::FormatOptions::kFlagAnnotations   |
                                asmjit::FormatOptions::kFlagMachineCode   ;
  _logger.setFile(stderr);
  _logger.addFlags(kFormatFlags);
  #endif
}
PipeRuntime::~PipeRuntime() noexcept {}

void PipeRuntime::_restrictFeatures(uint32_t mask) noexcept {
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
            if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_SSE3))
              _cpuFeatures.remove(asmjit::x86::Features::kSSE3);
          }
        }
      }
    }
  }
  #endif
}

BLPipeFillFunc PipeRuntime::_compileAndStore(uint32_t signature) noexcept {
  BLPipeFillFunc func = _compileFunction(signature);
  if (BL_UNLIKELY(!func))
    return nullptr;

  if (BL_UNLIKELY(_cache.put(signature, (void*)func) != BL_SUCCESS)) {
    _runtime.release(func);
    return nullptr;
  }

  _pipelineCount++;
  return func;
}

BLPipeFillFunc PipeRuntime::_compileFunction(uint32_t signature) noexcept {
  using namespace asmjit;

  PipeErrorHandler eh;
  CodeHolder code;

  code.init(_runtime.codeInfo());
  code.setErrorHandler(&eh);

  #ifndef ASMJIT_DISABLE_LOGGING
  if (_enableLogger)
    code.setLogger(&_logger);
  #endif

  x86::Compiler cc(&code);
  BLPipeSignature sig(signature);

  #ifndef ASMJIT_DISABLE_LOGGING
  if (_enableLogger) {
    // Dump a function signature, useful when logging is enabled.
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

    // TODO: Move somewhere else.
    if (sig.compOp() == BL_COMP_OP_CLEAR)
      srcPart->as<FetchSolidPart>()->setTransparent(true);

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

  if (cc.finalize() != BL_SUCCESS)
    return nullptr;

  #ifndef ASMJIT_DISABLE_LOGGING
  if (_enableLogger)
    _logger.logf("[Pipeline size: %u bytes]\n\n", uint32_t(code.codeSize()));
  #endif

  BLPipeFillFunc func = nullptr;
  _runtime.add(&func, &code);
  return func;
}

} // {BLPipeGen}

// ============================================================================
// [BLPipeGen::PipeRuntime - Runtime Init]
// ============================================================================

static void BL_CDECL blPipeGenRtShutdown(BLRuntimeContext* rt) noexcept {
  BL_UNUSED(rt);
  BLPipeGen::PipeRuntime::_global.destroy();
}

static void BL_CDECL blPipeGenRtMemoryInfo(BLRuntimeContext* rt, BLRuntimeMemoryInfo* memoryInfo) noexcept {
  BL_UNUSED(rt);

  BLPipeGen::PipeRuntime& pipeRuntime = BLPipeGen::PipeRuntime::_global();
  asmjit::JitAllocator::Statistics pipeStats = pipeRuntime._runtime.allocator()->statistics();

  memoryInfo->vmUsed += pipeStats.usedSize();
  memoryInfo->vmReserved += pipeStats.reservedSize();
  memoryInfo->vmOverhead += pipeStats.overheadSize();
  memoryInfo->vmBlockCount += pipeStats.blockCount();
  memoryInfo->dynamicPipelineCount += pipeRuntime._pipelineCount;
}

void blPipeGenRtInit(BLRuntimeContext* rt) noexcept {
  BLPipeGen::PipeRuntime::_global.init();

  rt->shutdownHandlers.add(blPipeGenRtShutdown);
  rt->memoryInfoHandlers.add(blPipeGenRtMemoryInfo);
}
