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
    _cpuFeatures(),
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

  _initCpuInfo(asmjit::CpuInfo::host());
}

PipeDynamicRuntime::~PipeDynamicRuntime() noexcept {}

void PipeDynamicRuntime::_initCpuInfo(const asmjit::CpuInfo& cpuInfo) noexcept {
  _cpuFeatures = cpuInfo.features();
  _optFlags = PipeOptFlags::kNone;

  const CpuFeatures::X86& f = _cpuFeatures.x86();

  // Vendor Independent CPU Features
  // -------------------------------

  if (f.hasAVX2()) {
    _optFlags |= PipeOptFlags::kMaskOps32Bit |
                 PipeOptFlags::kMaskOps64Bit ;
  }

  if (f.hasAVX512_BW()) {
    _optFlags |= PipeOptFlags::kMaskOps8Bit  |
                 PipeOptFlags::kMaskOps16Bit |
                 PipeOptFlags::kMaskOps32Bit |
                 PipeOptFlags::kMaskOps64Bit ;
  }

  // Select optimization flags based on CPU vendor and microarchitecture.

  // AMD Specific CPU Features
  // -------------------------

  if (strcmp(cpuInfo.vendor(), "AMD") == 0) {
    // AMD provides a low-latency VPMULLD instruction.
    if (_cpuFeatures.x86().hasAVX2()) {
      _optFlags |= PipeOptFlags::kFastVpmulld;
    }

    // AMD provides a low-latency VPMULLQ instruction.
    if (_cpuFeatures.x86().hasAVX512_DQ()) {
      _optFlags |= PipeOptFlags::kFastVpmullq;
    }

    // Zen 3 and onwards has fast gathers, scalar loads and shuffles are faster on Zen 2 and older CPUs.
    if (cpuInfo.familyId() >= 0x19u) {
      _optFlags |= PipeOptFlags::kFastGather;
    }

    // Zen 4 and onwards has fast mask operations (starts with AVX-512).
    if (_cpuFeatures.x86().hasAVX512_F()) {
      _optFlags |= PipeOptFlags::kFastStoreWithMask;
    }
  }

  // Intel Specific CPU Features
  // ---------------------------

  if (strcmp(cpuInfo.vendor(), "INTEL") == 0) {
    _optFlags |= PipeOptFlags::kFastGather;

    // TODO: It seems that masked stores are very expensive on consumer CPUs supporting AVX2 and AVX-512.
    // _optFlags |= PipeOptFlags::kFastStoreWithMask;
  }

  // Other vendors should follow here, if any...
}

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

    _optFlags &= ~(PipeOptFlags::kMaskOps8Bit       |
                   PipeOptFlags::kMaskOps16Bit      |
                   PipeOptFlags::kMaskOps32Bit      |
                   PipeOptFlags::kMaskOps64Bit      |
                   PipeOptFlags::kFastStoreWithMask |
                   PipeOptFlags::kFastGather        );

  }
#endif
}

#ifndef ASMJIT_NO_LOGGING
static const char* stringifyFormat(BLInternalFormat value) noexcept {
  switch (value) {
    case BLInternalFormat::kNone  : return "None";
    case BLInternalFormat::kPRGB32: return "PRGB32";
    case BLInternalFormat::kXRGB32: return "XRGB32";
    case BLInternalFormat::kA8    : return "A8";
    case BLInternalFormat::kFRGB32: return "FRGB32";
    case BLInternalFormat::kZERO32: return "ZERO32";

    default:
      return "<Unknown>";
  }
}

static const char* stringifyCompOp(uint32_t value) noexcept {
  switch (value) {
    case BL_COMP_OP_SRC_OVER    : return "SrcOver";
    case BL_COMP_OP_SRC_COPY    : return "SrcCopy";
    case BL_COMP_OP_SRC_IN      : return "SrcIn";
    case BL_COMP_OP_SRC_OUT     : return "SrcOut";
    case BL_COMP_OP_SRC_ATOP    : return "SrcAtop";
    case BL_COMP_OP_DST_OVER    : return "DstOver";
    case BL_COMP_OP_DST_COPY    : return "DstCopy";
    case BL_COMP_OP_DST_IN      : return "DstIn";
    case BL_COMP_OP_DST_OUT     : return "DstOut";
    case BL_COMP_OP_DST_ATOP    : return "DstAtop";
    case BL_COMP_OP_XOR         : return "Xor";
    case BL_COMP_OP_CLEAR       : return "Clear";
    case BL_COMP_OP_PLUS        : return "Plus";
    case BL_COMP_OP_MINUS       : return "Minus";
    case BL_COMP_OP_MODULATE    : return "Modulate";
    case BL_COMP_OP_MULTIPLY    : return "Multiply";
    case BL_COMP_OP_SCREEN      : return "Screen";
    case BL_COMP_OP_OVERLAY     : return "Overlay";
    case BL_COMP_OP_DARKEN      : return "Darken";
    case BL_COMP_OP_LIGHTEN     : return "Lighten";
    case BL_COMP_OP_COLOR_DODGE : return "ColorDodge";
    case BL_COMP_OP_COLOR_BURN  : return "ColorBurn";
    case BL_COMP_OP_LINEAR_BURN : return "LinearBurn";
    case BL_COMP_OP_LINEAR_LIGHT: return "LinearLight";
    case BL_COMP_OP_PIN_LIGHT   : return "PinLight";
    case BL_COMP_OP_HARD_LIGHT  : return "HardLight";
    case BL_COMP_OP_SOFT_LIGHT  : return "SoftLight";
    case BL_COMP_OP_DIFFERENCE  : return "Difference";
    case BL_COMP_OP_EXCLUSION   : return "Exclusion";

    case BL_COMP_OP_INTERNAL_ALPHA_INV:
      return "AlphaInv";

    default:
      return "<Unknown>";
  }
}

static const char* stringifyFillType(FillType value) noexcept {
  switch (value) {
    case FillType::kNone    : return "None";
    case FillType::kBoxA    : return "BoxA";
    case FillType::kBoxU    : return "BoxU";
    case FillType::kMask    : return "Mask";
    case FillType::kAnalytic: return "Analytic";

    default:
      return "<Unknown>";
  }
}

static const char* stringifyFetchType(FetchType value) noexcept {
  switch (value) {
    case FetchType::kSolid                : return "Solid";
    case FetchType::kPatternAlignedBlit   : return "PatternAlignedBlit";
    case FetchType::kPatternAlignedPad    : return "PatternAlignedPad";
    case FetchType::kPatternAlignedRepeat : return "PatternAlignedRepeat";
    case FetchType::kPatternAlignedRoR    : return "PatternAlignedRoR";
    case FetchType::kPatternFxPad         : return "PatternFxPad";
    case FetchType::kPatternFxRoR         : return "PatternFxRoR";
    case FetchType::kPatternFyPad         : return "PatternFyPad";
    case FetchType::kPatternFyRoR         : return "PatternFyRoR";
    case FetchType::kPatternFxFyPad       : return "PatternFxFyPad";
    case FetchType::kPatternFxFyRoR       : return "PatternFxFyRoR";
    case FetchType::kPatternAffineNNAny   : return "PatternAffineNNAny";
    case FetchType::kPatternAffineNNOpt   : return "PatternAffineNNOpt";
    case FetchType::kPatternAffineBIAny   : return "PatternAffineBIAny";
    case FetchType::kPatternAffineBIOpt   : return "PatternAffineBIOpt";
    case FetchType::kGradientLinearPad    : return "GradientLinearPad";
    case FetchType::kGradientLinearRoR    : return "GradientLinearRoR";
    case FetchType::kGradientRadialPad    : return "GradientRadialPad";
    case FetchType::kGradientRadialRepeat : return "GradientRadialRepeat";
    case FetchType::kGradientRadialReflect: return "GradientRadialReflect";
    case FetchType::kGradientConical      : return "GradientConical";
    case FetchType::kPixelPtr             : return "PixelPtr";
    case FetchType::kFailure              : return "<Failure>";

    default:
      return "<Unknown>";
  }
}
#endif

FillFunc PipeDynamicRuntime::_compileFillFunc(uint32_t signature) noexcept {
  Signature sig(signature);

  BL_ASSERT(sig.compOp() != BL_COMP_OP_CLEAR);    // Always simplified to SRC_COPY.
  BL_ASSERT(sig.compOp() != BL_COMP_OP_DST_COPY); // Should never pass through the rendering context.

  CompilerErrorHandler eh;
  asmjit::CodeHolder code;

  code.init(_jitRuntime.environment());
  code.setErrorHandler(&eh);

#ifndef ASMJIT_NO_LOGGING
  asmjit::StringLogger _logger;

  if (_loggerEnabled) {
    const asmjit::FormatFlags kFormatFlags =
      asmjit::FormatFlags::kRegCasts    |
      asmjit::FormatFlags::kMachineCode |
      asmjit::FormatFlags::kExplainImms ;

    _logger.addFlags(kFormatFlags);
    code.setLogger(&_logger);
  }
#endif

  asmjit::x86::Compiler cc(&code);
  cc.addEncodingOptions(
    asmjit::EncodingOptions::kOptimizeForSize |
    asmjit::EncodingOptions::kOptimizedAlign);

#ifdef BL_BUILD_DEBUG
  cc.addDiagnosticOptions(asmjit::DiagnosticOptions::kValidateIntermediate);
#endif

#ifndef ASMJIT_NO_LOGGING
  if (_loggerEnabled) {
    cc.addDiagnosticOptions(asmjit::DiagnosticOptions::kRAAnnotate);
    cc.commentf("Signature 0x%08X DstFmt=%s SrcFmt=%s CompOp=%s FillType=%s FetchType=%s",
      sig.value,
      stringifyFormat(sig.dstFormat()),
      stringifyFormat(sig.srcFormat()),
      stringifyCompOp(BLCompOp(sig.compOp())),
      stringifyFillType(sig.fillType()),
      stringifyFetchType(sig.fetchType()));
  }
#endif

  // Construct the pipeline and compile it.
  {
    PipeCompiler pc(&cc, _cpuFeatures, _optFlags);

    // TODO: [JIT] Limit MaxPixels.
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
  if (_loggerEnabled) {
    blRuntimeMessageOut(_logger.data());
    blRuntimeMessageFmt("[Pipeline size: %u bytes]\n\n", uint32_t(code.codeSize()));
  }
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
