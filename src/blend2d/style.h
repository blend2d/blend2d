// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_STYLE_H
#define BLEND2D_STYLE_H

#include "./rgba.h"
#include "./gradient.h"
#include "./pattern.h"
#include "./variant.h"

//! \addtogroup blend2d_api_styling
//! \{

// ============================================================================
// [Constants]
// ============================================================================

//! Style type.
BL_DEFINE_ENUM(BLStyleType) {
  //! No style, nothing will be paint.
  BL_STYLE_TYPE_NONE = 0,
  //! Solid color style.
  BL_STYLE_TYPE_SOLID = 1,
  //! Pattern style.
  BL_STYLE_TYPE_PATTERN = 2,
  //! Gradient style.
  BL_STYLE_TYPE_GRADIENT = 3,

  //! Count of style types.
  BL_STYLE_TYPE_COUNT = 4
};

// ============================================================================
// [BLStyle - Core]
// ============================================================================

//! Style [C Interface - Impl].
struct BLStyleCore {
  union {
    //! Holds RGBA components if the style is a solid color.
    BLRgba rgba;
    //! Holds variant data if the style is a variant object.
    BLVariantCore variant;
    //! Holds pattern object, if the style is `BL_STYLE_TYPE_PATTERN`.
    BLPatternCore pattern;
    //! Holds gradient object, if the style is `BL_STYLE_TYPE_GRADIENT`.
    BLGradientCore gradient;
    //! Internal data that is used to store the type and tag of the style.
    struct {
      uint64_t unknown;
      uint32_t type;
      uint32_t tag;
    } data;
    //! Internal data as two 64-bit integers, used by the implementation.
    uint64_t u64Data[2];
  };
};

// ============================================================================
// [BLStyle - C++]
// ============================================================================

#ifdef __cplusplus
//! Holds either RGBA color in floating point format or other style object like
//! `BLPattern` or `BLGradient`.
//!
//! Internal layout of the style data consists of 4 32-bit unsigned integers.
//! Their values describe the style type and its values:
//!   - None - internal members are set to [NaN, NaN, NaN, NaN].
//!   - Solid - internal members describe [R, G, B, A] components that are not NaN.
//!   - Object - internal members are set to [ImplPtr [+ padding], StyleType, NaN].
class BLStyle : public BLStyleCore {
public:
  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLStyle() noexcept { _makeTagged(BL_STYLE_TYPE_NONE); }

  BL_INLINE BLStyle(BLStyle&& other) noexcept {
    uint64_t q0 = other.u64Data[0];
    uint64_t q1 = other.u64Data[1];

    other._makeTagged(BL_STYLE_TYPE_NONE);

    this->u64Data[0] = q0;
    this->u64Data[1] = q1;
  }

  BL_INLINE BLStyle(const BLStyle& other) noexcept { blStyleInitWeak(this, &other); }
  explicit BL_INLINE BLStyle(const BLRgba& rgbaf) noexcept { blStyleInitRgba(this, &rgbaf); }
  explicit BL_INLINE BLStyle(const BLRgba32& rgba32) noexcept { rgba.reset(rgba32); }
  explicit BL_INLINE BLStyle(const BLRgba64& rgba64) noexcept { rgba.reset(rgba64); }
  explicit BL_INLINE BLStyle(const BLPattern& pattern) noexcept { blStyleInitObject(this, &pattern); }
  explicit BL_INLINE BLStyle(const BLGradient& gradient) noexcept { blStyleInitObject(this, &gradient); }
  BL_INLINE ~BLStyle() noexcept { blStyleDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  //! Tests whether the style is either color or an object.
  BL_INLINE explicit operator bool() const noexcept { return !isNone(); }

  //! Move assignment.
  //!
  //! \note The `other` style is reset by move assignment, so its state
  //! after the move operation is the same as a default constructed style.
  BL_INLINE BLStyle& operator=(BLStyle&& other) noexcept { blStyleAssignMove(this, &other); return *this; }
  //! Copy assignment, performs weak copy of the data held by the `other` style.
  BL_INLINE BLStyle& operator=(const BLStyle& other) noexcept { blStyleAssignWeak(this, &other); return *this; }

  BL_INLINE BLStyle& operator=(const BLRgba& rgbaf) noexcept { blStyleAssignRgba(this, &rgbaf); return *this; }
  BL_INLINE BLStyle& operator=(const BLRgba32& rgba32) noexcept { blStyleAssignRgba32(this, rgba32.value); return *this; }
  BL_INLINE BLStyle& operator=(const BLRgba64& rgba64) noexcept { blStyleAssignRgba64(this, rgba64.value); return *this; }
  BL_INLINE BLStyle& operator=(const BLPattern& pattern) noexcept { blStyleAssignObject(this, &pattern); return *this; }
  BL_INLINE BLStyle& operator=(const BLGradient& gradient) noexcept { blStyleAssignObject(this, &gradient); return *this; }

  BL_INLINE bool operator==(const BLStyle& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLStyle& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blStyleReset(this); }

  //! Swaps the content of this string with the `other` string.
  BL_INLINE void swap(BLStyle& other) noexcept {
    std::swap(this->u64Data[0], other.u64Data[0]);
    std::swap(this->u64Data[1], other.u64Data[1]);
  }

  BL_INLINE BLResult assign(BLStyle&& other) noexcept {
    return blStyleAssignMove(this, &other);
  }

  BL_INLINE BLResult assign(const BLStyle& other) noexcept {
    return blStyleAssignWeak(this, &other);
  }

  BL_INLINE BLResult assign(const BLRgba& rgbaf) noexcept {
    return blStyleAssignRgba(this, &rgbaf);
  }

  BL_INLINE BLResult assign(const BLRgba32& rgba32) noexcept {
    return blStyleAssignRgba32(this, rgba32.value);
  }

  BL_INLINE BLResult assign(const BLRgba64& rgba64) noexcept {
    return blStyleAssignRgba64(this, rgba64.value);
  }

  BL_INLINE BLResult assign(const BLPattern& pattern) noexcept {
    return blStyleAssignObject(this, &pattern);
  }

  BL_INLINE BLResult assign(const BLGradient& gradient) noexcept {
    return blStyleAssignObject(this, &gradient);
  }

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE uint32_t type() const noexcept {
    uint32_t result = data.type;
    if (!_isTagged())
      result = BL_STYLE_TYPE_SOLID;
    return result;
  }

  //! Returns true if this style doesn't hold anything (neither color nor object).
  //!
  //! \note This is a perfectly valid style that just doesn't specify anything.
  BL_INLINE bool isNone() const noexcept { return _isTagged(BL_STYLE_TYPE_NONE); }
  //! Returns true if this style is a solid color.
  BL_INLINE bool isSolid() const noexcept { return !_isTagged(); }
  //! Returns true if this style holds an object like `BLGradient` or `BLPattern`.
  BL_INLINE bool isObject() const noexcept { return (data.type > BL_STYLE_TYPE_SOLID) & _isTagged(); }
  //! Returns true if this style holds `BLPattern` object.
  BL_INLINE bool isPattern() const noexcept { return _isTagged(BL_STYLE_TYPE_PATTERN); }
  //! Returns true if this style holds `BLGradient` object.
  BL_INLINE bool isGradient() const noexcept { return _isTagged(BL_STYLE_TYPE_GRADIENT); }

  BL_INLINE BLResult getRgba(BLRgba* out) const noexcept { return blStyleGetRgba(this, out); }
  BL_INLINE BLResult getRgba32(BLRgba32* out) const noexcept { return blStyleGetRgba32(this, &out->value); }
  BL_INLINE BLResult getRgba64(BLRgba64* out) const noexcept { return blStyleGetRgba64(this, &out->value); }
  BL_INLINE BLResult getGradient(BLPattern* out) const noexcept { return blStyleGetObject(this, out); }
  BL_INLINE BLResult getGradient(BLGradient* out) const noexcept { return blStyleGetObject(this, out); }

  BL_INLINE const BLRgba& asRgba() const noexcept {
    BL_ASSERT(isSolid());
    return rgba;
  }

  BL_INLINE BLPattern& asPattern() noexcept {
    BL_ASSERT(isPattern());
    return blDownCast(pattern);
  }

  BL_INLINE const BLPattern& asPattern() const noexcept {
    BL_ASSERT(isPattern());
    return blDownCast(pattern);
  }

  BL_INLINE BLGradient& asGradient() noexcept {
    BL_ASSERT(isGradient());
    return blDownCast(gradient);
  }

  BL_INLINE const BLGradient& asGradient() const noexcept {
    BL_ASSERT(isGradient());
    return blDownCast(gradient);
  }

  //! \}

  //! \name Equality & Comparison
  //! \{

  BL_INLINE bool equals(const BLStyle& other) const noexcept { return blStyleEquals(this, &other); }

  //! \}

  //! \cond INTERNAL
  //! \name Internals
  //! \{

  BL_INLINE bool _isTagged() const noexcept {
    uint32_t kTag = blBitCast<uint32_t>(std::numeric_limits<float>::quiet_NaN());
    return data.tag == kTag;
  }

  BL_INLINE bool _isTagged(uint32_t styleType) const noexcept {
    uint32_t kTag = blBitCast<uint32_t>(std::numeric_limits<float>::quiet_NaN());
    if (sizeof(void*) >= 8) {
      // This should compile to a single mov followed by comparison (GCC/Clang).
      union U { uint32_t u32[2]; uint64_t u64; };
      U predicate {{ styleType, kTag }};
      return u64Data[1] == predicate.u64;
    }
    else {
      return (data.type == styleType) & (data.tag == kTag);
    }
  }

  BL_INLINE void _makeTagged(uint32_t styleType) noexcept {
    uint32_t kTag = blBitCast<uint32_t>(std::numeric_limits<float>::quiet_NaN());
    BL_ASSERT(styleType != BL_STYLE_TYPE_SOLID);

    data.unknown = 0;
    data.type = styleType;
    data.tag = kTag;
  }
  //! \endcond
};
#endif

//! \}

#endif // BLEND2D_STYLE_H
