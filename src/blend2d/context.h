// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CONTEXT_H_INCLUDED
#define BLEND2D_CONTEXT_H_INCLUDED

#include "font.h"
#include "geometry.h"
#include "glyphrun.h"
#include "gradient.h"
#include "image.h"
#include "matrix.h"
#include "object.h"
#include "pattern.h"
#include "path.h"
#include "rgba.h"
#include "var.h"

//! \addtogroup bl_rendering
//! \{

//! \name BLContext - Constants
//! \{

//! Rendering context type.
BL_DEFINE_ENUM(BLContextType) {
  //! No rendering context.
  BL_CONTEXT_TYPE_NONE = 0,
  //! Dummy rendering context.
  BL_CONTEXT_TYPE_DUMMY = 1,

  /*
  //! Proxy rendering context.
  BL_CONTEXT_TYPE_PROXY = 2,
  */

  //! Software-accelerated rendering context.
  BL_CONTEXT_TYPE_RASTER = 3,

  //! Maximum value of `BLContextType`.
  BL_CONTEXT_TYPE_MAX_VALUE = 3

  BL_FORCE_ENUM_UINT32(BL_CONTEXT_TYPE)
};

//! Rendering context hint.
BL_DEFINE_ENUM(BLContextHint) {
  //! Rendering quality.
  BL_CONTEXT_HINT_RENDERING_QUALITY = 0,
  //! Gradient quality.
  BL_CONTEXT_HINT_GRADIENT_QUALITY = 1,
  //! Pattern quality.
  BL_CONTEXT_HINT_PATTERN_QUALITY = 2,

  //! Maximum value of `BLContextHint`.
  BL_CONTEXT_HINT_MAX_VALUE = 7

  BL_FORCE_ENUM_UINT32(BL_CONTEXT_HINT)
};

//! Describes a rendering context style slot - fill or stroke.
BL_DEFINE_ENUM(BLContextStyleSlot) {
  //! Fill operation style slot.
  BL_CONTEXT_STYLE_SLOT_FILL = 0,
  //! Stroke operation style slot.
  BL_CONTEXT_STYLE_SLOT_STROKE = 1,

  //! Maximum value of `BLContextStyleSlot`
  BL_CONTEXT_STYLE_SLOT_MAX_VALUE = 1

  BL_FORCE_ENUM_UINT32(BL_CONTEXT_STYLE_SLOT)
};

//! The type of a text rendering operation.
//!
//! This value specifies the type of the parameter passed to the text rendering API.
//!
//! \note In most cases this should not be required to use by Blend2D users. The C API provides functions that
//! wrap all of the text operations and C++ API provides functions that use `BLContextRenderTextOp` internally.
BL_DEFINE_ENUM(BLContextRenderTextOp) {
  //! UTF-8 text rendering operation - UTF-8 string passed as \ref BLStringView or \ref BLArrayView<uint8_t>.
  BL_CONTEXT_RENDER_TEXT_OP_UTF8 = BL_TEXT_ENCODING_UTF8,
  //! UTF-16 text rendering operation - UTF-16 string passed as \ref BLArrayView<uint16_t>.
  BL_CONTEXT_RENDER_TEXT_OP_UTF16 = BL_TEXT_ENCODING_UTF16,
  //! UTF-32 text rendering operation - UTF-32 string passed as \ref BLArrayView<uint32_t>.
  BL_CONTEXT_RENDER_TEXT_OP_UTF32 = BL_TEXT_ENCODING_UTF32,
  //! LATIN1 text rendering operation - LATIN1 string is passed as \ref BLStringView or \ref BLArrayView<uint8_t>.
  BL_CONTEXT_RENDER_TEXT_OP_LATIN1 = BL_TEXT_ENCODING_LATIN1,
  //! `wchar_t` text rendering operation - wchar_t string is passed as \ref BLArrayView<wchar_t>.
  BL_CONTEXT_RENDER_TEXT_OP_WCHAR = BL_TEXT_ENCODING_WCHAR,
  //! Glyph run text rendering operation - the \ref BLGlyphRun parameter is passed.
  BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN = 4,

  //! Maximum value of `BLContextRenderTextInputType`
  BL_CONTEXT_RENDER_TEXT_OP_MAX_VALUE = 4

  BL_FORCE_ENUM_UINT32(BL_CONTEXT_RENDER_TEXT_OP_TYPE)
};

//! Rendering context flush flags, used by \ref BLContext::flush().
BL_DEFINE_ENUM(BLContextFlushFlags) {
  BL_CONTEXT_FLUSH_NO_FLAGS = 0u,

  //! Flushes the command queue and waits for its completion (will block until done).
  BL_CONTEXT_FLUSH_SYNC = 0x80000000u

  BL_FORCE_ENUM_UINT32(BL_CONTEXT_FLUSH)
};

//! Rendering context creation flags.
BL_DEFINE_ENUM(BLContextCreateFlags) {
  //! No flags.
  BL_CONTEXT_CREATE_NO_FLAGS = 0u,

  //! Disables JIT pipeline generator.
  BL_CONTEXT_CREATE_FLAG_DISABLE_JIT = 0x00000001u,

  //! Fallbacks to a synchronous rendering in case that the rendering engine wasn't able to acquire threads. This
  //! flag only makes sense when the asynchronous mode was specified by having `threadCount` greater than 0. If the
  //! rendering context fails to acquire at least one thread it would fallback to synchronous mode with no worker
  //! threads.
  //!
  //! \note If this flag is specified with `threadCount == 1` it means to immediately fallback to synchronous
  //! rendering. It's only practical to use this flag with 2 or more requested threads.
  BL_CONTEXT_CREATE_FLAG_FALLBACK_TO_SYNC = 0x00100000u,

  //! If this flag is specified and asynchronous rendering is enabled then the context would create its own isolated
  //! thread-pool, which is useful for debugging purposes.
  //!
  //! Do not use this flag in production as rendering contexts with isolated thread-pool have to create and destroy all
  //! threads they use. This flag is only useful for testing, debugging, and isolated benchmarking.
  BL_CONTEXT_CREATE_FLAG_ISOLATED_THREAD_POOL = 0x01000000u,

  //! If this flag is specified and JIT pipeline generation enabled then the rendering context would create its own
  //! isolated JIT runtime. which is useful for debugging purposes. This flag will be ignored if JIT pipeline
  //! compilation is either not supported or was disabled by other flags.
  //!
  //! Do not use this flag in production as rendering contexts with isolated JIT runtime do not use global pipeline
  //! cache, that's it, after the rendering context is destroyed the JIT runtime is destroyed with it with all
  //! compiled pipelines. This flag is only useful for testing, debugging, and isolated benchmarking.
  BL_CONTEXT_CREATE_FLAG_ISOLATED_JIT_RUNTIME = 0x02000000u,

  //! Enables logging to stderr of isolated runtime.
  //!
  //! \note Must be used with \ref BL_CONTEXT_CREATE_FLAG_ISOLATED_JIT_RUNTIME otherwise it would have no effect.
  BL_CONTEXT_CREATE_FLAG_ISOLATED_JIT_LOGGING = 0x04000000u,

  //! Override CPU features when creating isolated context.
  BL_CONTEXT_CREATE_FLAG_OVERRIDE_CPU_FEATURES = 0x08000000u

  BL_FORCE_ENUM_UINT32(BL_CONTEXT_CREATE_FLAG)
};

//! Error flags that are accumulated during the rendering context lifetime and that can be queried through
//! \ref BLContext::accumulatedErrorFlags(). The reason why these flags exist is that errors can happen during
//! asynchronous rendering, and there is no way the user can catch these errors.
BL_DEFINE_ENUM(BLContextErrorFlags) {
  //! No flags.
  BL_CONTEXT_ERROR_NO_FLAGS = 0u,

  //! The rendering context returned or encountered \ref BL_ERROR_INVALID_VALUE, which is mostly related to
  //! the function argument handling. It's very likely some argument was wrong when calling \ref BLContext API.
  BL_CONTEXT_ERROR_FLAG_INVALID_VALUE = 0x00000001u,

  //! Invalid state describes something wrong, for example a pipeline compilation error.
  BL_CONTEXT_ERROR_FLAG_INVALID_STATE = 0x00000002u,

  //! The rendering context has encountered invalid geometry.
  BL_CONTEXT_ERROR_FLAG_INVALID_GEOMETRY = 0x00000004u,

  //! The rendering context has encountered invalid glyph.
  BL_CONTEXT_ERROR_FLAG_INVALID_GLYPH = 0x00000008u,

  //! The rendering context has encountered invalid or uninitialized font.
  BL_CONTEXT_ERROR_FLAG_INVALID_FONT = 0x00000010u,

  //! Thread pool was exhausted and couldn't acquire the requested number of threads.
  BL_CONTEXT_ERROR_FLAG_THREAD_POOL_EXHAUSTED = 0x20000000u,

  //! Out of memory condition.
  BL_CONTEXT_ERROR_FLAG_OUT_OF_MEMORY = 0x40000000u,

  //! Unknown error, which we don't have flag for.
  BL_CONTEXT_ERROR_FLAG_UNKNOWN_ERROR = 0x80000000u

  BL_FORCE_ENUM_UINT32(BL_CONTEXT_ERROR_FLAG)
};

//! Specifies the behavior of \ref BLContext::swapStyles() operation.
BL_DEFINE_ENUM(BLContextStyleSwapMode) {
  //! Swap only fill and stroke styles without affecting fill and stroke alpha.
  BL_CONTEXT_STYLE_SWAP_MODE_STYLES = 0,

  //! Swap both fill and stroke styles and their alpha values.
  BL_CONTEXT_STYLE_SWAP_MODE_STYLES_WITH_ALPHA = 1,

  //! Maximum value of `BLContextStyleSwapMode`.
  BL_CONTEXT_STYLE_SWAP_MODE_MAX_VALUE = 1

  BL_FORCE_ENUM_UINT32(BL_CONTEXT_STYLE_SWAP_MODE)
};

//! Specifies how style transformation matrix is combined with the rendering context transformation matrix, used by
//! \ref BLContext::setStyle() function.
BL_DEFINE_ENUM(BLContextStyleTransformMode) {
  //! Style transformation matrix should be transformed with the rendering context user and meta matrix (default).
  //!
  //! \note This transformation mode is identical to how user geometry is transformed and it's the default
  //! transformation and most likely the behavior expected in most cases.
  BL_CONTEXT_STYLE_TRANSFORM_MODE_USER = 0,

  //! Style transformation matrix should be transformed with the rendering context meta matrix.
  BL_CONTEXT_STYLE_TRANSFORM_MODE_META = 1,

  //! Style transformation matrix is considered absolute, and is not combined with a rendering context transform.
  BL_CONTEXT_STYLE_TRANSFORM_MODE_NONE = 2,

  //! Maximum value of `BLContextStyleTransformMode`.
  BL_CONTEXT_STYLE_TRANSFORM_MODE_MAX_VALUE = 2

  BL_FORCE_ENUM_UINT32(BL_CONTEXT_STYLE_TRANSFORM_MODE)
};

//! Clip mode.
BL_DEFINE_ENUM(BLClipMode) {
  //! Clipping to a rectangle that is aligned to the pixel grid.
  BL_CLIP_MODE_ALIGNED_RECT = 0,
  //! Clipping to a rectangle that is not aligned to pixel grid.
  BL_CLIP_MODE_UNALIGNED_RECT = 1,
  //! Clipping to a non-rectangular area that is defined by using mask.
  BL_CLIP_MODE_MASK = 2,

  //! Count of clip modes.
  BL_CLIP_MODE_COUNT = 3

  BL_FORCE_ENUM_UINT32(BL_CLIP_MODE)
};

//! Composition & blending operator.
BL_DEFINE_ENUM(BLCompOp) {
  //! Source-over [default].
  BL_COMP_OP_SRC_OVER = 0,
  //! Source-copy.
  BL_COMP_OP_SRC_COPY = 1,
  //! Source-in.
  BL_COMP_OP_SRC_IN = 2,
  //! Source-out.
  BL_COMP_OP_SRC_OUT = 3,
  //! Source-atop.
  BL_COMP_OP_SRC_ATOP = 4,
  //! Destination-over.
  BL_COMP_OP_DST_OVER = 5,
  //! Destination-copy [nop].
  BL_COMP_OP_DST_COPY = 6,
  //! Destination-in.
  BL_COMP_OP_DST_IN = 7,
  //! Destination-out.
  BL_COMP_OP_DST_OUT = 8,
  //! Destination-atop.
  BL_COMP_OP_DST_ATOP = 9,
  //! Xor.
  BL_COMP_OP_XOR = 10,
  //! Clear.
  BL_COMP_OP_CLEAR = 11,
  //! Plus.
  BL_COMP_OP_PLUS = 12,
  //! Minus.
  BL_COMP_OP_MINUS = 13,
  //! Modulate.
  BL_COMP_OP_MODULATE = 14,
  //! Multiply.
  BL_COMP_OP_MULTIPLY = 15,
  //! Screen.
  BL_COMP_OP_SCREEN = 16,
  //! Overlay.
  BL_COMP_OP_OVERLAY = 17,
  //! Darken.
  BL_COMP_OP_DARKEN = 18,
  //! Lighten.
  BL_COMP_OP_LIGHTEN = 19,
  //! Color dodge.
  BL_COMP_OP_COLOR_DODGE = 20,
  //! Color burn.
  BL_COMP_OP_COLOR_BURN = 21,
  //! Linear burn.
  BL_COMP_OP_LINEAR_BURN = 22,
  //! Linear light.
  BL_COMP_OP_LINEAR_LIGHT = 23,
  //! Pin light.
  BL_COMP_OP_PIN_LIGHT = 24,
  //! Hard-light.
  BL_COMP_OP_HARD_LIGHT = 25,
  //! Soft-light.
  BL_COMP_OP_SOFT_LIGHT = 26,
  //! Difference.
  BL_COMP_OP_DIFFERENCE = 27,
  //! Exclusion.
  BL_COMP_OP_EXCLUSION = 28,

  //! Count of composition & blending operators.
  BL_COMP_OP_MAX_VALUE = 28

  BL_FORCE_ENUM_UINT32(BL_COMP_OP)
};

//! Rendering quality.
BL_DEFINE_ENUM(BLRenderingQuality) {
  //! Render using anti-aliasing.
  BL_RENDERING_QUALITY_ANTIALIAS = 0,

  //! Maximum value of `BLRenderingQuality`.
  BL_RENDERING_QUALITY_MAX_VALUE = 0

  BL_FORCE_ENUM_UINT32(BL_RENDERING_QUALITY)
};

//! \}

//! \name BLContext - Structs
//! \{

//! Information that can be used to customize the rendering context.
struct BLContextCreateInfo {
  //! Create flags, see \ref BLContextCreateFlags.
  uint32_t flags;

  //! Number of worker threads to use for asynchronous rendering, if non-zero.
  //!
  //! If `threadCount` is zero it means to initialize the context for synchronous rendering. This means that every
  //! operation will take effect immediately. If `threadCount` is `1` it means that the rendering will be asynchronous,
  //! but no thread would be acquired from a thread-pool, because the user thread will be used as a worker. And
  //! finally, if `threadCount` is greater than `1` then total of `threadCount - 1` threads will be acquired from
  //! thread-pool and used as additional workers.
  uint32_t threadCount;

  //! CPU features to use in isolated JIT runtime (if supported), only used when `flags` contains
  //! \ref BL_CONTEXT_CREATE_FLAG_OVERRIDE_CPU_FEATURES.
  uint32_t cpuFeatures;

  //! Maximum number of commands to be queued.
  //!
  //! If this parameter is zero the queue size will be determined automatically.
  //!
  //! TODO: To be documented, has no effect at the moment.
  uint32_t commandQueueLimit;

  //! Maximum number of saved states.
  //!
  //! \note Zero value tells the rendering engine to use the default saved state limit, which currently defaults
  //! to 4096 states. This option allows to even increase or decrease the limit, depending on the use case.
  uint32_t savedStateLimit;

  //! Pixel origin.
  //!
  //! Pixel origin is an offset in pixel units that can be used as an origin for fetchers and effects that use a pixel
  //! X/Y coordinate in the calculation. One example of using pixel origin is dithering, where it's used to shift the
  //! dithering matrix.
  BLPointI pixelOrigin;

  //! Reserved for future use, must be zero.
  uint32_t reserved[1];

#ifdef __cplusplus
  BL_INLINE_NODEBUG void reset() noexcept { *this = BLContextCreateInfo{}; }
#endif
};

//! Holds an arbitrary 128-bit value (cookie) that can be used to match other cookies. Blend2D uses cookies in places
//! where it allows to "lock" some state that can only be unlocked by a matching cookie. Please don't confuse cookies
//! with a security of any kind, it's just an arbitrary data that must match to proceed with a certain operation.
//!
//! Cookies can be used with \ref BLContext::save() and \ref BLContext::restore() operations.
struct BLContextCookie {
  uint64_t data[2];

#ifdef __cplusplus
  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator==(const BLContextCookie& other) const noexcept { return  equals(other); }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator!=(const BLContextCookie& other) const noexcept { return !equals(other); }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool empty() const noexcept {
    return data[0] == 0 && data[1] == 0;
  }

  BL_INLINE_NODEBUG void reset() noexcept { reset(0, 0); }
  BL_INLINE_NODEBUG void reset(const BLContextCookie& other) noexcept { reset(other.data[0], other.data[1]); }

  BL_INLINE_NODEBUG void reset(uint64_t data0Init, uint64_t data1Init) noexcept {
    data[0] = data0Init;
    data[1] = data1Init;
  }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool equals(const BLContextCookie& other) const noexcept {
    return bool(unsigned(blEquals(data[0], other.data[0])) &
                unsigned(blEquals(data[1], other.data[1])));
  }

  #endif
};

//! Rendering context hints.
struct BLContextHints {
  union {
    struct {
      uint8_t renderingQuality;
      uint8_t gradientQuality;
      uint8_t patternQuality;
    };

    uint8_t hints[BL_CONTEXT_HINT_MAX_VALUE + 1];
  };

#ifdef __cplusplus
  BL_INLINE_NODEBUG void reset() noexcept { *this = BLContextHints{}; }
#endif
};

//! \}

//! \}

//! \addtogroup bl_c_api
//! \{

//! \name BLContext - C API
//! \{

//! Rendering context [C API].
struct BLContextCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
  BL_DEFINE_OBJECT_DCAST(BLContext)

#ifdef __cplusplus
  //! \name Impl Utilities
  //! \{

  //! Returns Impl of the rendering context (only provided for use cases that implement BLContext).
  template<typename T = BLContextImpl>
  BL_INLINE_NODEBUG T* _impl() const noexcept { return static_cast<T*>(_d.impl); }

  //! \}
#endif
};

//! \cond INTERNAL
//! Rendering context [Virtual Function Table].
struct BLContextVirt BL_CLASS_INHERITS(BLObjectVirt) {
  BL_DEFINE_VIRT_BASE

  // Interface - Most Used Functions
  // -------------------------------

  // NOTE 1: These functions are called directly by the BLContext C++ API (the dispatch is inlined). So in general
  // on x86 targets the compiler will generate something like `call [base + offset]` instruction to perform the call.
  // We want to have the most used functions first as these would use 8-bit offset instead of 32-bit offset. We have
  // space for 12 functions as 8-bit offset is signed (from -128 to 127) and the BLObjectVirt already uses 3 functions.
  //
  // NOTE 2: On non-X86 platforms such as AArch64 we don't have to worry about offsets as the instruction would be
  // encoded in 32-bits regardless of the offset.

  BLResult (BL_CDECL* applyTransformOp        )(BLContextImpl* impl, BLTransformOp opType, const void* opData) BL_NOEXCEPT;

  BLResult (BL_CDECL* fillRectI               )(BLContextImpl* impl, const BLRectI* rect) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillRectIRgba32         )(BLContextImpl* impl, const BLRectI* rect, uint32_t rgba32) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillRectIExt            )(BLContextImpl* impl, const BLRectI* rect, const BLObjectCore* style) BL_NOEXCEPT;

  BLResult (BL_CDECL* fillRectD               )(BLContextImpl* impl, const BLRect* rect) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillRectDRgba32         )(BLContextImpl* impl, const BLRect* rect, uint32_t rgba32) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillRectDExt            )(BLContextImpl* impl, const BLRect* rect, const BLObjectCore* style) BL_NOEXCEPT;

  BLResult (BL_CDECL* fillPathD               )(BLContextImpl* impl, const BLPoint* origin, const BLPathCore* path) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillPathDRgba32         )(BLContextImpl* impl, const BLPoint* origin, const BLPathCore* path, uint32_t rgba32) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillPathDExt            )(BLContextImpl* impl, const BLPoint* origin, const BLPathCore* path, const BLObjectCore* style) BL_NOEXCEPT;

  BLResult (BL_CDECL* blitImageI              )(BLContextImpl* impl, const BLPointI* origin, const BLImageCore* img, const BLRectI* imgArea) BL_NOEXCEPT;
  BLResult (BL_CDECL* blitScaledImageI        )(BLContextImpl* impl, const BLRectI* rect, const BLImageCore* img, const BLRectI* imgArea) BL_NOEXCEPT;

  // Interface
  // ---------

  BLResult (BL_CDECL* flush                   )(BLContextImpl* impl, BLContextFlushFlags flags) BL_NOEXCEPT;

  BLResult (BL_CDECL* save                    )(BLContextImpl* impl, BLContextCookie* cookie) BL_NOEXCEPT;
  BLResult (BL_CDECL* restore                 )(BLContextImpl* impl, const BLContextCookie* cookie) BL_NOEXCEPT;

  BLResult (BL_CDECL* userToMeta              )(BLContextImpl* impl) BL_NOEXCEPT;

  BLResult (BL_CDECL* setHint                 )(BLContextImpl* impl, BLContextHint hintType, uint32_t value) BL_NOEXCEPT;
  BLResult (BL_CDECL* setHints                )(BLContextImpl* impl, const BLContextHints* hints) BL_NOEXCEPT;
  BLResult (BL_CDECL* setFlattenMode          )(BLContextImpl* impl, BLFlattenMode mode) BL_NOEXCEPT;
  BLResult (BL_CDECL* setFlattenTolerance     )(BLContextImpl* impl, double tolerance) BL_NOEXCEPT;
  BLResult (BL_CDECL* setApproximationOptions )(BLContextImpl* impl, const BLApproximationOptions* options) BL_NOEXCEPT;

  BLResult (BL_CDECL* getStyle                )(const BLContextImpl* impl, BLContextStyleSlot slot, bool transformed, BLVarCore* styleOut) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStyle                )(BLContextImpl* impl, BLContextStyleSlot slot, const BLObjectCore* style, BLContextStyleTransformMode transformMode) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStyleRgba            )(BLContextImpl* impl, BLContextStyleSlot slot, const BLRgba* rgba) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStyleRgba32          )(BLContextImpl* impl, BLContextStyleSlot slot, uint32_t rgba32) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStyleRgba64          )(BLContextImpl* impl, BLContextStyleSlot slot, uint64_t rgba64) BL_NOEXCEPT;
  BLResult (BL_CDECL* disableStyle            )(BLContextImpl* impl, BLContextStyleSlot slot) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStyleAlpha           )(BLContextImpl* impl, BLContextStyleSlot slot, double alpha) BL_NOEXCEPT;

  BLResult (BL_CDECL* swapStyles              )(BLContextImpl* impl, BLContextStyleSwapMode mode) BL_NOEXCEPT;

  BLResult (BL_CDECL* setGlobalAlpha          )(BLContextImpl* impl, double alpha) BL_NOEXCEPT;
  BLResult (BL_CDECL* setCompOp               )(BLContextImpl* impl, BLCompOp compOp) BL_NOEXCEPT;

  BLResult (BL_CDECL* setFillRule             )(BLContextImpl* impl, BLFillRule fillRule) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStrokeWidth          )(BLContextImpl* impl, double width) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStrokeMiterLimit     )(BLContextImpl* impl, double miterLimit) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStrokeCap            )(BLContextImpl* impl, BLStrokeCapPosition position, BLStrokeCap strokeCap) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStrokeCaps           )(BLContextImpl* impl, BLStrokeCap strokeCap) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStrokeJoin           )(BLContextImpl* impl, BLStrokeJoin strokeJoin) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStrokeDashOffset     )(BLContextImpl* impl, double dashOffset) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStrokeDashArray      )(BLContextImpl* impl, const BLArrayCore* dashArray) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStrokeTransformOrder )(BLContextImpl* impl, BLStrokeTransformOrder transformOrder) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStrokeOptions        )(BLContextImpl* impl, const BLStrokeOptionsCore* options) BL_NOEXCEPT;

  BLResult (BL_CDECL* clipToRectI             )(BLContextImpl* impl, const BLRectI* rect) BL_NOEXCEPT;
  BLResult (BL_CDECL* clipToRectD             )(BLContextImpl* impl, const BLRect* rect) BL_NOEXCEPT;
  BLResult (BL_CDECL* restoreClipping         )(BLContextImpl* impl) BL_NOEXCEPT;

  BLResult (BL_CDECL* clearAll                )(BLContextImpl* impl) BL_NOEXCEPT;
  BLResult (BL_CDECL* clearRectI              )(BLContextImpl* impl, const BLRectI* rect) BL_NOEXCEPT;
  BLResult (BL_CDECL* clearRectD              )(BLContextImpl* impl, const BLRect* rect) BL_NOEXCEPT;

  BLResult (BL_CDECL* fillAll                 )(BLContextImpl* impl) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillAllRgba32           )(BLContextImpl* impl, uint32_t rgba32) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillAllExt              )(BLContextImpl* impl, const BLObjectCore* style) BL_NOEXCEPT;

  BLResult (BL_CDECL* fillGeometry            )(BLContextImpl* impl, BLGeometryType type, const void* data) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillGeometryRgba32      )(BLContextImpl* impl, BLGeometryType type, const void* data, uint32_t rgba32) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillGeometryExt         )(BLContextImpl* impl, BLGeometryType type, const void* data, const BLObjectCore* style) BL_NOEXCEPT;

  BLResult (BL_CDECL* fillTextOpI             )(BLContextImpl* self, const BLPointI* origin, const BLFontCore* font, BLContextRenderTextOp op, const void* data) BL_NOEXCEPT_C;
  BLResult (BL_CDECL* fillTextOpIRgba32       )(BLContextImpl* self, const BLPointI* origin, const BLFontCore* font, BLContextRenderTextOp op, const void* data, uint32_t rgba32) BL_NOEXCEPT_C;
  BLResult (BL_CDECL* fillTextOpIExt          )(BLContextImpl* self, const BLPointI* origin, const BLFontCore* font, BLContextRenderTextOp op, const void* data, const BLObjectCore* style) BL_NOEXCEPT_C;

  BLResult (BL_CDECL* fillTextOpD             )(BLContextImpl* self, const BLPoint* origin, const BLFontCore* font, BLContextRenderTextOp op, const void* data) BL_NOEXCEPT_C;
  BLResult (BL_CDECL* fillTextOpDRgba32       )(BLContextImpl* self, const BLPoint* origin, const BLFontCore* font, BLContextRenderTextOp op, const void* data, uint32_t rgba32) BL_NOEXCEPT_C;
  BLResult (BL_CDECL* fillTextOpDExt          )(BLContextImpl* self, const BLPoint* origin, const BLFontCore* font, BLContextRenderTextOp op, const void* data, const BLObjectCore* style) BL_NOEXCEPT_C;

  BLResult (BL_CDECL* fillMaskI               )(BLContextImpl* impl, const BLPointI* origin, const BLImageCore* mask, const BLRectI* maskArea) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillMaskIRgba32         )(BLContextImpl* impl, const BLPointI* origin, const BLImageCore* mask, const BLRectI* maskArea, uint32_t rgba32) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillMaskIExt            )(BLContextImpl* impl, const BLPointI* origin, const BLImageCore* mask, const BLRectI* maskArea, const BLObjectCore* style) BL_NOEXCEPT;

  BLResult (BL_CDECL* fillMaskD               )(BLContextImpl* impl, const BLPoint* origin, const BLImageCore* mask, const BLRectI* maskArea) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillMaskDRgba32         )(BLContextImpl* impl, const BLPoint* origin, const BLImageCore* mask, const BLRectI* maskArea, uint32_t rgba32) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillMaskDExt            )(BLContextImpl* impl, const BLPoint* origin, const BLImageCore* mask, const BLRectI* maskArea, const BLObjectCore* style) BL_NOEXCEPT;

  BLResult (BL_CDECL* strokePathD             )(BLContextImpl* impl, const BLPoint* origin, const BLPathCore* path) BL_NOEXCEPT;
  BLResult (BL_CDECL* strokePathDRgba32       )(BLContextImpl* impl, const BLPoint* origin, const BLPathCore* path, uint32_t rgba32) BL_NOEXCEPT;
  BLResult (BL_CDECL* strokePathDExt          )(BLContextImpl* impl, const BLPoint* origin, const BLPathCore* path, const BLObjectCore* style) BL_NOEXCEPT;

  BLResult (BL_CDECL* strokeGeometry          )(BLContextImpl* impl, BLGeometryType type, const void* data) BL_NOEXCEPT;
  BLResult (BL_CDECL* strokeGeometryRgba32    )(BLContextImpl* impl, BLGeometryType type, const void* data, uint32_t rgba32) BL_NOEXCEPT;
  BLResult (BL_CDECL* strokeGeometryExt       )(BLContextImpl* impl, BLGeometryType type, const void* data, const BLObjectCore* style) BL_NOEXCEPT;

  BLResult (BL_CDECL* strokeTextOpI           )(BLContextImpl* self, const BLPointI* origin, const BLFontCore* font, BLContextRenderTextOp op, const void* data) BL_NOEXCEPT_C;
  BLResult (BL_CDECL* strokeTextOpIRgba32     )(BLContextImpl* self, const BLPointI* origin, const BLFontCore* font, BLContextRenderTextOp op, const void* data, uint32_t rgba32) BL_NOEXCEPT_C;
  BLResult (BL_CDECL* strokeTextOpIExt        )(BLContextImpl* self, const BLPointI* origin, const BLFontCore* font, BLContextRenderTextOp op, const void* data, const BLObjectCore* style) BL_NOEXCEPT_C;

  BLResult (BL_CDECL* strokeTextOpD           )(BLContextImpl* self, const BLPoint* origin, const BLFontCore* font, BLContextRenderTextOp op, const void* data) BL_NOEXCEPT_C;
  BLResult (BL_CDECL* strokeTextOpDRgba32     )(BLContextImpl* self, const BLPoint* origin, const BLFontCore* font, BLContextRenderTextOp op, const void* data, uint32_t rgba32) BL_NOEXCEPT_C;
  BLResult (BL_CDECL* strokeTextOpDExt        )(BLContextImpl* self, const BLPoint* origin, const BLFontCore* font, BLContextRenderTextOp op, const void* data, const BLObjectCore* style) BL_NOEXCEPT_C;

  BLResult (BL_CDECL* blitImageD              )(BLContextImpl* impl, const BLPoint* origin, const BLImageCore* img, const BLRectI* imgArea) BL_NOEXCEPT;
  BLResult (BL_CDECL* blitScaledImageD        )(BLContextImpl* impl, const BLRect* rect, const BLImageCore* img, const BLRectI* imgArea) BL_NOEXCEPT;
};

//! Rendering context state.
//!
//! This state is not meant to be created by users, it's only provided for users that want to introspect
//! the rendering context state and for C++ API that accesses it directly for performance reasons.
struct BLContextState {
  //! Target image or image object with nullptr impl in case that the rendering context doesn't render to an image.
  BLImageCore* targetImage;
  //! Current size of the target in abstract units, pixels if rendering to \ref BLImage.
  BLSize targetSize;

  //! Current rendering context hints.
  BLContextHints hints;
  //! Current composition operator.
  uint8_t compOp;
  //! Current fill rule.
  uint8_t fillRule;
  //! Current type of a style object of fill and stroke operations indexed by \ref BLContextStyleSlot.
  uint8_t styleType[2];
  //! Count of saved states in the context.
  uint32_t savedStateCount;

  //! Current global alpha value [0, 1].
  double globalAlpha;
  //! Current fill or stroke alpha indexed by style slot, see \ref BLContextStyleSlot.
  double styleAlpha[2];

  //! Current stroke options.
  BLStrokeOptionsCore strokeOptions;

  //! Current approximation options.
  BLApproximationOptions approximationOptions;

  //! Current meta transformation matrix.
  BLMatrix2D metaTransform;
  //! Current user transformation matrix.
  BLMatrix2D userTransform;
  //! Current final transformation matrix, which combines all transformation matrices.
  BLMatrix2D finalTransform;
};

//! Rendering context [C API Impl].
struct BLContextImpl BL_CLASS_INHERITS(BLObjectImpl) {
  //! Virtual function table.
  const BLContextVirt* virt;
  //! Current state of the context.
  const BLContextState* state;

  //! Type of the rendering context, see \ref BLContextType.
  uint32_t contextType;
};
//! \endcond

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blContextInit(BLContextCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextInitMove(BLContextCore* self, BLContextCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextInitWeak(BLContextCore* self, const BLContextCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextInitAs(BLContextCore* self, BLImageCore* image, const BLContextCreateInfo* cci) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextDestroy(BLContextCore* self) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextReset(BLContextCore* self) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextAssignMove(BLContextCore* self, BLContextCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextAssignWeak(BLContextCore* self, const BLContextCore* other) BL_NOEXCEPT_C;

BL_API BLContextType BL_CDECL blContextGetType(const BLContextCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API BLResult BL_CDECL blContextGetTargetSize(const BLContextCore* self, BLSize* targetSizeOut) BL_NOEXCEPT_C;
BL_API BLImageCore* BL_CDECL blContextGetTargetImage(const BLContextCore* self) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextBegin(BLContextCore* self, BLImageCore* image, const BLContextCreateInfo* cci) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextEnd(BLContextCore* self) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextFlush(BLContextCore* self, BLContextFlushFlags flags) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextSave(BLContextCore* self, BLContextCookie* cookie) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextRestore(BLContextCore* self, const BLContextCookie* cookie) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextGetMetaTransform(const BLContextCore* self, BLMatrix2D* transformOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextGetUserTransform(const BLContextCore* self, BLMatrix2D* transformOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextGetFinalTransform(const BLContextCore* self, BLMatrix2D* transformOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextUserToMeta(BLContextCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextApplyTransformOp(BLContextCore* self, BLTransformOp opType, const void* opData) BL_NOEXCEPT_C;

BL_API uint32_t BL_CDECL blContextGetHint(const BLContextCore* self, BLContextHint hintType) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetHint(BLContextCore* self, BLContextHint hintType, uint32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextGetHints(const BLContextCore* self, BLContextHints* hintsOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetHints(BLContextCore* self, const BLContextHints* hints) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextSetFlattenMode(BLContextCore* self, BLFlattenMode mode) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetFlattenTolerance(BLContextCore* self, double tolerance) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetApproximationOptions(BLContextCore* self, const BLApproximationOptions* options) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextGetFillStyle(const BLContextCore* self, BLVarCore* styleOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextGetTransformedFillStyle(const BLContextCore* self, BLVarCore* styleOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetFillStyle(BLContextCore* self, const BLUnknown* style) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetFillStyleWithMode(BLContextCore* self, const BLUnknown* style, BLContextStyleTransformMode transformMode) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetFillStyleRgba(BLContextCore* self, const BLRgba* rgba) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetFillStyleRgba32(BLContextCore* self, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetFillStyleRgba64(BLContextCore* self, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextDisableFillStyle(BLContextCore* self) BL_NOEXCEPT_C;
BL_API double BL_CDECL blContextGetFillAlpha(const BLContextCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetFillAlpha(BLContextCore* self, double alpha) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextGetStrokeStyle(const BLContextCore* self, BLVarCore* styleOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextGetTransformedStrokeStyle(const BLContextCore* self, BLVarCore* styleOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetStrokeStyle(BLContextCore* self, const BLUnknown* style) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetStrokeStyleWithMode(BLContextCore* self, const BLUnknown* style, BLContextStyleTransformMode transformMode) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetStrokeStyleRgba(BLContextCore* self, const BLRgba* rgba) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetStrokeStyleRgba32(BLContextCore* self, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetStrokeStyleRgba64(BLContextCore* self, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextDisableStrokeStyle(BLContextCore* self) BL_NOEXCEPT_C;
BL_API double BL_CDECL blContextGetStrokeAlpha(const BLContextCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetStrokeAlpha(BLContextCore* self, double alpha) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextSwapStyles(BLContextCore* self, BLContextStyleSwapMode mode) BL_NOEXCEPT_C;

BL_API double BL_CDECL blContextGetGlobalAlpha(const BLContextCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetGlobalAlpha(BLContextCore* self, double alpha) BL_NOEXCEPT_C;

BL_API BLCompOp BL_CDECL blContextGetCompOp(const BLContextCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetCompOp(BLContextCore* self, BLCompOp compOp) BL_NOEXCEPT_C;

BL_API BLFillRule BL_CDECL blContextGetFillRule(const BLContextCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetFillRule(BLContextCore* self, BLFillRule fillRule) BL_NOEXCEPT_C;

BL_API double BL_CDECL blContextGetStrokeWidth(const BLContextCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetStrokeWidth(BLContextCore* self, double width) BL_NOEXCEPT_C;

BL_API double BL_CDECL blContextGetStrokeMiterLimit(const BLContextCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetStrokeMiterLimit(BLContextCore* self, double miterLimit) BL_NOEXCEPT_C;

BL_API BLStrokeCap BL_CDECL blContextGetStrokeCap(const BLContextCore* self, BLStrokeCapPosition position) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetStrokeCap(BLContextCore* self, BLStrokeCapPosition position, BLStrokeCap strokeCap) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetStrokeCaps(BLContextCore* self, BLStrokeCap strokeCap) BL_NOEXCEPT_C;

BL_API BLStrokeJoin BL_CDECL blContextGetStrokeJoin(const BLContextCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetStrokeJoin(BLContextCore* self, BLStrokeJoin strokeJoin) BL_NOEXCEPT_C;

BL_API BLStrokeTransformOrder BL_CDECL blContextGetStrokeTransformOrder(const BLContextCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetStrokeTransformOrder(BLContextCore* self, BLStrokeTransformOrder transformOrder) BL_NOEXCEPT_C;

BL_API double BL_CDECL blContextGetStrokeDashOffset(const BLContextCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetStrokeDashOffset(BLContextCore* self, double dashOffset) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextGetStrokeDashArray(const BLContextCore* self, BLArrayCore* dashArrayOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetStrokeDashArray(BLContextCore* self, const BLArrayCore* dashArray) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextGetStrokeOptions(const BLContextCore* self, BLStrokeOptionsCore* options) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextSetStrokeOptions(BLContextCore* self, const BLStrokeOptionsCore* options) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextClipToRectI(BLContextCore* self, const BLRectI* rect) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextClipToRectD(BLContextCore* self, const BLRect* rect) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextRestoreClipping(BLContextCore* self) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextClearAll(BLContextCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextClearRectI(BLContextCore* self, const BLRectI* rect) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextClearRectD(BLContextCore* self, const BLRect* rect) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextFillAll(BLContextCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillAllRgba32(BLContextCore* self, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillAllRgba64(BLContextCore* self, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillAllExt(BLContextCore* self, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextFillRectI(BLContextCore* self, const BLRectI* rect) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillRectIRgba32(BLContextCore* self, const BLRectI* rect, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillRectIRgba64(BLContextCore* self, const BLRectI* rect, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillRectIExt(BLContextCore* self, const BLRectI* rect, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextFillRectD(BLContextCore* self, const BLRect* rect) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillRectDRgba32(BLContextCore* self, const BLRect* rect, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillRectDRgba64(BLContextCore* self, const BLRect* rect, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillRectDExt(BLContextCore* self, const BLRect* rect, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextFillPathD(BLContextCore* self, const BLPoint* origin, const BLPathCore* path) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillPathDRgba32(BLContextCore* self, const BLPoint* origin, const BLPathCore* path, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillPathDRgba64(BLContextCore* self, const BLPoint* origin, const BLPathCore* path, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillPathDExt(BLContextCore* self, const BLPoint* origin, const BLPathCore* path, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextFillGeometry(BLContextCore* self, BLGeometryType type, const void* data) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillGeometryRgba32(BLContextCore* self, BLGeometryType type, const void* data, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillGeometryRgba64(BLContextCore* self, BLGeometryType type, const void* data, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillGeometryExt(BLContextCore* self, BLGeometryType type, const void* data, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextFillUtf8TextI(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillUtf8TextIRgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillUtf8TextIRgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillUtf8TextIExt(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextFillUtf8TextD(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillUtf8TextDRgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillUtf8TextDRgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillUtf8TextDExt(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextFillUtf16TextI(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillUtf16TextIRgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillUtf16TextIRgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillUtf16TextIExt(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextFillUtf16TextD(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillUtf16TextDRgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillUtf16TextDRgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillUtf16TextDExt(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextFillUtf32TextI(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillUtf32TextIRgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillUtf32TextIRgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillUtf32TextIExt(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextFillUtf32TextD(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillUtf32TextDRgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillUtf32TextDRgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillUtf32TextDExt(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextFillGlyphRunI(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyphRun) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillGlyphRunIRgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillGlyphRunIRgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillGlyphRunIExt(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextFillGlyphRunD(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyphRun) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillGlyphRunDRgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillGlyphRunDRgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillGlyphRunDExt(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextFillMaskI(BLContextCore* self, const BLPointI* origin, const BLImageCore* mask, const BLRectI* maskArea) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillMaskIRgba32(BLContextCore* self, const BLPointI* origin, const BLImageCore* mask, const BLRectI* maskArea, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillMaskIRgba64(BLContextCore* self, const BLPointI* origin, const BLImageCore* mask, const BLRectI* maskArea, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillMaskIExt(BLContextCore* self, const BLPointI* origin, const BLImageCore* mask, const BLRectI* maskArea, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextFillMaskD(BLContextCore* self, const BLPoint* origin, const BLImageCore* mask, const BLRectI* maskArea) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillMaskDRgba32(BLContextCore* self, const BLPoint* origin, const BLImageCore* mask, const BLRectI* maskArea, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillMaskDRgba64(BLContextCore* self, const BLPoint* origin, const BLImageCore* mask, const BLRectI* maskArea, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextFillMaskDExt(BLContextCore* self, const BLPoint* origin, const BLImageCore* mask, const BLRectI* maskArea, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextStrokeRectI(BLContextCore* self, const BLRectI* rect) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeRectIRgba32(BLContextCore* self, const BLRectI* rect, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeRectIRgba64(BLContextCore* self, const BLRectI* rect, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeRectIExt(BLContextCore* self, const BLRectI* rect, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextStrokeRectD(BLContextCore* self, const BLRect* rect) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeRectDRgba32(BLContextCore* self, const BLRect* rect, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeRectDRgba64(BLContextCore* self, const BLRect* rect, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeRectDExt(BLContextCore* self, const BLRect* rect, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextStrokePathD(BLContextCore* self, const BLPoint* origin, const BLPathCore* path) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokePathDRgba32(BLContextCore* self, const BLPoint* origin, const BLPathCore* path, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokePathDRgba64(BLContextCore* self, const BLPoint* origin, const BLPathCore* path, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokePathDExt(BLContextCore* self, const BLPoint* origin, const BLPathCore* path, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextStrokeGeometry(BLContextCore* self, BLGeometryType type, const void* data) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeGeometryRgba32(BLContextCore* self, BLGeometryType type, const void* data, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeGeometryRgba64(BLContextCore* self, BLGeometryType type, const void* data, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeGeometryExt(BLContextCore* self, BLGeometryType type, const void* data, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextStrokeUtf8TextI(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeUtf8TextIRgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeUtf8TextIRgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeUtf8TextIExt(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextStrokeUtf8TextD(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeUtf8TextDRgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeUtf8TextDRgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeUtf8TextDExt(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextStrokeUtf16TextI(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeUtf16TextIRgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeUtf16TextIRgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeUtf16TextIExt(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextStrokeUtf16TextD(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeUtf16TextDRgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeUtf16TextDRgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeUtf16TextDExt(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextStrokeUtf32TextI(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeUtf32TextIRgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeUtf32TextIRgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeUtf32TextIExt(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextStrokeUtf32TextD(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeUtf32TextDRgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeUtf32TextDRgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeUtf32TextDExt(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextStrokeGlyphRunI(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyphRun) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeGlyphRunIRgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeGlyphRunIRgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeGlyphRunIExt(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextStrokeGlyphRunD(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyphRun) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeGlyphRunDRgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeGlyphRunDRgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextStrokeGlyphRunDExt(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, const BLUnknown* style) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blContextBlitImageI(BLContextCore* self, const BLPointI* origin, const BLImageCore* img, const BLRectI* imgArea) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextBlitImageD(BLContextCore* self, const BLPoint* origin, const BLImageCore* img, const BLRectI* imgArea) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextBlitScaledImageI(BLContextCore* self, const BLRectI* rect, const BLImageCore* img, const BLRectI* imgArea) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blContextBlitScaledImageD(BLContextCore* self, const BLRect* rect, const BLImageCore* img, const BLRectI* imgArea) BL_NOEXCEPT_C;

BL_END_C_DECLS

//! \}
//! \}

//! \addtogroup bl_rendering
//! \{

#ifdef __cplusplus

//! \cond INTERNAL

namespace BLInternal {

static BL_INLINE BLVarCore makeInlineStyle(const BLRgba32& rgba32) noexcept {
  BLVarCore style;
  style._d.initRgba32(rgba32.value);
  return style;
}

static BL_INLINE BLVarCore makeInlineStyle(const BLRgba64& rgba64) noexcept {
  BLVarCore style;
  style._d.initRgba64(rgba64.value);
  return style;
}

static BL_INLINE BLVarCore makeInlineStyle(const BLRgba& rgba) noexcept {
  uint32_t r = blBitCast<uint32_t>(rgba.r);
  uint32_t g = blBitCast<uint32_t>(rgba.g);
  uint32_t b = blBitCast<uint32_t>(rgba.b);
  uint32_t a = blBitCast<uint32_t>(blMax(0.0f, rgba.a)) & 0x7FFFFFFFu;

  BLVarCore style;
  style._d.initU32x4(r, g, b, a);
  return style;
}

template<typename StyleT> struct ForwardedStyleOf { typedef const StyleT& Type; };
template<> struct ForwardedStyleOf<BLRgba> { typedef BLVarCore Type; };
template<> struct ForwardedStyleOf<BLRgba32> { typedef BLVarCore Type; };
template<> struct ForwardedStyleOf<BLRgba64> { typedef BLVarCore Type; };

static BL_INLINE_NODEBUG BLVarCore forwardStyle(const BLRgba& rgba) { return makeInlineStyle(rgba); }
static BL_INLINE_NODEBUG BLVarCore forwardStyle(const BLRgba32& rgba) { return makeInlineStyle(rgba); }
static BL_INLINE_NODEBUG BLVarCore forwardStyle(const BLRgba64& rgba) { return makeInlineStyle(rgba); }

static BL_INLINE_NODEBUG const BLVarCore& forwardStyle(const BLVarCore& var) { return var; }
static BL_INLINE_NODEBUG const BLPatternCore& forwardStyle(const BLPatternCore& pattern) { return pattern; }
static BL_INLINE_NODEBUG const BLGradientCore& forwardStyle(const BLGradientCore& gradient) { return gradient; }

static BL_INLINE_NODEBUG const BLVar& forwardStyle(const BLVar& var) { return var; }
static BL_INLINE_NODEBUG const BLPattern& forwardStyle(const BLPattern& pattern) { return pattern; }
static BL_INLINE_NODEBUG const BLGradient& forwardStyle(const BLGradient& gradient) { return gradient; }

} // {BLInternal}

//! \endcond

//! \name BLContext - C++ API
//! \{

//! Rendering context [C++ API].
class BLContext /* final */ : public BLContextCore {
public:
  //! \cond INTERNAL

  enum : uint32_t {
    kDefaultSignature = BLObjectInfo::packTypeWithMarker(BL_OBJECT_TYPE_CONTEXT) | BL_OBJECT_INFO_D_FLAG
  };

  // This macro is here so stepping-in into BLContext API doesn't step into _impl() method.
  #define BL_CONTEXT_IMPL() static_cast<BLContextImpl*>(_d.impl)

  // Optimize the virtual call dispatch (only clang currently understands this optimization).
#if defined(__clang__)
  #define BL_CONTEXT_CALL_RETURN(fn, ...)          \
    BLContextImpl* impl = BL_CONTEXT_IMPL();       \
    const BLContextVirt* virt = impl->virt;        \
    const BLContextState* state = impl->state;     \
                                                   \
    BLResult result = virt->fn(__VA_ARGS__);       \
                                                   \
    BL_ASSUME(impl == BL_CONTEXT_IMPL());          \
    BL_ASSUME(virt == BL_CONTEXT_IMPL()->virt);    \
    BL_ASSUME(state == BL_CONTEXT_IMPL()->state);  \
                                                   \
    return result
#else
  #define BL_CONTEXT_CALL_RETURN(fn, ...)          \
    BLContextImpl* impl = BL_CONTEXT_IMPL();       \
    return impl->virt->fn(__VA_ARGS__)
#endif
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  //! Creates a default constructed rendering context.
  //!
  //! Default constructed means that the instance is valid, but uninitialized,  which means the rendering context does
  //! not have attached any target. Any attempt to use uninitialized context results in \ref BL_ERROR_NOT_INITIALIZED
  //! error.
  BL_INLINE_NODEBUG BLContext() noexcept {
    blContextInit(this);
    BL_ASSUME(_d.info.bits == kDefaultSignature);
  }

  //! Move constructor.
  //!
  //! Moves the `other` rendering context into this one and resets the `other` rendering context to
  //! a default-constructed state. This is an efficient way of "moving" the rendering context as it doesn't touch its
  //! internal reference count, which is atomic, because moving is internally implemented as a trivial memory copy.
  BL_INLINE_NODEBUG BLContext(BLContext&& other) noexcept {
    blContextInitMove(this, &other);
  }

  //! Copy constructor.
  //!
  //! Creates a weak-copy of the `other` rendering context by increasing it's internal reference counter. This context
  //! and `other` would point to the same data and would be otherwise identical. Any change to `other` would also
  //! affect this context.
  //!
  //! This function is mostly provided for C++ users that may keep a global reference to the same rendering context,
  //! for example, otherwise sharing is not that useful as the rendering context has states that are manipulated during
  //! rendering.
  //!
  //! Two weak copies of the same rendering context cannot be used by different threads simultaneously.
  BL_INLINE_NODEBUG BLContext(const BLContext& other) noexcept {
    blContextInitWeak(this, &other);
  }

  //! Creates a new rendering context for rendering to the image `target`.
  //!
  //! This is a simplified constructor that can be used to create a rendering context without any additional parameters,
  //! which means that the rendering context will use a single-threaded synchronous rendering.
  //!
  //! \note Since Blend2D doesn't use exceptions in C++ this function always succeeds even when an error happened.
  //! The underlying C-API function \ref blContextInitAs() returns an error, but it just cannot be propagated back.
  //! Use \ref begin() function, which returns a \ref BLResult to check the status of the call immediately.
  BL_INLINE_NODEBUG explicit BLContext(BLImageCore& target) noexcept {
    blContextInitAs(this, &target, nullptr);
  }

  //! Creates a new rendering context for rendering to the image `target`.
  //!
  //! This is an advanced constructor that can be used to create a rendering context with additional parameters. These
  //! parameters can be used to specify the number of threads to be used during rendering and to select other features.
  //!
  //! \note Since Blend2D doesn't use exceptions in C++ this function always succeeds even when an error happened.
  //! The underlying C-API function \ref blContextInitAs() returns an error, but it just cannot be propagated back.
  //! Use \ref begin() function, which returns a \ref BLResult to check the status of the call immediately.
  BL_INLINE_NODEBUG BLContext(BLImageCore& target, const BLContextCreateInfo& createInfo) noexcept {
    blContextInitAs(this, &target, &createInfo);
  }

  //! \overload
  BL_INLINE_NODEBUG BLContext(BLImageCore& target, const BLContextCreateInfo* createInfo) noexcept {
    blContextInitAs(this, &target, createInfo);
  }

  //! Destroys the rendering context.
  //!
  //! Waits for all operations, detaches the target from the rendering context and then destroys it. Does nothing if
  //! the context is not initialized.
  //!
  //! \note Destroying the rendering context would always internally call `flush(BL_CONTEXT_FLUSH_SYNC)`, which would
  //! flush the render calls queue in case multi-threaded rendering is used.
  BL_INLINE_NODEBUG ~BLContext() noexcept {
    if (BLInternal::objectNeedsCleanup(_d.info.bits))
      blContextDestroy(this);
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  //! Returns true if the rendering context is initialized (has target attached).
  //!
  //! Provided for users that want to use bool idiom in C++ to check for the status of the object.
  //!
  //! ```
  //! BLContext ctx(some_image);
  //! if (ctx) {
  //!   // Rendering context is initialized.
  //! }
  //! ```
  BL_INLINE_NODEBUG explicit operator bool() const noexcept { return isValid(); }

  //! Implements a copy-assignment operator.
  //!
  //! Copying a rendering context creates a weak-copy only, which means that all copied instances share the same
  //! underlying rendering context. The rendering context internally uses atomic reference counting to manage ots
  //! lifetime.
  BL_INLINE_NODEBUG BLContext& operator=(const BLContext& other) noexcept { blContextAssignWeak(this, &other); return *this; }

  //! Implements a move-assignment operator.
  //!
  //! Moves the rendering context of `other` to this rendering context and makes the `other` rendering context
  //! default constructed (which uses an internal "null" implementation that renders to nothing).
  BL_INLINE_NODEBUG BLContext& operator=(BLContext&& other) noexcept { blContextAssignMove(this, &other); return *this; }

  //! Returns whether this and `other` point to the same rendering context.
  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator==(const BLContext& other) const noexcept { return equals(other); }

  //! Returns whether this and `other` are different rendering contexts.
  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator!=(const BLContext& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Target Information
  //! \{

  //! Returns the target size in abstract units (pixels in case of \ref BLImage).
  BL_NODISCARD
  BL_INLINE_NODEBUG BLSize targetSize() const noexcept { return BL_CONTEXT_IMPL()->state->targetSize; }

  //! Returns the target width in abstract units (pixels in case of \ref BLImage).
  BL_NODISCARD
  BL_INLINE_NODEBUG double targetWidth() const noexcept { return BL_CONTEXT_IMPL()->state->targetSize.w; }

  //! Returns the target height in abstract units (pixels in case of \ref BLImage).
  BL_NODISCARD
  BL_INLINE_NODEBUG double targetHeight() const noexcept { return BL_CONTEXT_IMPL()->state->targetSize.h; }

  //! Returns the target image or null if there is no target image.
  //!
  //! \note The rendering context doesn't own the image, but it increases its writer count, which means that the image
  //! will not be destroyed even when user destroys it during the rendering (in such case it will be destroyed after
  //! the rendering ends when the writer count goes to zero). This means that the rendering context must hold the image
  //! and not the pointer to the \ref BLImage passed to either the constructor or `begin()` function. So the returned
  //! pointer is not the same as the pointer passed to `begin()`, but it points to the same underlying data.
  BL_NODISCARD
  BL_INLINE_NODEBUG BLImage* targetImage() const noexcept {
    return static_cast<BLImage*>(BL_CONTEXT_IMPL()->state->targetImage);
  }

  //! \}

  //! \name Context Lifetime and Others
  //! \{

  //! Returns the type of this context, see \ref BLContextType.
  BL_NODISCARD
  BL_INLINE_NODEBUG BLContextType contextType() const noexcept {
    return BLContextType(BL_CONTEXT_IMPL()->contextType);
  }

  //! Tests whether the context is a valid rendering context that has attached target to it.
  BL_NODISCARD
  BL_INLINE_NODEBUG bool isValid() const noexcept {
    return contextType() != BL_CONTEXT_TYPE_NONE;
  }

  //! Returns whether this and `other` point to the same rendering context.
  BL_NODISCARD
  BL_INLINE_NODEBUG bool equals(const BLContext& other) const noexcept {
    return this->_d.impl == other._d.impl;
  }

  //! Resets this rendering context to the default constructed one.
  //!
  //! Similar behavior to the destructor, but the rendering context will still be a valid object after the call to
  //! `reset()` and would behave like a default constructed context.
  BL_INLINE_NODEBUG BLResult reset() noexcept {
    BLResult result = blContextReset(this);
    BL_ASSUME(result == BL_SUCCESS);
    BL_ASSUME(_d.info.bits == kDefaultSignature);
    return result;
  }

  //! Assigns the `other` rendering context to this rendering context.
  //!
  //! This is the same as using C++ copy-assignment operator, see \ref operator=().
  BL_INLINE_NODEBUG BLResult assign(const BLContext& other) noexcept {
    return blContextAssignWeak(this, &other);
  }

  //! Moves the `other` rendering context to this rendering context, which would make ther `other` rendering context
  //! default initialized.
  //!
  //! This is the same as using C++ move-assignment operator, see \ref operator=().
  BL_INLINE_NODEBUG BLResult assign(BLContext&& other) noexcept {
    return blContextAssignMove(this, &other);
  }

  //! Begins rendering to the given `image`.
  //!
  //! This is a simplified `begin()` function that can be used to create a rendering context without any additional
  //! parameters, which means that the rendering context will use a single-threaded synchronous rendering.
  //!
  //! If this operation succeeds then the rendering context will have exclusive access to the image data. This means
  //! that no other renderer can use it during rendering.
  BL_INLINE_NODEBUG BLResult begin(BLImageCore& image) noexcept {
    return blContextBegin(this, &image, nullptr);
  }

  //! Begins rendering to the given `image`.
  //!
  //! This is an advanced `begin()` function that can be used to create a rendering context with additional parameters.
  //! These parameters can be used to specify the number of threads to be used during rendering and to select other
  //! features.
  //!
  //! If this operation succeeds then the rendering context will have exclusive access to the image data. This means
  //! that no other renderer can use it during rendering.
  BL_INLINE_NODEBUG BLResult begin(BLImageCore& image, const BLContextCreateInfo& createInfo) noexcept {
    return blContextBegin(this, &image, &createInfo);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult begin(BLImageCore& image, const BLContextCreateInfo* createInfo) noexcept {
    return blContextBegin(this, &image, createInfo);
  }

  //! Waits for completion of all render commands and detaches the rendering context from the rendering target.
  //! After `end()` completes the rendering context implementation would be released and replaced by a built-in
  //! null instance (no context).
  //!
  //! \note Calling `end()` would implicitly call `flush(BL_CONTEXT_FLUSH_SYNC)`, which would flush the render calls
  //! queue in case multi-threaded rendering is used.
  BL_INLINE_NODEBUG BLResult end() noexcept {
    BLResult result = blContextEnd(this);
    BL_ASSUME(_d.info.bits == kDefaultSignature);
    return result;
  }

  //! Flushes the context, see \ref BLContextFlushFlags.
  BL_INLINE_NODEBUG BLResult flush(BLContextFlushFlags flags) noexcept {
    BL_CONTEXT_CALL_RETURN(flush, impl, flags);
  }

  //! \}

  //! \name Properties
  //! \{

  BL_DEFINE_OBJECT_PROPERTY_API

  //! Queries the number of threads that the rendering context uses.
  //!
  //! If the returned value is zero it means that the rendering is synchronous, otherwise it describes the number of
  //! threads used for asynchronous rendering which include the user thread. For example if the returned value is `2`
  //! it means that the rendering context uses the user thread and one more worker.
  BL_NODISCARD
  BL_INLINE_NODEBUG uint32_t threadCount() const noexcept {
    uint32_t value;
    blObjectGetPropertyUInt32(this, "threadCount", 11, &value);
    return value;
  }

  //! Queries accumulated errors as flags, see \ref BLContextErrorFlags.
  //!
  //! Errors may accumulate during the lifetime of the rendering context.
  BL_NODISCARD
  BL_INLINE_NODEBUG BLContextErrorFlags accumulatedErrorFlags() const noexcept {
    uint32_t value;
    blObjectGetPropertyUInt32(this, "accumulatedErrorFlags", 21, &value);
    return BLContextErrorFlags(value);
  }

  //! \}

  //! \name State Management
  //! \{

  //! Returns the number of saved states in the context (0 means no saved states).
  //!
  //! \note Each successful call to \ref save() increments the saved-state counter and each successful call to
  //! `restore()` decrements it. However, the calls must be successful as the rendering context allows to restrict
  //! the number of save states, for example, or to use a \ref BLContextCookie to guard state save and restoration.
  BL_NODISCARD
  BL_INLINE_NODEBUG uint32_t savedStateCount() const noexcept {
    return BL_CONTEXT_IMPL()->state->savedStateCount;
  }

  //! Saves the current rendering context state.
  //!
  //! Blend2D uses optimizations that make \ref save() a cheap operation. Only core values are actually saved in
  //! \ref save(), others will only be saved if they are modified. This means that consecutive calls to \ref save()
  //! and \ref restore() do almost nothing.
  BL_INLINE_NODEBUG BLResult save() noexcept {
    BL_CONTEXT_CALL_RETURN(save, impl, nullptr);
  }

  //! Saves the current rendering context state and creates a restoration `cookie`.
  //!
  //! If you use a `cookie` to save a state you have to use the same cookie to restore it otherwise the \ref restore()
  //! would fail. Please note that cookies are not a means of security, they are provided for making it easier to
  //! guarantee that a code that you may not control won't break your context.
  BL_INLINE_NODEBUG BLResult save(BLContextCookie& cookie) noexcept {
    BL_CONTEXT_CALL_RETURN(save, impl, &cookie);
  }

  //! Restores the top-most saved context-state.
  //!
  //! Possible return conditions:
  //!
  //!   - \ref BL_SUCCESS - State was restored successfully.
  //!   - \ref BL_ERROR_NO_STATES_TO_RESTORE - There are no saved states to restore.
  //!   - \ref BL_ERROR_NO_MATCHING_COOKIE - Previous state was saved with cookie, which was not provided. You would need
  //!     the correct cookie to restore such state.
  BL_INLINE_NODEBUG BLResult restore() noexcept {
    BL_CONTEXT_CALL_RETURN(restore, impl, nullptr);
  }

  //! Restores to the point that matches the given `cookie`.
  //!
  //! More than one state can be restored in case that the `cookie` points to some previous state in the list.
  //!
  //! Possible return conditions:
  //!
  //!   - \ref BL_SUCCESS - Matching state was restored successfully.
  //!   - \ref BL_ERROR_NO_STATES_TO_RESTORE - There are no saved states to restore.
  //!   - \ref BL_ERROR_NO_MATCHING_COOKIE - The cookie did't match any saved state.
  BL_INLINE_NODEBUG BLResult restore(const BLContextCookie& cookie) noexcept {
    BL_CONTEXT_CALL_RETURN(restore, impl, &cookie);
  }

  //! \}

  //! \cond INTERNAL
  //! \name Transformations (Internal)
  //! \{

  //! Applies a matrix operation to the current transformation matrix (internal).
  BL_INLINE_NODEBUG BLResult _applyTransformOp(BLTransformOp opType, const void* opData) noexcept {
    BL_CONTEXT_CALL_RETURN(applyTransformOp, impl, opType, opData);
  }

  //! Applies a matrix operation to the current transformation matrix (internal).
  template<typename... Args>
  BL_INLINE_NODEBUG BLResult _applyTransformOpV(BLTransformOp opType, Args&&... args) noexcept {
    double opData[] = { double(args)... };
    BL_CONTEXT_CALL_RETURN(applyTransformOp, impl, opType, opData);
  }

  //! \}
  //! \endcond

  //! \name Transformations
  //! \{

  //! Returns a meta transformation matrix.
  //!
  //! Meta matrix is a core transformation matrix that is normally not changed by transformations applied to the
  //! context. Instead it acts as a secondary matrix used to create the final transformation matrix from meta and
  //! user matrices.
  //!
  //! Meta matrix can be used to scale the whole context for HI-DPI rendering or to change the orientation of the
  //! image being rendered, however, the number of use-cases is unlimited.
  //!
  //! To change the meta-matrix you must first change user-matrix and then call `userToMeta()`, which would update
  //! meta-matrix and clear user-matrix.
  //!
  //! See `userTransform()` and `userToMeta()`.
  BL_NODISCARD
  BL_INLINE_NODEBUG const BLMatrix2D& metaTransform() const noexcept { return BL_CONTEXT_IMPL()->state->metaTransform; }

  //! Returns a user transformation matrix.
  //!
  //! User matrix contains all transformations that happened to the rendering context unless the context was restored
  //! or `userToMeta()` was called.
  BL_NODISCARD
  BL_INLINE_NODEBUG const BLMatrix2D& userTransform() const noexcept { return BL_CONTEXT_IMPL()->state->userTransform; }

  //! Returns a final transformation matrix.
  //!
  //! Final transformation matrix is a combination of meta and user transformation matrices. It's the final
  //! transformation that the rendering context applies to all input coordinates.
  BL_NODISCARD
  BL_INLINE_NODEBUG const BLMatrix2D& finalTransform() const noexcept { return BL_CONTEXT_IMPL()->state->finalTransform; }

  //! Sets user transformation matrix to `m`.
  //!
  //! \note This only assigns the user transformation matrix, which means that the meta transformation matrix is kept
  //! as is. This means that the final transformation matrix will be recalculated based on the given `transform`.
  BL_INLINE_NODEBUG BLResult setTransform(const BLMatrix2D& transform) noexcept { return _applyTransformOp(BL_TRANSFORM_OP_ASSIGN, &transform); }

  //! Resets user transformation matrix to identity.
  //!
  //! \note This only resets the user transformation matrix, which means that the meta transformation matrix is kept
  //! as is. This means that the final transformation matrix after \ref resetTransform() would be the same as meta
  //! transformation matrix.
  BL_INLINE_NODEBUG BLResult resetTransform() noexcept { return _applyTransformOp(BL_TRANSFORM_OP_RESET, nullptr); }

  //! Translates the used transformation matrix by `[x, y]`.
  BL_INLINE_NODEBUG BLResult translate(double x, double y) noexcept { return _applyTransformOpV(BL_TRANSFORM_OP_TRANSLATE, x, y); }

  //! Translates the used transformation matrix by `[p]` (integer).
  BL_INLINE_NODEBUG BLResult translate(const BLPointI& p) noexcept { return _applyTransformOpV(BL_TRANSFORM_OP_TRANSLATE, p.x, p.y); }

  //! Translates the used transformation matrix by `[p]` (floating-point).
  BL_INLINE_NODEBUG BLResult translate(const BLPoint& p) noexcept { return _applyTransformOp(BL_TRANSFORM_OP_TRANSLATE, &p); }

  //! Scales the user transformation matrix by `xy` (both X and Y is scaled by `xy`).
  BL_INLINE_NODEBUG BLResult scale(double xy) noexcept { return _applyTransformOpV(BL_TRANSFORM_OP_SCALE, xy, xy); }

  //! Scales the user transformation matrix by `[x, y]`.
  BL_INLINE_NODEBUG BLResult scale(double x, double y) noexcept { return _applyTransformOpV(BL_TRANSFORM_OP_SCALE, x, y); }

  //! Scales the user transformation matrix by `[p]` (integer).
  BL_INLINE_NODEBUG BLResult scale(const BLPointI& p) noexcept { return _applyTransformOpV(BL_TRANSFORM_OP_SCALE, p.x, p.y); }

  //! Scales the user transformation matrix by `[p]` (floating-point).
  BL_INLINE_NODEBUG BLResult scale(const BLPoint& p) noexcept { return _applyTransformOp(BL_TRANSFORM_OP_SCALE, &p); }

  //! Skews the user transformation matrix by `[x, y]`.
  BL_INLINE_NODEBUG BLResult skew(double x, double y) noexcept { return _applyTransformOpV(BL_TRANSFORM_OP_SKEW, x, y); }

  //! Skews the user transformation matrix by `[p]` (floating-point).
  BL_INLINE_NODEBUG BLResult skew(const BLPoint& p) noexcept { return _applyTransformOp(BL_TRANSFORM_OP_SKEW, &p); }

  //! Rotates the user transformation matrix by `angle`.
  BL_INLINE_NODEBUG BLResult rotate(double angle) noexcept { return _applyTransformOp(BL_TRANSFORM_OP_ROTATE, &angle); }

  //! Rotates the user transformation matrix at `[x, y]` by `angle`.
  BL_INLINE_NODEBUG BLResult rotate(double angle, double x, double y) noexcept { return _applyTransformOpV(BL_TRANSFORM_OP_ROTATE_PT, angle, x, y); }

  //! Rotates the user transformation matrix at `origin` (integer) by `angle`.
  BL_INLINE_NODEBUG BLResult rotate(double angle, const BLPoint& origin) noexcept { return _applyTransformOpV(BL_TRANSFORM_OP_ROTATE_PT, angle, origin.x, origin.y); }

  //! Rotates the user transformation matrix at `origin` (floating-point) by `angle`.
  BL_INLINE_NODEBUG BLResult rotate(double angle, const BLPointI& origin) noexcept { return _applyTransformOpV(BL_TRANSFORM_OP_ROTATE_PT, angle, origin.x, origin.y); }

  //! Transforms the user transformation matrix by `transform`.
  BL_INLINE_NODEBUG BLResult applyTransform(const BLMatrix2D& transform) noexcept { return _applyTransformOp(BL_TRANSFORM_OP_TRANSFORM, &transform); }

  //! Post-translates the used transformation matrix by `[x, y]`.
  //!
  //! \note Post-translation uses a reversed order of matrix multiplication when compared to \ref translate().
  BL_INLINE_NODEBUG BLResult postTranslate(double x, double y) noexcept { return _applyTransformOpV(BL_TRANSFORM_OP_POST_TRANSLATE, x, y); }

  //! Post-Translates the used transformation matrix by `[p]` (integer).
  //!
  //! \note Post-translation uses a reversed order of matrix multiplication when compared to \ref translate().
  BL_INLINE_NODEBUG BLResult postTranslate(const BLPointI& p) noexcept { return _applyTransformOpV(BL_TRANSFORM_OP_POST_TRANSLATE, p.x, p.y); }

  //! Post-translates the used transformation matrix by `[p]` (floating-point).
  //!
  //! \note Post-translation uses a reversed order of matrix multiplication when compared to \ref translate().
  BL_INLINE_NODEBUG BLResult postTranslate(const BLPoint& p) noexcept { return _applyTransformOp(BL_TRANSFORM_OP_POST_TRANSLATE, &p); }

  //! Post-scales the user transformation matrix by `xy` (both X and Y is scaled by `xy`).
  //!
  //! \note Post-scale uses a reversed order of matrix multiplication when compared to \ref scale().
  BL_INLINE_NODEBUG BLResult postScale(double xy) noexcept { return _applyTransformOpV(BL_TRANSFORM_OP_POST_SCALE, xy, xy); }

  //! Post-scales the user transformation matrix by `[x, y]`.
  //!
  //! \note Post-scale uses a reversed order of matrix multiplication when compared to \ref scale().
  BL_INLINE_NODEBUG BLResult postScale(double x, double y) noexcept { return _applyTransformOpV(BL_TRANSFORM_OP_POST_SCALE, x, y); }

  //! Post-scales the user transformation matrix by `[p]` (integer).
  //!
  //! \note Post-scale uses a reversed order of matrix multiplication when compared to \ref scale().
  BL_INLINE_NODEBUG BLResult postScale(const BLPointI& p) noexcept { return _applyTransformOpV(BL_TRANSFORM_OP_POST_SCALE, p.x, p.y); }

  //! Post-scales the user transformation matrix by `[p]` (floating-point).
  //!
  //! \note Post-scale uses a reversed order of matrix multiplication when compared to \ref scale().
  BL_INLINE_NODEBUG BLResult postScale(const BLPoint& p) noexcept { return _applyTransformOp(BL_TRANSFORM_OP_POST_SCALE, &p); }

  //! Skews the user transformation matrix by `[x, y]`.
  //!
  //! \note Post-skew uses a reversed order of matrix multiplication when compared to \ref skew().
  BL_INLINE_NODEBUG BLResult postSkew(double x, double y) noexcept { return _applyTransformOpV(BL_TRANSFORM_OP_POST_SKEW, x, y); }

  //! Skews the user transformation matrix by `[p]` (floating-point).
  //!
  //! \note Post-skew uses a reversed order of matrix multiplication when compared to \ref skew().
  BL_INLINE_NODEBUG BLResult postSkew(const BLPoint& p) noexcept { return _applyTransformOp(BL_TRANSFORM_OP_POST_SKEW, &p); }

  //! Rotates the user transformation matrix by `angle`.
  //!
  //! \note Post-rotation uses a reversed order of matrix multiplication when compared to \ref rotate().
  BL_INLINE_NODEBUG BLResult postRotate(double angle) noexcept { return _applyTransformOp(BL_TRANSFORM_OP_POST_ROTATE, &angle); }

  //! Rotates the user transformation matrix at `[x, y]` by `angle`.
  //!
  //! \note Post-rotation uses a reversed order of matrix multiplication when compared to \ref rotate().
  BL_INLINE_NODEBUG BLResult postRotate(double angle, double x, double y) noexcept { return _applyTransformOpV(BL_TRANSFORM_OP_POST_ROTATE_PT, angle, x, y); }

  //! Rotates the user transformation matrix at `origin` (integer) by `angle`.
  //!
  //! \note Post-rotation uses a reversed order of matrix multiplication when compared to \ref rotate().
  BL_INLINE_NODEBUG BLResult postRotate(double angle, const BLPoint& origin) noexcept { return _applyTransformOpV(BL_TRANSFORM_OP_POST_ROTATE_PT, angle, origin.x, origin.y); }

  //! Rotates the user transformation matrix at `origin` (floating-point) by `angle`.
  //!
  //! \note Post-rotation uses a reversed order of matrix multiplication when compared to \ref rotate().
  BL_INLINE_NODEBUG BLResult postRotate(double angle, const BLPointI& origin) noexcept { return _applyTransformOpV(BL_TRANSFORM_OP_POST_ROTATE_PT, angle, origin.x, origin.y); }

  //! Transforms the user transformation matrix by `transform`.
  //!
  //! \note Post-transform uses a reversed order of matrix multiplication when compared to \ref applyTransform().
  BL_INLINE_NODEBUG BLResult postTransform(const BLMatrix2D& transform) noexcept { return _applyTransformOp(BL_TRANSFORM_OP_POST_TRANSFORM, &transform); }

  //! Stores the result of combining the current `MetaTransform` and `UserTransform` to `MetaTransform` and resets
  //! `UserTransform` to identity as shown below:
  //!
  //! ```
  //! MetaTransform = MetaTransform x UserTransform
  //! UserTransform = Identity
  //! ```
  //!
  //! Please note that this operation is irreversible. The only way to restore a meta-matrix is to \ref save() the
  //! rendering context state, then to use \ref userToMeta(), and then restored by \ref restore() when needed.
  BL_INLINE_NODEBUG BLResult userToMeta() noexcept {
    BL_CONTEXT_CALL_RETURN(userToMeta, impl);
  }

  //! \}

  //! \name Rendering Hints
  //! \{

  //! Returns rendering context hints.
  BL_NODISCARD
  BL_INLINE_NODEBUG const BLContextHints& hints() const noexcept {
    return BL_CONTEXT_IMPL()->state->hints;
  }

  //! Sets the given rendering hint `hintType` to `value`.
  BL_INLINE_NODEBUG BLResult setHint(BLContextHint hintType, uint32_t value) noexcept {
    BL_CONTEXT_CALL_RETURN(setHint, impl, hintType, value);
  }

  //! Sets all rendering hints of this context to `hints`.
  BL_INLINE_NODEBUG BLResult setHints(const BLContextHints& hints) noexcept {
    BL_CONTEXT_CALL_RETURN(setHints, impl, &hints);
  }

  //! Returns the rendering quality hint.
  //!
  //! \note This is the same as calling \ref hints() and extracting the rendering quality from it.
  BL_NODISCARD
  BL_INLINE_NODEBUG BLRenderingQuality renderingQuality() const noexcept {
    return BLRenderingQuality(hints().renderingQuality);
  }

  //! Sets rendering quality hint to `value`.
  BL_INLINE_NODEBUG BLResult setRenderingQuality(BLRenderingQuality value) noexcept {
    return setHint(BL_CONTEXT_HINT_RENDERING_QUALITY, uint32_t(value));
  }

  //! Returns the gradient quality hint.
  //!
  //! \note This is the same as calling \ref hints() and extracting the gradient quality from it.
  BL_NODISCARD
  BL_INLINE_NODEBUG BLGradientQuality gradientQuality() const noexcept {
    return BLGradientQuality(hints().gradientQuality);
  }

  //! Sets gradient quality hint to `value`.
  BL_INLINE_NODEBUG BLResult setGradientQuality(BLGradientQuality value) noexcept {
    return setHint(BL_CONTEXT_HINT_GRADIENT_QUALITY, uint32_t(value));
  }

  //! Returns the pattern quality hint.
  //!
  //! \note This is the same as calling \ref hints() and extracting the pattern quality from it.
  BL_NODISCARD
  BL_INLINE_NODEBUG BLPatternQuality patternQuality() const noexcept {
    return BLPatternQuality(hints().patternQuality);
  }

  //! Sets pattern quality hint to `value`.
  BL_INLINE_NODEBUG BLResult setPatternQuality(BLPatternQuality value) noexcept {
    return setHint(BL_CONTEXT_HINT_PATTERN_QUALITY, uint32_t(value));
  }

  //! \}

  //! \name Approximation Options
  //! \{

  //! Returns approximation options.
  BL_NODISCARD
  BL_INLINE_NODEBUG const BLApproximationOptions& approximationOptions() const noexcept {
    return BL_CONTEXT_IMPL()->state->approximationOptions;
  }

  //! Sets approximation options to `options`.
  BL_INLINE_NODEBUG BLResult setApproximationOptions(const BLApproximationOptions& options) noexcept {
    BL_CONTEXT_CALL_RETURN(setApproximationOptions, impl, &options);
  }

  //! Returns flatten mode (how curves are flattened).
  BL_NODISCARD
  BL_INLINE_NODEBUG BLFlattenMode flattenMode() const noexcept {
    return BLFlattenMode(BL_CONTEXT_IMPL()->state->approximationOptions.flattenMode);
  }

  //! Sets flatten `mode` (how curves are flattened).
  BL_INLINE_NODEBUG BLResult setFlattenMode(BLFlattenMode mode) noexcept {
    BL_CONTEXT_CALL_RETURN(setFlattenMode, impl, mode);
  }

  //! Returns tolerance used for curve flattening.
  BL_NODISCARD
  BL_INLINE_NODEBUG double flattenTolerance() const noexcept {
    return BL_CONTEXT_IMPL()->state->approximationOptions.flattenTolerance;
  }

  //! Sets tolerance used for curve flattening.
  BL_INLINE_NODEBUG BLResult setFlattenTolerance(double tolerance) noexcept {
    BL_CONTEXT_CALL_RETURN(setFlattenTolerance, impl, tolerance);
  }

  //! \}

  //! \name Composition Options
  //! \{

  //! Returns a composition operator.
  BL_NODISCARD
  BL_INLINE_NODEBUG BLCompOp compOp() const noexcept { return BLCompOp(BL_CONTEXT_IMPL()->state->compOp); }

  //! Sets the composition operator to `compOp`, see \ref BLCompOp.
  //!
  //! The composition operator is part of the rendering context state and is subject to \ref save() and
  //! \ref restore(). The default composition operator is \ref BL_COMP_OP_SRC_OVER, which would be returned
  //! immediately after the rendering context is created.
  BL_INLINE_NODEBUG BLResult setCompOp(BLCompOp compOp) noexcept { BL_CONTEXT_CALL_RETURN(setCompOp, impl, compOp); }

  //! Returns a global alpha value.
  BL_NODISCARD
  BL_INLINE_NODEBUG double globalAlpha() const noexcept { return BL_CONTEXT_IMPL()->state->globalAlpha; }

  //! Sets the global alpha value.
  //!
  //! The global alpha value is part of the rendering context state and is subject to \ref save() and
  //! \ref restore(). The default value is `1.0`, which would be returned immediately after the rendering
  //! context is created.
  BL_INLINE_NODEBUG BLResult setGlobalAlpha(double alpha) noexcept { BL_CONTEXT_CALL_RETURN(setGlobalAlpha, impl, alpha); }

  //! \}

  //! \cond INTERNAL
  //! \name Style Options (Internal)
  //! \{

private:

  BL_INLINE_NODEBUG BLResult _setStyleInternal(BLContextStyleSlot slot, const BLRgba& rgba) noexcept {
    BL_CONTEXT_CALL_RETURN(setStyleRgba, impl, slot, &rgba);
  }

  BL_INLINE_NODEBUG BLResult _setStyleInternal(BLContextStyleSlot slot, const BLRgba32& rgba32) noexcept {
    BL_CONTEXT_CALL_RETURN(setStyleRgba32, impl, slot, rgba32.value);
  }

  BL_INLINE_NODEBUG BLResult _setStyleInternal(BLContextStyleSlot slot, const BLRgba64& rgba64) noexcept {
    BL_CONTEXT_CALL_RETURN(setStyleRgba64, impl, slot, rgba64.value);
  }

  BL_INLINE_NODEBUG BLResult _setStyleInternal(BLContextStyleSlot slot, const BLVarCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(setStyle, impl, slot, &style, BL_CONTEXT_STYLE_TRANSFORM_MODE_USER);
  }

  BL_INLINE_NODEBUG BLResult _setStyleInternal(BLContextStyleSlot slot, const BLPatternCore& pattern) noexcept {
    BL_CONTEXT_CALL_RETURN(setStyle, impl, slot, &pattern, BL_CONTEXT_STYLE_TRANSFORM_MODE_USER);
  }

  BL_INLINE_NODEBUG BLResult _setStyleInternal(BLContextStyleSlot slot, const BLGradientCore& gradient) noexcept {
    BL_CONTEXT_CALL_RETURN(setStyle, impl, slot, &gradient, BL_CONTEXT_STYLE_TRANSFORM_MODE_USER);
  }

  BL_INLINE_NODEBUG BLResult _setStyleWithMode(BLContextStyleSlot slot, const BLVarCore& style, BLContextStyleTransformMode transformMode) noexcept {
    BL_CONTEXT_CALL_RETURN(setStyle, impl, slot, &style, transformMode);
  }

  BL_INLINE_NODEBUG BLResult _setStyleWithMode(BLContextStyleSlot slot, const BLPatternCore& pattern, BLContextStyleTransformMode transformMode) noexcept {
    BL_CONTEXT_CALL_RETURN(setStyle, impl, slot, &pattern, transformMode);
  }

  BL_INLINE_NODEBUG BLResult _setStyleWithMode(BLContextStyleSlot slot, const BLGradientCore& gradient, BLContextStyleTransformMode transformMode) noexcept {
    BL_CONTEXT_CALL_RETURN(setStyle, impl, slot, &gradient, transformMode);
  }

public:

  //! \}
  //! \endcond

  //! \name Style Options
  //! \{

  //! Returns the current style type associated with the given style `slot`.
  BL_NODISCARD
  BL_INLINE_NODEBUG BLObjectType styleType(BLContextStyleSlot slot) const noexcept {
    return uint32_t(slot) <= BL_CONTEXT_STYLE_SLOT_MAX_VALUE ? BLObjectType(BL_CONTEXT_IMPL()->state->styleType[slot]) : BL_OBJECT_TYPE_NULL;
  }

  //! Reads a style state associated with the given style `slot` and writes it into `styleOut`.
  //!
  //! \note This function returns the original style passed to the rendering context with its original transformation
  //! matrix if it's not a solid color. Consider using \ref getTransformedStyle() if you want to get a style with the
  //! transformation matrix that the rendering context actually uses to render it.
  BL_INLINE_NODEBUG BLResult getStyle(BLContextStyleSlot slot, BLVarCore& styleOut) const noexcept {
    BL_CONTEXT_CALL_RETURN(getStyle, impl, slot, false, &styleOut);
  }

  //! Reads a style state associated with the given style `slot` and writes it into `styleOut`.
  //!
  //! The retrieved style uses a transformation matrix that is a combination of style transformation matrix and
  //! the rendering context matrix at a time \ref setStyle() was called).
  BL_INLINE_NODEBUG BLResult getTransformedStyle(BLContextStyleSlot slot, BLVarCore& styleOut) const noexcept {
    BL_CONTEXT_CALL_RETURN(getStyle, impl, slot, true, &styleOut);
  }

  //! Sets `style` to be used with the given style `slot` operation.
  //!
  //! \note The `style` argument could be \ref BLRgba, \ref BLRgba32, \ref BLRgba64, \ref BLGradient, \ref BLPattern, and \ref BLVar.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult setStyle(BLContextStyleSlot slot, const StyleT& style) noexcept {
    return _setStyleInternal(slot, style);
  }

  //! Sets `style` to be used with the given style `slot` operation and applied `transformMode`.
  //!
  //! This is a convenience function that allows to control how the given `style` is transformed. By default, if `transformMode` is not
  //! provided, the rendering context combines the style transformation matrix with user transformation matrix, which is compatible with
  //! how it transforms geometry. However, if that' undesired, a `transformMode` can override the default operation.
  //!
  //! \note The `style` argument could be \ref BLGradient, \ref BLPattern, and \ref BLVar.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult setStyle(BLContextStyleSlot slot, const StyleT& style, BLContextStyleTransformMode transformMode) noexcept {
    return _setStyleWithMode(slot, style, transformMode);
  }

  //! Sets the given style `slot` to NULL, which disables it.
  //!
  //! Styles set to NULL would reject all rendering operations that would otherwise use that style.
  BL_INLINE_NODEBUG BLResult disableStyle(BLContextStyleSlot slot) noexcept {
    BL_CONTEXT_CALL_RETURN(disableStyle, impl, slot);
  }

  //! Returns fill or alpha value associated with the given style `slot`.
  //!
  //! The function behaves like `fillAlpha()` or `strokeAlpha()` depending on style `slot`, see \ref BLContextStyleSlot.
  BL_NODISCARD
  BL_INLINE_NODEBUG double styleAlpha(BLContextStyleSlot slot) const noexcept {
    return slot <= BL_CONTEXT_STYLE_SLOT_MAX_VALUE ? BL_CONTEXT_IMPL()->state->styleAlpha[slot] : 0.0;
  }

  //! Set fill or stroke `alpha` value associated with the given style `slot`.
  //!
  //! The function behaves like `setFillAlpha()` or `setStrokeAlpha()` depending on style `slot`, see \ref BLContextStyleSlot.
  BL_INLINE_NODEBUG BLResult setStyleAlpha(BLContextStyleSlot slot, double alpha) noexcept {
    BL_CONTEXT_CALL_RETURN(setStyleAlpha, impl, slot, alpha);
  }

  //! Swaps fill and stroke styles, see \ref BLContextStyleSwapMode for options.
  BL_INLINE_NODEBUG BLResult swapStyles(BLContextStyleSwapMode mode) noexcept {
    BL_CONTEXT_CALL_RETURN(swapStyles, impl, mode);
  }

  //! \}

  //! \name Fill Style & Options
  //! \{


  //! Returns the current fill style type.
  BL_NODISCARD
  BL_INLINE_NODEBUG BLObjectType fillStyleType() const noexcept {
    return BLObjectType(BL_CONTEXT_IMPL()->state->styleType[BL_CONTEXT_STYLE_SLOT_FILL]);
  }

  //! Reads a fill style state and writes it into `styleOut` variant.
  //!
  //! \note This function returns the original style passed to the rendering context with its original transformation
  //! matrix if it's not a solid color. Consider using \ref getTransformedFillStyle() if you want to get a fill style
  //! with the transformation matrix that the rendering context actually uses to render it.
  BL_INLINE_NODEBUG BLResult getFillStyle(BLVarCore& out) const noexcept {
    BL_CONTEXT_CALL_RETURN(getStyle, impl, BL_CONTEXT_STYLE_SLOT_FILL, false, &out);
  }

  //! Reads a fill style state and writes it into `styleOut` variant.
  BL_INLINE_NODEBUG BLResult getTransformedFillStyle(BLVarCore& out) const noexcept {
    BL_CONTEXT_CALL_RETURN(getStyle, impl, BL_CONTEXT_STYLE_SLOT_FILL, true, &out);
  }

  //! Sets fill style.
  //!
  //! \note The `style` argument could be \ref BLRgba, \ref BLRgba32, \ref BLRgba64, \ref BLGradient, \ref BLPattern, and \ref BLVar.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult setFillStyle(const StyleT& style) noexcept {
    return _setStyleInternal(BL_CONTEXT_STYLE_SLOT_FILL, style);
  }

  //! Sets fill style to `style`.
  //!
  //! \note The `style` argument could be \ref BLGradient, \ref BLPattern, and \ref BLVar.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult setFillStyle(const StyleT& style, BLContextStyleTransformMode transformMode) noexcept {
    return _setStyleWithMode(BL_CONTEXT_STYLE_SLOT_FILL, style, transformMode);
  }

  //! Sets fill style to NULL, which disables it.
  BL_INLINE_NODEBUG BLResult disableFillStyle() noexcept {
    BL_CONTEXT_CALL_RETURN(disableStyle, impl, BL_CONTEXT_STYLE_SLOT_FILL);
  }

  //! Returns fill alpha value.
  BL_NODISCARD
  BL_INLINE_NODEBUG double fillAlpha() const noexcept {
    return BL_CONTEXT_IMPL()->state->styleAlpha[BL_CONTEXT_STYLE_SLOT_FILL];
  }

  //! Sets fill `alpha` value.
  BL_INLINE_NODEBUG BLResult setFillAlpha(double alpha) noexcept {
    BL_CONTEXT_CALL_RETURN(setStyleAlpha, impl, BL_CONTEXT_STYLE_SLOT_FILL, alpha);
  }

  //! Returns fill-rule, see \ref BLFillRule.
  BL_NODISCARD
  BL_INLINE_NODEBUG BLFillRule fillRule() const noexcept {
    return BLFillRule(BL_CONTEXT_IMPL()->state->fillRule);
  }

  //! Sets fill-rule, see \ref BLFillRule.
  BL_INLINE_NODEBUG BLResult setFillRule(BLFillRule fillRule) noexcept {
    BL_CONTEXT_CALL_RETURN(setFillRule, impl, fillRule);
  }

  //! \}

  //! \name Stroke Style & Options
  //! \{

  //! Returns the current stroke style type.
  BL_NODISCARD
  BL_INLINE_NODEBUG BLObjectType strokeStyleType() const noexcept {
    return BLObjectType(BL_CONTEXT_IMPL()->state->styleType[BL_CONTEXT_STYLE_SLOT_STROKE]);
  }

  //! Reads a stroke style state and writes it into `styleOut` variant.
  //!
  //! \note This function returns the original style passed to the rendering context with its original transformation
  //! matrix if it's not a solid color. Consider using \ref getTransformedStrokeStyle() if you want to get a stroke
  //! style with the transformation matrix that the rendering context actually uses to render it.
  BL_INLINE_NODEBUG BLResult getStrokeStyle(BLVarCore& out) const noexcept {
    BL_CONTEXT_CALL_RETURN(getStyle, impl, BL_CONTEXT_STYLE_SLOT_STROKE, false, &out);
  }

  //! Reads a stroke style state and writes it into `styleOut` variant.
  BL_INLINE_NODEBUG BLResult getTransformedStrokeStyle(BLVarCore& out) const noexcept {
    BL_CONTEXT_CALL_RETURN(getStyle, impl, BL_CONTEXT_STYLE_SLOT_STROKE, true, &out);
  }

  //! Sets stroke style.
  //!
  //! \note The `style` argument could be \ref BLRgba, \ref BLRgba32, \ref BLRgba64, \ref BLGradient, \ref BLPattern, and \ref BLVar.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult setStrokeStyle(const StyleT& style) noexcept {
    return _setStyleInternal(BL_CONTEXT_STYLE_SLOT_STROKE, style);
  }

  //! Sets fill style to `style`.
  //!
  //! \note The `style` argument could be \ref BLGradient, \ref BLPattern, and \ref BLVar.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult setStrokeStyle(const StyleT& style, BLContextStyleTransformMode transformMode) noexcept {
    return _setStyleWithMode(BL_CONTEXT_STYLE_SLOT_STROKE, style, transformMode);
  }

  //! Sets stroke style to NULL, which disables it.
  BL_INLINE_NODEBUG BLResult disableStrokeStyle() noexcept {
    BL_CONTEXT_CALL_RETURN(disableStyle, impl, BL_CONTEXT_STYLE_SLOT_STROKE);
  }

  //! Returns stroke width.
  BL_NODISCARD
  BL_INLINE_NODEBUG double strokeWidth() const noexcept {
    return BL_CONTEXT_IMPL()->state->strokeOptions.width;
  }

  //! Returns stroke miter-limit.
  BL_NODISCARD
  BL_INLINE_NODEBUG double strokeMiterLimit() const noexcept {
    return BL_CONTEXT_IMPL()->state->strokeOptions.miterLimit;
  }

  //! Returns stroke join, see \ref BLStrokeJoin.
  BL_NODISCARD
  BL_INLINE_NODEBUG BLStrokeJoin strokeJoin() const noexcept {
    return BLStrokeJoin(BL_CONTEXT_IMPL()->state->strokeOptions.join);
  }

  //! Returns stroke start-cap, see \ref BLStrokeCap.
  BL_NODISCARD
  BL_INLINE_NODEBUG BLStrokeCap strokeStartCap() const noexcept {
    return BLStrokeCap(BL_CONTEXT_IMPL()->state->strokeOptions.startCap);
  }

  //! Returns stroke end-cap, see \ref BLStrokeCap.
  BL_NODISCARD
  BL_INLINE_NODEBUG BLStrokeCap strokeEndCap() const noexcept {
    return BLStrokeCap(BL_CONTEXT_IMPL()->state->strokeOptions.endCap);
  }

  //! Returns stroke transform order, see \ref BLStrokeTransformOrder.
  BL_NODISCARD
  BL_INLINE_NODEBUG BLStrokeTransformOrder strokeTransformOrder() const noexcept {
    return BLStrokeTransformOrder(BL_CONTEXT_IMPL()->state->strokeOptions.transformOrder);
  }

  //! Returns stroke dash-offset.
  BL_NODISCARD
  BL_INLINE_NODEBUG double strokeDashOffset() const noexcept {
    return BL_CONTEXT_IMPL()->state->strokeOptions.dashOffset;
  }

  //! Returns stroke dash-array.
  BL_NODISCARD
  BL_INLINE_NODEBUG const BLArray<double>& strokeDashArray() const noexcept {
    return BL_CONTEXT_IMPL()->state->strokeOptions.dashArray;
  }

  //! Returns stroke options as a reference to \ref BLStrokeOptions.
  BL_NODISCARD
  BL_INLINE_NODEBUG const BLStrokeOptions& strokeOptions() const noexcept {
    return BL_CONTEXT_IMPL()->state->strokeOptions.dcast();
  }

  //! Sets stroke width to `width`.
  BL_INLINE_NODEBUG BLResult setStrokeWidth(double width) noexcept {
    BL_CONTEXT_CALL_RETURN(setStrokeWidth, impl, width);
  }

  //! Sets miter limit to `miterLimit`.
  BL_INLINE_NODEBUG BLResult setStrokeMiterLimit(double miterLimit) noexcept {
    BL_CONTEXT_CALL_RETURN(setStrokeMiterLimit, impl, miterLimit);
  }

  //! Sets stroke join to `strokeJoin`, see \ref BLStrokeJoin.
  BL_INLINE_NODEBUG BLResult setStrokeJoin(BLStrokeJoin strokeJoin) noexcept {
    BL_CONTEXT_CALL_RETURN(setStrokeJoin, impl, strokeJoin);
  }

  //! Sets stroke cap of the specified `type` to `strokeCap`, see \ref BLStrokeCap.
  BL_INLINE_NODEBUG BLResult setStrokeCap(BLStrokeCapPosition position, BLStrokeCap strokeCap) noexcept {
    BL_CONTEXT_CALL_RETURN(setStrokeCap, impl, position, strokeCap);
  }

  //! Sets stroke start cap to `strokeCap`, see \ref BLStrokeCap.
  BL_INLINE_NODEBUG BLResult setStrokeStartCap(BLStrokeCap strokeCap) noexcept {
    return setStrokeCap(BL_STROKE_CAP_POSITION_START, strokeCap);
  }

  //! Sets stroke end cap to `strokeCap`, see \ref BLStrokeCap.
  BL_INLINE_NODEBUG BLResult setStrokeEndCap(BLStrokeCap strokeCap) noexcept {
    return setStrokeCap(BL_STROKE_CAP_POSITION_END, strokeCap);
  }

  //! Sets all stroke caps to `strokeCap`, see \ref BLStrokeCap.
  BL_INLINE_NODEBUG BLResult setStrokeCaps(BLStrokeCap strokeCap) noexcept {
    BL_CONTEXT_CALL_RETURN(setStrokeCaps, impl, strokeCap);
  }

  //! Sets stroke transformation order to `transformOrder`, see \ref BLStrokeTransformOrder.
  BL_INLINE_NODEBUG BLResult setStrokeTransformOrder(BLStrokeTransformOrder transformOrder) noexcept {
    BL_CONTEXT_CALL_RETURN(setStrokeTransformOrder, impl, transformOrder);
  }

  //! Sets stroke dash-offset to `dashOffset`.
  BL_INLINE_NODEBUG BLResult setStrokeDashOffset(double dashOffset) noexcept {
    BL_CONTEXT_CALL_RETURN(setStrokeDashOffset, impl, dashOffset);
  }

  //! Sets stroke dash-array to `dashArray`.
  BL_INLINE_NODEBUG BLResult setStrokeDashArray(const BLArray<double>& dashArray) noexcept {
    BL_CONTEXT_CALL_RETURN(setStrokeDashArray, impl, &dashArray);
  }

  //! Sets all stroke `options`.
  BL_INLINE_NODEBUG BLResult setStrokeOptions(const BLStrokeOptions& options) noexcept {
    BL_CONTEXT_CALL_RETURN(setStrokeOptions, impl, &options);
  }

  //! Returns stroke alpha value.
  BL_NODISCARD
  BL_INLINE_NODEBUG double strokeAlpha() const noexcept {
    return BL_CONTEXT_IMPL()->state->styleAlpha[BL_CONTEXT_STYLE_SLOT_STROKE];
  }

  //! Sets stroke alpha value to `alpha`.
  BL_INLINE_NODEBUG BLResult setStrokeAlpha(double alpha) noexcept {
    BL_CONTEXT_CALL_RETURN(setStyleAlpha, impl, BL_CONTEXT_STYLE_SLOT_STROKE, alpha);
  }

  //! \}

  //! \name Clip Operations
  //! \{

  //! Restores clipping to the last saved state or to the context default clipping if there is no saved state.
  //!
  //! If there are no saved states then it resets clipping completely to the initial state that was used when
  //! the rendering context was created.
  BL_INLINE_NODEBUG BLResult restoreClipping() noexcept {
    BL_CONTEXT_CALL_RETURN(restoreClipping, impl);
  }

  BL_INLINE_NODEBUG BLResult clipToRect(const BLRectI& rect) noexcept {
    BL_CONTEXT_CALL_RETURN(clipToRectI, impl, &rect);
  }

  BL_INLINE_NODEBUG BLResult clipToRect(const BLRect& rect) noexcept {
    BL_CONTEXT_CALL_RETURN(clipToRectD, impl, &rect);
  }

  BL_INLINE_NODEBUG BLResult clipToRect(double x, double y, double w, double h) noexcept {
    return clipToRect(BLRect(x, y, w, h));
  }

  //! \}

  //! \name Clear Geometry Operations
  //! \{

  //! Clear everything to a transparent black, which is the same operation as temporarily setting the composition
  //! operator to \ref BL_COMP_OP_CLEAR and then filling everything by `fillAll()`.
  //!
  //! \note If the target surface doesn't have alpha, but has X component, like \ref BL_FORMAT_XRGB32, the `X`
  //! component would be set to `1.0`, which would translate to `0xFF` in case of \ref BL_FORMAT_XRGB32.
  BL_INLINE_NODEBUG BLResult clearAll() noexcept {
    BL_CONTEXT_CALL_RETURN(clearAll, impl);
  }

  //! Clears a rectangle `rect` (integer coordinates) to a transparent black, which is the same operation as
  //! temporarily setting the composition operator to \ref BL_COMP_OP_CLEAR and then calling `fillRect(rect)`.
  //!
  //! \note If the target surface doesn't have alpha, but has X component, like \ref BL_FORMAT_XRGB32, the `X`
  //! component would be set to `1.0`, which would translate to `0xFF` in case of \ref BL_FORMAT_XRGB32.
  BL_INLINE_NODEBUG BLResult clearRect(const BLRectI& rect) noexcept {
    BL_CONTEXT_CALL_RETURN(clearRectI, impl, &rect);
  }

  //! Clears a rectangle `rect` (floating-point coordinates) to a transparent black, which is the same operation
  //! as temporarily setting the composition operator to \ref BL_COMP_OP_CLEAR and then calling `fillRect(rect)`.
  //!
  //! \note If the target surface doesn't have alpha, but has X component, like \ref BL_FORMAT_XRGB32, the `X`
  //! component would be set to `1.0`, which would translate to `0xFF` in case of \ref BL_FORMAT_XRGB32.
  BL_INLINE_NODEBUG BLResult clearRect(const BLRect& rect) noexcept {
    BL_CONTEXT_CALL_RETURN(clearRectD, impl, &rect);
  }

  //! Clears a rectangle `[x, y, w, h]` (floating-point coordinates) to a transparent black, which is the same
  //! operation as temporarily setting the composition operator to \ref BL_COMP_OP_CLEAR and then calling
  //! `fillRect(x, y, w, h)`.
  //!
  //! \note If the target surface doesn't have alpha, but has X component, like \ref BL_FORMAT_XRGB32, the `X`
  //! component would be set to `1.0`, which would translate to `0xFF` in case of \ref BL_FORMAT_XRGB32.
  BL_INLINE_NODEBUG BLResult clearRect(double x, double y, double w, double h) noexcept {
    return clearRect(BLRect(x, y, w, h));
  }

  //! \}

  //! \cond INTERNAL
  //! \name Fill Wrappers (Internal)
  //! \{

private:
  BL_INLINE_NODEBUG BLResult _fillAll(const BLRgba& rgba) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba);
    BL_CONTEXT_CALL_RETURN(fillAllExt, impl, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillAll(const BLRgba32& rgba32) noexcept {
    BL_CONTEXT_CALL_RETURN(fillAllRgba32, impl, rgba32.value);
  }

  BL_INLINE_NODEBUG BLResult _fillAll(const BLRgba64& rgba64) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba64);
    BL_CONTEXT_CALL_RETURN(fillAllExt, impl, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillAll(const BLVarCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillAllExt, impl, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillAll(const BLPatternCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillAllExt, impl, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillAll(const BLGradientCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillAllExt, impl, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillRectI(const BLRectI& rect, const BLRgba& rgba) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba);
    BL_CONTEXT_CALL_RETURN(fillRectIExt, impl, &rect, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillRectI(const BLRectI& rect, const BLRgba32& rgba32) noexcept {
    BL_CONTEXT_CALL_RETURN(fillRectIRgba32, impl, &rect, rgba32.value);
  }

  BL_INLINE_NODEBUG BLResult _fillRectI(const BLRectI& rect, const BLRgba64& rgba64) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba64);
    BL_CONTEXT_CALL_RETURN(fillRectIExt, impl, &rect, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillRectI(const BLRectI& rect, const BLVarCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillRectIExt, impl, &rect, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillRectI(const BLRectI& rect, const BLPatternCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillRectIExt, impl, &rect, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillRectI(const BLRectI& rect, const BLGradientCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillRectIExt, impl, &rect, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillRectD(const BLRect& rect, const BLRgba& rgba) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba);
    BL_CONTEXT_CALL_RETURN(fillRectDExt, impl, &rect, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillRectD(const BLRect& rect, const BLRgba32& rgba32) noexcept {
    BL_CONTEXT_CALL_RETURN(fillRectDRgba32, impl, &rect, rgba32.value);
  }

  BL_INLINE_NODEBUG BLResult _fillRectD(const BLRect& rect, const BLRgba64& rgba64) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba64);
    BL_CONTEXT_CALL_RETURN(fillRectDExt, impl, &rect, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillRectD(const BLRect& rect, const BLVarCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillRectDExt, impl, &rect, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillRectD(const BLRect& rect, const BLPatternCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillRectDExt, impl, &rect, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillRectD(const BLRect& rect, const BLGradientCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillRectDExt, impl, &rect, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillGeometryOp(BLGeometryType type, const void* data) noexcept {
    BL_CONTEXT_CALL_RETURN(fillGeometry, impl, type, data);
  }

  BL_INLINE_NODEBUG BLResult _fillGeometryOp(BLGeometryType type, const void* data, const BLRgba& rgba) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba);
    BL_CONTEXT_CALL_RETURN(fillGeometryExt, impl, type, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillGeometryOp(BLGeometryType type, const void* data, const BLRgba32& rgba32) noexcept {
    BL_CONTEXT_CALL_RETURN(fillGeometryRgba32, impl, type, data, rgba32.value);
  }

  BL_INLINE_NODEBUG BLResult _fillGeometryOp(BLGeometryType type, const void* data, const BLRgba64& rgba64) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba64);
    BL_CONTEXT_CALL_RETURN(fillGeometryExt, impl, type, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillGeometryOp(BLGeometryType type, const void* data, const BLVarCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillGeometryExt, impl, type, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillGeometryOp(BLGeometryType type, const void* data, const BLPatternCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillGeometryExt, impl, type, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillGeometryOp(BLGeometryType type, const void* data, const BLGradientCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillGeometryExt, impl, type, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillPathD(const BLPoint& origin, const BLPathCore& path, const BLRgba& rgba) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba);
    BL_CONTEXT_CALL_RETURN(fillPathDExt, impl, &origin, &path, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillPathD(const BLPoint& origin, const BLPathCore& path, const BLRgba32& rgba32) noexcept {
    BL_CONTEXT_CALL_RETURN(fillPathDRgba32, impl, &origin, &path, rgba32.value);
  }

  BL_INLINE_NODEBUG BLResult _fillPathD(const BLPoint& origin, const BLPathCore& path, const BLRgba64& rgba64) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba64);
    BL_CONTEXT_CALL_RETURN(fillPathDExt, impl, &origin, &path, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillPathD(const BLPoint& origin, const BLPathCore& path, const BLVarCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillPathDExt, impl, &origin, &path, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillPathD(const BLPoint& origin, const BLPathCore& path, const BLPatternCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillPathDExt, impl, &origin, &path, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillPathD(const BLPoint& origin, const BLPathCore& path, const BLGradientCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillPathDExt, impl, &origin, &path, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillTextOpI(const BLPointI& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data) noexcept {
    BL_CONTEXT_CALL_RETURN(fillTextOpI, impl, &origin, &font, op, data);
  }

  BL_INLINE_NODEBUG BLResult _fillTextOpI(const BLPointI& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLRgba& rgba) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba);
    BL_CONTEXT_CALL_RETURN(fillTextOpIExt, impl, &origin, &font, op, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillTextOpI(const BLPointI& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLRgba32& rgba32) noexcept {
    BL_CONTEXT_CALL_RETURN(fillTextOpIRgba32, impl, &origin, &font, op, data, rgba32.value);
  }

  BL_INLINE_NODEBUG BLResult _fillTextOpI(const BLPointI& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLRgba64& rgba64) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba64);
    BL_CONTEXT_CALL_RETURN(fillTextOpIExt, impl, &origin, &font, op, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillTextOpI(const BLPointI& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLVarCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillTextOpIExt, impl, &origin, &font, op, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillTextOpI(const BLPointI& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLPatternCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillTextOpIExt, impl, &origin, &font, op, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillTextOpI(const BLPointI& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLGradientCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillTextOpIExt, impl, &origin, &font, op, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillTextOpD(const BLPoint& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data) noexcept {
    BL_CONTEXT_CALL_RETURN(fillTextOpD, impl, &origin, &font, op, data);
  }

  BL_INLINE_NODEBUG BLResult _fillTextOpD(const BLPoint& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLRgba& rgba) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba);
    BL_CONTEXT_CALL_RETURN(fillTextOpDExt, impl, &origin, &font, op, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillTextOpD(const BLPoint& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLRgba32& rgba32) noexcept {
    BL_CONTEXT_CALL_RETURN(fillTextOpDRgba32, impl, &origin, &font, op, data, rgba32.value);
  }

  BL_INLINE_NODEBUG BLResult _fillTextOpD(const BLPoint& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLRgba64& rgba64) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba64);
    BL_CONTEXT_CALL_RETURN(fillTextOpDExt, impl, &origin, &font, op, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillTextOpD(const BLPoint& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLVarCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillTextOpDExt, impl, &origin, &font, op, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillTextOpD(const BLPoint& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLPatternCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillTextOpDExt, impl, &origin, &font, op, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillTextOpD(const BLPoint& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLGradientCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillTextOpDExt, impl, &origin, &font, op, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillMaskI(const BLPointI& origin, const BLImage& mask, const BLRectI* maskArea) noexcept {
    BL_CONTEXT_CALL_RETURN(fillMaskI, impl, &origin, &mask, maskArea);
  }

  BL_INLINE_NODEBUG BLResult _fillMaskI(const BLPointI& origin, const BLImage& mask, const BLRectI* maskArea, const BLRgba& rgba) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba);
    BL_CONTEXT_CALL_RETURN(fillMaskIExt, impl, &origin, &mask, maskArea, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillMaskI(const BLPointI& origin, const BLImage& mask, const BLRectI* maskArea, const BLRgba32& rgba32) noexcept {
    BL_CONTEXT_CALL_RETURN(fillMaskIRgba32, impl, &origin, &mask, maskArea, rgba32.value);
  }

  BL_INLINE_NODEBUG BLResult _fillMaskI(const BLPointI& origin, const BLImage& mask, const BLRectI* maskArea, const BLRgba64& rgba64) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba64);
    BL_CONTEXT_CALL_RETURN(fillMaskIExt, impl, &origin, &mask, maskArea, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillMaskI(const BLPointI& origin, const BLImage& mask, const BLRectI* maskArea, const BLVarCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillMaskIExt, impl, &origin, &mask, maskArea, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillMaskI(const BLPointI& origin, const BLImage& mask, const BLRectI* maskArea, const BLPatternCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillMaskIExt, impl, &origin, &mask, maskArea, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillMaskI(const BLPointI& origin, const BLImage& mask, const BLRectI* maskArea, const BLGradientCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillMaskIExt, impl, &origin, &mask, maskArea, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillMaskD(const BLPoint& origin, const BLImage& mask, const BLRectI* maskArea) noexcept {
    BL_CONTEXT_CALL_RETURN(fillMaskD, impl, &origin, &mask, maskArea);
  }

  BL_INLINE_NODEBUG BLResult _fillMaskD(const BLPoint& origin, const BLImage& mask, const BLRectI* maskArea, const BLRgba& rgba) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba);
    BL_CONTEXT_CALL_RETURN(fillMaskDExt, impl, &origin, &mask, maskArea, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillMaskD(const BLPoint& origin, const BLImage& mask, const BLRectI* maskArea, const BLRgba32& rgba32) noexcept {
    BL_CONTEXT_CALL_RETURN(fillMaskDRgba32, impl, &origin, &mask, maskArea, rgba32.value);
  }

  BL_INLINE_NODEBUG BLResult _fillMaskD(const BLPoint& origin, const BLImage& mask, const BLRectI* maskArea, const BLRgba64& rgba64) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba64);
    BL_CONTEXT_CALL_RETURN(fillMaskDExt, impl, &origin, &mask, maskArea, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillMaskD(const BLPoint& origin, const BLImage& mask, const BLRectI* maskArea, const BLVarCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillMaskDExt, impl, &origin, &mask, maskArea, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillMaskD(const BLPoint& origin, const BLImage& mask, const BLRectI* maskArea, const BLPatternCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillMaskDExt, impl, &origin, &mask, maskArea, &style);
  }

  BL_INLINE_NODEBUG BLResult _fillMaskD(const BLPoint& origin, const BLImage& mask, const BLRectI* maskArea, const BLGradientCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(fillMaskDExt, impl, &origin, &mask, maskArea, &style);
  }

public:

  //! \}
  //! \endcond

  //! \name Fill Geometry Operations
  //! \{

  //! Fills everything non-clipped with the current fill style.
  BL_INLINE_NODEBUG BLResult fillAll() noexcept {
    BL_CONTEXT_CALL_RETURN(fillAll, impl);
  }

  //! Fills everything non-clipped with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillAll(const StyleT& style) noexcept {
    return _fillAll(style);
  }

  //! Fills a `box` (floating point coordinates) with the current fill style.
  //!
  //! \note Box is defined as `[x0, y0, x1, y1]`, if you need `[x, y, w, h]`, use \ref fillRect() instead.
  BL_INLINE_NODEBUG BLResult fillBox(const BLBox& box) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_BOXD, &box);
  }

  //! Fills a `box` (floating point coordinates) with an explicit fill `style`.
  //!
  //! \note Box is defined as `[x0, y0, x1, y1]`, if you need `[x, y, w, h]`, use \ref fillRect() instead.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillBox(const BLBox& box, const StyleT& style) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_BOXD, &box, style);
  }

  //! Fills a `box` (integer coordinates) with the current fill style.
  //!
  //! \note Box is defined as `[x0, y0, x1, y1]`, if you need `[x, y, w, h]`, use \ref fillRect() instead.
  BL_INLINE_NODEBUG BLResult fillBox(const BLBoxI& box) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_BOXI, &box);
  }

  //! Fills a `box` (integer coordinates) with an explicit fill `style`.
  //!
  //! \note Box is defined as `[x0, y0, x1, y1]`, if you need `[x, y, w, h]`, use \ref fillRect() instead.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillBox(const BLBoxI& box, const StyleT& style) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_BOXI, &box, style);
  }

  //! Fills a box `[x0, y0, x1, y1]` (floating point coordinates) with the current fill style.
  //!
  //! \note Box is defined as `[x0, y0, x1, y1]`, if you need `[x, y, w, h]`, use \ref fillRect() instead.
  BL_INLINE_NODEBUG BLResult fillBox(double x0, double y0, double x1, double y1) noexcept {
    return fillBox(BLBox(x0, y0, x1, y1));
  }

  //! Fills a box `[x0, y0, x1, y1]` (floating point coordinates) with an explicit fill `style`.
  //!
  //! \note Box is defined as `[x0, y0, x1, y1]`, if you need `[x, y, w, h]`, use \ref fillRect() instead.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillBox(double x0, double y0, double x1, double y1, const StyleT& style) noexcept {
    return fillBox(BLBox(x0, y0, x1, y1), style);
  }

  //! Fills a rectangle `rect` (integer coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillRect(const BLRectI& rect) noexcept {
    BL_CONTEXT_CALL_RETURN(fillRectI, impl, &rect);
  }

  //! Fills a rectangle `rect` (integer coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillRect(const BLRectI& rect, const StyleT& style) noexcept {
    return _fillRectI(rect, style);
  }

  //! Fills a rectangle `rect` (floating point coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillRect(const BLRect& rect) noexcept {
    BL_CONTEXT_CALL_RETURN(fillRectD, impl, &rect);
  }

  //! Fills a rectangle `rect` (floating point coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillRect(const BLRect& rect, const StyleT& style) noexcept {
    return _fillRectD(rect, style);
  }

  //! Fills a rectangle `[x, y, w, h]` (floating point coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillRect(double x, double y, double w, double h) noexcept {
    return fillRect(BLRect(x, y, w, h));
  }

  //! Fills a rectangle `[x, y, w, h]` (floating point coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillRect(double x, double y, double w, double h, const StyleT& style) noexcept {
    return _fillRectD(BLRect(x, y, w, h), style);
  }

  //! Fills a `circle` (floating point coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillCircle(const BLCircle& circle) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_CIRCLE, &circle);
  }

  //! Fills a `circle` (floating point coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillCircle(const BLCircle& circle, const StyleT& style) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_CIRCLE, &circle, style);
  }

  //! Fills a circle at `[cx, cy]` and radius `r` (floating point coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillCircle(double cx, double cy, double r) noexcept {
    return fillCircle(BLCircle(cx, cy, r));
  }

  //! Fills a circle at `[cx, cy]` and radius `r` (floating point coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillCircle(double cx, double cy, double r, const StyleT& style) noexcept {
    return fillCircle(BLCircle(cx, cy, r), style);
  }

  //! Fills an `ellipse` (floating point coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillEllipse(const BLEllipse& ellipse) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_ELLIPSE, &ellipse);
  }

  //! Fills an `ellipse` (floating point coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillEllipse(const BLEllipse& ellipse, const StyleT& style) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_ELLIPSE, &ellipse, style);
  }

  //! Fills an ellipse at `[cx, cy]` with radius `[rx, ry]` (floating point coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillEllipse(double cx, double cy, double rx, double ry) noexcept {
    return fillEllipse(BLEllipse(cx, cy, rx, ry));
  }

  //! Fills an ellipse at `[cx, cy]` with radius `[rx, ry]` (floating point coordinates) with en explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillEllipse(double cx, double cy, double rx, double ry, const StyleT& style) noexcept {
    return fillEllipse(BLEllipse(cx, cy, rx, ry), style);
  }

  //! Fills a rounded rectangle `rr` (floating point coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillRoundRect(const BLRoundRect& rr) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_ROUND_RECT, &rr);
  }

  //! Fills a rounded rectangle `rr` (floating point coordinates) with en explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillRoundRect(const BLRoundRect& rr, const StyleT& style) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_ROUND_RECT, &rr, style);
  }

  //! Fills a rounded rectangle bounded by `rect` with radius `r` with the current fill style.
  BL_INLINE_NODEBUG BLResult fillRoundRect(const BLRect& rect, double r) noexcept {
    return fillRoundRect(BLRoundRect(rect.x, rect.y, rect.w, rect.h, r));
  }

  //! Fills a rounded rectangle bounded by `rect` with radius `[rx, ry]` with the current fill style.
  BL_INLINE_NODEBUG BLResult fillRoundRect(const BLRect& rect, double rx, double ry) noexcept {
    return fillRoundRect(BLRoundRect(rect.x, rect.y, rect.w, rect.h, rx, ry));
  }

  //! Fills a rounded rectangle bounded by `rect` with radius `[rx, ry]` with en explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillRoundRect(const BLRect& rect, double rx, double ry, const StyleT& style) noexcept {
    return fillRoundRect(BLRoundRect(rect.x, rect.y, rect.w, rect.h, rx, ry), style);
  }

  //! Fills a rounded rectangle bounded by `[x, y, w, h]` with radius `r` with the current fill style.
  BL_INLINE_NODEBUG BLResult fillRoundRect(double x, double y, double w, double h, double r) noexcept {
    return fillRoundRect(BLRoundRect(x, y, w, h, r));
  }

  //! Fills a rounded rectangle bounded as `[x, y, w, h]` with radius `[rx, ry]` with the current fill style.
  BL_INLINE_NODEBUG BLResult fillRoundRect(double x, double y, double w, double h, double rx, double ry) noexcept {
    return fillRoundRect(BLRoundRect(x, y, w, h, rx, ry));
  }

  //! Fills a rounded rectangle bounded as `[x, y, w, h]` with radius `[rx, ry]` with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillRoundRect(double x, double y, double w, double h, double rx, double ry, const StyleT& style) noexcept {
    return fillRoundRect(BLRoundRect(x, y, w, h, rx, ry), style);
  }

  //! Fills a `chord` (floating point coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillChord(const BLArc& chord) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_CHORD, &chord);
  }

  //! Fills a `chord` (floating point coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillChord(const BLArc& chord, const StyleT& style) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_CHORD, &chord, style);
  }

  //! Fills a chord at `[cx, cy]` with radius `r` at `start` of `sweep` (floating point coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillChord(double cx, double cy, double r, double start, double sweep) noexcept {
    return fillChord(BLArc(cx, cy, r, r, start, sweep));
  }

  //! Fills a chord at `[cx, cy]` with radius `[rx, ry]` at `start` of `sweep` (floating point coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillChord(double cx, double cy, double rx, double ry, double start, double sweep) noexcept {
    return fillChord(BLArc(cx, cy, rx, ry, start, sweep));
  }

  //! Fills a chord at `[cx, cy]` with radius `[rx, ry]` at `start` of `sweep` (floating point coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillChord(double cx, double cy, double rx, double ry, double start, double sweep, const StyleT& style) noexcept {
    return fillChord(BLArc(cx, cy, rx, ry, start, sweep), style);
  }

  //! Fills a `pie` (floating point coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillPie(const BLArc& pie) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_PIE, &pie);
  }

  //! Fills a `pie` (floating point coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillPie(const BLArc& pie, const StyleT& style) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_PIE, &pie, style);
  }

  //! Fills a pie at `[cx, cy]` with radius `r` at `start` of `sweep` (floating point coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillPie(double cx, double cy, double r, double start, double sweep) noexcept {
    return fillPie(BLArc(cx, cy, r, r, start, sweep));
  }

  //! Fills a pie at `[cx, cy]` with radius `[rx, ry]` at `start` of `sweep` (floating point coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillPie(double cx, double cy, double rx, double ry, double start, double sweep) noexcept {
    return fillPie(BLArc(cx, cy, rx, ry, start, sweep));
  }

  //! Fills a pie at `[cx, cy]` with radius `[rx, ry]` at `start` of `sweep` (floating point coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillPie(double cx, double cy, double rx, double ry, double start, double sweep, const StyleT& style) noexcept {
    return fillPie(BLArc(cx, cy, rx, ry, start, sweep), style);
  }

  //! Fills a `triangle` (floating point coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillTriangle(const BLTriangle& triangle) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_TRIANGLE, &triangle);
  }

  //! Fills a `triangle` (floating point coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillTriangle(const BLTriangle& triangle, const StyleT& style) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_TRIANGLE, &triangle, style);
  }

  //! Fills a triangle defined by `[x0, y0]`, `[x1, y1]`, `[x2, y2]` (floating point coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillTriangle(double x0, double y0, double x1, double y1, double x2, double y2) noexcept {
    return fillTriangle(BLTriangle(x0, y0, x1, y1, x2, y2));
  }

  //! Fills a triangle defined by `[x0, y0]`, `[x1, y1]`, `[x2, y2]` (floating point coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillTriangle(double x0, double y0, double x1, double y1, double x2, double y2, const StyleT& style) noexcept {
    return fillTriangle(BLTriangle(x0, y0, x1, y1, x2, y2), style);
  }

  //! Fills a polygon `poly` (floating point coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillPolygon(const BLArrayView<BLPoint>& poly) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_POLYGOND, &poly);
  }

  //! Fills a polygon `poly` (floating point coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillPolygon(const BLArrayView<BLPoint>& poly, const StyleT& style) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_POLYGOND, &poly, style);
  }

  //! Fills a polygon `poly` having `n` vertices (floating point coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillPolygon(const BLPoint* poly, size_t n) noexcept {
    return fillPolygon(BLArrayView<BLPoint>{poly, n});
  }

  //! Fills a polygon `poly` having `n` vertices (floating point coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillPolygon(const BLPoint* poly, size_t n, const StyleT& style) noexcept {
    return fillPolygon(BLArrayView<BLPoint>{poly, n}, style);
  }

  //! Fills a polygon `poly` (integer coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillPolygon(const BLArrayView<BLPointI>& poly) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_POLYGONI, &poly);
  }

  //! Fills a polygon `poly` (integer coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillPolygon(const BLArrayView<BLPointI>& poly, const StyleT& style) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_POLYGONI, &poly, style);
  }

  //! Fills a polygon `poly` having `n` vertices (integer coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillPolygon(const BLPointI* poly, size_t n) noexcept {
    return fillPolygon(BLArrayView<BLPointI>{poly, n});
  }

  //! Fills a polygon `poly` having `n` vertices (integer coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillPolygon(const BLPointI* poly, size_t n, const StyleT& style) noexcept {
    return fillPolygon(BLArrayView<BLPointI>{poly, n}, style);
  }

  //! Fills an `array` of boxes (floating point coordinates) with the default fill style.
  BL_INLINE_NODEBUG BLResult fillBoxArray(const BLArrayView<BLBox>& array) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXD, &array);
  }

  //! Fills an `array` of boxes (floating point coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillBoxArray(const BLArrayView<BLBox>& array, const StyleT& style) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXD, &array, style);
  }

  //! Fills an `array` of boxes of size `n` (floating point coordinates) with the default fill style.
  BL_INLINE_NODEBUG BLResult fillBoxArray(const BLBox* array, size_t n) noexcept {
    return fillBoxArray(BLArrayView<BLBox>{array, n});
  }

  //! Fills an `array` of boxes of size `n` (floating point coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillBoxArray(const BLBox* array, size_t n, const StyleT& style) noexcept {
    return fillBoxArray(BLArrayView<BLBox>{array, n}, style);
  }

  //! Fills an `array` of boxes (integer coordinates) with the default fill style.
  BL_INLINE_NODEBUG BLResult fillBoxArray(const BLArrayView<BLBoxI>& array) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXI, &array);
  }

  //! Fills an `array` of boxes (integer coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillBoxArray(const BLArrayView<BLBoxI>& array, const StyleT& style) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXI, &array, style);
  }

  //! Fills an `array` of boxes of size `n` (integer coordinates) with the default fill style.
  BL_INLINE_NODEBUG BLResult fillBoxArray(const BLBoxI* array, size_t n) noexcept {
    return fillBoxArray(BLArrayView<BLBoxI>{array, n});
  }

  //! Fills an array of boxes of size `n` (integer coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillBoxArray(const BLBoxI* array, size_t n, const StyleT& style) noexcept {
    return fillBoxArray(BLArrayView<BLBoxI>{array, n}, style);
  }

  //! Fills an `array` of rectangles (floating point coordinates) with the default fill style.
  BL_INLINE_NODEBUG BLResult fillRectArray(const BLArrayView<BLRect>& array) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTD, &array);
  }

  //! Fills an `array` of rectangles (floating point coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillRectArray(const BLArrayView<BLRect>& array, const StyleT& style) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTD, &array, style);
  }

  //! Fills an `array` of rectangles of size `n` (floating point coordinates) with the default fill style.
  BL_INLINE_NODEBUG BLResult fillRectArray(const BLRect* array, size_t n) noexcept {
    return fillRectArray(BLArrayView<BLRect>{array, n});
  }

  //! Fills an `array` of rectangles of size `n` (floating point coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillRectArray(const BLRect* array, size_t n, const StyleT& style) noexcept {
    return fillRectArray(BLArrayView<BLRect>{array, n}, style);
  }

  //! Fills an array of rectangles (integer coordinates) with the default fill style.
  BL_INLINE_NODEBUG BLResult fillRectArray(const BLArrayView<BLRectI>& array) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTI, &array);
  }

  //! Fills an array of rectangles (integer coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillRectArray(const BLArrayView<BLRectI>& array, const StyleT& style) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTI, &array, style);
  }

  //! Fills an `array` of rectangles of size `n` (integer coordinates) with the default fill style.
  BL_INLINE_NODEBUG BLResult fillRectArray(const BLRectI* array, size_t n) noexcept {
    return fillRectArray(BLArrayView<BLRectI>{array, n});
  }

  //! Fills an `array` of rectangles of size `n` (integer coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillRectArray(const BLRectI* array, size_t n, const StyleT& style) noexcept {
    return fillRectArray(BLArrayView<BLRectI>{array, n}, style);
  }

  //! Fills the given `path` with the default fill style.
  BL_INLINE_NODEBUG BLResult fillPath(const BLPathCore& path) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_PATH, &path);
  }

  //! Fills the given `path` with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillPath(const BLPathCore& path, const StyleT& style) noexcept {
    return _fillGeometryOp(BL_GEOMETRY_TYPE_PATH, &path, style);
  }

  //! Fills the given `path` translated by `origin` with the default fill style.
  BL_INLINE_NODEBUG BLResult fillPath(const BLPoint& origin, const BLPathCore& path) noexcept {
    BL_CONTEXT_CALL_RETURN(fillPathD, impl, &origin, &path);
  }

  //! Fills the given `path` translated by `origin` with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillPath(const BLPoint& origin, const BLPathCore& path, const StyleT& style) noexcept {
    return _fillPathD(origin, path, style);
  }

  //! Fills the passed geometry specified by geometry `type` and `data` with the default fill style.
  //!
  //! \note This function provides a low-level interface that can be used in cases in which geometry `type` and `data`
  //! parameters are passed to a wrapper function that just passes them to the rendering context. It's a good way of
  //! creating wrappers, but generally low-level for a general purpose use, so please use this with caution.
  BL_INLINE_NODEBUG BLResult fillGeometry(BLGeometryType type, const void* data) noexcept {
    return _fillGeometryOp(type, data);
  }

  //! Fills the passed geometry specified by geometry `type` and `data` with an explicit fill `style`.
  //!
  //! \note This function provides a low-level interface that can be used in cases in which geometry `type` and `data`
  //! parameters are passed to a wrapper function that just passes them to the rendering context. It's a good way of
  //! creating wrappers, but generally low-level for a general purpose use, so please use this with caution.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillGeometry(BLGeometryType type, const void* data, const StyleT& style) noexcept {
    return _fillGeometryOp(type, data, style);
  }

  //! \}

  //! \name Fill Text & Glyphs Operations
  //! \{

  //! Fills UTF-8 encoded string passed as `text` and `size` by using the given `font` at `origin` (integer coordinates)
  //! with the default fill style.
  //!
  //! \note The `size` parameter defaults to `SIZE_MAX`, which informs Blend2D that the input is a null terminated string.
  //! If you want to pass a non-null terminated string or a substring of an existing string, use either this function with
  //! a `size` parameter set (which contains the number of bytes of `text` string, and not the number of UTF-8 characters)
  //! or an overloaded function that accepts a convenience \ref BLStringView parameter instead of `text` and size`.
  BL_INLINE_NODEBUG BLResult fillUtf8Text(const BLPointI& origin, const BLFontCore& font, const char* text, size_t size = SIZE_MAX) noexcept {
    BLStringView view{text, size};
    return _fillTextOpI(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view);
  }

  //! Fills UTF-8 encoded string passed as `text` and `size` by using the given `font` at `origin` (integer coordinates)
  //! with an explicit fill `style`.
  //!
  //! \note Pass `SIZE_MAX` to `size` to inform Blend2D that the input is a null terminated string. If you want to pass
  //! a non-null terminated string or a substring of an existing string, use either this function with a `size` parameter
  //! set (which contains the number of bytes of `text` string, and not the number of UTF-8 characters) or an overloaded
  //! function that accepts a convenience \ref BLStringView parameter instead of `text` and size`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillUtf8Text(const BLPointI& origin, const BLFontCore& font, const char* text, size_t size, const StyleT& style) noexcept {
    BLStringView view{text, size};
    return _fillTextOpI(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view, style);
  }

  //! Fills UTF-8 encoded string passed as string `view` by using the given `font` at `origin` (floating point coordinates)
  //! with the default fill style.
  BL_INLINE_NODEBUG BLResult fillUtf8Text(const BLPointI& origin, const BLFontCore& font, const BLStringView& view) noexcept {
    return _fillTextOpI(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view);
  }

  //! Fills UTF-8 encoded string passed as string `view` by using the given `font` at `origin` (floating point coordinates)
  //! with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillUtf8Text(const BLPointI& origin, const BLFontCore& font, const BLStringView& view, const StyleT& style) noexcept {
    return _fillTextOpI(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view, style);
  }

  //! Fills UTF-8 encoded string passed as `text` and `size` by using the given `font` at `origin` (integer coordinates)
  //! with the default fill style.
  //!
  //! \note The `size` parameter defaults to `SIZE_MAX`, which informs Blend2D that the input is a null terminated string.
  //! If you want to pass a non-null terminated string or a substring of an existing string, use either this function with
  //! a `size` parameter set (which contains the number of bytes of `text` string, and not the number of UTF-8 characters)
  //! or an overloaded function that accepts a convenience \ref BLStringView parameter instead of `text` and size`.
  BL_INLINE_NODEBUG BLResult fillUtf8Text(const BLPoint& origin, const BLFontCore& font, const char* text, size_t size = SIZE_MAX) noexcept {
    BLStringView view{text, size};
    return _fillTextOpD(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view);
  }

  //! Fills UTF-8 encoded string passed as `text` and `size` by using the given `font` at `origin` (integer coordinates)
  //! with an explicit fill `style`.
  //!
  //! \note Pass `SIZE_MAX` in `size` to inform Blend2D that the input is a null terminated string. If you want to pass
  //! a non-null terminated string or a substring of an existing string, use either this function with a `size` parameter
  //! set (which contains the number of bytes of `text` string, and not the number of UTF-8 characters) or an overloaded
  //! function that accepts a convenience \ref BLStringView parameter instead of `text` and size`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillUtf8Text(const BLPoint& origin, const BLFontCore& font, const char* text, size_t size, const StyleT& style) noexcept {
    BLStringView view{text, size};
    return _fillTextOpD(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view, style);
  }

  //! Fills UTF-8 encoded string passed as string `view` by using the given `font` at `origin` (floating point coordinates).
  BL_INLINE_NODEBUG BLResult fillUtf8Text(const BLPoint& origin, const BLFontCore& font, const BLStringView& view) noexcept {
    return _fillTextOpD(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view);
  }
  //! \overload
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillUtf8Text(const BLPoint& origin, const BLFontCore& font, const BLStringView& view, const StyleT& style) noexcept {
    return _fillTextOpD(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view, style);
  }

  //! Fills UTF-16 encoded string passed as `text` and `size` by using the given `font` at `origin` (integer coordinates)
  //! with the default fill style.
  //!
  //! \note The `size` parameter defaults to `SIZE_MAX`, which informs Blend2D that the input is a null terminated UTF-16
  //! string. If you want to pass a non-null terminated string or a substring of an existing string, specify `size`
  //! parameter to the size of the `text` buffer in code units (the number of 16-bit values).
  BL_INLINE_NODEBUG BLResult fillUtf16Text(const BLPointI& origin, const BLFontCore& font, const uint16_t* text, size_t size = SIZE_MAX) noexcept {
    BLArrayView<uint16_t> view{text, size};
    return _fillTextOpI(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view);
  }

  //! Fills UTF-16 encoded string passed as `text` and `size` by using the given `font` at `origin` (integer coordinates)
  //! with an explicit fill `style`.
  //!
  //! \note Pass `SIZE_MAX` in `size` to inform Blend2D that the input is a null terminated string. If you want to pass a
  //! non-null terminated string or a substring of an existing string, specify `size` parameter to the size of the `text`
  //! buffer in code units (the number of 16-bit values).
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillUtf16Text(const BLPointI& origin, const BLFontCore& font, const uint16_t* text, size_t size, const StyleT& style) noexcept {
    BLArrayView<uint16_t> view{text, size};
    return _fillTextOpI(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view, style);
  }

  //! Fills UTF-16 encoded string passed as `text` and `size` by using the given `font` at `origin` (floating point coordinates)
  //! with the default fill style.
  //!
  //! \note The `size` parameter defaults to `SIZE_MAX`, which informs Blend2D that the input is a null terminated UTF-16
  //! string. If you want to pass a non-null terminated string or a substring of an existing string, specify `size`
  //! parameter to the size of the `text` buffer in code units (the number of 16-bit values).
  BL_INLINE_NODEBUG BLResult fillUtf16Text(const BLPoint& origin, const BLFontCore& font, const uint16_t* text, size_t size = SIZE_MAX) noexcept {
    BLArrayView<uint16_t> view{text, size};
    return _fillTextOpD(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view);
  }

  //! Fills UTF-16 encoded string passed as `text` and `size` by using the given `font` at `origin` (floating point coordinates)
  //! with an explicit fill `style`.
  //!
  //! \note Pass `SIZE_MAX` in `size` to inform Blend2D that the input is a null terminated string. If you want to pass a
  //! non-null terminated string or a substring of an existing string, specify `size` parameter to the size of the `text`
  //! buffer in code units (the number of 16-bit values).
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillUtf16Text(const BLPoint& origin, const BLFontCore& font, const uint16_t* text, size_t size, const StyleT& style) noexcept {
    BLArrayView<uint16_t> view{text, size};
    return _fillTextOpD(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view, style);
  }

  //! Fills UTF-32 encoded string passed as `text` and `size` by using the given `font` at `origin` (integer coordinates)
  //! with the default fill style.
  //!
  //! \note The `size` parameter defaults to `SIZE_MAX`, which informs Blend2D that the input is a null terminated UTF-32
  //! string. If you want to pass a non-null terminated string or a substring of an existing string, specify `size`
  //! parameter to the size of the `text` buffer in code units (the number of 32-bit values).
  BL_INLINE_NODEBUG BLResult fillUtf32Text(const BLPointI& origin, const BLFontCore& font, const uint32_t* text, size_t size = SIZE_MAX) noexcept {
    BLArrayView<uint32_t> view{text, size};
    return _fillTextOpI(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view);
  }

  //! Fills UTF-32 encoded string passed as `text` and `size` by using the given `font` at `origin` (integer coordinates)
  //! with an explicit fill `style`.
  //!
  //! \note Pass `SIZE_MAX` in `size` to inform Blend2D that the input is a null terminated string. If you want to pass a
  //! non-null terminated string or a substring of an existing string, specify `size` parameter to the size of the `text`
  //! buffer in code units (the number of 32-bit values).
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillUtf32Text(const BLPointI& origin, const BLFontCore& font, const uint32_t* text, size_t size, const StyleT& style) noexcept {
    BLArrayView<uint32_t> view{text, size};
    return _fillTextOpI(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view, style);
  }

  //! Fills UTF-32 encoded string passed as `text` and `size` by using the given `font` at `origin` (floating point coordinates)
  //! with the default fill style.
  //!
  //! \note The `size` parameter defaults to `SIZE_MAX`, which informs Blend2D that the input is a null terminated UTF-32
  //! string. If you want to pass a non-null terminated string or a substring of an existing string, specify `size`
  //! parameter to the size of the `text` buffer in code units (the number of 32-bit values).
  BL_INLINE_NODEBUG BLResult fillUtf32Text(const BLPoint& origin, const BLFontCore& font, const uint32_t* text, size_t size = SIZE_MAX) noexcept {
    BLArrayView<uint32_t> view{text, size};
    return _fillTextOpD(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view);
  }

  //! Fills UTF-32 encoded string passed as `text` and `size` by using the given `font` at `origin` (floating point coordinates)
  //! with an explicit fill `style`.
  //!
  //! \note Pass `SIZE_MAX` in `size` to inform Blend2D that the input is a null terminated string. If you want to pass a
  //! non-null terminated string or a substring of an existing string, specify `size` parameter to the size of the `text`
  //! buffer in code units (the number of 32-bit values).
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillUtf32Text(const BLPoint& origin, const BLFontCore& font, const uint32_t* text, size_t size, const StyleT& style) noexcept {
    BLArrayView<uint32_t> view{text, size};
    return _fillTextOpD(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view, style);
  }

  //! Fills a `glyphRun` by using the given `font` at `origin` (integer coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillGlyphRun(const BLPointI& origin, const BLFontCore& font, const BLGlyphRun& glyphRun) noexcept {
    return _fillTextOpI(origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, &glyphRun);
  }

  //! Fills a `glyphRun` by using the given `font` at `origin` (integer coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillGlyphRun(const BLPointI& origin, const BLFontCore& font, const BLGlyphRun& glyphRun, const StyleT& style) noexcept {
    return _fillTextOpI(origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, &glyphRun, style);
  }

  //! Fills the passed `glyphRun` by using the given `font` at `origin` (floating point coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillGlyphRun(const BLPoint& origin, const BLFontCore& font, const BLGlyphRun& glyphRun) noexcept {
    return _fillTextOpD(origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, &glyphRun);
  }

  //! Fills the passed `glyphRun` by using the given `font` at `origin` (floating point coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillGlyphRun(const BLPoint& origin, const BLFontCore& font, const BLGlyphRun& glyphRun, const StyleT& style) noexcept {
    return _fillTextOpD(origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, &glyphRun, style);
  }

  //! \}

  //! \name Fill Mask Operations
  //! \{

  //! Fills a source `mask` image at coordinates specified by `origin` (int coordinates) with the current fill style.
  BL_INLINE_NODEBUG BLResult fillMask(const BLPointI& origin, const BLImage& mask) noexcept {
    return _fillMaskI(origin, mask, nullptr);
  }

  //! Fills a source `mask` image at coordinates specified by `origin` (int coordinates) with an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillMask(const BLPointI& origin, const BLImage& mask, const StyleT& style) noexcept {
    return _fillMaskI(origin, mask, nullptr, style);
  }

  //! Fills a source `mask` image specified by `maskArea` at coordinates specified by `origin` (int coordinates) with
  //! the current fill style.
  BL_INLINE_NODEBUG BLResult fillMask(const BLPointI& origin, const BLImage& mask, const BLRectI& maskArea) noexcept {
    return _fillMaskI(origin, mask, &maskArea);
  }

  //! Fills a source `mask` image specified by `maskArea` at coordinates specified by `origin` (int coordinates) with
  //! an explicit fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillMask(const BLPointI& origin, const BLImage& mask, const BLRectI& maskArea, const StyleT& style) noexcept {
    return _fillMaskI(origin, mask, &maskArea, style);
  }

  //! Fills a source `mask` image at coordinates specified by `origin` (floating point coordinates) with the current
  //! fill style.
  BL_INLINE_NODEBUG BLResult fillMask(const BLPoint& origin, const BLImage& mask) noexcept {
    return _fillMaskD(origin, mask, nullptr);
  }

  //! Fills a source `mask` image at coordinates specified by `origin` (floating point coordinates) with an explicit
  //! fill `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillMask(const BLPoint& origin, const BLImage& mask, const StyleT& style) noexcept {
    return _fillMaskD(origin, mask, nullptr, style);
  }

  //! Fills a source `mask` image specified by `maskArea` at coordinates specified by `origin` (floating point coordinates)
  //! with the current fill style.
  BL_INLINE_NODEBUG BLResult fillMask(const BLPoint& origin, const BLImage& mask, const BLRectI& maskArea) noexcept {
    return _fillMaskD(origin, mask, &maskArea);
  }

  //! Fills a source `mask` image specified by `maskArea` at coordinates specified by `origin` (floating point coordinates)
  //! with an explicit fill style.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult fillMask(const BLPoint& origin, const BLImage& mask, const BLRectI& maskArea, const StyleT& style) noexcept {
    return _fillMaskD(origin, mask, &maskArea, style);
  }

  //! \}

  //! \cond INTERNAL
  //! \name Stroke Wrappers (Internal)
  //! \{

private:
  BL_INLINE_NODEBUG BLResult _strokeGeometryOp(BLGeometryType type, const void* data) noexcept {
    BL_CONTEXT_CALL_RETURN(strokeGeometry, impl, type, data);
  }

  BL_INLINE_NODEBUG BLResult _strokeGeometryOp(BLGeometryType type, const void* data, const BLRgba& rgba) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba);
    BL_CONTEXT_CALL_RETURN(strokeGeometryExt, impl, type, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _strokeGeometryOp(BLGeometryType type, const void* data, const BLRgba32& rgba32) noexcept {
    BL_CONTEXT_CALL_RETURN(strokeGeometryRgba32, impl, type, data, rgba32.value);
  }

  BL_INLINE_NODEBUG BLResult _strokeGeometryOp(BLGeometryType type, const void* data, const BLRgba64& rgba64) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba64);
    BL_CONTEXT_CALL_RETURN(strokeGeometryExt, impl, type, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _strokeGeometryOp(BLGeometryType type, const void* data, const BLVarCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(strokeGeometryExt, impl, type, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _strokeGeometryOp(BLGeometryType type, const void* data, const BLPatternCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(strokeGeometryExt, impl, type, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _strokeGeometryOp(BLGeometryType type, const void* data, const BLGradientCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(strokeGeometryExt, impl, type, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _strokePathD(const BLPoint& origin, const BLPathCore& path, const BLRgba& rgba) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba);
    BL_CONTEXT_CALL_RETURN(strokePathDExt, impl, &origin, &path, &style);
  }

  BL_INLINE_NODEBUG BLResult _strokePathD(const BLPoint& origin, const BLPathCore& path, const BLRgba32& rgba32) noexcept {
    BL_CONTEXT_CALL_RETURN(strokePathDRgba32, impl, &origin, &path, rgba32.value);
  }

  BL_INLINE_NODEBUG BLResult _strokePathD(const BLPoint& origin, const BLPathCore& path, const BLRgba64& rgba64) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba64);
    BL_CONTEXT_CALL_RETURN(strokePathDExt, impl, &origin, &path, &style);
  }

  BL_INLINE_NODEBUG BLResult _strokePathD(const BLPoint& origin, const BLPathCore& path, const BLVarCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(strokePathDExt, impl, &origin, &path, &style);
  }

  BL_INLINE_NODEBUG BLResult _strokePathD(const BLPoint& origin, const BLPathCore& path, const BLPatternCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(strokePathDExt, impl, &origin, &path, &style);
  }

  BL_INLINE_NODEBUG BLResult _strokePathD(const BLPoint& origin, const BLPathCore& path, const BLGradientCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(strokePathDExt, impl, &origin, &path, &style);
  }

  BL_INLINE_NODEBUG BLResult _strokeTextOpI(const BLPointI& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data) noexcept {
    BL_CONTEXT_CALL_RETURN(strokeTextOpI, impl, &origin, &font, op, data);
  }

  BL_INLINE_NODEBUG BLResult _strokeTextOpI(const BLPointI& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLRgba& rgba) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba);
    BL_CONTEXT_CALL_RETURN(strokeTextOpIExt, impl, &origin, &font, op, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _strokeTextOpI(const BLPointI& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLRgba32& rgba32) noexcept {
    BL_CONTEXT_CALL_RETURN(strokeTextOpIRgba32, impl, &origin, &font, op, data, rgba32.value);
  }

  BL_INLINE_NODEBUG BLResult _strokeTextOpI(const BLPointI& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLRgba64& rgba64) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba64);
    BL_CONTEXT_CALL_RETURN(strokeTextOpIExt, impl, &origin, &font, op, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _strokeTextOpI(const BLPointI& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLVarCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(strokeTextOpIExt, impl, &origin, &font, op, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _strokeTextOpI(const BLPointI& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLPatternCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(strokeTextOpIExt, impl, &origin, &font, op, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _strokeTextOpI(const BLPointI& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLGradientCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(strokeTextOpIExt, impl, &origin, &font, op, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _strokeTextOpD(const BLPoint& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data) noexcept {
    BL_CONTEXT_CALL_RETURN(strokeTextOpD, impl, &origin, &font, op, data);
  }

  BL_INLINE_NODEBUG BLResult _strokeTextOpD(const BLPoint& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLRgba& rgba) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba);
    BL_CONTEXT_CALL_RETURN(strokeTextOpDExt, impl, &origin, &font, op, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _strokeTextOpD(const BLPoint& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLRgba32& rgba32) noexcept {
    BL_CONTEXT_CALL_RETURN(strokeTextOpDRgba32, impl, &origin, &font, op, data, rgba32.value);
  }

  BL_INLINE_NODEBUG BLResult _strokeTextOpD(const BLPoint& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLRgba64& rgba64) noexcept {
    BLVarCore style = BLInternal::makeInlineStyle(rgba64);
    BL_CONTEXT_CALL_RETURN(strokeTextOpDExt, impl, &origin, &font, op, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _strokeTextOpD(const BLPoint& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLVarCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(strokeTextOpDExt, impl, &origin, &font, op, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _strokeTextOpD(const BLPoint& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLPatternCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(strokeTextOpDExt, impl, &origin, &font, op, data, &style);
  }

  BL_INLINE_NODEBUG BLResult _strokeTextOpD(const BLPoint& origin, const BLFontCore& font, BLContextRenderTextOp op, const void* data, const BLGradientCore& style) noexcept {
    BL_CONTEXT_CALL_RETURN(strokeTextOpDExt, impl, &origin, &font, op, data, &style);
  }

public:

  //! \}
  //! \endcond

  //! \name Stroke Geometry Operations
  //! \{

  //! Strokes a `box` (floating point coordinates) with the current stroke style.
  //!
  //! \note Box is defined as `[x0, y0, x1, y1]`, if you need `[x, y, w, h]`, use \ref strokeRect() instead.
  BL_INLINE_NODEBUG BLResult strokeBox(const BLBox& box) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_BOXD, &box);
  }

  //! Strokes a `box` (floating point coordinates) with an explicit stroke `style`.
  //!
  //! \note Box is defined as `[x0, y0, x1, y1]`, if you need `[x, y, w, h]`, use \ref strokeRect() instead.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeBox(const BLBox& box, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_BOXD, &box, style);
  }

  //! Strokes a `box` (integer coordinates) with the current stroke style.
  //!
  //! \note Box is defined as `[x0, y0, x1, y1]`, if you need `[x, y, w, h]`, use \ref strokeRect() instead.
  BL_INLINE_NODEBUG BLResult strokeBox(const BLBoxI& box) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_BOXI, &box);
  }

  //! Strokes a `box` (integer coordinates) with an explicit stroke `style`.
  //!
  //! \note Box is defined as `[x0, y0, x1, y1]`, if you need `[x, y, w, h]`, use \ref strokeRect() instead.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeBox(const BLBoxI& box, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_BOXI, &box, style);
  }

  //! Strokes a box `[x0, y0, x1, y1]` (floating point coordinates) with the current stroke style.
  //!
  //! \note Box is defined as `[x0, y0, x1, y1]`, if you need `[x, y, w, h]`, use \ref strokeRect() instead.
  BL_INLINE_NODEBUG BLResult strokeBox(double x0, double y0, double x1, double y1) noexcept {
    return strokeBox(BLBox(x0, y0, x1, y1));
  }

  //! Strokes a box `[x0, y0, x1, y1]` (floating point coordinates) with an explicit stroke `style`.
  //!
  //! \note Box is defined as `[x0, y0, x1, y1]`, if you need `[x, y, w, h]`, use \ref strokeRect() instead.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeBox(double x0, double y0, double x1, double y1, const StyleT& style) noexcept {
    return strokeBox(BLBox(x0, y0, x1, y1), style);
  }

  //! Strokes a rectangle `rect` (integer coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeRect(const BLRectI& rect) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_RECTI, &rect);
  }

  //! Strokes a rectangle `rect` (integer coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeRect(const BLRectI& rect, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_RECTI, &rect, style);
  }

  //! Strokes a rectangle `rect` (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeRect(const BLRect& rect) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_RECTD, &rect);
  }

  //! Strokes a rectangle `rect` (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeRect(const BLRect& rect, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_RECTD, &rect, style);
  }

  //! Strokes a rectangle `[x, y, w, h]` (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeRect(double x, double y, double w, double h) noexcept {
    return strokeRect(BLRect(x, y, w, h));
  }

  //! Strokes a rectangle `[x, y, w, h]` (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeRect(double x, double y, double w, double h, const StyleT& style) noexcept {
    return strokeRect(BLRect(x, y, w, h), style);
  }

  //! Strokes a line specified as `line` (floating point coordinates) with the default stroke style.
  BL_INLINE_NODEBUG BLResult strokeLine(const BLLine& line) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_LINE, &line);
  }

  //! Strokes a line specified as `line` (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeLine(const BLLine& line, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_LINE, &line, style);
  }

  //! Strokes a line starting at `p0` and ending at `p1` (floating point coordinates) with the default stroke style.
  BL_INLINE_NODEBUG BLResult strokeLine(const BLPoint& p0, const BLPoint& p1) noexcept {
    return strokeLine(BLLine(p0.x, p0.y, p1.x, p1.y));
  }

  //! Strokes a line starting at `p0` and ending at `p1` (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeLine(const BLPoint& p0, const BLPoint& p1, const StyleT& style) noexcept {
    return strokeLine(BLLine(p0.x, p0.y, p1.x, p1.y), style);
  }

  //! Strokes a line starting at `[x0, y0]` and ending at `[x1, y1]` (floating point coordinates) with the default
  //! stroke style.
  BL_INLINE_NODEBUG BLResult strokeLine(double x0, double y0, double x1, double y1) noexcept {
    return strokeLine(BLLine(x0, y0, x1, y1));
  }

  //! Strokes a line starting at `[x0, y0]` and ending at `[x1, y1]` (floating point coordinates) with an explicit
  //! stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeLine(double x0, double y0, double x1, double y1, const StyleT& style) noexcept {
    return strokeLine(BLLine(x0, y0, x1, y1), style);
  }

  //! Strokes a `circle` (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeCircle(const BLCircle& circle) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_CIRCLE, &circle);
  }

  //! Strokes a `circle` (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeCircle(const BLCircle& circle, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_CIRCLE, &circle, style);
  }

  //! Strokes a circle at `[cx, cy]` and radius `r` (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeCircle(double cx, double cy, double r) noexcept {
    return strokeCircle(BLCircle(cx, cy, r));
  }

  //! Strokes a circle at `[cx, cy]` and radius `r` (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeCircle(double cx, double cy, double r, const StyleT& style) noexcept {
    return strokeCircle(BLCircle(cx, cy, r), style);
  }

  //! Strokes an `ellipse` (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeEllipse(const BLEllipse& ellipse) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_ELLIPSE, &ellipse);
  }

  //! Strokes an `ellipse` (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeEllipse(const BLEllipse& ellipse, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_ELLIPSE, &ellipse, style);
  }

  //! Strokes an ellipse at `[cx, cy]` with radius `[rx, ry]` (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeEllipse(double cx, double cy, double rx, double ry) noexcept {
    return strokeEllipse(BLEllipse(cx, cy, rx, ry));
  }

  //! Strokes an ellipse at `[cx, cy]` with radius `[rx, ry]` (floating point coordinates) with en explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeEllipse(double cx, double cy, double rx, double ry, const StyleT& style) noexcept {
    return strokeEllipse(BLEllipse(cx, cy, rx, ry), style);
  }

  //! Strokes a rounded rectangle `rr` (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeRoundRect(const BLRoundRect& rr) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_ROUND_RECT, &rr);
  }

  //! Strokes a rounded rectangle `rr` (floating point coordinates) with en explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeRoundRect(const BLRoundRect& rr, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_ROUND_RECT, &rr, style);
  }

  //! Strokes a rounded rectangle bounded by `rect` with radius `r` with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeRoundRect(const BLRect& rect, double r) noexcept {
    return strokeRoundRect(BLRoundRect(rect.x, rect.y, rect.w, rect.h, r));
  }

  //! Strokes a rounded rectangle bounded by `rect` with radius `[rx, ry]` with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeRoundRect(const BLRect& rect, double rx, double ry) noexcept {
    return strokeRoundRect(BLRoundRect(rect.x, rect.y, rect.w, rect.h, rx, ry));
  }

  //! Strokes a rounded rectangle bounded by `rect` with radius `[rx, ry]` with en explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeRoundRect(const BLRect& rect, double rx, double ry, const StyleT& style) noexcept {
    return strokeRoundRect(BLRoundRect(rect.x, rect.y, rect.w, rect.h, rx, ry), style);
  }

  //! Strokes a rounded rectangle bounded by `[x, y, w, h]` with radius `r` with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeRoundRect(double x, double y, double w, double h, double r) noexcept {
    return strokeRoundRect(BLRoundRect(x, y, w, h, r));
  }

  //! Strokes a rounded rectangle bounded as `[x, y, w, h]` with radius `[rx, ry]` with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeRoundRect(double x, double y, double w, double h, double rx, double ry) noexcept {
    return strokeRoundRect(BLRoundRect(x, y, w, h, rx, ry));
  }

  //! Strokes a rounded rectangle bounded as `[x, y, w, h]` with radius `[rx, ry]` with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeRoundRect(double x, double y, double w, double h, double rx, double ry, const StyleT& style) noexcept {
    return strokeRoundRect(BLRoundRect(x, y, w, h, rx, ry), style);
  }

  //! Strokes an `arc` with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeArc(const BLArc& arc) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_ARC, &arc);
  }

  //! Strokes an `arc` with an explicit stroke `style.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeArc(const BLArc& arc, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_ARC, &arc, style);
  }

  //! Strokes a chord at `[cx, cy]` with radius `r` at `start` of `sweep` (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeArc(double cx, double cy, double r, double start, double sweep) noexcept {
    return strokeArc(BLArc(cx, cy, r, r, start, sweep));
  }

  //! Strokes a chord at `[cx, cy]` with radius `[rx, ry]` at `start` of `sweep` (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeArc(double cx, double cy, double rx, double ry, double start, double sweep) noexcept {
    return strokeArc(BLArc(cx, cy, rx, ry, start, sweep));
  }

  //! Strokes a chord at `[cx, cy]` with radius `[rx, ry]` at `start` of `sweep` (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeArc(double cx, double cy, double rx, double ry, double start, double sweep, const StyleT& style) noexcept {
    return strokeArc(BLArc(cx, cy, rx, ry, start, sweep), style);
  }

  //! Strokes a `chord` (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeChord(const BLArc& chord) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_CHORD, &chord);
  }

  //! Strokes a `chord` (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeChord(const BLArc& chord, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_CHORD, &chord, style);
  }

  //! Strokes a chord at `[cx, cy]` with radius `r` at `start` of `sweep` (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeChord(double cx, double cy, double r, double start, double sweep) noexcept {
    return strokeChord(BLArc(cx, cy, r, r, start, sweep));
  }

  //! Strokes a chord at `[cx, cy]` with radius `[rx, ry]` at `start` of `sweep` (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeChord(double cx, double cy, double rx, double ry, double start, double sweep) noexcept {
    return strokeChord(BLArc(cx, cy, rx, ry, start, sweep));
  }

  //! Strokes a chord at `[cx, cy]` with radius `[rx, ry]` at `start` of `sweep` (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeChord(double cx, double cy, double rx, double ry, double start, double sweep, const StyleT& style) noexcept {
    return strokeChord(BLArc(cx, cy, rx, ry, start, sweep), style);
  }

  //! Strokes a `pie` (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokePie(const BLArc& pie) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_PIE, &pie);
  }

  //! Strokes a `pie` (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokePie(const BLArc& pie, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_PIE, &pie, style);
  }

  //! Strokes a pie at `[cx, cy]` with radius `r` at `start` of `sweep` (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokePie(double cx, double cy, double r, double start, double sweep) noexcept {
    return strokePie(BLArc(cx, cy, r, r, start, sweep));
  }

  //! Strokes a pie at `[cx, cy]` with radius `[rx, ry]` at `start` of `sweep` (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokePie(double cx, double cy, double rx, double ry, double start, double sweep) noexcept {
    return strokePie(BLArc(cx, cy, rx, ry, start, sweep));
  }

  //! Strokes a pie at `[cx, cy]` with radius `[rx, ry]` at `start` of `sweep` (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokePie(double cx, double cy, double rx, double ry, double start, double sweep, const StyleT& style) noexcept {
    return strokePie(BLArc(cx, cy, rx, ry, start, sweep), style);
  }

  //! Strokes a `triangle` (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeTriangle(const BLTriangle& triangle) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_TRIANGLE, &triangle);
  }

  //! Strokes a `triangle` (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeTriangle(const BLTriangle& triangle, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_TRIANGLE, &triangle, style);
  }

  //! Strokes a triangle defined by `[x0, y0]`, `[x1, y1]`, `[x2, y2]` (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeTriangle(double x0, double y0, double x1, double y1, double x2, double y2) noexcept {
    return strokeTriangle(BLTriangle(x0, y0, x1, y1, x2, y2));
  }

  //! Strokes a triangle defined by `[x0, y0]`, `[x1, y1]`, `[x2, y2]` (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeTriangle(double x0, double y0, double x1, double y1, double x2, double y2, const StyleT& style) noexcept {
    return strokeTriangle(BLTriangle(x0, y0, x1, y1, x2, y2), style);
  }

  //! Strokes a polyline `poly` (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokePolyline(const BLArrayView<BLPoint>& poly) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_POLYLINED, &poly);
  }

  //! Strokes a polyline `poly` (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokePolyline(const BLArrayView<BLPoint>& poly, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_POLYLINED, &poly, style);
  }

  //! Strokes a polyline `poly` having `n` vertices (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokePolyline(const BLPoint* poly, size_t n) noexcept {
    return strokePolyline(BLArrayView<BLPoint>{poly, n});
  }

  //! Strokes a polyline `poly` having `n` vertices (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokePolyline(const BLPoint* poly, size_t n, const StyleT& style) noexcept {
    return strokePolyline(BLArrayView<BLPoint>{poly, n}, style);
  }

  //! Strokes a polyline `poly` (integer coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokePolyline(const BLArrayView<BLPointI>& poly) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_POLYLINEI, &poly);
  }

  //! Strokes a polyline `poly` (integer coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokePolyline(const BLArrayView<BLPointI>& poly, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_POLYLINEI, &poly, style);
  }

  //! Strokes a polyline `poly` having `n` vertices (integer coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokePolyline(const BLPointI* poly, size_t n) noexcept {
    return strokePolyline(BLArrayView<BLPointI>{poly, n});
  }

  //! Strokes a polyline `poly` having `n` vertices (integer coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokePolyline(const BLPointI* poly, size_t n, const StyleT& style) noexcept {
    return strokePolyline(BLArrayView<BLPointI>{poly, n}, style);
  }

  //! Strokes a polygon `poly` (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokePolygon(const BLArrayView<BLPoint>& poly) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_POLYGOND, &poly);
  }

  //! Strokes a polygon `poly` (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokePolygon(const BLArrayView<BLPoint>& poly, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_POLYGOND, &poly, style);
  }

  //! Strokes a polygon `poly` having `n` vertices (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokePolygon(const BLPoint* poly, size_t n) noexcept {
    return strokePolygon(BLArrayView<BLPoint>{poly, n});
  }

  //! Strokes a polygon `poly` having `n` vertices (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokePolygon(const BLPoint* poly, size_t n, const StyleT& style) noexcept {
    return strokePolygon(BLArrayView<BLPoint>{poly, n}, style);
  }

  //! Strokes a polygon `poly` (integer coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokePolygon(const BLArrayView<BLPointI>& poly) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_POLYGONI, &poly);
  }

  //! Strokes a polygon `poly` (integer coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokePolygon(const BLArrayView<BLPointI>& poly, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_POLYGONI, &poly, style);
  }

  //! Strokes a polygon `poly` having `n` vertices (integer coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokePolygon(const BLPointI* poly, size_t n) noexcept {
    return strokePolygon(BLArrayView<BLPointI>{poly, n});
  }

  //! Strokes a polygon `poly` having `n` vertices (integer coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokePolygon(const BLPointI* poly, size_t n, const StyleT& style) noexcept {
    return strokePolygon(BLArrayView<BLPointI>{poly, n}, style);
  }

  //! Strokes an `array` of boxes (floating point coordinates) with the default stroke style.
  BL_INLINE_NODEBUG BLResult strokeBoxArray(const BLArrayView<BLBox>& array) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXD, &array);
  }

  //! Strokes an `array` of boxes (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeBoxArray(const BLArrayView<BLBox>& array, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXD, &array, style);
  }

  //! Strokes an `array` of boxes of size `n` (floating point coordinates) with the default stroke style.
  BL_INLINE_NODEBUG BLResult strokeBoxArray(const BLBox* array, size_t n) noexcept {
    return strokeBoxArray(BLArrayView<BLBox>{array, n});
  }

  //! Strokes an `array` of boxes of size `n` (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeBoxArray(const BLBox* array, size_t n, const StyleT& style) noexcept {
    return strokeBoxArray(BLArrayView<BLBox>{array, n}, style);
  }

  //! Strokes an `array` of boxes (integer coordinates) with the default stroke style.
  BL_INLINE_NODEBUG BLResult strokeBoxArray(const BLArrayView<BLBoxI>& array) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXI, &array);
  }

  //! Strokes an `array` of boxes (integer coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeBoxArray(const BLArrayView<BLBoxI>& array, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXI, &array, style);
  }

  //! Strokes an `array` of boxes of size `n` (integer coordinates) with the default stroke style.
  BL_INLINE_NODEBUG BLResult strokeBoxArray(const BLBoxI* array, size_t n) noexcept {
    return strokeBoxArray(BLArrayView<BLBoxI>{array, n});
  }

  //! Strokes an array of boxes of size `n` (integer coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeBoxArray(const BLBoxI* array, size_t n, const StyleT& style) noexcept {
    return strokeBoxArray(BLArrayView<BLBoxI>{array, n}, style);
  }

  //! Strokes an `array` of rectangles (floating point coordinates) with the default stroke style.
  BL_INLINE_NODEBUG BLResult strokeRectArray(const BLArrayView<BLRect>& array) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTD, &array);
  }

  //! Strokes an `array` of rectangles (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeRectArray(const BLArrayView<BLRect>& array, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTD, &array, style);
  }

  //! Strokes an `array` of rectangles of size `n` (floating point coordinates) with the default stroke style.
  BL_INLINE_NODEBUG BLResult strokeRectArray(const BLRect* array, size_t n) noexcept {
    return strokeRectArray(BLArrayView<BLRect>{array, n});
  }

  //! Strokes an `array` of rectangles of size `n` (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeRectArray(const BLRect* array, size_t n, const StyleT& style) noexcept {
    return strokeRectArray(BLArrayView<BLRect>{array, n}, style);
  }

  //! Strokes an array of rectangles (integer coordinates) with the default stroke style.
  BL_INLINE_NODEBUG BLResult strokeRectArray(const BLArrayView<BLRectI>& array) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTI, &array);
  }

  //! Strokes an array of rectangles (integer coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeRectArray(const BLArrayView<BLRectI>& array, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTI, &array, style);
  }

  //! Strokes an `array` of rectangles of size `n` (integer coordinates) with the default stroke style.
  BL_INLINE_NODEBUG BLResult strokeRectArray(const BLRectI* array, size_t n) noexcept {
    return strokeRectArray(BLArrayView<BLRectI>{array, n});
  }

  //! Strokes an `array` of rectangles of size `n` (integer coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeRectArray(const BLRectI* array, size_t n, const StyleT& style) noexcept {
    return strokeRectArray(BLArrayView<BLRectI>{array, n}, style);
  }

  //! Strokes the given `path` with the default stroke style.
  BL_INLINE_NODEBUG BLResult strokePath(const BLPathCore& path) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_PATH, &path);
  }

  //! Strokes the given `path` with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokePath(const BLPathCore& path, const StyleT& style) noexcept {
    return _strokeGeometryOp(BL_GEOMETRY_TYPE_PATH, &path, style);
  }

  //! Strokes the given `path` translated by `origin` with the default stroke style.
  BL_INLINE_NODEBUG BLResult strokePath(const BLPoint& origin, const BLPathCore& path) noexcept {
    BL_CONTEXT_CALL_RETURN(strokePathD, impl, &origin, &path);
  }

  //! Strokes the given `path` translated by `origin` with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokePath(const BLPoint& origin, const BLPathCore& path, const StyleT& style) noexcept {
    return _strokePathD(origin, path, style);
  }

  //! Strokes the passed geometry specified by geometry `type` and `data` with the default stroke style.
  //!
  //! \note This function provides a low-level interface that can be used in cases in which geometry `type` and `data`
  //! parameters are passed to a wrapper function that just passes them to the rendering context. It's a good way of
  //! creating wrappers, but generally low-level for a general purpose use, so please use this with caution.
  BL_INLINE_NODEBUG BLResult strokeGeometry(BLGeometryType type, const void* data) noexcept {
    return _strokeGeometryOp(type, data);
  }

  //! Strokes the passed geometry specified by geometry `type` and `data` with an explicit stroke `style`.
  //!
  //! \note This function provides a low-level interface that can be used in cases in which geometry `type` and `data`
  //! parameters are passed to a wrapper function that just passes them to the rendering context. It's a good way of
  //! creating wrappers, but generally low-level for a general purpose use, so please use this with caution.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeGeometry(BLGeometryType type, const void* data, const StyleT& style) noexcept {
    return _strokeGeometryOp(type, data, style);
  }

  //! \}

  //! \name Stroke Text & Glyphs Operations
  //! \{

  //! Strokes UTF-8 encoded string passed as `text` and `size` by using the given `font` at `origin` (integer coordinates)
  //! with the default stroke style.
  //!
  //! \note The `size` parameter defaults to `SIZE_MAX`, which informs Blend2D that the input is a null terminated string.
  //! If you want to pass a non-null terminated string or a substring of an existing string, use either this function with
  //! a `size` parameter set (which contains the number of bytes of `text` string, and not the number of UTF-8 characters)
  //! or an overloaded function that accepts a convenience \ref BLStringView parameter instead of `text` and size`.
  BL_INLINE_NODEBUG BLResult strokeUtf8Text(const BLPointI& origin, const BLFontCore& font, const char* text, size_t size = SIZE_MAX) noexcept {
    BLStringView view{text, size};
    return _strokeTextOpI(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view);
  }

  //! Strokes UTF-8 encoded string passed as `text` and `size` by using the given `font` at `origin` (integer coordinates)
  //! with an explicit stroke `style`.
  //!
  //! \note Pass `SIZE_MAX` to `size` to inform Blend2D that the input is a null terminated string. If you want to pass
  //! a non-null terminated string or a substring of an existing string, use either this function with a `size` parameter
  //! set (which contains the number of bytes of `text` string, and not the number of UTF-8 characters) or an overloaded
  //! function that accepts a convenience \ref BLStringView parameter instead of `text` and size`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeUtf8Text(const BLPointI& origin, const BLFontCore& font, const char* text, size_t size, const StyleT& style) noexcept {
    BLStringView view{text, size};
    return _strokeTextOpI(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view, style);
  }

  //! Strokes UTF-8 encoded string passed as string `view` by using the given `font` at `origin` (floating point coordinates)
  //! with the default stroke style.
  BL_INLINE_NODEBUG BLResult strokeUtf8Text(const BLPointI& origin, const BLFontCore& font, const BLStringView& view) noexcept {
    return _strokeTextOpI(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view);
  }

  //! Strokes UTF-8 encoded string passed as string `view` by using the given `font` at `origin` (floating point coordinates)
  //! with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeUtf8Text(const BLPointI& origin, const BLFontCore& font, const BLStringView& view, const StyleT& style) noexcept {
    return _strokeTextOpI(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view, style);
  }

  //! Strokes UTF-8 encoded string passed as `text` and `size` by using the given `font` at `origin` (integer coordinates)
  //! with the default stroke style.
  //!
  //! \note The `size` parameter defaults to `SIZE_MAX`, which informs Blend2D that the input is a null terminated string.
  //! If you want to pass a non-null terminated string or a substring of an existing string, use either this function with
  //! a `size` parameter set (which contains the number of bytes of `text` string, and not the number of UTF-8 characters)
  //! or an overloaded function that accepts a convenience \ref BLStringView parameter instead of `text` and size`.
  BL_INLINE_NODEBUG BLResult strokeUtf8Text(const BLPoint& origin, const BLFontCore& font, const char* text, size_t size = SIZE_MAX) noexcept {
    BLStringView view{text, size};
    return _strokeTextOpD(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view);
  }

  //! Strokes UTF-8 encoded string passed as `text` and `size` by using the given `font` at `origin` (integer coordinates)
  //! with an explicit stroke `style`.
  //!
  //! \note Pass `SIZE_MAX` in `size` to inform Blend2D that the input is a null terminated string. If you want to pass
  //! a non-null terminated string or a substring of an existing string, use either this function with a `size` parameter
  //! set (which contains the number of bytes of `text` string, and not the number of UTF-8 characters) or an overloaded
  //! function that accepts a convenience \ref BLStringView parameter instead of `text` and size`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeUtf8Text(const BLPoint& origin, const BLFontCore& font, const char* text, size_t size, const StyleT& style) noexcept {
    BLStringView view{text, size};
    return _strokeTextOpD(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view, style);
  }

  //! Strokes UTF-8 encoded string passed as string `view` by using the given `font` at `origin` (floating point coordinates).
  BL_INLINE_NODEBUG BLResult strokeUtf8Text(const BLPoint& origin, const BLFontCore& font, const BLStringView& view) noexcept {
    return _strokeTextOpD(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view);
  }
  //! \overload
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeUtf8Text(const BLPoint& origin, const BLFontCore& font, const BLStringView& view, const StyleT& style) noexcept {
    return _strokeTextOpD(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view, style);
  }

  //! Strokes UTF-16 encoded string passed as `text` and `size` by using the given `font` at `origin` (integer coordinates)
  //! with the default stroke style.
  //!
  //! \note The `size` parameter defaults to `SIZE_MAX`, which informs Blend2D that the input is a null terminated UTF-16
  //! string. If you want to pass a non-null terminated string or a substring of an existing string, specify `size`
  //! parameter to the size of the `text` buffer in code units (the number of 16-bit values).
  BL_INLINE_NODEBUG BLResult strokeUtf16Text(const BLPointI& origin, const BLFontCore& font, const uint16_t* text, size_t size = SIZE_MAX) noexcept {
    BLArrayView<uint16_t> view{text, size};
    return _strokeTextOpI(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view);
  }

  //! Strokes UTF-16 encoded string passed as `text` and `size` by using the given `font` at `origin` (integer coordinates)
  //! with an explicit stroke `style`.
  //!
  //! \note Pass `SIZE_MAX` in `size` to inform Blend2D that the input is a null terminated string. If you want to pass a
  //! non-null terminated string or a substring of an existing string, specify `size` parameter to the size of the `text`
  //! buffer in code units (the number of 16-bit values).
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeUtf16Text(const BLPointI& origin, const BLFontCore& font, const uint16_t* text, size_t size, const StyleT& style) noexcept {
    BLArrayView<uint16_t> view{text, size};
    return _strokeTextOpI(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view, style);
  }

  //! Strokes UTF-16 encoded string passed as `text` and `size` by using the given `font` at `origin` (floating point coordinates)
  //! with the default stroke style.
  //!
  //! \note The `size` parameter defaults to `SIZE_MAX`, which informs Blend2D that the input is a null terminated UTF-16
  //! string. If you want to pass a non-null terminated string or a substring of an existing string, specify `size`
  //! parameter to the size of the `text` buffer in code units (the number of 16-bit values).
  BL_INLINE_NODEBUG BLResult strokeUtf16Text(const BLPoint& origin, const BLFontCore& font, const uint16_t* text, size_t size = SIZE_MAX) noexcept {
    BLArrayView<uint16_t> view{text, size};
    return _strokeTextOpD(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view);
  }

  //! Strokes UTF-16 encoded string passed as `text` and `size` by using the given `font` at `origin` (floating point coordinates)
  //! with an explicit stroke `style`.
  //!
  //! \note Pass `SIZE_MAX` in `size` to inform Blend2D that the input is a null terminated string. If you want to pass a
  //! non-null terminated string or a substring of an existing string, specify `size` parameter to the size of the `text`
  //! buffer in code units (the number of 16-bit values).
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeUtf16Text(const BLPoint& origin, const BLFontCore& font, const uint16_t* text, size_t size, const StyleT& style) noexcept {
    BLArrayView<uint16_t> view{text, size};
    return _strokeTextOpD(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view, style);
  }

  //! Strokes UTF-32 encoded string passed as `text` and `size` by using the given `font` at `origin` (integer coordinates)
  //! with the default stroke style.
  //!
  //! \note The `size` parameter defaults to `SIZE_MAX`, which informs Blend2D that the input is a null terminated UTF-32
  //! string. If you want to pass a non-null terminated string or a substring of an existing string, specify `size`
  //! parameter to the size of the `text` buffer in code units (the number of 32-bit values).
  BL_INLINE_NODEBUG BLResult strokeUtf32Text(const BLPointI& origin, const BLFontCore& font, const uint32_t* text, size_t size = SIZE_MAX) noexcept {
    BLArrayView<uint32_t> view{text, size};
    return _strokeTextOpI(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view);
  }

  //! Strokes UTF-32 encoded string passed as `text` and `size` by using the given `font` at `origin` (integer coordinates)
  //! with an explicit stroke `style`.
  //!
  //! \note Pass `SIZE_MAX` in `size` to inform Blend2D that the input is a null terminated string. If you want to pass a
  //! non-null terminated string or a substring of an existing string, specify `size` parameter to the size of the `text`
  //! buffer in code units (the number of 32-bit values).
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeUtf32Text(const BLPointI& origin, const BLFontCore& font, const uint32_t* text, size_t size, const StyleT& style) noexcept {
    BLArrayView<uint32_t> view{text, size};
    return _strokeTextOpI(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view, style);
  }

  //! Strokes UTF-32 encoded string passed as `text` and `size` by using the given `font` at `origin` (floating point coordinates)
  //! with the default stroke style.
  //!
  //! \note The `size` parameter defaults to `SIZE_MAX`, which informs Blend2D that the input is a null terminated UTF-32
  //! string. If you want to pass a non-null terminated string or a substring of an existing string, specify `size`
  //! parameter to the size of the `text` buffer in code units (the number of 32-bit values).
  BL_INLINE_NODEBUG BLResult strokeUtf32Text(const BLPoint& origin, const BLFontCore& font, const uint32_t* text, size_t size = SIZE_MAX) noexcept {
    BLArrayView<uint32_t> view{text, size};
    return _strokeTextOpD(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view);
  }

  //! Strokes UTF-32 encoded string passed as `text` and `size` by using the given `font` at `origin` (floating point coordinates)
  //! with an explicit stroke `style`.
  //!
  //! \note Pass `SIZE_MAX` in `size` to inform Blend2D that the input is a null terminated string. If you want to pass a
  //! non-null terminated string or a substring of an existing string, specify `size` parameter to the size of the `text`
  //! buffer in code units (the number of 32-bit values).
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeUtf32Text(const BLPoint& origin, const BLFontCore& font, const uint32_t* text, size_t size, const StyleT& style) noexcept {
    BLArrayView<uint32_t> view{text, size};
    return _strokeTextOpD(origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view, style);
  }

  //! Strokes a `glyphRun` by using the given `font` at `origin` (integer coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeGlyphRun(const BLPointI& origin, const BLFontCore& font, const BLGlyphRun& glyphRun) noexcept {
    return _strokeTextOpI(origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, &glyphRun);
  }

  //! Strokes a `glyphRun` by using the given `font` at `origin` (integer coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeGlyphRun(const BLPointI& origin, const BLFontCore& font, const BLGlyphRun& glyphRun, const StyleT& style) noexcept {
    return _strokeTextOpI(origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, &glyphRun, style);
  }

  //! Strokes the passed `glyphRun` by using the given `font` at `origin` (floating point coordinates) with the current stroke style.
  BL_INLINE_NODEBUG BLResult strokeGlyphRun(const BLPoint& origin, const BLFontCore& font, const BLGlyphRun& glyphRun) noexcept {
    return _strokeTextOpD(origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, &glyphRun);
  }

  //! Strokes the passed `glyphRun` by using the given `font` at `origin` (floating point coordinates) with an explicit stroke `style`.
  template<typename StyleT>
  BL_INLINE_NODEBUG BLResult strokeGlyphRun(const BLPoint& origin, const BLFontCore& font, const BLGlyphRun& glyphRun, const StyleT& style) noexcept {
    return _strokeTextOpD(origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, &glyphRun, style);
  }

  //! \}

  //! \name Image Blit Operations
  //! \{

  //! Blits source image `src` at coordinates specified by `origin` (int coordinates).
  BL_INLINE_NODEBUG BLResult blitImage(const BLPointI& origin, const BLImageCore& src) noexcept {
    BL_CONTEXT_CALL_RETURN(blitImageI, impl, &origin, &src, nullptr);
  }

  //! Blits an area in source image `src` specified by `srcArea` at coordinates specified by `origin` (int coordinates).
  BL_INLINE_NODEBUG BLResult blitImage(const BLPointI& origin, const BLImageCore& src, const BLRectI& srcArea) noexcept {
    BL_CONTEXT_CALL_RETURN(blitImageI, impl, &origin, &src, &srcArea);
  }

  //! Blits source image `src` at coordinates specified by `origin` (floating point coordinates).
  BL_INLINE_NODEBUG BLResult blitImage(const BLPoint& origin, const BLImageCore& src) noexcept {
    BL_CONTEXT_CALL_RETURN(blitImageD, impl, &origin, &src, nullptr);
  }

  //! Blits an area of source image `src` specified by `srcArea` at coordinates specified by `origin` (floating point coordinates).
  BL_INLINE_NODEBUG BLResult blitImage(const BLPoint& origin, const BLImageCore& src, const BLRectI& srcArea) noexcept {
    BL_CONTEXT_CALL_RETURN(blitImageD, impl, &origin, &src, &srcArea);
  }

  //! Blits a source image `src` scaled to fit into `rect` rectangle (int coordinates).
  BL_INLINE_NODEBUG BLResult blitImage(const BLRectI& rect, const BLImageCore& src) noexcept {
    BL_CONTEXT_CALL_RETURN(blitScaledImageI, impl, &rect, &src, nullptr);
  }

  //! Blits an area of source image `src` specified by `srcArea` scaled to fit into `rect` rectangle (int coordinates).
  BL_INLINE_NODEBUG BLResult blitImage(const BLRectI& rect, const BLImageCore& src, const BLRectI& srcArea) noexcept {
    BL_CONTEXT_CALL_RETURN(blitScaledImageI, impl, &rect, &src, &srcArea);
  }

  //! Blits a source image `src` scaled to fit into `rect` rectangle (floating point coordinates).
  BL_INLINE_NODEBUG BLResult blitImage(const BLRect& rect, const BLImageCore& src) noexcept {
    BL_CONTEXT_CALL_RETURN(blitScaledImageD, impl, &rect, &src, nullptr);
  }

  //! Blits an area of source image `src` specified by `srcArea` scaled to fit into `rect` rectangle (floating point coordinates).
  BL_INLINE_NODEBUG BLResult blitImage(const BLRect& rect, const BLImageCore& src, const BLRectI& srcArea) noexcept {
    BL_CONTEXT_CALL_RETURN(blitScaledImageD, impl, &rect, &src, &srcArea);
  }

  //! \}

  #undef BL_CONTEXT_CALL_RETURN
  #undef BL_CONTEXT_IMPL
};

//! \}
#endif

//! \}

#endif // BLEND2D_CONTEXT_H_INCLUDED
