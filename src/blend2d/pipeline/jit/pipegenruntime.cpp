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

namespace bl::Pipeline::JIT {

// bl::Pipeline::JIT::Runtime - Globals
// ====================================

Wrap<PipeDynamicRuntime> PipeDynamicRuntime::_global;

// bl::Pipeline::JIT::Runtime - FunctionCache
// ==========================================

FunctionCache::FunctionCache() noexcept
  : _allocator(4096),
    _func_map(&_allocator) {}
FunctionCache::~FunctionCache() noexcept {}

BLResult FunctionCache::put(uint32_t signature, void* func) noexcept {
  FuncNode* node = _func_map.get(FuncMatcher(signature));
  if (node)
    return bl_trace_error(BL_ERROR_ALREADY_EXISTS);

  node = _allocator.new_t<FuncNode>(signature, func);
  if (BL_UNLIKELY(!node))
    return bl_trace_error(BL_ERROR_OUT_OF_MEMORY);

  _func_map.insert(node);
  return BL_SUCCESS;
}

// bl::Pipeline::JIT::Runtime - Compiler Error Handler
// ===================================================

//! JIT error handler that implements `asmjit::ErrorHandler` interface.
class CompilerErrorHandler : public asmjit::ErrorHandler {
public:
  asmjit::Error _err;

  CompilerErrorHandler() noexcept : _err(asmjit::Error::kOk) {}
  ~CompilerErrorHandler() noexcept override {}

  void handle_error(asmjit::Error err, const char* message, asmjit::BaseEmitter* origin) override {
    bl_unused(origin);
    _err = err;
    bl_runtime_message_fmt("bl::Pipeline::JIT assembling error: %s\n", message);
  }
};

// bl::Pipeline::JIT::Runtime - Dynamic Runtime Implementation
// ===========================================================

static void BL_CDECL bl_pipe_gen_runtime_destroy(PipeRuntime* self_) noexcept {
  PipeDynamicRuntime* self = static_cast<PipeDynamicRuntime*>(self_);
  self->~PipeDynamicRuntime();
}

static BLResult BL_CDECL bl_pipe_gen_runtime_test(PipeRuntime* self_, uint32_t signature, DispatchData* out, PipeLookupCache* cache) noexcept {
  bl_unused(cache);

  PipeDynamicRuntime* self = static_cast<PipeDynamicRuntime*>(self_);
  FillFunc fill_func = self->_mutex.protect_shared([&] { return (FillFunc)self->_function_cache.get(signature); });

  // NOTE: This is not traced by bl_trace_error() as this case is expected.
  if (!fill_func)
    return BL_ERROR_NO_ENTRY;

  out->init(fill_func);
  cache->store(signature, out);

  return BL_SUCCESS;
}

static BLResult BL_CDECL bl_pipe_gen_runtime_get(PipeRuntime* self_, uint32_t signature, DispatchData* out, PipeLookupCache* cache) noexcept {
  PipeDynamicRuntime* self = static_cast<PipeDynamicRuntime*>(self_);
  FillFunc fill_func = self->_mutex.protect_shared([&] { return (FillFunc)self->_function_cache.get(signature); });

  if (!fill_func) {
    fill_func = self->_compile_fill_func(signature);
    if (BL_UNLIKELY(!fill_func))
      return bl_trace_error(BL_ERROR_INVALID_STATE);

    BLResult result = self->_mutex.protect([&] { return self->_function_cache.put(signature, (void*)fill_func); });
    if (result == BL_SUCCESS) {
      self->_pipeline_count++;
    }
    else {
      self->_jit_runtime.release(fill_func);
      if (result != BL_ERROR_ALREADY_EXISTS) {
        return result;
      }
      else {
        // NOTE: There is a slight chance that some other thread registered the pipeline meanwhile
        // it was being compiled. In that case we drop the one we have just compiled and use the
        // one that is already in the function cache.
        fill_func = self->_mutex.protect_shared([&] { return (FillFunc)self->_function_cache.get(signature); });

        // It must be there...
        if (!fill_func)
          return bl_trace_error(BL_ERROR_INVALID_STATE);
      }
    }
  }

  out->init(fill_func);
  cache->store(signature, out);

  return BL_SUCCESS;
}

PipeDynamicRuntime::PipeDynamicRuntime(PipeRuntimeFlags runtime_flags) noexcept
  : _jit_runtime(),
    _function_cache(),
    _pipeline_count(0),
    _cpu_features(),
    _max_pixels(0),
    _logger_enabled(false),
    _emit_stack_frames(false) {

  // Setup the `PipeRuntime` base.
  _runtime_type = PipeRuntimeType::kJIT;
  _runtime_flags = runtime_flags;
  _runtime_size = uint16_t(sizeof(PipeDynamicRuntime));

  // PipeDynamicRuntime destructor - callable from other places.
  _destroy = bl_pipe_gen_runtime_destroy;

  // PipeDynamicRuntime interface - used by the rendering context and `PipeProvider`.
  _funcs.test = bl_pipe_gen_runtime_test;
  _funcs.get = bl_pipe_gen_runtime_get;

  _init_cpu_info(asmjit::CpuInfo::host());
}

PipeDynamicRuntime::~PipeDynamicRuntime() noexcept {}

void PipeDynamicRuntime::_init_cpu_info(const asmjit::CpuInfo& cpu_info) noexcept {
  _cpu_features = cpu_info.features();
  _opt_flags = PipeOptFlags::kNone;

#if defined(BL_JIT_ARCH_X86)
  const CpuFeatures::X86& f = _cpu_features.x86();

  // Vendor Independent CPU Features
  // -------------------------------

  if (f.has_avx2()) {
    _opt_flags |= PipeOptFlags::kMaskOps32Bit |
                 PipeOptFlags::kMaskOps64Bit ;
  }

  if (f.has_avx512_bw()) {
    _opt_flags |= PipeOptFlags::kMaskOps8Bit  |
                 PipeOptFlags::kMaskOps16Bit |
                 PipeOptFlags::kMaskOps32Bit |
                 PipeOptFlags::kMaskOps64Bit ;
  }

  // Select optimization flags based on CPU vendor and micro-architecture.

  // AMD Specific CPU Features
  // -------------------------

  if (strcmp(cpu_info.vendor(), "AMD") == 0) {
    // Zen 3+ has fast gathers, scalar loads and shuffles are faster on Zen 2 and older CPUs.
    if (cpu_info.family_id() >= 0x19u) {
      _opt_flags |= PipeOptFlags::kFastGather;
    }

    // Zen 1+ provides a low-latency VPMULLD instruction.
    if (_cpu_features.x86().has_avx2()) {
      _opt_flags |= PipeOptFlags::kFastVpmulld;
    }

    // Zen 4+ provides a low-latency VPMULLQ instruction.
    if (_cpu_features.x86().has_avx512_dq()) {
      _opt_flags |= PipeOptFlags::kFastVpmullq;
    }

    // Zen 4+ has fast mask operations (starts with AVX-512).
    if (_cpu_features.x86().has_avx512_f()) {
      _opt_flags |= PipeOptFlags::kFastStoreWithMask;
    }
  }

  // Intel Specific CPU Features
  // ---------------------------

  if (strcmp(cpu_info.vendor(), "INTEL") == 0) {
    if (_cpu_features.x86().has_avx2()) {
      uint32_t family_id = cpu_info.family_id();
      uint32_t model_id = cpu_info.model_id();

      // NOTE: We only want to hint fast gathers in cases the CPU is immune to DOWNFALL. The reason is that the
      // DOWNFALL mitigation delivered via a micro-code update makes gathers almost useless in a way that scalar
      // loads can beat it significantly (in Blend2D case scalar loads can offer up to 50% more performance).
      // This table basically picks CPUs that are known to not be affected by DOWNFALL.
      if (family_id == 0x06u) {
        switch (model_id) {
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
            _opt_flags |= PipeOptFlags::kFastGather;
            break;

          default:
            break;
        }
      }
    }

    // TODO: [JIT] It seems that masked stores are very expensive on consumer CPUs supporting AVX2 and AVX-512.
    // _opt_flags |= PipeOptFlags::kFastStoreWithMask;
  }
#endif // BL_JIT_ARCH_X86

  // Other vendors should follow here, if any...
}

void PipeDynamicRuntime::_restrict_features(uint32_t mask) noexcept {
#if defined(BL_JIT_ARCH_X86)
  if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_AVX512)) {
    _cpu_features.remove(asmjit::CpuFeatures::X86::kAVX512_F,
                        asmjit::CpuFeatures::X86::kAVX512_BW,
                        asmjit::CpuFeatures::X86::kAVX512_DQ,
                        asmjit::CpuFeatures::X86::kAVX512_CD,
                        asmjit::CpuFeatures::X86::kAVX512_VL);

    if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_AVX2)) {
      _cpu_features.remove(asmjit::CpuFeatures::X86::kAVX2);
      _cpu_features.remove(asmjit::CpuFeatures::X86::kFMA);
      _cpu_features.remove(asmjit::CpuFeatures::X86::kF16C);
      _cpu_features.remove(asmjit::CpuFeatures::X86::kGFNI);
      _cpu_features.remove(asmjit::CpuFeatures::X86::kVPCLMULQDQ);
      if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_AVX)) {
        _cpu_features.remove(asmjit::CpuFeatures::X86::kAVX);
        if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_SSE4_2)) {
          _cpu_features.remove(asmjit::CpuFeatures::X86::kSSE4_2);
          if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_SSE4_1)) {
            _cpu_features.remove(asmjit::CpuFeatures::X86::kSSE4_1);
            if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_SSSE3)) {
              _cpu_features.remove(asmjit::CpuFeatures::X86::kSSSE3);
              if (!(mask & BL_RUNTIME_CPU_FEATURE_X86_SSE3)) {
                _cpu_features.remove(asmjit::CpuFeatures::X86::kSSE3);
              }
            }
          }
        }
      }
    }

    _opt_flags &= ~(PipeOptFlags::kMaskOps8Bit       |
                    PipeOptFlags::kMaskOps16Bit      |
                    PipeOptFlags::kMaskOps32Bit      |
                    PipeOptFlags::kMaskOps64Bit      |
                    PipeOptFlags::kFastStoreWithMask |
                    PipeOptFlags::kFastGather        );
  }
#else
  bl_unused(mask);
#endif
}

#ifndef ASMJIT_NO_LOGGING
static const char* stringify_format(FormatExt value) noexcept {
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

static const char* stringify_comp_op(CompOpExt value) noexcept {
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

static const char* stringify_fill_type(FillType value) noexcept {
  switch (value) {
    case FillType::kNone    : return "None";
    case FillType::kBoxA    : return "BoxA";
    case FillType::kMask    : return "Mask";
    case FillType::kAnalytic: return "Analytic";

    default:
      return "<Unknown>";
  }
}

static const char* stringify_fetch_type(FetchType value) noexcept {
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

FillFunc PipeDynamicRuntime::_compile_fill_func(uint32_t signature) noexcept {
  Signature sig{signature};

  // CLEAR is Always simplified to SRC_COPY.
  // DST_COPY is NOP, which should never be propagated to the compiler
  if (sig.comp_op() == CompOpExt::kClear || sig.comp_op() == CompOpExt::kDstCopy || uint32_t(sig.comp_op()) >= kCompOpExtCount)
    return nullptr;

  if (sig.fill_type() == FillType::kNone)
    return nullptr;

  if (sig.dst_format() == FormatExt::kNone)
    return nullptr;

  if (sig.src_format() == FormatExt::kNone)
    return nullptr;

  CompilerErrorHandler eh;
  asmjit::CodeHolder code;

  code.init(_jit_runtime.environment());
  code.set_error_handler(&eh);

#ifndef ASMJIT_NO_LOGGING
  asmjit::StringLogger logger;
  asmjit::FileLogger fl(stdout);

  if (_logger_enabled) {
    const asmjit::FormatFlags kFormatFlags =
      asmjit::FormatFlags::kMachineCode |
      asmjit::FormatFlags::kShowAliases |
      asmjit::FormatFlags::kExplainImms |
      asmjit::FormatFlags::kRegCasts    ;

    fl.add_flags(kFormatFlags);
    code.set_logger(&fl);
  }
#endif

  AsmCompiler cc(&code);
  cc.add_encoding_options(
    asmjit::EncodingOptions::kOptimizeForSize |
    asmjit::EncodingOptions::kOptimizedAlign);

#ifdef BL_BUILD_DEBUG
  cc.add_diagnostic_options(asmjit::DiagnosticOptions::kValidateIntermediate);
#endif
  cc.add_diagnostic_options(asmjit::DiagnosticOptions::kRAAnnotate);

#ifndef ASMJIT_NO_LOGGING
  if (_logger_enabled) {
    cc.add_diagnostic_options(asmjit::DiagnosticOptions::kRAAnnotate);
    cc.commentf("Signature 0x%08X DstFmt=%s SrcFmt=%s CompOp=%s FillType=%s FetchType=%s",
      sig.value,
      stringify_format(sig.dst_format()),
      stringify_format(sig.src_format()),
      stringify_comp_op(sig.comp_op()),
      stringify_fill_type(sig.fill_type()),
      stringify_fetch_type(sig.fetch_type()));
  }
#endif

  // Construct the pipeline and compile it.
  {
    // TODO: [JIT] Limit MaxPixels.
    PipeCompiler pc(&cc, _cpu_features, _opt_flags);

    PipeComposer pipe_composer(pc);
    FetchPart* dst_part = pipe_composer.new_fetch_part(FetchType::kPixelPtr, sig.dst_format());
    FetchPart* src_part = pipe_composer.new_fetch_part(sig.fetch_type(), sig.src_format());
    CompOpPart* comp_op_part = pipe_composer.new_comp_op_part(sig.comp_op(), dst_part, src_part);
    FillPart* fill_part = pipe_composer.new_fill_part(sig.fill_type(), dst_part, comp_op_part);

    PipeFunction pipe_function;

    pipe_function.prepare(pc, fill_part);
    pipe_function.begin_function(pc);

    if (_emit_stack_frames)
      pc._func_node->frame().add_attributes(asmjit::FuncAttributes::kHasPreservedFP);
    fill_part->compile(pipe_function);

    pipe_function.end_function(pc);
  }

  if (eh._err != asmjit::Error::kOk) {
    return nullptr;
  }

  if (cc.finalize() != asmjit::Error::kOk) {
#ifndef ASMJIT_NO_LOGGING
    if (_logger_enabled) {
      bl_runtime_message_out(logger.data());
    }
#endif

    return nullptr;
  }

#ifndef ASMJIT_NO_LOGGING
  if (_logger_enabled) {
    bl_runtime_message_out(logger.data());
    bl_runtime_message_fmt("[Pipeline size: %u bytes]\n\n", uint32_t(code.code_size()));
  }
#endif

  FillFunc func = nullptr;
  _jit_runtime.add(&func, &code);
  return func;
}

} // {bl::Pipeline::JIT}

// bl::Pipeline::JIT::Runtime - Runtime Registration
// =================================================

static void BL_CDECL bl_dynamic_pipe_rt_resource_info(BLRuntimeContext* rt, BLRuntimeResourceInfo* resource_info) noexcept {
  bl_unused(rt);

  bl::Pipeline::JIT::PipeDynamicRuntime& pipe_gen_runtime = bl::Pipeline::JIT::PipeDynamicRuntime::_global();
  asmjit::JitAllocator::Statistics pipe_stats = pipe_gen_runtime._jit_runtime.allocator().statistics();

  resource_info->vm_used += pipe_stats.used_size();
  resource_info->vm_reserved += pipe_stats.reserved_size();
  resource_info->vm_overhead += pipe_stats.overhead_size();
  resource_info->vm_block_count += pipe_stats.block_count();
  resource_info->dynamic_pipeline_count += pipe_gen_runtime._pipeline_count.load();
}

static void BL_CDECL bl_dynamic_pipe_rt_shutdown(BLRuntimeContext* rt) noexcept {
  bl_unused(rt);
  bl::Pipeline::JIT::PipeDynamicRuntime::_global.destroy();
}

void bl_dynamic_pipeline_rt_init(BLRuntimeContext* rt) noexcept {
  bl::Pipeline::JIT::PipeDynamicRuntime::_global.init();

  rt->shutdown_handlers.add(bl_dynamic_pipe_rt_shutdown);
  rt->resource_info_handlers.add(bl_dynamic_pipe_rt_resource_info);
}

#endif // !BL_BUILD_NO_JIT
