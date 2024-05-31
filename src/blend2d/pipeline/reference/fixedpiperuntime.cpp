// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#include "../../compopsimplifyimpl_p.h"
#include "../../pipeline/reference/compopgeneric_p.h"
#include "../../pipeline/reference/fillgeneric_p.h"
#include "../../pipeline/reference/fixedpiperuntime_p.h"
#include "../../support/wrap_p.h"

namespace bl {
namespace Pipeline {

// FixedPipelineRuntime - Globals
// ==============================

Wrap<PipeStaticRuntime> PipeStaticRuntime::_global;

// FixedPipelineRuntime - Get
// ==========================

template<CompOpExt kCompOp, FormatExt kDstFomat, FormatExt kSrcFomat, FetchType kFetchType>
struct CompOpValid {
  static constexpr bool kCompOpChanged =
    CompOpSimplifyInfoImpl::simplify(kCompOp, kDstFomat, kSrcFomat).compOp() != kCompOp;

  static constexpr bool kDstFormatChanged =
    CompOpSimplifyInfoImpl::simplify(kCompOp, kDstFomat, kSrcFomat).dstFormat() != kDstFomat;

  static constexpr bool kFetchTypeChanged =
    kFetchType != FetchType::kSolid &&
    CompOpSimplifyInfoImpl::simplify(kCompOp, kDstFomat, kSrcFomat).solidId() != CompOpSolidId::kNone;

  static constexpr bool kValid = !kCompOpChanged && !kFetchTypeChanged;
};

struct FillSolidFuncTable {
  static constexpr uint32_t kFillTypeCount = uint32_t(FillType::_kMaxValue);

  FillFunc funcs[kFillTypeCount];
};

struct FillPatternFuncTable {
  static constexpr uint32_t kFillTypeCount = uint32_t(FillType::_kMaxValue);
  static constexpr uint32_t kPatternTypeCount = uint32_t(FetchType::kPatternAnyLast) - uint32_t(FetchType::kPatternAnyFirst) + 1u;

  FillFunc funcs[kFillTypeCount * kPatternTypeCount];
};

struct FillGradientFuncTable {
  static constexpr uint32_t kFillTypeCount = uint32_t(FillType::_kMaxValue);
  static constexpr uint32_t kGradientTypeCount = uint32_t(FetchType::kGradientAnyLast) - uint32_t(FetchType::kGradientAnyFirst) + 1u;

  FillFunc funcs[kFillTypeCount * kGradientTypeCount];
};

template<FillType kFillType, FormatExt kDstFormat, uint32_t kDstBPP, typename CompOp>
static constexpr FillFunc get_fill_solid_func() noexcept {
  return Reference::FillDispatch<
    kFillType,
    Reference::CompOp_Base<
      CompOp,
      typename CompOp::PixelType,
      typename Reference::FetchSolid<typename CompOp::PixelType>,
      kDstBPP
    >
  >::Fill::fillFunc;
}

template<FillType kFillType, FormatExt kDstFormat, uint32_t kDstBPP, typename CompOp, FetchType kFetchType, FormatExt kSrcFormat>
static constexpr FillFunc get_fill_pattern_func() noexcept {
  return CompOpValid<CompOpExt(CompOp::kCompOp), kDstFormat, kSrcFormat, kFetchType>::kValid
    ? Reference::FillDispatch<
        kFillType,
        Reference::CompOp_Base<
          CompOp,
          typename CompOp::PixelType,
          typename Reference::FetchPatternDispatch<kFetchType, typename CompOp::PixelType, kSrcFormat>::Fetch,
          kDstBPP
        >
      >::Fill::fillFunc
    : nullptr;
}

template<FillType kFillType, FormatExt kDstFormat, uint32_t kDstBPP, typename CompOp, FetchType kFetchType>
static constexpr FillFunc get_fill_gradient_func() noexcept {
  return CompOpValid<CompOpExt(CompOp::kCompOp), kDstFormat, FormatExt::kPRGB32, kFetchType>::kValid
    ? Reference::FillDispatch<
        kFillType,
        Reference::CompOp_Base<
          CompOp,
          typename CompOp::PixelType,
          typename Reference::FetchGradientDispatch<kFetchType, typename CompOp::PixelType>::Fetch,
          kDstBPP
        >
      >::Fill::fillFunc
    : nullptr;
}

template<FormatExt kDstFormat, uint32_t kDstBPP, typename CompOp>
static constexpr FillSolidFuncTable get_fill_solid_func_table() noexcept {
  return FillSolidFuncTable{{
    get_fill_solid_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp>(),
    get_fill_solid_func<FillType::kMask, kDstFormat, kDstBPP, CompOp>(),
    get_fill_solid_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp>(),
  }};
}

template<FormatExt kDstFormat, uint32_t kDstBPP, typename CompOp, FormatExt kSrcFormat>
static constexpr FillPatternFuncTable get_fill_pattern_func_table() noexcept {
  return FillPatternFuncTable{{
    get_fill_pattern_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAlignedBlit  , kSrcFormat>(),
    get_fill_pattern_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAlignedPad   , kSrcFormat>(),
    get_fill_pattern_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAlignedRepeat, kSrcFormat>(),
    get_fill_pattern_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAlignedRoR   , kSrcFormat>(),
    get_fill_pattern_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kPatternFxPad        , kSrcFormat>(),
    get_fill_pattern_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kPatternFxRoR        , kSrcFormat>(),
    get_fill_pattern_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kPatternFyPad        , kSrcFormat>(),
    get_fill_pattern_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kPatternFyRoR        , kSrcFormat>(),
    get_fill_pattern_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kPatternFxFyPad      , kSrcFormat>(),
    get_fill_pattern_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kPatternFxFyRoR      , kSrcFormat>(),
    get_fill_pattern_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAffineNNAny  , kSrcFormat>(),
    get_fill_pattern_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAffineNNOpt  , kSrcFormat>(),
    get_fill_pattern_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAffineBIAny  , kSrcFormat>(),
    get_fill_pattern_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAffineBIOpt  , kSrcFormat>(),

    get_fill_pattern_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAlignedBlit  , kSrcFormat>(),
    get_fill_pattern_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAlignedPad   , kSrcFormat>(),
    get_fill_pattern_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAlignedRepeat, kSrcFormat>(),
    get_fill_pattern_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAlignedRoR   , kSrcFormat>(),
    get_fill_pattern_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kPatternFxPad        , kSrcFormat>(),
    get_fill_pattern_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kPatternFxRoR        , kSrcFormat>(),
    get_fill_pattern_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kPatternFyPad        , kSrcFormat>(),
    get_fill_pattern_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kPatternFyRoR        , kSrcFormat>(),
    get_fill_pattern_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kPatternFxFyPad      , kSrcFormat>(),
    get_fill_pattern_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kPatternFxFyRoR      , kSrcFormat>(),
    get_fill_pattern_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAffineNNAny  , kSrcFormat>(),
    get_fill_pattern_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAffineNNOpt  , kSrcFormat>(),
    get_fill_pattern_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAffineBIAny  , kSrcFormat>(),
    get_fill_pattern_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAffineBIOpt  , kSrcFormat>(),

    get_fill_pattern_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAlignedBlit  , kSrcFormat>(),
    get_fill_pattern_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAlignedPad   , kSrcFormat>(),
    get_fill_pattern_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAlignedRepeat, kSrcFormat>(),
    get_fill_pattern_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAlignedRoR   , kSrcFormat>(),
    get_fill_pattern_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kPatternFxPad        , kSrcFormat>(),
    get_fill_pattern_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kPatternFxRoR        , kSrcFormat>(),
    get_fill_pattern_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kPatternFyPad        , kSrcFormat>(),
    get_fill_pattern_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kPatternFyRoR        , kSrcFormat>(),
    get_fill_pattern_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kPatternFxFyPad      , kSrcFormat>(),
    get_fill_pattern_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kPatternFxFyRoR      , kSrcFormat>(),
    get_fill_pattern_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAffineNNAny  , kSrcFormat>(),
    get_fill_pattern_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAffineNNOpt  , kSrcFormat>(),
    get_fill_pattern_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAffineBIAny  , kSrcFormat>(),
    get_fill_pattern_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kPatternAffineBIOpt  , kSrcFormat>()
  }};
}

template<FormatExt kDstFormat, uint32_t kDstBPP, typename CompOp>
static constexpr FillGradientFuncTable get_fill_gradient_func_table() noexcept {
  return FillGradientFuncTable{{
    get_fill_gradient_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kGradientLinearNNPad    >(),
    get_fill_gradient_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kGradientLinearNNRoR    >(),
    get_fill_gradient_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kGradientLinearDitherPad>(),
    get_fill_gradient_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kGradientLinearDitherRoR>(),
    get_fill_gradient_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kGradientRadialNNPad    >(),
    get_fill_gradient_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kGradientRadialNNRoR    >(),
    get_fill_gradient_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kGradientRadialDitherPad>(),
    get_fill_gradient_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kGradientRadialDitherRoR>(),
    get_fill_gradient_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kGradientConicNN        >(),
    get_fill_gradient_func<FillType::kBoxA, kDstFormat, kDstBPP, CompOp, FetchType::kGradientConicDither    >(),

    get_fill_gradient_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kGradientLinearNNPad    >(),
    get_fill_gradient_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kGradientLinearNNRoR    >(),
    get_fill_gradient_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kGradientLinearDitherPad>(),
    get_fill_gradient_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kGradientLinearDitherRoR>(),
    get_fill_gradient_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kGradientRadialNNPad    >(),
    get_fill_gradient_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kGradientRadialNNRoR    >(),
    get_fill_gradient_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kGradientRadialDitherPad>(),
    get_fill_gradient_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kGradientRadialDitherRoR>(),
    get_fill_gradient_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kGradientConicNN        >(),
    get_fill_gradient_func<FillType::kMask, kDstFormat, kDstBPP, CompOp, FetchType::kGradientConicDither    >(),

    get_fill_gradient_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kGradientLinearNNPad    >(),
    get_fill_gradient_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kGradientLinearNNRoR    >(),
    get_fill_gradient_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kGradientLinearDitherPad>(),
    get_fill_gradient_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kGradientLinearDitherRoR>(),
    get_fill_gradient_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kGradientRadialNNPad    >(),
    get_fill_gradient_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kGradientRadialNNRoR    >(),
    get_fill_gradient_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kGradientRadialDitherPad>(),
    get_fill_gradient_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kGradientRadialDitherRoR>(),
    get_fill_gradient_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kGradientConicNN        >(),
    get_fill_gradient_func<FillType::kAnalytic, kDstFormat, kDstBPP, CompOp, FetchType::kGradientConicDither    >()
  }};
}

static const constexpr FillSolidFuncTable prgb32_fill_solid_funcs[2] = {
  get_fill_solid_func_table<FormatExt::kPRGB32, 4, Reference::CompOp_SrcOver_Op<Reference::Pixel::P32_A8R8G8B8>>(),
  get_fill_solid_func_table<FormatExt::kPRGB32, 4, Reference::CompOp_SrcCopy_Op<Reference::Pixel::P32_A8R8G8B8>>()
};

static const constexpr FillPatternFuncTable prgb32_fill_pattern_prgb32_funcs[2] = {
  get_fill_pattern_func_table<FormatExt::kPRGB32, 4, Reference::CompOp_SrcOver_Op<Reference::Pixel::P32_A8R8G8B8>, FormatExt::kPRGB32>(),
  get_fill_pattern_func_table<FormatExt::kPRGB32, 4, Reference::CompOp_SrcCopy_Op<Reference::Pixel::P32_A8R8G8B8>, FormatExt::kPRGB32>()
};

static const constexpr FillPatternFuncTable prgb32_fill_pattern_xrgb32_funcs[2] = {
  get_fill_pattern_func_table<FormatExt::kPRGB32, 4, Reference::CompOp_SrcOver_Op<Reference::Pixel::P32_A8R8G8B8>, FormatExt::kXRGB32>(),
  get_fill_pattern_func_table<FormatExt::kPRGB32, 4, Reference::CompOp_SrcCopy_Op<Reference::Pixel::P32_A8R8G8B8>, FormatExt::kXRGB32>()
};

static const constexpr FillPatternFuncTable prgb32_fill_pattern_a8_funcs[2] = {
  get_fill_pattern_func_table<FormatExt::kPRGB32, 4, Reference::CompOp_SrcOver_Op<Reference::Pixel::P32_A8R8G8B8>, FormatExt::kA8>(),
  get_fill_pattern_func_table<FormatExt::kPRGB32, 4, Reference::CompOp_SrcCopy_Op<Reference::Pixel::P32_A8R8G8B8>, FormatExt::kA8>()
};

static const constexpr FillGradientFuncTable prgb32_fill_gradient_funcs[2] = {
  get_fill_gradient_func_table<FormatExt::kPRGB32, 4, Reference::CompOp_SrcOver_Op<Reference::Pixel::P32_A8R8G8B8>>(),
  get_fill_gradient_func_table<FormatExt::kPRGB32, 4, Reference::CompOp_SrcCopy_Op<Reference::Pixel::P32_A8R8G8B8>>()
};

static const constexpr FillSolidFuncTable a8_fill_solid_funcs[2] = {
  get_fill_solid_func_table<FormatExt::kA8, 1, Reference::CompOp_SrcOver_Op<Reference::Pixel::P8_Alpha>>(),
  get_fill_solid_func_table<FormatExt::kA8, 1, Reference::CompOp_SrcCopy_Op<Reference::Pixel::P8_Alpha>>()
};

static const constexpr FillPatternFuncTable a8_fill_pattern_prgb32_funcs[2] = {
  get_fill_pattern_func_table<FormatExt::kA8, 1, Reference::CompOp_SrcOver_Op<Reference::Pixel::P8_Alpha>, FormatExt::kPRGB32>(),
  get_fill_pattern_func_table<FormatExt::kA8, 1, Reference::CompOp_SrcCopy_Op<Reference::Pixel::P8_Alpha>, FormatExt::kPRGB32>()
};

static const constexpr FillPatternFuncTable a8_fill_pattern_a8_funcs[2] = {
  get_fill_pattern_func_table<FormatExt::kA8, 1, Reference::CompOp_SrcOver_Op<Reference::Pixel::P8_Alpha>, FormatExt::kA8>(),
  get_fill_pattern_func_table<FormatExt::kA8, 1, Reference::CompOp_SrcCopy_Op<Reference::Pixel::P8_Alpha>, FormatExt::kA8>()
};

static const constexpr FillGradientFuncTable a8_fill_gradient_funcs[2] = {
  get_fill_gradient_func_table<FormatExt::kA8, 1, Reference::CompOp_SrcOver_Op<Reference::Pixel::P8_Alpha>>(),
  get_fill_gradient_func_table<FormatExt::kA8, 1, Reference::CompOp_SrcCopy_Op<Reference::Pixel::P8_Alpha>>()
};

static BLResult BL_CDECL blPipeGenRuntimeGet(PipeRuntime* self_, uint32_t signature, DispatchData* dispatchData, PipeLookupCache* cache) noexcept {
  blUnused(self_);

  Signature s{signature};
  CompOpExt compOp = s.compOp();
  FetchType fetchType = s.fetchType();
  uint32_t fillTypeIdx = uint32_t(s.fillType()) - 1u;

  FillFunc fillFunc = nullptr;
  FetchFunc fetchFunc = nullptr;

  if (compOp == CompOpExt::kSrcCopy || compOp == CompOpExt::kSrcOver) {
    uint32_t compOpIndex = uint32_t(compOp);
    switch (s.dstFormat()) {
      case FormatExt::kPRGB32:
      case FormatExt::kXRGB32: {
        if (fetchType == FetchType::kSolid) {
          fillFunc = prgb32_fill_solid_funcs[compOpIndex].funcs[fillTypeIdx];
        }
        else if (fetchType >= FetchType::kPatternAnyFirst && fetchType <= FetchType::kPatternAnyLast) {
          uint32_t patternIndex = uint32_t(fetchType) - uint32_t(FetchType::kPatternAnyFirst);
          switch (s.srcFormat()) {
            case FormatExt::kPRGB32:
              fillFunc = prgb32_fill_pattern_prgb32_funcs[compOpIndex].funcs[fillTypeIdx * FillPatternFuncTable::kPatternTypeCount + patternIndex];
              break;
            case FormatExt::kXRGB32:
              fillFunc = prgb32_fill_pattern_xrgb32_funcs[compOpIndex].funcs[fillTypeIdx * FillPatternFuncTable::kPatternTypeCount + patternIndex];
              break;
            case FormatExt::kA8:
              fillFunc = prgb32_fill_pattern_a8_funcs[compOpIndex].funcs[fillTypeIdx * FillPatternFuncTable::kPatternTypeCount + patternIndex];
              break;
            default:
              break;
          }
        }
        else if (fetchType >= FetchType::kGradientAnyFirst && fetchType <= FetchType::kGradientAnyLast) {
          uint32_t gradientIndex = uint32_t(fetchType) - uint32_t(FetchType::kGradientAnyFirst);
          fillFunc = prgb32_fill_gradient_funcs[compOpIndex].funcs[fillTypeIdx * FillGradientFuncTable::kGradientTypeCount + gradientIndex];
        }
        break;
      }

      case FormatExt::kA8: {
        if (fetchType == FetchType::kSolid) {
          fillFunc = a8_fill_solid_funcs[compOpIndex].funcs[fillTypeIdx];
        }
        else if (fetchType >= FetchType::kPatternAnyFirst && fetchType <= FetchType::kPatternAnyLast) {
          uint32_t patternIndex = uint32_t(fetchType) - uint32_t(FetchType::kPatternAnyFirst);
          switch (s.srcFormat()) {
            case FormatExt::kPRGB32:
              fillFunc = a8_fill_pattern_prgb32_funcs[compOpIndex].funcs[fillTypeIdx * FillPatternFuncTable::kPatternTypeCount + patternIndex];
              break;
            case FormatExt::kA8:
              fillFunc = a8_fill_pattern_a8_funcs[compOpIndex].funcs[fillTypeIdx * FillPatternFuncTable::kPatternTypeCount + patternIndex];
              break;
            default:
              break;
          }
        }
        else if (fetchType >= FetchType::kGradientAnyFirst && fetchType <= FetchType::kGradientAnyLast) {
          uint32_t gradientIndex = uint32_t(fetchType) - uint32_t(FetchType::kGradientAnyFirst);
          fillFunc = a8_fill_gradient_funcs[compOpIndex].funcs[fillTypeIdx * FillGradientFuncTable::kGradientTypeCount + gradientIndex];
        }
        break;
      }

      default:
        break;
    }
  }

  if (!fillFunc)
    return blTraceError(BL_ERROR_NOT_IMPLEMENTED);

  dispatchData->init(fillFunc, fetchFunc);

  if (cache)
    cache->store(signature, dispatchData);

  return BL_SUCCESS;
}

PipeStaticRuntime::PipeStaticRuntime() noexcept {
  // Setup the `PipeRuntime` base.
  _runtimeType = PipeRuntimeType::kStatic;
  _runtimeFlags = PipeRuntimeFlags::kNone;
  _runtimeSize = uint16_t(sizeof(PipeStaticRuntime));

  // PipeStaticRuntime destructor - never called.
  _destroy = nullptr;

  // PipeStaticRuntime interface - used by the rendering context and `PipeProvider`.
  _funcs.test = blPipeGenRuntimeGet;
  _funcs.get = blPipeGenRuntimeGet;
}

PipeStaticRuntime::~PipeStaticRuntime() noexcept {}

} // {Pipeline}
} // {bl}

// FixedPipelineRuntime - Runtime Registration
// ===========================================

void blStaticPipelineRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  bl::Pipeline::PipeStaticRuntime::_global.init();
}
