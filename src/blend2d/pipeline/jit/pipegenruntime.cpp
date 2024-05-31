// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/compoppart_p.h"
#include "../../pipeline/jit/fetchpart_p.h"
#include "../../pipeline/jit/fetchsolidpart_p.h"
#include "../../pipeline/jit/fillpart_p.h"
#include "../../pipeline/jit/pipecompiler_p.h"
#include "../../pipeline/jit/pipecomposer_p.h"
#include "../../pipeline/jit/pipegenruntime_p.h"
#include "../../support/wrap_p.h"

namespace bl {
namespace Pipeline {
namespace JIT {

// bl::Pipeline::JIT::Runtime - Globals
// ====================================

Wrap<PipeDynamicRuntime> PipeDynamicRuntime::_global;

// bl::Pipeline::JIT::Runtime - FunctionCache
// ==========================================

FunctionCache::FunctionCache() noexcept
  : _allocator(4096 - ArenaAllocator::kBlockOverhead),
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

// bl::Pipeline::JIT::Runtime - Compiler Error Handler
// ===================================================

//! JIT error handler that implements `asmjit::ErrorHandler` interface.
class CompilerErrorHandler : public asmjit::ErrorHandler {
public:
  asmjit::Error _err;

  CompilerErrorHandler() noexcept : _err(asmjit::kErrorOk) {}
  ~CompilerErrorHandler() noexcept override {}

  void handleError(asmjit::Error err, const char* message, asmjit::BaseEmitter* origin) override {
    blUnused(origin);
    _err = err;
    blRuntimeMessageFmt("bl::Pipeline::JIT assembling error: %s\n", message);
  }
};

// bl::Pipeline::JIT::Runtime - Dynamic Runtime Implementation
// ===========================================================

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

#if defined(BL_JIT_ARCH_X86)
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

  // Select optimization flags based on CPU vendor and micro-architecture.

  // AMD Specific CPU Features
  // -------------------------

  if (strcmp(cpuInfo.vendor(), "AMD") == 0) {
    // Zen 3+ has fast gathers, scalar loads and shuffles are faster on Zen 2 and older CPUs.
    if (cpuInfo.familyId() >= 0x19u) {
      _optFlags |= PipeOptFlags::kFastGather;
    }

    // Zen 1+ provides a low-latency VPMULLD instruction.
    if (_cpuFeatures.x86().hasAVX2()) {
      _optFlags |= PipeOptFlags::kFastVpmulld;
    }

    // Zen 4+ provides a low-latency VPMULLQ instruction.
    if (_cpuFeatures.x86().hasAVX512_DQ()) {
      _optFlags |= PipeOptFlags::kFastVpmullq;
    }

    // Zen 4+ has fast mask operations (starts with AVX-512).
    if (_cpuFeatures.x86().hasAVX512_F()) {
      _optFlags |= PipeOptFlags::kFastStoreWithMask;
    }
  }

  // Intel Specific CPU Features
  // ---------------------------

  if (strcmp(cpuInfo.vendor(), "INTEL") == 0) {
    if (_cpuFeatures.x86().hasAVX2()) {
      uint32_t familyId = cpuInfo.familyId();
      uint32_t modelId = cpuInfo.modelId();

      // NOTE: We only want to hint fast gathers in cases the CPU is immune to DOWNFALL. The reason is that the
      // DOWNFALL mitigation delivered via a micro-code update makes gathers almost useless in a way that scalar
      // loads can beat it significantly (in Blend2D case scalar loads can offer up to 50% more performance).
      // This table basically picks CPUs that are known to not be affected by DOWNFALL.
      if (familyId == 0x06u) {
        switch (modelId) {
          case 0x8Fu: // Sapphire Rapids.
          case 0x96u: // Elkhart Lake.
          case 0x97u: // Alder Lake / Catlow.
          case 0x9Au: // Alder Lake / Arizona Beach.
          case 0x9Cu: // Jasper Lake.
          case 0xAAu: // Meteor Lake.
          case 0xACu: // Meteor Lake.
          case 0xADu: // Granite Rapids.
          case 0xAEu: // Granite Rapids.
          case 0xAFu: // Sierra Forest.
          case 0xBAu: // Raptor Lake.
          case 0xB5u: // Arrow Lake.
          case 0xB6u: // Grand Ridge.
          case 0xB7u: // Raptor Lake / Catlow.
          case 0xBDu: // Lunar Lake.
          case 0xBEu: // Alder Lake (N).
          case 0xBFu: // Raptor Lake.
          case 0xC5u: // Arrow Lake.
          case 0xC6u: // Arrow Lake.
          case 0xCFu: // Emerald Rapids.
          case 0xDDu: // Clearwater Forest.
            _optFlags |= PipeOptFlags::kFastGather;
            break;

          default:
            break;
        }
      }
    }

    // TODO: [JIT] It seems that masked stores are very expensive on consumer CPUs supporting AVX2 and AVX-512.
    // _optFlags |= PipeOptFlags::kFastStoreWithMask;
  }
#endif // BL_JIT_ARCH_X86

  // Other vendors should follow here, if any...
}

void PipeDynamicRuntime::_restrictFeatures(uint32_t mask) noexcept {
#if defined(BL_JIT_ARCH_X86)
  if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_AVX512)) {
    _cpuFeatures.remove(asmjit::CpuFeatures::X86::kAVX512_F,
                        asmjit::CpuFeatures::X86::kAVX512_BW,
                        asmjit::CpuFeatures::X86::kAVX512_DQ,
                        asmjit::CpuFeatures::X86::kAVX512_CD,
                        asmjit::CpuFeatures::X86::kAVX512_VL);

    if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_AVX2)) {
      _cpuFeatures.remove(asmjit::CpuFeatures::X86::kAVX2);
      _cpuFeatures.remove(asmjit::CpuFeatures::X86::kFMA);
      _cpuFeatures.remove(asmjit::CpuFeatures::X86::kF16C);
      _cpuFeatures.remove(asmjit::CpuFeatures::X86::kGFNI);
      _cpuFeatures.remove(asmjit::CpuFeatures::X86::kVPCLMULQDQ);
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

    _optFlags &= ~(PipeOptFlags::kMaskOps8Bit       |
                   PipeOptFlags::kMaskOps16Bit      |
                   PipeOptFlags::kMaskOps32Bit      |
                   PipeOptFlags::kMaskOps64Bit      |
                   PipeOptFlags::kFastStoreWithMask |
                   PipeOptFlags::kFastGather        );
  }
#else
  blUnused(mask);
#endif
}

#ifndef ASMJIT_NO_LOGGING
static const char* stringifyFormat(FormatExt value) noexcept {
  switch (value) {
    case FormatExt::kNone  : return "None";
    case FormatExt::kPRGB32: return "PRGB32";
    case FormatExt::kXRGB32: return "XRGB32";
    case FormatExt::kA8    : return "A8";
    case FormatExt::kFRGB32: return "FRGB32";
    case FormatExt::kZERO32: return "ZERO32";

    default:
      return "<Unknown>";
  }
}

static const char* stringifyCompOp(CompOpExt value) noexcept {
  switch (value) {
    case CompOpExt::kSrcOver    : return "SrcOver";
    case CompOpExt::kSrcCopy    : return "SrcCopy";
    case CompOpExt::kSrcIn      : return "SrcIn";
    case CompOpExt::kSrcOut     : return "SrcOut";
    case CompOpExt::kSrcAtop    : return "SrcAtop";
    case CompOpExt::kDstOver    : return "DstOver";
    case CompOpExt::kDstCopy    : return "DstCopy";
    case CompOpExt::kDstIn      : return "DstIn";
    case CompOpExt::kDstOut     : return "DstOut";
    case CompOpExt::kDstAtop    : return "DstAtop";
    case CompOpExt::kXor        : return "Xor";
    case CompOpExt::kClear      : return "Clear";
    case CompOpExt::kPlus       : return "Plus";
    case CompOpExt::kMinus      : return "Minus";
    case CompOpExt::kModulate   : return "Modulate";
    case CompOpExt::kMultiply   : return "Multiply";
    case CompOpExt::kScreen     : return "Screen";
    case CompOpExt::kOverlay    : return "Overlay";
    case CompOpExt::kDarken     : return "Darken";
    case CompOpExt::kLighten    : return "Lighten";
    case CompOpExt::kColorDodge : return "ColorDodge";
    case CompOpExt::kColorBurn  : return "ColorBurn";
    case CompOpExt::kLinearBurn : return "LinearBurn";
    case CompOpExt::kLinearLight: return "LinearLight";
    case CompOpExt::kPinLight   : return "PinLight";
    case CompOpExt::kHardLight  : return "HardLight";
    case CompOpExt::kSoftLight  : return "SoftLight";
    case CompOpExt::kDifference : return "Difference";
    case CompOpExt::kExclusion  : return "Exclusion";
    case CompOpExt::kAlphaInv   : return "AlphaInv";

    default:
      return "<Unknown>";
  }
}

static const char* stringifyFillType(FillType value) noexcept {
  switch (value) {
    case FillType::kNone    : return "None";
    case FillType::kBoxA    : return "BoxA";
    case FillType::kMask    : return "Mask";
    case FillType::kAnalytic: return "Analytic";

    default:
      return "<Unknown>";
  }
}

static const char* stringifyFetchType(FetchType value) noexcept {
  switch (value) {
    case FetchType::kSolid                  : return "Solid";
    case FetchType::kPatternAlignedBlit     : return "PatternAlignedBlit";
    case FetchType::kPatternAlignedPad      : return "PatternAlignedPad";
    case FetchType::kPatternAlignedRepeat   : return "PatternAlignedRepeat";
    case FetchType::kPatternAlignedRoR      : return "PatternAlignedRoR";
    case FetchType::kPatternFxPad           : return "PatternFxPad";
    case FetchType::kPatternFxRoR           : return "PatternFxRoR";
    case FetchType::kPatternFyPad           : return "PatternFyPad";
    case FetchType::kPatternFyRoR           : return "PatternFyRoR";
    case FetchType::kPatternFxFyPad         : return "PatternFxFyPad";
    case FetchType::kPatternFxFyRoR         : return "PatternFxFyRoR";
    case FetchType::kPatternAffineNNAny     : return "PatternAffineNNAny";
    case FetchType::kPatternAffineNNOpt     : return "PatternAffineNNOpt";
    case FetchType::kPatternAffineBIAny     : return "PatternAffineBIAny";
    case FetchType::kPatternAffineBIOpt     : return "PatternAffineBIOpt";
    case FetchType::kGradientLinearNNPad    : return "GradientLinearNNPad";
    case FetchType::kGradientLinearNNRoR    : return "GradientLinearNNRoR";
    case FetchType::kGradientLinearDitherPad: return "GradientLinearDitherPad";
    case FetchType::kGradientLinearDitherRoR: return "GradientLinearDitherRoR";
    case FetchType::kGradientRadialNNPad    : return "GradientRadialNNPad";
    case FetchType::kGradientRadialNNRoR    : return "GradientRadialNNRoR";
    case FetchType::kGradientRadialDitherPad: return "GradientRadialDitherPad";
    case FetchType::kGradientRadialDitherRoR: return "GradientRadialDitherRoR";
    case FetchType::kGradientConicNN        : return "GradientConic";
    case FetchType::kPixelPtr               : return "PixelPtr";
    case FetchType::kFailure                : return "<Failure>";

    default:
      return "<Unknown>";
  }
}
#endif

FillFunc PipeDynamicRuntime::_compileFillFunc(uint32_t signature) noexcept {
  Signature sig{signature};

  // CLEAR is Always simplified to SRC_COPY.
  // DST_COPY is NOP, which should never be propagated to the compiler
  if (sig.compOp() == CompOpExt::kClear || sig.compOp() == CompOpExt::kDstCopy || uint32_t(sig.compOp()) >= kCompOpExtCount)
    return nullptr;

  if (sig.fillType() == FillType::kNone)
    return nullptr;

  if (sig.dstFormat() == FormatExt::kNone)
    return nullptr;

  if (sig.srcFormat() == FormatExt::kNone)
    return nullptr;

  CompilerErrorHandler eh;
  asmjit::CodeHolder code;

  code.init(_jitRuntime.environment());
  code.setErrorHandler(&eh);

#ifndef ASMJIT_NO_LOGGING
  asmjit::StringLogger logger;
  asmjit::FileLogger fl(stdout);

  if (_loggerEnabled) {
    const asmjit::FormatFlags kFormatFlags =
      asmjit::FormatFlags::kRegCasts    |
      asmjit::FormatFlags::kMachineCode |
      asmjit::FormatFlags::kExplainImms ;

    fl.addFlags(kFormatFlags);
    code.setLogger(&fl);
  }
#endif

  AsmCompiler cc(&code);
  cc.addEncodingOptions(
    asmjit::EncodingOptions::kOptimizeForSize |
    asmjit::EncodingOptions::kOptimizedAlign);

#ifdef BL_BUILD_DEBUG
  cc.addDiagnosticOptions(asmjit::DiagnosticOptions::kValidateIntermediate);
#endif
  cc.addDiagnosticOptions(asmjit::DiagnosticOptions::kRAAnnotate);

  //cc.addDiagnosticOptions(asmjit::DiagnosticOptions::kRADebugAssignment |
  //                        asmjit::DiagnosticOptions::kRADebugAll);

#ifndef ASMJIT_NO_LOGGING
  if (_loggerEnabled) {
    cc.addDiagnosticOptions(asmjit::DiagnosticOptions::kRAAnnotate);
    cc.commentf("Signature 0x%08X DstFmt=%s SrcFmt=%s CompOp=%s FillType=%s FetchType=%s",
      sig.value,
      stringifyFormat(sig.dstFormat()),
      stringifyFormat(sig.srcFormat()),
      stringifyCompOp(sig.compOp()),
      stringifyFillType(sig.fillType()),
      stringifyFetchType(sig.fetchType()));
  }
#endif

  // Construct the pipeline and compile it.
  {
    // TODO: [JIT] Limit MaxPixels.
    PipeCompiler pc(&cc, _cpuFeatures, _optFlags);

    PipeComposer pipeComposer(pc);
    FetchPart* dstPart = pipeComposer.newFetchPart(FetchType::kPixelPtr, sig.dstFormat());
    FetchPart* srcPart = pipeComposer.newFetchPart(sig.fetchType(), sig.srcFormat());
    CompOpPart* compOpPart = pipeComposer.newCompOpPart(sig.compOp(), dstPart, srcPart);
    FillPart* fillPart = pipeComposer.newFillPart(sig.fillType(), dstPart, compOpPart);

    PipeFunction pipeFunction;

    pipeFunction.prepare(pc, fillPart);
    pipeFunction.beginFunction(pc);

    if (_emitStackFrames)
      pc._funcNode->frame().addAttributes(asmjit::FuncAttributes::kHasPreservedFP);
    fillPart->compile(pipeFunction);

    pipeFunction.endFunction(pc);
  }

  if (eh._err)
    return nullptr;

  if (cc.finalize() != asmjit::kErrorOk)
    return nullptr;

#ifndef ASMJIT_NO_LOGGING
  if (_loggerEnabled) {
    blRuntimeMessageOut(logger.data());
    blRuntimeMessageFmt("[Pipeline size: %u bytes]\n\n", uint32_t(code.codeSize()));
  }
#endif

  FillFunc func = nullptr;
  _jitRuntime.add(&func, &code);
  return func;
}

} // {JIT}
} // {Pipeline}
} // {bl}

// bl::Pipeline::JIT::Runtime - Runtime Registration
// =================================================

static void BL_CDECL blDynamicPipeRtResourceInfo(BLRuntimeContext* rt, BLRuntimeResourceInfo* resourceInfo) noexcept {
  blUnused(rt);

  bl::Pipeline::JIT::PipeDynamicRuntime& pipeGenRuntime = bl::Pipeline::JIT::PipeDynamicRuntime::_global();
  asmjit::JitAllocator::Statistics pipeStats = pipeGenRuntime._jitRuntime.allocator()->statistics();

  resourceInfo->vmUsed += pipeStats.usedSize();
  resourceInfo->vmReserved += pipeStats.reservedSize();
  resourceInfo->vmOverhead += pipeStats.overheadSize();
  resourceInfo->vmBlockCount += pipeStats.blockCount();
  resourceInfo->dynamicPipelineCount += pipeGenRuntime._pipelineCount.load();
}

static void BL_CDECL blDynamicPipeRtShutdown(BLRuntimeContext* rt) noexcept {
  blUnused(rt);
  bl::Pipeline::JIT::PipeDynamicRuntime::_global.destroy();
}

void blDynamicPipelineRtInit(BLRuntimeContext* rt) noexcept {
  bl::Pipeline::JIT::PipeDynamicRuntime::_global.init();

  rt->shutdownHandlers.add(blDynamicPipeRtShutdown);
  rt->resourceInfoHandlers.add(blDynamicPipeRtResourceInfo);
}

#endif // !BL_BUILD_NO_JIT
