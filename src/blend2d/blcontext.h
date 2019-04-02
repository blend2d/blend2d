// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLCONTEXT_H
#define BLEND2D_BLCONTEXT_H

#include "./blfont.h"
#include "./blgeometry.h"
#include "./blimage.h"
#include "./blmatrix.h"
#include "./blpath.h"
#include "./blrgba.h"
#include "./blregion.h"
#include "./blvariant.h"

//! \addtogroup blend2d_api_rendering
//! \{

// ============================================================================
// [Constants]
// ============================================================================

//! Rendering context type.
BL_DEFINE_ENUM(BLContextType) {
  //! No rendering context.
  BL_CONTEXT_TYPE_NONE = 0,
  //! Dummy rendering context.
  BL_CONTEXT_TYPE_DUMMY = 1,
  //! Raster rendering context.
  BL_CONTEXT_TYPE_RASTER = 2,
  //! Raster rendering context using asynchronous dispatch.
  BL_CONTEXT_TYPE_RASTER_ASYNC = 3,

  //! Count of rendering context types.
  BL_CONTEXT_TYPE_COUNT = 4
};

//! Rendering context hint.
BL_DEFINE_ENUM(BLContextHint) {
  //! Rendering quality.
  BL_CONTEXT_HINT_RENDERING_QUALITY = 0,
  //! Gradient quality.
  BL_CONTEXT_HINT_GRADIENT_QUALITY = 1,
  //! Pattern quality.
  BL_CONTEXT_HINT_PATTERN_QUALITY = 2,

  //! Count of rendering context hints.
  BL_CONTEXT_HINT_COUNT = 8
};

//! Describes a rendering operation type - fill or stroke.
//!
//! The rendering context allows to get and set fill & stroke options directly
//! or via "op" functions that take the rendering operation type and dispatch
//! to the right function.
BL_DEFINE_ENUM(BLContextOpType) {
  //! Fill operation type.
  BL_CONTEXT_OP_TYPE_FILL = 0,
  //! Stroke operation type.
  BL_CONTEXT_OP_TYPE_STROKE = 1,

  //! Count of rendering operations.
  BL_CONTEXT_OP_TYPE_COUNT = 2
};

//! Rendering context flush-flags, use with `BLContext::flush()`.
BL_DEFINE_ENUM(BLContextFlushFlags) {
  //! Wait for completion (block).
  BL_CONTEXT_FLUSH_SYNC = 0x80000000u
};

//! Rendering context create-flags.
BL_DEFINE_ENUM(BLContextCreateFlags) {
  //! Create isolated context with own JIT runtime (testing).
  BL_CONTEXT_CREATE_FLAG_ISOLATED_RUNTIME = 0x10000000u,
  //! Override CPU features when creating isolated context.
  BL_CONTEXT_CREATE_FLAG_OVERRIDE_FEATURES = 0x20000000u
};

//! Clip operation.
BL_DEFINE_ENUM(BLClipOp) {
  //! Replaces the current clip area.
  BL_CLIP_OP_REPLACE = 0,
  //! Intersects with the current clip area.
  BL_CLIP_OP_INTERSECT = 1,

  //! Count of clip operations.
  BL_CLIP_OP_COUNT = 2
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
  //! Multiply.
  BL_COMP_OP_MULTIPLY = 14,
  //! Screen.
  BL_COMP_OP_SCREEN = 15,
  //! Overlay.
  BL_COMP_OP_OVERLAY = 16,
  //! Darken.
  BL_COMP_OP_DARKEN = 17,
  //! Lighten.
  BL_COMP_OP_LIGHTEN = 18,
  //! Color dodge.
  BL_COMP_OP_COLOR_DODGE = 19,
  //! Color burn.
  BL_COMP_OP_COLOR_BURN = 20,
  //! Linear burn.
  BL_COMP_OP_LINEAR_BURN = 21,
  //! Linear light.
  BL_COMP_OP_LINEAR_LIGHT = 22,
  //! Pin light.
  BL_COMP_OP_PIN_LIGHT = 23,
  //! Hard-light.
  BL_COMP_OP_HARD_LIGHT = 24,
  //! Soft-light.
  BL_COMP_OP_SOFT_LIGHT = 25,
  //! Difference.
  BL_COMP_OP_DIFFERENCE = 26,
  //! Exclusion.
  BL_COMP_OP_EXCLUSION = 27,

  //! Count of composition & blending operators.
  BL_COMP_OP_COUNT = 28
};

//! Gradient rendering quality.
BL_DEFINE_ENUM(BLGradientQuality) {
  //! Nearest neighbor.
  BL_GRADIENT_QUALITY_NEAREST = 0,

  //! Count of gradient quality options.
  BL_GRADIENT_QUALITY_COUNT = 1
};

//! Pattern quality.
BL_DEFINE_ENUM(BLPatternQuality) {
  //! Nearest neighbor.
  BL_PATTERN_QUALITY_NEAREST = 0,
  //! Bilinear.
  BL_PATTERN_QUALITY_BILINEAR = 1,

  //! Count of pattern quality options.
  BL_PATTERN_QUALITY_COUNT = 2
};

//! Rendering quality.
BL_DEFINE_ENUM(BLRenderingQuality) {
  //! Render using anti-aliasing.
  BL_RENDERING_QUALITY_ANTIALIAS = 0,

  //! Count of rendering quality options.
  BL_RENDERING_QUALITY_COUNT = 1
};

// ============================================================================
// [BLContext - CreateOptions]
// ============================================================================

//! Information that can be used to customize the rendering context.
struct BLContextCreateOptions {
  //! Initialization flags.
  uint32_t flags;
  //! CPU features to use in isolated JIT runtime (if supported), only used
  //! when `flags` contains `BL_CONTEXT_CREATE_FLAG_OVERRIDE_FEATURES`.
  uint32_t cpuFeatures;
};

// ============================================================================
// [BLContext - Cookie]
// ============================================================================

//! Holds an arbitrary 128-bit value (cookie) that can be used to match other
//! cookies. Blend2D uses cookies in places where it allows to "lock" some
//! state that can only be unlocked by a matching cookie. Please don't confuse
//! cookies with a security of any kind, it's just an arbitrary data that must
//! match to proceed with a certain operation.
//!
//! Cookies can be used with `BLContext::save()` and `BLContext::restore()`
//! functions
struct BLContextCookie {
  uint64_t data[2];

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE bool operator==(const BLContextCookie& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLContextCookie& other) const noexcept { return !equals(other); }

  BL_INLINE bool empty() const noexcept {
    return this->data[0] == 0 && this->data[1] == 0;
  }

  BL_INLINE void reset() noexcept { reset(0, 0); }
  BL_INLINE void reset(const BLContextCookie& other) noexcept { reset(other.data[0], other.data[1]); }

  BL_INLINE void reset(uint64_t data0, uint64_t data1) noexcept {
    this->data[0] = data0;
    this->data[1] = data1;
  }

  BL_INLINE bool equals(const BLContextCookie& other) const noexcept {
    return blEquals(this->data[0], other.data[0]) &
           blEquals(this->data[1], other.data[1]);
  }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLContext - Hints]
// ============================================================================

//! Rendering context hints.
struct BLContextHints {
  union {
    struct {
      uint8_t renderingQuality;
      uint8_t gradientQuality;
      uint8_t patternQuality;
    };

    uint8_t hints[BL_CONTEXT_HINT_COUNT];
  };

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLContext - State]
// ============================================================================

//! Rendering context state.
//!
//! This state is not meant to be created by users, it's only provided so users
//! can access it inline and possibly inspect it.
struct BLContextState {
  union {
    //! Current context hints.
    BLContextHints hints;
    //! Flattened `BLContextHints` struct so the members can be accessed directly.
    struct {
      uint8_t renderingQuality;
      uint8_t gradientQuality;
      uint8_t patternQuality;
    };
  };

  //! Current composition operator.
  uint8_t compOp;
  //! Current fill rule.
  uint8_t fillRule;

  //! Either `opStyleType` or decomposed `fillStyleType` and `strokeStyleType`.
  union {
    //! Current type of a style for fill and stroke operations.
    uint8_t opStyleType[2];
    //! Flattened `opStyleType[]` so the members can be accessed directly.
    struct {
      //! Current type of a style for fill operations.
      uint8_t fillStyleType;
      //! Current type of a style for stroke operations.
      uint8_t strokeStyleType;
    };
  };

  //! Reserved for future use, must be zero.
  uint8_t reserved[4];

  //! Approximation options.
  BLApproximationOptions approximationOptions;

  //! Current global alpha value [0, 1].
  double globalAlpha;
  //! Either opAlpha[] array of decomposed `fillAlpha` and `strokeAlpha`.
  union {
    //! Current fill or stroke alpha by slot type.
    double opAlpha[2];
    //! Flattened `opAlpha[]` so the members can be accessed directly.
    struct {
      //! Current fill alpha value [0, 1].
      double fillAlpha;
      //! Current stroke alpha value [0, 1].
      double strokeAlpha;
    };
  };

  //! Current stroke options.
  BL_TYPED_MEMBER(BLStrokeOptionsCore, BLStrokeOptions, strokeOptions);

  //! Current meta transformation matrix.
  BLMatrix2D metaMatrix;
  //! Current user transformation matrix.
  BLMatrix2D userMatrix;

  //! Count of saved states in the context.
  size_t savedStateCount;

  BL_HAS_TYPED_MEMBERS(BLContextState)
};

// ============================================================================
// [BLContext - Core]
// ============================================================================

//! Rendering context [C Interface - Virtual Function Table].
struct BLContextVirt {
  BLResult (BL_CDECL* destroy                 )(BLContextImpl* impl) BL_NOEXCEPT;
  BLResult (BL_CDECL* flush                   )(BLContextImpl* impl, uint32_t flags) BL_NOEXCEPT;

  BLResult (BL_CDECL* save                    )(BLContextImpl* impl, BLContextCookie* cookie) BL_NOEXCEPT;
  BLResult (BL_CDECL* restore                 )(BLContextImpl* impl, const BLContextCookie* cookie) BL_NOEXCEPT;

  BLResult (BL_CDECL* matrixOp                )(BLContextImpl* impl, uint32_t opType, const void* opData) BL_NOEXCEPT;
  BLResult (BL_CDECL* userToMeta              )(BLContextImpl* impl) BL_NOEXCEPT;

  BLResult (BL_CDECL* setHint                 )(BLContextImpl* impl, uint32_t hintType, uint32_t value) BL_NOEXCEPT;
  BLResult (BL_CDECL* setHints                )(BLContextImpl* impl, const BLContextHints* hints) BL_NOEXCEPT;
  BLResult (BL_CDECL* setFlattenMode          )(BLContextImpl* impl, uint32_t mode) BL_NOEXCEPT;
  BLResult (BL_CDECL* setFlattenTolerance     )(BLContextImpl* impl, double tolerance) BL_NOEXCEPT;
  BLResult (BL_CDECL* setApproximationOptions )(BLContextImpl* impl, const BLApproximationOptions* options) BL_NOEXCEPT;

  BLResult (BL_CDECL* setCompOp               )(BLContextImpl* impl, uint32_t compOp) BL_NOEXCEPT;
  BLResult (BL_CDECL* setGlobalAlpha          )(BLContextImpl* impl, double alpha) BL_NOEXCEPT;

  BLResult (BL_CDECL* setFillRule             )(BLContextImpl* impl, uint32_t fillRule) BL_NOEXCEPT;

  BLResult (BL_CDECL* setStrokeWidth          )(BLContextImpl* impl, double width) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStrokeMiterLimit     )(BLContextImpl* impl, double miterLimit) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStrokeCap            )(BLContextImpl* impl, uint32_t position, uint32_t strokeCap) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStrokeCaps           )(BLContextImpl* impl, uint32_t strokeCap) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStrokeJoin           )(BLContextImpl* impl, uint32_t strokeJoin) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStrokeDashOffset     )(BLContextImpl* impl, double dashOffset) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStrokeDashArray      )(BLContextImpl* impl, const BLArrayCore* dashArray) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStrokeTransformOrder )(BLContextImpl* impl, uint32_t transformOrder) BL_NOEXCEPT;
  BLResult (BL_CDECL* setStrokeOptions        )(BLContextImpl* impl, const BLStrokeOptionsCore* options) BL_NOEXCEPT;

  union {
    struct {
      BLResult (BL_CDECL* setFillAlpha        )(BLContextImpl* impl, double alpha) BL_NOEXCEPT;
      BLResult (BL_CDECL* setStrokeAlpha      )(BLContextImpl* impl, double alpha) BL_NOEXCEPT;
      BLResult (BL_CDECL* getFillStyle        )(BLContextImpl* impl, void* object) BL_NOEXCEPT;
      BLResult (BL_CDECL* getStrokeStyle      )(BLContextImpl* impl, void* object) BL_NOEXCEPT;
      BLResult (BL_CDECL* getFillStyleRgba32  )(BLContextImpl* impl, uint32_t* rgba32) BL_NOEXCEPT;
      BLResult (BL_CDECL* getStrokeStyleRgba32)(BLContextImpl* impl, uint32_t* rgba32) BL_NOEXCEPT;
      BLResult (BL_CDECL* getFillStyleRgba64  )(BLContextImpl* impl, uint64_t* rgba64) BL_NOEXCEPT;
      BLResult (BL_CDECL* getStrokeStyleRgba64)(BLContextImpl* impl, uint64_t* rgba64) BL_NOEXCEPT;
      BLResult (BL_CDECL* setFillStyle        )(BLContextImpl* impl, const void* object) BL_NOEXCEPT;
      BLResult (BL_CDECL* setStrokeStyle      )(BLContextImpl* impl, const void* object) BL_NOEXCEPT;
      BLResult (BL_CDECL* setFillStyleRgba32  )(BLContextImpl* impl, uint32_t rgba32) BL_NOEXCEPT;
      BLResult (BL_CDECL* setStrokeStyleRgba32)(BLContextImpl* impl, uint32_t rgba32) BL_NOEXCEPT;
      BLResult (BL_CDECL* setFillStyleRgba64  )(BLContextImpl* impl, uint64_t rgba64) BL_NOEXCEPT;
      BLResult (BL_CDECL* setStrokeStyleRgba64)(BLContextImpl* impl, uint64_t rgba64) BL_NOEXCEPT;
    };
    struct {
      // Allows to dispatch fill/stroke by `BLContextOpType`.
      BLResult (BL_CDECL* setOpAlpha[2]       )(BLContextImpl* impl, double alpha) BL_NOEXCEPT;
      BLResult (BL_CDECL* getOpStyle[2]       )(BLContextImpl* impl, void* object) BL_NOEXCEPT;
      BLResult (BL_CDECL* getOpStyleRgba32[2] )(BLContextImpl* impl, uint32_t* rgba32) BL_NOEXCEPT;
      BLResult (BL_CDECL* getOpStyleRgba64[2] )(BLContextImpl* impl, uint64_t* rgba64) BL_NOEXCEPT;
      BLResult (BL_CDECL* setOpStyle[2]       )(BLContextImpl* impl, const void* object) BL_NOEXCEPT;
      BLResult (BL_CDECL* setOpStyleRgba32[2] )(BLContextImpl* impl, uint32_t rgba32) BL_NOEXCEPT;
      BLResult (BL_CDECL* setOpStyleRgba64[2] )(BLContextImpl* impl, uint64_t rgba64) BL_NOEXCEPT;
    };
  };

  BLResult (BL_CDECL* clipToRectI             )(BLContextImpl* impl, const BLRectI* rect) BL_NOEXCEPT;
  BLResult (BL_CDECL* clipToRectD             )(BLContextImpl* impl, const BLRect* rect) BL_NOEXCEPT;
  BLResult (BL_CDECL* restoreClipping         )(BLContextImpl* impl) BL_NOEXCEPT;

  BLResult (BL_CDECL* clearAll                )(BLContextImpl* impl) BL_NOEXCEPT;
  BLResult (BL_CDECL* clearRectI              )(BLContextImpl* impl, const BLRectI* rect) BL_NOEXCEPT;
  BLResult (BL_CDECL* clearRectD              )(BLContextImpl* impl, const BLRect* rect) BL_NOEXCEPT;

  BLResult (BL_CDECL* fillAll                 )(BLContextImpl* impl) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillRectI               )(BLContextImpl* impl, const BLRectI* rect) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillRectD               )(BLContextImpl* impl, const BLRect* rect) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillPathD               )(BLContextImpl* impl, const BLPathCore* path) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillGeometry            )(BLContextImpl* impl, uint32_t geometryType, const void* geometryData) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillTextI               )(BLContextImpl* impl, const BLPointI* pt, const BLFontCore* font, const void* text, size_t size, uint32_t encoding) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillTextD               )(BLContextImpl* impl, const BLPoint* pt, const BLFontCore* font, const void* text, size_t size, uint32_t encoding) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillGlyphRunI           )(BLContextImpl* impl, const BLPointI* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) BL_NOEXCEPT;
  BLResult (BL_CDECL* fillGlyphRunD           )(BLContextImpl* impl, const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) BL_NOEXCEPT;

  BLResult (BL_CDECL* strokeRectI             )(BLContextImpl* impl, const BLRectI* rect) BL_NOEXCEPT;
  BLResult (BL_CDECL* strokeRectD             )(BLContextImpl* impl, const BLRect*  rect) BL_NOEXCEPT;
  BLResult (BL_CDECL* strokePathD             )(BLContextImpl* impl, const BLPathCore* path) BL_NOEXCEPT;
  BLResult (BL_CDECL* strokeGeometry          )(BLContextImpl* impl, uint32_t geometryType, const void* geometryData) BL_NOEXCEPT;
  BLResult (BL_CDECL* strokeTextI             )(BLContextImpl* impl, const BLPointI* pt, const BLFontCore* font, const void* text, size_t size, uint32_t encoding) BL_NOEXCEPT;
  BLResult (BL_CDECL* strokeTextD             )(BLContextImpl* impl, const BLPoint* pt, const BLFontCore* font, const void* text, size_t size, uint32_t encoding) BL_NOEXCEPT;
  BLResult (BL_CDECL* strokeGlyphRunI         )(BLContextImpl* impl, const BLPointI* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) BL_NOEXCEPT;
  BLResult (BL_CDECL* strokeGlyphRunD         )(BLContextImpl* impl, const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) BL_NOEXCEPT;

  BLResult (BL_CDECL* blitImageI              )(BLContextImpl* impl, const BLPointI* pt, const BLImageCore* img, const BLRectI* imgArea) BL_NOEXCEPT;
  BLResult (BL_CDECL* blitImageD              )(BLContextImpl* impl, const BLPoint* pt, const BLImageCore* img, const BLRectI* imgArea) BL_NOEXCEPT;
  BLResult (BL_CDECL* blitScaledImageI        )(BLContextImpl* impl, const BLRectI* rect, const BLImageCore* img, const BLRectI* imgArea) BL_NOEXCEPT;
  BLResult (BL_CDECL* blitScaledImageD        )(BLContextImpl* impl, const BLRect* rect, const BLImageCore* img, const BLRectI* imgArea) BL_NOEXCEPT;
};

//! Rendering context [C Interface - Impl].
struct BLContextImpl {
  //! Virtual function table.
  const BLContextVirt* virt;
  //! Current state of the context.
  const BLContextState* state;
  //! Reserved header for future use.
  void* reservedHeader[1];

  //! Reference count.
  volatile size_t refCount;
  //! Impl type.
  uint8_t implType;
  //! Impl traits.
  uint8_t implTraits;
  //! Memory pool data.
  uint16_t memPoolData;
  //! Type of the context, see `BLContextType`.
  uint32_t contextType;

  //! Current size of the target in abstract units, pixels if rendering to `BLImage`.
  BLSize targetSize;
};

//! Rendering context [C Interface - Core].
struct BLContextCore {
  BLContextImpl* impl;
};

// ============================================================================
// [BLContext - C++]
// ============================================================================

#ifdef __cplusplus
//! Rendering context [C++ API].
class BLContext : public BLContextCore {
public:
  //! \cond INTERNAL
  static constexpr const uint32_t kImplType = BL_IMPL_TYPE_CONTEXT;
  //! \endcond

  //! \name Constructors and Destructors
  //! \{

  BL_INLINE BLContext() noexcept { this->impl = none().impl; }
  BL_INLINE BLContext(BLContext&& other) noexcept { blVariantInitMove(this, &other); }
  BL_INLINE BLContext(const BLContext& other) noexcept { blVariantInitWeak(this, &other); }
  BL_INLINE explicit BLContext(BLContextImpl* impl) noexcept { this->impl = impl; }

  BL_INLINE explicit BLContext(BLImage& target) noexcept { blContextInitAs(this, &target, nullptr); }
  BL_INLINE BLContext(BLImage& target, const BLContextCreateOptions& options) noexcept { blContextInitAs(this, &target, &options); }
  BL_INLINE BLContext(BLImage& target, const BLContextCreateOptions* options) noexcept { blContextInitAs(this, &target, options); }

  BL_INLINE ~BLContext() noexcept { blContextReset(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE BLContext& operator=(BLContext&& other) noexcept { blContextAssignMove(this, &other); return *this; }
  BL_INLINE BLContext& operator=(const BLContext& other) noexcept { blContextAssignWeak(this, &other); return *this; }

  BL_INLINE bool operator==(const BLContext& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLContext& other) const noexcept { return !equals(other); }

  BL_INLINE explicit operator bool() const noexcept { return !isNone(); }

  //! \}

  //! \name Target Information
  //! \{

  //! Returns target size in abstract units (pixels in case of `BLImage`).
  BL_INLINE BLSize targetSize() const noexcept { return impl->targetSize; }
  //! Returns target width in abstract units (pixels in case of `BLImage`).
  BL_INLINE double targetWidth() const noexcept { return impl->targetSize.w; }
  //! Returns target height in abstract units (pixels in case of `BLImage`).
  BL_INLINE double targetHeight() const noexcept { return impl->targetSize.h; }

  //! \}

  //! \name Context Lifetime and Others
  //! \{

  //! Returns the type of this context, see `BLContextType`.
  BL_INLINE uint32_t contextType() const noexcept { return impl->contextType; }

  //! Gets whether the context is a built-in null instance.
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }

  BL_INLINE bool equals(const BLContext& other) const noexcept { return this->impl == other.impl; }

  BL_INLINE BLResult reset() noexcept { return blContextReset(this); }

  BL_INLINE BLResult assign(BLContext&& other) noexcept { return blContextAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLContext& other) noexcept { return blContextAssignWeak(this, &other); }

  //! Begins rendering to the given `image`.
  //!
  //! If this operation succeeds then the rendering context will have exclusive
  //! access to the image data. This means that no other renderer can use it
  //! during rendering.
  BL_INLINE BLResult begin(BLImage& image) noexcept { return blContextBegin(this, &image, nullptr); }
  //! \overload
  BL_INLINE BLResult begin(BLImage& image, const BLContextCreateOptions& options) noexcept { return blContextBegin(this, &image, &options); }
  //! \overload
  BL_INLINE BLResult begin(BLImage& image, const BLContextCreateOptions* options) noexcept { return blContextBegin(this, &image, options); }

  //! Waits for completion of all render commands and detaches the rendering
  //! context from the rendering target. After `end()` completes the rendering
  //! context implementation would be released and replaced by a built-in null
  //! instance (no context).
  BL_INLINE BLResult end() noexcept { return blContextEnd(this); }

  //! Flushes the context, see `BLContextFlushFlags`.
  BL_INLINE BLResult flush(uint32_t flags) noexcept { return impl->virt->flush(impl, flags); }

  //! \}

  //! \name State Management
  //! \{

  //! Returns the number of saved states in the context (0 means no saved states).
  BL_INLINE size_t savedStateCount() const noexcept { return impl->state->savedStateCount; }

  //! Saves the current rendering context state.
  //!
  //! Blend2D uses optimizations that make `save()` a cheap operation. Only core
  //! values are actually saved in `save()`, others will only be saved if they
  //! are modified. This means that consecutive calls to `save()` and `restore()`
  //! do almost nothing.
  BL_INLINE BLResult save() noexcept { return impl->virt->save(impl, nullptr); }

  //! Saves the current rendering context state and creates a restoration `cookie`.
  //!
  //! If you use a `cookie` to save a state you have to use the same cookie to
  //! restore it otherwise the `restore()` would fail. Please note that cookies
  //! are not a means of security, they are provided for making it easier to
  //! guarantee that a code that you may not control won't break your context.
  BL_INLINE BLResult save(BLContextCookie& cookie) noexcept { return impl->virt->save(impl, &cookie); }

  //! Restores the top-most saved context-state.
  //!
  //! Possible return conditions:
  //!
  //!   * `BL_SUCCESS` - State was restored successfully.
  //!   * `BL_ERROR_NO_STATES_TO_RESTORE` - There are no saved states to restore.
  //!   * `BL_ERROR_NO_MATCHING_COOKIE` - Previous state was saved with cookie,
  //!     which was not provided. You would need the correct cookie to restore
  //!     such state.
  BL_INLINE BLResult restore() noexcept { return impl->virt->restore(impl, nullptr); }

  //! Restores to the point that matches the given `cookie`.
  //!
  //! More than one state can be restored in case that the `cookie` points to
  //! some previous state in the list.
  //!
  //! Possible return conditions:
  //!
  //!   * `BL_SUCCESS` - Matching state was restored successfully.
  //!   * `BL_ERROR_NO_STATES_TO_RESTORE` - There are no saved states to restore.
  //!   * `BL_ERROR_NO_MATCHING_COOKIE` - The cookie did't match any saved state.
  BL_INLINE BLResult restore(const BLContextCookie& cookie) noexcept { return impl->virt->restore(impl, &cookie); }

  //! \}

  //! \name Transformations
  //! \{

  //! Returns meta-matrix.
  //!
  //! Meta matrix is a core transformation matrix that is normally not changed
  //! by transformations applied to the context. Instead it acts as a secondary
  //! matrix used to create the final transformation matrix from meta and user
  //! matrices.
  //!
  //! Meta matrix can be used to scale the whole context for HI-DPI rendering
  //! or to change the orientation of the image being rendered, however, the
  //! number of use-cases is unlimited.
  //!
  //! To change the meta-matrix you must first change user-matrix and then call
  //! `userToMeta()`, which would update meta-matrix and clear user-matrix.
  //!
  //! See `userMatrix()` and `userToMeta()`.
  BL_INLINE const BLMatrix2D& metaMatrix() const noexcept { return impl->state->metaMatrix; }

  //! Returns user-matrix.
  //!
  //! User matrix contains all transformations that happened to the rendering
  //! context unless the context was restored or `userToMeta()` was called.
  BL_INLINE const BLMatrix2D& userMatrix() const noexcept { return impl->state->userMatrix; }

  //! Applies a matrix operation to the current transformation matrix (internal).
  BL_INLINE BLResult _applyMatrixOp(uint32_t opType, const void* opData) noexcept {
    return impl->virt->matrixOp(impl, opType, opData);
  }

  //! \cond INTERNAL
  //! Applies a matrix operation to the current transformation matrix (internal).
  template<typename... Args>
  BL_INLINE BLResult _applyMatrixOpV(uint32_t opType, Args&&... args) noexcept {
    double opData[] = { double(args)... };
    return impl->virt->matrixOp(impl, opType, opData);
  }
  //! \endcond

  //! Sets user matrix to `m`.
  BL_INLINE BLResult setMatrix(const BLMatrix2D& m) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_ASSIGN, &m); }
  //! Resets user matrix to identity.
  BL_INLINE BLResult resetMatrix() noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_RESET, nullptr); }

  BL_INLINE BLResult translate(double x, double y) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_TRANSLATE, x, y); }
  BL_INLINE BLResult translate(const BLPointI& p) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_TRANSLATE, p.x, p.y); }
  BL_INLINE BLResult translate(const BLPoint& p) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_TRANSLATE, &p); }
  BL_INLINE BLResult scale(double xy) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_SCALE, xy, xy); }
  BL_INLINE BLResult scale(double x, double y) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_SCALE, x, y); }
  BL_INLINE BLResult scale(const BLPointI& p) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_SCALE, p.x, p.y); }
  BL_INLINE BLResult scale(const BLPoint& p) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_SCALE, &p); }
  BL_INLINE BLResult skew(double x, double y) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_SKEW, x, y); }
  BL_INLINE BLResult skew(const BLPoint& p) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_SKEW, &p); }
  BL_INLINE BLResult rotate(double angle) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_ROTATE, &angle); }
  BL_INLINE BLResult rotate(double angle, double x, double y) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_ROTATE_PT, angle, x, y); }
  BL_INLINE BLResult rotate(double angle, const BLPoint& p) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_ROTATE_PT, angle, p.x, p.y); }
  BL_INLINE BLResult rotate(double angle, const BLPointI& p) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_ROTATE_PT, angle, p.x, p.y); }
  BL_INLINE BLResult transform(const BLMatrix2D& m) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_TRANSFORM, &m); }

  BL_INLINE BLResult postTranslate(double x, double y) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_POST_TRANSLATE, x, y); }
  BL_INLINE BLResult postTranslate(const BLPointI& p) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_POST_TRANSLATE, p.x, p.y); }
  BL_INLINE BLResult postTranslate(const BLPoint& p) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_POST_TRANSLATE, &p); }
  BL_INLINE BLResult postScale(double xy) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_POST_SCALE, xy, xy); }
  BL_INLINE BLResult postScale(double x, double y) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_POST_SCALE, x, y); }
  BL_INLINE BLResult postScale(const BLPointI& p) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_POST_SCALE, p.x, p.y); }
  BL_INLINE BLResult postScale(const BLPoint& p) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_POST_SCALE, &p); }
  BL_INLINE BLResult postSkew(double x, double y) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_POST_SKEW, x, y); }
  BL_INLINE BLResult postSkew(const BLPoint& p) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_POST_SKEW, &p); }
  BL_INLINE BLResult postRotate(double angle) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_POST_ROTATE, &angle); }
  BL_INLINE BLResult postRotate(double angle, double x, double y) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_POST_ROTATE_PT, angle, x, y); }
  BL_INLINE BLResult postRotate(double angle, const BLPoint& p) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_POST_ROTATE_PT, angle, p.x, p.y); }
  BL_INLINE BLResult postRotate(double angle, const BLPointI& p) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_POST_ROTATE_PT, angle, p.x, p.y); }
  BL_INLINE BLResult postTransform(const BLMatrix2D& m) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_POST_TRANSFORM, &m); }

  //! Store the result of combining the current `MetaMatrix` and `UserMatrix`
  //! to `MetaMatrix` and reset `UserMatrix` to identity as shown below:
  //!
  //! ```
  //! MetaMatrix = MetaMatrix x UserMatrix
  //! UserMatrix = Identity
  //! ```
  //!
  //! Please note that this operation is irreversible. The only way to restore
  //! both matrices to the state before the call to `userToMeta()` is to use
  //! `save()` and `restore()` functions.
  BL_INLINE BLResult userToMeta() noexcept { return impl->virt->userToMeta(impl); }

  //! \}

  //! \name Rendering Hints
  //! \{

  //! Returns rendering hints.
  BL_INLINE const BLContextHints& hints() const noexcept { return impl->state->hints; }

  //! Sets the given rendering hint `hintType` to the `value`.
  BL_INLINE BLResult setHint(uint32_t hintType, uint32_t value) noexcept { return impl->virt->setHint(impl, hintType, value); }
  BL_INLINE BLResult setHints(const BLContextHints& hints) noexcept { return impl->virt->setHints(impl, &hints); }

  BL_INLINE BLResult setRenderingQuality(uint32_t value) noexcept { return setHint(BL_CONTEXT_HINT_RENDERING_QUALITY, value); }
  BL_INLINE BLResult setGradientQuality(uint32_t value) noexcept { return setHint(BL_CONTEXT_HINT_GRADIENT_QUALITY, value); }
  BL_INLINE BLResult setPatternQuality(uint32_t value) noexcept { return setHint(BL_CONTEXT_HINT_PATTERN_QUALITY, value); }

  //! \}

  //! \name Approximation Options
  //! \{

  //! Returns approximation options.
  BL_INLINE const BLApproximationOptions& approximationOptions() const noexcept { return impl->state->approximationOptions; }

  //! Returns flatten mode (how curves are flattened), see `BLFlattenMmode`.
  BL_INLINE uint32_t flattenMode() const noexcept { return impl->state->approximationOptions.flattenMode; }
  //! Sets flatten `mode` (how curves are flattened), see `BLFlattenMmode`.
  BL_INLINE BLResult setFlattenMode(uint32_t mode) noexcept { return impl->virt->setFlattenMode(impl, mode); }

  //! Returns tolerance used for curve flattening.
  BL_INLINE double flattenTolerance() const noexcept { return impl->state->approximationOptions.flattenTolerance; }
  //! Sets tolerance used for curve flattening.
  BL_INLINE BLResult setFlattenTolerance(double tolerance) noexcept { return impl->virt->setFlattenTolerance(impl, tolerance); }

  //! \}

  //! \name Compositing Options
  //! \{

  //! Returns compositing operator.
  BL_INLINE uint32_t compOp() const noexcept { return impl->state->compOp; }
  //! Sets composition operator to `compOp`, see `BLCompOp`.
  BL_INLINE BLResult setCompOp(uint32_t compOp) noexcept { return impl->virt->setCompOp(impl, compOp); }

  //! Returns global alpha value.
  BL_INLINE double globalAlpha() const noexcept { return impl->state->globalAlpha; }
  //! Sets global alpha value.
  BL_INLINE BLResult setGlobalAlpha(double alpha) noexcept { return impl->virt->setGlobalAlpha(impl, alpha); }

  //! \}

  //! \name Fill Options
  //! \{

  //! Returns fill-rule, see `BLFillRule`.
  BL_INLINE uint32_t fillRule() const noexcept { return impl->state->fillRule; }
  //! Sets fill-rule, see `BLFillRule`.
  BL_INLINE BLResult setFillRule(uint32_t fillRule) noexcept { return impl->virt->setFillRule(impl, fillRule); }

  //! Returns fill alpha value.
  BL_INLINE double fillAlpha() const noexcept { return impl->state->fillAlpha; }
  //! Sets fill `alpha` value.
  BL_INLINE BLResult setFillAlpha(double alpha) noexcept { return impl->virt->setFillAlpha(impl, alpha); }

  BL_INLINE uint32_t fillStyleType() const noexcept { return impl->state->fillStyleType; }
  BL_INLINE BLResult getFillStyle(BLRgba32& out) noexcept { return impl->virt->getFillStyleRgba32(impl, &out.value); }
  BL_INLINE BLResult getFillStyle(BLRgba64& out) noexcept { return impl->virt->getFillStyleRgba64(impl, &out.value); }
  BL_INLINE BLResult getFillStyle(BLPattern& out) noexcept { return impl->virt->getFillStyle(impl, &out); }
  BL_INLINE BLResult getFillStyle(BLGradient& out) noexcept { return impl->virt->getFillStyle(impl, &out); }

  BL_INLINE BLResult setFillStyle(const BLGradient& gradient) noexcept { return impl->virt->setFillStyle(impl, &gradient); }
  BL_INLINE BLResult setFillStyle(const BLPattern& pattern) noexcept { return impl->virt->setFillStyle(impl, &pattern); }
  BL_INLINE BLResult setFillStyle(const BLImage& image) noexcept { return impl->virt->setFillStyle(impl, &image); }
  BL_INLINE BLResult setFillStyle(const BLVariant& variant) noexcept { return impl->virt->setFillStyle(impl, &variant); }
  BL_INLINE BLResult setFillStyle(const BLRgba32& rgba32) noexcept { return impl->virt->setFillStyleRgba32(impl, rgba32.value); }
  BL_INLINE BLResult setFillStyle(const BLRgba64& rgba64) noexcept { return impl->virt->setFillStyleRgba64(impl, rgba64.value); }

  //! \}

  //! \name Stroke Options
  //! \{

  //! Returns stroke width.
  BL_INLINE double strokeWidth() const noexcept { return impl->state->strokeOptions.width; }
  //! Returns stroke miter-limit.
  BL_INLINE double strokeMiterLimit() const noexcept { return impl->state->strokeOptions.miterLimit; }
  //! Returns stroke join, see `BLStrokeJoin`.
  BL_INLINE uint32_t strokeJoin() const noexcept { return impl->state->strokeOptions.join; }
  //! Returns stroke start-cap, see `BLStrokeCap`.
  BL_INLINE uint32_t strokeStartCap() const noexcept { return impl->state->strokeOptions.startCap; }
  //! Returns stroke end-cap, see `BLStrokeCap`.
  BL_INLINE uint32_t strokeEndCap() const noexcept { return impl->state->strokeOptions.endCap; }
  //! Returns stroke dash-offset.
  BL_INLINE double strokeDashOffset() const noexcept { return impl->state->strokeOptions.dashOffset; }
  //! Returns stroke dash-array.
  BL_INLINE const BLArray<double>& strokeDashArray() const noexcept { return impl->state->strokeOptions.dashArray; }
  //! Returns stroke transform order, see `BLStrokeTransformOrder`.
  BL_INLINE uint32_t strokeTransformOrder() const noexcept { return impl->state->strokeOptions.transformOrder; }
  //! Returns stroke options as a reference to `BLStrokeOptions`.
  BL_INLINE const BLStrokeOptions& strokeOptions() const noexcept { return impl->state->strokeOptions; }

  //! Sets stroke width to `width`.
  BL_INLINE BLResult setStrokeWidth(double width) noexcept { return impl->virt->setStrokeWidth(impl, width); }
  //! Sets miter limit to `miterLimit`.
  BL_INLINE BLResult setStrokeMiterLimit(double miterLimit) noexcept { return impl->virt->setStrokeMiterLimit(impl, miterLimit); }
  //! Sets stroke join to `strokeJoin`, see `BLStrokeJoin`.
  BL_INLINE BLResult setStrokeJoin(uint32_t strokeJoin) noexcept { return impl->virt->setStrokeJoin(impl, strokeJoin); }
  //! Sets stroke cap of the specified `type` to `strokeCap`, see `BLStrokeCap`.
  BL_INLINE BLResult setStrokeCap(uint32_t type, uint32_t strokeCap) noexcept { return impl->virt->setStrokeCap(impl, type, strokeCap); }
  //! Sets stroke start cap to `strokeCap`, see `BLStrokeCap`.
  BL_INLINE BLResult setStrokeStartCap(uint32_t strokeCap) noexcept { return setStrokeCap(BL_STROKE_CAP_POSITION_START, strokeCap); }
  //! Sets stroke end cap to `strokeCap`, see `BLStrokeCap`.
  BL_INLINE BLResult setStrokeEndCap(uint32_t strokeCap) noexcept { return setStrokeCap(BL_STROKE_CAP_POSITION_END, strokeCap); }
  //! Sets all stroke caps to `strokeCap`, see `BLStrokeCap`.
  BL_INLINE BLResult setStrokeCaps(uint32_t strokeCap) noexcept { return impl->virt->setStrokeCaps(impl, strokeCap); }
  //! Sets stroke dash-offset to `dashOffset`.
  BL_INLINE BLResult setStrokeDashOffset(double dashOffset) noexcept { return impl->virt->setStrokeDashOffset(impl, dashOffset); }
  //! Sets stroke dash-array to `dashArray`.
  BL_INLINE BLResult setStrokeDashArray(const BLArray<double>& dashArray) noexcept { return impl->virt->setStrokeDashArray(impl, &dashArray); }
  //! Sets stroke transformation order to `transformOrder`, see `BLStrokeTransformOrder`.
  BL_INLINE BLResult setStrokeTransformOrder(uint32_t transformOrder) noexcept { return impl->virt->setStrokeTransformOrder(impl, transformOrder); }
  //! Sets all stroke `options`.
  BL_INLINE BLResult setStrokeOptions(const BLStrokeOptions& options) noexcept { return impl->virt->setStrokeOptions(impl, &options); }

  //! Returns stroke alpha value.
  BL_INLINE double strokeAlpha() const noexcept { return impl->state->strokeAlpha; }
  //! Sets stroke `alpha` value.
  BL_INLINE BLResult setStrokeAlpha(double alpha) noexcept { return impl->virt->setStrokeAlpha(impl, alpha); }

  BL_INLINE uint32_t strokeStyleType() const noexcept { return impl->state->strokeStyleType; }
  BL_INLINE BLResult getStrokeStyle(BLRgba32& out) noexcept { return impl->virt->getStrokeStyleRgba32(impl, &out.value); }
  BL_INLINE BLResult getStrokeStyle(BLRgba64& out) noexcept { return impl->virt->getStrokeStyleRgba64(impl, &out.value); }
  BL_INLINE BLResult getStrokeStyle(BLPattern& out) noexcept { return impl->virt->getStrokeStyle(impl, &out); }
  BL_INLINE BLResult getStrokeStyle(BLGradient& out) noexcept { return impl->virt->getStrokeStyle(impl, &out); }

  BL_INLINE BLResult setStrokeStyle(const BLRgba32& rgba32) noexcept { return impl->virt->setStrokeStyleRgba32(impl, rgba32.value); }
  BL_INLINE BLResult setStrokeStyle(const BLRgba64& rgba64) noexcept { return impl->virt->setStrokeStyleRgba64(impl, rgba64.value); }
  BL_INLINE BLResult setStrokeStyle(const BLImage& image) noexcept { return impl->virt->setStrokeStyle(impl, &image); }
  BL_INLINE BLResult setStrokeStyle(const BLPattern& pattern) noexcept { return impl->virt->setStrokeStyle(impl, &pattern); }
  BL_INLINE BLResult setStrokeStyle(const BLGradient& gradient) noexcept { return impl->virt->setStrokeStyle(impl, &gradient); }
  BL_INLINE BLResult setStrokeStyle(const BLVariant& variant) noexcept { return impl->virt->setStrokeStyle(impl, &variant); }

  //! \}

  //! \name Miscellaneous Options
  //! \{

  BL_INLINE uint32_t opStyleType(uint32_t op) const noexcept {
    return op <= BL_CONTEXT_OP_TYPE_COUNT ? uint32_t(impl->state->opStyleType[op]) : uint32_t(0);
  }

  BL_INLINE BLResult getOpStyle(uint32_t op, BLRgba32& out) noexcept {
    if (BL_UNLIKELY(op >= BL_CONTEXT_OP_TYPE_COUNT))
      return blTraceError(BL_ERROR_INVALID_VALUE);
    return impl->virt->getOpStyleRgba32[op](impl, &out.value);
  }

  BL_INLINE BLResult getOpStyle(uint32_t op, BLRgba64& out) noexcept {
    if (BL_UNLIKELY(op >= BL_CONTEXT_OP_TYPE_COUNT))
      return blTraceError(BL_ERROR_INVALID_VALUE);
    return impl->virt->getOpStyleRgba64[op](impl, &out.value);
  }

  BL_INLINE BLResult getOpStyle(uint32_t op, BLPattern& out) noexcept {
    if (BL_UNLIKELY(op >= BL_CONTEXT_OP_TYPE_COUNT))
      return blTraceError(BL_ERROR_INVALID_VALUE);
    return impl->virt->getOpStyle[op](impl, &out);
  }

  BL_INLINE BLResult getOpStyle(uint32_t op, BLGradient& out) noexcept {
    if (BL_UNLIKELY(op >= BL_CONTEXT_OP_TYPE_COUNT))
      return blTraceError(BL_ERROR_INVALID_VALUE);
    return impl->virt->getOpStyle[op](impl, &out);
  }

  BL_INLINE BLResult setOpStyle(uint32_t op, const BLGradient& gradient) noexcept {
    if (BL_UNLIKELY(op >= BL_CONTEXT_OP_TYPE_COUNT))
      return blTraceError(BL_ERROR_INVALID_VALUE);
    return impl->virt->setOpStyle[op](impl, &gradient);
  }

  BL_INLINE BLResult setOpStyle(uint32_t op, const BLPattern& pattern) noexcept {
    if (BL_UNLIKELY(op >= BL_CONTEXT_OP_TYPE_COUNT))
      return blTraceError(BL_ERROR_INVALID_VALUE);
    return impl->virt->setOpStyle[op](impl, &pattern);
  }

  BL_INLINE BLResult setOpStyle(uint32_t op, const BLImage& image) noexcept {
    if (BL_UNLIKELY(op >= BL_CONTEXT_OP_TYPE_COUNT))
      return blTraceError(BL_ERROR_INVALID_VALUE);
    return impl->virt->setOpStyle[op](impl, &image);
  }

  BL_INLINE BLResult setOpStyle(uint32_t op, const BLVariant& variant) noexcept {
    if (BL_UNLIKELY(op >= BL_CONTEXT_OP_TYPE_COUNT))
      return blTraceError(BL_ERROR_INVALID_VALUE);
    return impl->virt->setOpStyle[op](impl, &variant);
  }

  BL_INLINE BLResult setOpStyle(uint32_t op, const BLRgba32& rgba32) noexcept {
    if (BL_UNLIKELY(op >= BL_CONTEXT_OP_TYPE_COUNT))
      return blTraceError(BL_ERROR_INVALID_VALUE);
    return impl->virt->setOpStyleRgba32[op](impl, rgba32.value);
  }

  BL_INLINE BLResult setOpStyle(uint32_t op, const BLRgba64& rgba64) noexcept {
    if (BL_UNLIKELY(op >= BL_CONTEXT_OP_TYPE_COUNT))
      return blTraceError(BL_ERROR_INVALID_VALUE);
    return impl->virt->setOpStyleRgba64[op](impl, rgba64.value);
  }

  //! Returns fill or alpha value dependeing on the rendering operation `op`.
  //!
  //! The function behaves like `fillAlpha()` or `strokeAlpha()` depending on
  //! `op` value, see `BLContextOpType`.
  BL_INLINE double opAlpha(uint32_t op) const noexcept {
    return op < BL_CONTEXT_OP_TYPE_COUNT ? impl->state->opAlpha[op] : 0.0;
  }

  //! Set fill or stroke `alpha` value depending on the rendering operation `op`.
  //!
  //! The function behaves like `setFillAlpha()` or `setStrokeAlpha()` depending
  //! on `op` value, see `BLContextOpType`.
  BL_INLINE BLResult setOpAlpha(uint32_t op, double alpha) noexcept {
    if (BL_UNLIKELY(op >= BL_CONTEXT_OP_TYPE_COUNT))
      return blTraceError(BL_ERROR_INVALID_VALUE);
    return impl->virt->setOpAlpha[op](impl, alpha);
  }

  //! \}

  //! \name Clip Operations
  //! \{

  //! Restores clipping to the last saved state or to the context default
  //! clipping if there is no saved state.
  //!
  //! If there are no saved states then it resets clipping completely to the
  //! initial state that was used when the rendering context was created.
  BL_INLINE BLResult restoreClipping() noexcept { return impl->virt->restoreClipping(impl); }

  BL_INLINE BLResult clipToRect(const BLRectI& rect) noexcept { return impl->virt->clipToRectI(impl, &rect); }
  BL_INLINE BLResult clipToRect(const BLRect& rect) noexcept { return impl->virt->clipToRectD(impl, &rect); }
  BL_INLINE BLResult clipToRect(double x, double y, double w, double h) noexcept { return clipToRect(BLRect(x, y, w, h)); }

  //! \}

  //! \name Clear Operations
  //! \{

  //! Clear everything.
  BL_INLINE BLResult clearAll() noexcept { return impl->virt->clearAll(impl); }

  //! Clears a rectangle `rect`.
  BL_INLINE BLResult clearRect(const BLRectI& rect) noexcept { return impl->virt->clearRectI(impl, &rect); }
  //! Clears a rectangle `rect`.
  BL_INLINE BLResult clearRect(const BLRect& rect) noexcept { return impl->virt->clearRectD(impl, &rect); }
  //! \overload
  BL_INLINE BLResult clearRect(double x, double y, double w, double h) noexcept { return clearRect(BLRect(x, y, w, h)); }

  //! \}

  //! \name Fill Operations
  //! \{

  //! Fills the passed geometry specified by `geometryType` and `geometryData` [Internal].
  BL_INLINE BLResult fillGeometry(uint32_t geometryType, const void* geometryData) noexcept { return impl->virt->fillGeometry(impl, geometryType, geometryData); }

  //! Fills everything.
  BL_INLINE BLResult fillAll() noexcept { return impl->virt->fillAll(impl); }

  //! Fills a box.
  BL_INLINE BLResult fillBox(const BLBox& box) noexcept { return fillGeometry(BL_GEOMETRY_TYPE_BOXD, &box); }
  // \overload
  BL_INLINE BLResult fillBox(const BLBoxI& box) noexcept { return fillGeometry(BL_GEOMETRY_TYPE_BOXI, &box); }
  // \overload
  BL_INLINE BLResult fillBox(double x0, double y0, double x1, double y1) noexcept { return fillBox(BLBox(x0, y0, x1, y1)); }

  //! Fills a rectangle `rect`.
  BL_INLINE BLResult fillRect(const BLRectI& rect) noexcept { return impl->virt->fillRectI(impl, &rect); }
  //! Fills a rectangle `rect`.
  BL_INLINE BLResult fillRect(const BLRect& rect) noexcept { return impl->virt->fillRectD(impl, &rect); }
  //! \overload
  BL_INLINE BLResult fillRect(double x, double y, double w, double h) noexcept { return fillRect(BLRect(x, y, w, h)); }

  //! Fills a circle.
  BL_INLINE BLResult fillCircle(const BLCircle& circle) noexcept { return fillGeometry(BL_GEOMETRY_TYPE_CIRCLE, &circle); }
  //! \overload
  BL_INLINE BLResult fillCircle(double cx, double cy, double r) noexcept { return fillCircle(BLCircle(cx, cy, r)); }

  //! Fills an ellipse.
  BL_INLINE BLResult fillEllipse(const BLEllipse& ellipse) noexcept { return fillGeometry(BL_GEOMETRY_TYPE_ELLIPSE, &ellipse); }
  //! \overload
  BL_INLINE BLResult fillEllipse(double cx, double cy, double rx, double ry) noexcept { return fillEllipse(BLEllipse(cx, cy, rx, ry)); }

  //! Fills a rounded rectangle.
  BL_INLINE BLResult fillRoundRect(const BLRoundRect& rr) noexcept { return fillGeometry(BL_GEOMETRY_TYPE_ROUND_RECT, &rr); }
  //! \overload
  BL_INLINE BLResult fillRoundRect(const BLRect& rect, double r) noexcept { return fillRoundRect(BLRoundRect(rect.x, rect.y, rect.w, rect.h, r)); }
  //! \overload
  BL_INLINE BLResult fillRoundRect(const BLRect& rect, double rx, double ry) noexcept { return fillRoundRect(BLRoundRect(rect.x, rect.y, rect.w, rect.h, rx, ry)); }
  //! \overload
  BL_INLINE BLResult fillRoundRect(double x, double y, double w, double h, double r) noexcept { return fillRoundRect(BLRoundRect(x, y, w, h, r)); }
  //! \overload
  BL_INLINE BLResult fillRoundRect(double x, double y, double w, double h, double rx, double ry) noexcept { return fillRoundRect(BLRoundRect(x, y, w, h, rx, ry)); }

  //! Fills a chord.
  BL_INLINE BLResult fillChord(const BLArc& chord) noexcept { return fillGeometry(BL_GEOMETRY_TYPE_CHORD, &chord); }
  //! \overload
  BL_INLINE BLResult fillChord(double cx, double cy, double r, double start, double sweep) noexcept { return fillChord(BLArc(cx, cy, r, r, start, sweep)); }
  //! \overload
  BL_INLINE BLResult fillChord(double cx, double cy, double rx, double ry, double start, double sweep) noexcept { return fillChord(BLArc(cx, cy, rx, ry, start, sweep)); }

  //! Fills a pie.
  BL_INLINE BLResult fillPie(const BLArc& pie) noexcept { return fillGeometry(BL_GEOMETRY_TYPE_PIE, &pie); }
  //! \overload
  BL_INLINE BLResult fillPie(double cx, double cy, double r, double start, double sweep) noexcept { return fillPie(BLArc(cx, cy, r, r, start, sweep)); }
  //! \overload
  BL_INLINE BLResult fillPie(double cx, double cy, double rx, double ry, double start, double sweep) noexcept { return fillPie(BLArc(cx, cy, rx, ry, start, sweep)); }

  //! Fills a triangle.
  BL_INLINE BLResult fillTriangle(const BLTriangle& triangle) noexcept { return fillGeometry(BL_GEOMETRY_TYPE_TRIANGLE, &triangle); }
  //! \overload
  BL_INLINE BLResult fillTriangle(double x0, double y0, double x1, double y1, double x2, double y2) noexcept { return fillTriangle(BLTriangle(x0, y0, x1, y1, x2, y2)); }

  //! Fills a polygon.
  BL_INLINE BLResult fillPolygon(const BLArrayView<BLPoint>& poly) noexcept { return fillGeometry(BL_GEOMETRY_TYPE_POLYGOND, &poly); }
  //! \overload
  BL_INLINE BLResult fillPolygon(const BLPoint* poly, size_t n) noexcept { return fillPolygon(BLArrayView<BLPoint>{poly, n}); }

  //! Fills a polygon.
  BL_INLINE BLResult fillPolygon(const BLArrayView<BLPointI>& poly) noexcept { return fillGeometry(BL_GEOMETRY_TYPE_POLYGONI, &poly); }
  //! \overload
  BL_INLINE BLResult fillPolygon(const BLPointI* poly, size_t n) noexcept { return fillPolygon(BLArrayView<BLPointI>{poly, n}); }

  //! Fills an array of boxes.
  BL_INLINE BLResult fillBoxArray(const BLArrayView<BLBox>& array) noexcept { return fillGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXD, &array); }
  //! \overload
  BL_INLINE BLResult fillBoxArray(const BLBox* data, size_t n) noexcept { return fillBoxArray(BLArrayView<BLBox>{data, n}); }

  //! Fills an array of boxes.
  BL_INLINE BLResult fillBoxArray(const BLArrayView<BLBoxI>& array) noexcept { return fillGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXI, &array); }
  //! \overload
  BL_INLINE BLResult fillBoxArray(const BLBoxI* data, size_t n) noexcept { return fillBoxArray(BLArrayView<BLBoxI>{data, n}); }

  //! Fills an array of rectangles.
  BL_INLINE BLResult fillRectArray(const BLArrayView<BLRect>& array) noexcept { return fillGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTD, &array); }
  //! \overload
  BL_INLINE BLResult fillRectArray(const BLRect* data, size_t n) noexcept { return fillRectArray(BLArrayView<BLRect>{data, n}); }

  //! Fills an array of rectangles.
  BL_INLINE BLResult fillRectArray(const BLArrayView<BLRectI>& array) noexcept { return fillGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTI, &array); }
  //! \overload
  BL_INLINE BLResult fillRectArray(const BLRectI* data, size_t n) noexcept { return fillRectArray(BLArrayView<BLRectI>{data, n}); }

  //! Fills the given `region`.
  BL_INLINE BLResult fillRegion(const BLRegion& region) noexcept { return fillGeometry(BL_GEOMETRY_TYPE_REGION, &region); }

  //! Fills the given `path`.
  BL_INLINE BLResult fillPath(const BLPath& path) noexcept { return fillGeometry(BL_GEOMETRY_TYPE_PATH, &path); }

  //! Fills the passed UTF-8 text by using the given `font`.
  BL_INLINE BLResult fillUtf8Text(const BLPointI& dst, const BLFont& font, const char* text, size_t size = SIZE_MAX) noexcept {
    return impl->virt->fillTextI(impl, &dst, &font, text, size, BL_TEXT_ENCODING_UTF8);
  }

  //! Fills the passed UTF-8 text by using the given `font`.
  BL_INLINE BLResult fillUtf8Text(const BLPoint& dst, const BLFont& font, const char* text, size_t size = SIZE_MAX) noexcept {
    return impl->virt->fillTextD(impl, &dst, &font, text, size, BL_TEXT_ENCODING_UTF8);
  }

  //! Fills the passed UTF-16 text by using the given `font`.
  BL_INLINE BLResult fillUtf16Text(const BLPointI& dst, const BLFont& font, const uint16_t* text, size_t size = SIZE_MAX) noexcept {
    return impl->virt->fillTextI(impl, &dst, &font, text, size, BL_TEXT_ENCODING_UTF16);
  }

  //! Fills the passed UTF-16 text by using the given `font`.
  BL_INLINE BLResult fillUtf16Text(const BLPoint& dst, const BLFont& font, const uint16_t* text, size_t size = SIZE_MAX) noexcept {
    return impl->virt->fillTextD(impl, &dst, &font, text, size, BL_TEXT_ENCODING_UTF16);
  }

  //! Fills the passed UTF-32 text by using the given `font`.
  BL_INLINE BLResult fillUtf32Text(const BLPointI& dst, const BLFont& font, const uint32_t* text, size_t size = SIZE_MAX) noexcept {
    return impl->virt->fillTextI(impl, &dst, &font, text, size, BL_TEXT_ENCODING_UTF32);
  }

  //! Fills the passed UTF-32 text by using the given `font`.
  BL_INLINE BLResult fillUtf32Text(const BLPoint& dst, const BLFont& font, const uint32_t* text, size_t size = SIZE_MAX) noexcept {
    return impl->virt->fillTextD(impl, &dst, &font, text, size, BL_TEXT_ENCODING_UTF32);
  }

  //! Fills the passed `glyphRun` by using the given `font`.
  BL_INLINE BLResult fillGlyphRun(const BLPointI& dst, const BLFont& font, const BLGlyphRun& glyphRun) noexcept {
    return impl->virt->fillGlyphRunI(impl, &dst, &font, &glyphRun);
  }

  //! Fills the passed `glyphRun` by using the given `font`.
  BL_INLINE BLResult fillGlyphRun(const BLPoint& dst, const BLFont& font, const BLGlyphRun& glyphRun) noexcept {
    return impl->virt->fillGlyphRunD(impl, &dst, &font, &glyphRun);
  }

  //! \}

  //! \name Stroke Operations
  //! \{

  //! Strokes the passed geometry specified by `geometryType` and `geometryData` [Internal].
  BL_INLINE BLResult strokeGeometry(uint32_t geometryType, const void* geometryData) noexcept { return impl->virt->strokeGeometry(impl, geometryType, geometryData); }

  //! Strokes a box.
  BL_INLINE BLResult strokeBox(const BLBox& box) noexcept { return strokeGeometry(BL_GEOMETRY_TYPE_BOXD, &box); }
  // \overload
  BL_INLINE BLResult strokeBox(const BLBoxI& box) noexcept { return strokeGeometry(BL_GEOMETRY_TYPE_BOXI, &box); }
  // \overload
  BL_INLINE BLResult strokeBox(double x0, double y0, double x1, double y1) noexcept { return strokeBox(BLBox(x0, y0, x1, y1)); }
  // \overload
  BL_INLINE BLResult strokeBox(int x0, int y0, int x1, int y1) noexcept { return strokeBox(BLBoxI(x0, y0, x1, y1)); }

  //! \overload
  BL_INLINE BLResult strokeRect(const BLRectI& rect) noexcept { return impl->virt->strokeRectI(impl, &rect); }
  //! Strokes a rectangle.
  BL_INLINE BLResult strokeRect(const BLRect& rect) noexcept { return impl->virt->strokeRectD(impl, &rect); }
  //! \overload
  BL_INLINE BLResult strokeRect(double x, double y, double w, double h) noexcept { return strokeRect(BLRect(x, y, w, h)); }

  //! Strokes a line.
  BL_INLINE BLResult strokeLine(const BLLine& line) noexcept { return strokeGeometry(BL_GEOMETRY_TYPE_LINE, &line); }
  //! \overload
  BL_INLINE BLResult strokeLine(const BLPoint& p0, const BLPoint& p1) noexcept { return strokeLine(BLLine(p0.x, p0.y, p1.x, p1.y)); }
  //! \overload
  BL_INLINE BLResult strokeLine(double x0, double y0, double x1, double y1) noexcept { return strokeLine(BLLine(x0, y0, x1, y1)); }

  //! Strokes a circle.
  BL_INLINE BLResult strokeCircle(const BLCircle& circle) noexcept { return strokeGeometry(BL_GEOMETRY_TYPE_CIRCLE, &circle); }
  //! \overload
  BL_INLINE BLResult strokeCircle(double cx, double cy, double r) noexcept { return strokeCircle(BLCircle(cx, cy, r)); }

  //! Strokes an ellipse.
  BL_INLINE BLResult strokeEllipse(const BLEllipse& ellipse) noexcept { return strokeGeometry(BL_GEOMETRY_TYPE_ELLIPSE, &ellipse); }
  //! \overload
  BL_INLINE BLResult strokeEllipse(double cx, double cy, double rx, double ry) noexcept { return strokeEllipse(BLEllipse(cx, cy, rx, ry)); }

  //! Strokes a round.
  BL_INLINE BLResult strokeRoundRect(const BLRoundRect& rr) noexcept { return strokeGeometry(BL_GEOMETRY_TYPE_ROUND_RECT, &rr); }
  //! \overload
  BL_INLINE BLResult strokeRoundRect(const BLRect& rect, double r) noexcept { return strokeRoundRect(BLRoundRect(rect.x, rect.y, rect.w, rect.h, r)); }
  //! \overload
  BL_INLINE BLResult strokeRoundRect(const BLRect& rect, double rx, double ry) noexcept { return strokeRoundRect(BLRoundRect(rect.x, rect.y, rect.w, rect.h, rx, ry)); }
  //! \overload
  BL_INLINE BLResult strokeRoundRect(double x, double y, double w, double h, double r) noexcept { return strokeRoundRect(BLRoundRect(x, y, w, h, r)); }
  //! \overload
  BL_INLINE BLResult strokeRoundRect(double x, double y, double w, double h, double rx, double ry) noexcept { return strokeRoundRect(BLRoundRect(x, y, w, h, rx, ry)); }

  //! Strokes an arc.
  BL_INLINE BLResult strokeArc(const BLArc& arc) noexcept { return strokeGeometry(BL_GEOMETRY_TYPE_ARC, &arc); }
  //! \overload
  BL_INLINE BLResult strokeArc(double cx, double cy, double r, double start, double sweep) noexcept { return strokeArc(BLArc(cx, cy, r, r, start, sweep)); }
  //! \overload
  BL_INLINE BLResult strokeArc(double cx, double cy, double rx, double ry, double start, double sweep) noexcept { return strokeArc(BLArc(cx, cy, rx, ry, start, sweep)); }

  //! Strokes a chord.
  BL_INLINE BLResult strokeChord(const BLArc& chord) noexcept { return strokeGeometry(BL_GEOMETRY_TYPE_CHORD, &chord); }
  //! \overload
  BL_INLINE BLResult strokeChord(double cx, double cy, double r, double start, double sweep) noexcept { return strokeChord(BLArc(cx, cy, r, r, start, sweep)); }
  //! \overload
  BL_INLINE BLResult strokeChord(double cx, double cy, double rx, double ry, double start, double sweep) noexcept { return strokeChord(BLArc(cx, cy, rx, ry, start, sweep)); }

  //! Strokes a pie.
  BL_INLINE BLResult strokePie(const BLArc& pie) noexcept { return strokeGeometry(BL_GEOMETRY_TYPE_PIE, &pie); }
  //! \overload
  BL_INLINE BLResult strokePie(double cx, double cy, double r, double start, double sweep) noexcept { return strokePie(BLArc(cx, cy, r, r, start, sweep)); }
  //! \overload
  BL_INLINE BLResult strokePie(double cx, double cy, double rx, double ry, double start, double sweep) noexcept { return strokePie(BLArc(cx, cy, rx, ry, start, sweep)); }

  //! Strokes a triangle.
  BL_INLINE BLResult strokeTriangle(const BLTriangle& triangle) noexcept { return strokeGeometry(BL_GEOMETRY_TYPE_TRIANGLE, &triangle); }
  //! \overload
  BL_INLINE BLResult strokeTriangle(double x0, double y0, double x1, double y1, double x2, double y2) noexcept { return strokeTriangle(BLTriangle(x0, y0, x1, y1, x2, y2)); }

  //! Strokes a polyline.
  BL_INLINE BLResult strokePolyline(const BLArrayView<BLPoint>& poly) noexcept { return strokeGeometry(BL_GEOMETRY_TYPE_POLYLINED, &poly); }
  //! \overload
  BL_INLINE BLResult strokePolyline(const BLPoint* poly, size_t n) noexcept { return strokePolyline(BLArrayView<BLPoint>{poly, n}); }

  //! Strokes a polyline.
  BL_INLINE BLResult strokePolyline(const BLArrayView<BLPointI>& poly) noexcept { return strokeGeometry(BL_GEOMETRY_TYPE_POLYLINED, &poly); }
  //! \overload
  BL_INLINE BLResult strokePolyline(const BLPointI* poly, size_t n) noexcept { return strokePolyline(BLArrayView<BLPointI>{poly, n}); }

  //! Strokes a polygon.
  BL_INLINE BLResult strokePolygon(const BLArrayView<BLPoint>& poly) noexcept { return strokeGeometry(BL_GEOMETRY_TYPE_POLYGOND, &poly); }
  //! \overload
  BL_INLINE BLResult strokePolygon(const BLPoint* poly, size_t n) noexcept { return strokePolygon(BLArrayView<BLPoint>{poly, n}); }

  //! Strokes a polygon.
  BL_INLINE BLResult strokePolygon(const BLArrayView<BLPointI>& poly) noexcept { return strokeGeometry(BL_GEOMETRY_TYPE_POLYGONI, &poly); }
  //! \overload
  BL_INLINE BLResult strokePolygon(const BLPointI* poly, size_t n) noexcept { return strokePolygon(BLArrayView<BLPointI>{poly, n}); }

  //! Strokes an array of boxes.
  BL_INLINE BLResult strokeBoxArray(const BLArrayView<BLBox>& array) noexcept { return strokeGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXD, &array); }
  //! \overload
  BL_INLINE BLResult strokeBoxArray(const BLBox* data, size_t n) noexcept { return strokeBoxArray(BLArrayView<BLBox>{data, n}); }

  //! Strokes an array of boxes.
  BL_INLINE BLResult strokeBoxArray(const BLArrayView<BLBoxI>& array) noexcept { return strokeGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXI, &array); }
  //! \overload
  BL_INLINE BLResult strokeBoxArray(const BLBoxI* data, size_t n) noexcept { return strokeBoxArray(BLArrayView<BLBoxI>{data, n}); }

  //! Strokes an array of rectangles.
  BL_INLINE BLResult strokeRectArray(const BLArrayView<BLRect>& array) noexcept { return strokeGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTD, &array); }
  //! \overload
  BL_INLINE BLResult strokeRectArray(const BLRect* data, size_t n) noexcept { return strokeRectArray(BLArrayView<BLRect>{data, n}); }

  //! Strokes an array of rectangles.
  BL_INLINE BLResult strokeRectArray(const BLArrayView<BLRectI>& array) noexcept { return strokeGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTI, &array); }
  //! \overload
  BL_INLINE BLResult strokeRectArray(const BLRectI* data, size_t n) noexcept { return strokeRectArray(BLArrayView<BLRectI>{data, n}); }

  //! Strokes a path.
  BL_INLINE BLResult strokePath(const BLPath& path) noexcept { return strokeGeometry(BL_GEOMETRY_TYPE_PATH, &path); }

  //! Strokes the passed UTF-8 text by using the given `font`.
  BL_INLINE BLResult strokeUtf8Text(const BLPointI& dst, const BLFont& font, const char* text, size_t size = SIZE_MAX) noexcept {
    return impl->virt->strokeTextI(impl, &dst, &font, text, size, BL_TEXT_ENCODING_UTF8);
  }

  //! Strokes the passed UTF-8 text by using the given `font`.
  BL_INLINE BLResult strokeUtf8Text(const BLPoint& dst, const BLFont& font, const char* text, size_t size = SIZE_MAX) noexcept {
    return impl->virt->strokeTextD(impl, &dst, &font, text, size, BL_TEXT_ENCODING_UTF8);
  }

  //! Strokes the passed UTF-16 text by using the given `font`.
  BL_INLINE BLResult strokeUtf16Text(const BLPointI& dst, const BLFont& font, const uint16_t* text, size_t size = SIZE_MAX) noexcept {
    return impl->virt->strokeTextI(impl, &dst, &font, text, size, BL_TEXT_ENCODING_UTF16);
  }

  //! Strokes the passed UTF-16 text by using the given `font`.
  BL_INLINE BLResult strokeUtf16Text(const BLPoint& dst, const BLFont& font, const uint16_t* text, size_t size = SIZE_MAX) noexcept {
    return impl->virt->strokeTextD(impl, &dst, &font, text, size, BL_TEXT_ENCODING_UTF16);
  }

  //! Strokes the passed UTF-32 text by using the given `font`.
  BL_INLINE BLResult strokeUtf32Text(const BLPointI& dst, const BLFont& font, const uint32_t* text, size_t size = SIZE_MAX) noexcept {
    return impl->virt->strokeTextI(impl, &dst, &font, text, size, BL_TEXT_ENCODING_UTF32);
  }

  //! Strokes the passed UTF-32 text by using the given `font`.
  BL_INLINE BLResult strokeUtf32Text(const BLPoint& dst, const BLFont& font, const uint32_t* text, size_t size = SIZE_MAX) noexcept {
    return impl->virt->strokeTextD(impl, &dst, &font, text, size, BL_TEXT_ENCODING_UTF32);
  }

  //! Strokes the passed `glyphRun` by using the given `font`.
  BL_INLINE BLResult strokeGlyphRun(const BLPointI& dst, const BLFont& font, const BLGlyphRun& glyphRun) noexcept {
    return impl->virt->strokeGlyphRunI(impl, &dst, &font, &glyphRun);
  }

  //! Strokes the passed `glyphRun` by using the given `font`.
  BL_INLINE BLResult strokeGlyphRun(const BLPoint& dst, const BLFont& font, const BLGlyphRun& glyphRun) noexcept {
    return impl->virt->strokeGlyphRunD(impl, &dst, &font, &glyphRun);
  }

  //! \}

  //! \name Image Blitting
  //! \{

  BL_INLINE BLResult blitImage(const BLPoint& dst, const BLImage& src) noexcept { return impl->virt->blitImageD(impl, &dst, &src, nullptr); }
  BL_INLINE BLResult blitImage(const BLPoint& dst, const BLImage& src, const BLRectI& srcArea) noexcept { return impl->virt->blitImageD(impl, &dst, &src, &srcArea); }

  BL_INLINE BLResult blitImage(const BLPointI& dst, const BLImage& src) noexcept { return impl->virt->blitImageI(impl, &dst, &src, nullptr); }
  BL_INLINE BLResult blitImage(const BLPointI& dst, const BLImage& src, const BLRectI& srcArea) noexcept { return impl->virt->blitImageI(impl, &dst, &src, &srcArea); }

  BL_INLINE BLResult blitImage(const BLRect& dst, const BLImage& src) noexcept { return impl->virt->blitScaledImageD(impl, &dst, &src, nullptr); }
  BL_INLINE BLResult blitImage(const BLRect& dst, const BLImage& src, const BLRectI& srcArea) noexcept { return impl->virt->blitScaledImageD(impl, &dst, &src, &srcArea); }

  BL_INLINE BLResult blitImage(const BLRectI& dst, const BLImage& src) noexcept { return impl->virt->blitScaledImageI(impl, &dst, &src, nullptr); }
  BL_INLINE BLResult blitImage(const BLRectI& dst, const BLImage& src, const BLRectI& srcArea) noexcept { return impl->virt->blitScaledImageI(impl, &dst, &src, &srcArea); }

  //! \}

  static BL_INLINE const BLContext& none() noexcept { return reinterpret_cast<const BLContext*>(blNone)[kImplType]; }
};
#endif

//! \}

#endif // BLEND2D_BLCONTEXT_H
