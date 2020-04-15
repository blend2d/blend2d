// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_RGBA_H
#define BLEND2D_RGBA_H

#include "./api.h"

BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_SHADOW)

//! \addtogroup blend2d_api_styling
//! \{

// ============================================================================
// [BLRgba32]
// ============================================================================

//! 32-bit RGBA color (8-bit per component) stored as `0xAARRGGBB`.
struct BLRgba32 {
  union {
    uint32_t value;
    struct {
    #if BL_BYTE_ORDER == 1234 // LITTLE ENDIAN
      uint32_t b : 8;
      uint32_t g : 8;
      uint32_t r : 8;
      uint32_t a : 8;
    #else
      uint32_t a : 8;
      uint32_t r : 8;
      uint32_t g : 8;
      uint32_t b : 8;
    #endif
    };
  };

  // --------------------------------------------------------------------------
  #ifdef __cplusplus
  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLRgba32() noexcept = default;
  BL_INLINE BLRgba32(const BLRgba32&) noexcept = default;
  BL_INLINE explicit BLRgba32(uint32_t rgba32) noexcept : value(rgba32) {}

  BL_INLINE explicit BLRgba32(const BLRgba64& rgba64) noexcept { reset(rgba64); }
  BL_INLINE BLRgba32(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 0xFFu) noexcept
    : value((a << 24) | (r << 16) | (g << 8) | b) {}

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return this->value != 0; }

  BL_INLINE bool operator==(const BLRgba32& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLRgba32& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE void reset() noexcept { this->value = 0u; }
  BL_INLINE void reset(uint32_t rgba32) noexcept { this->value = rgba32;}
  BL_INLINE void reset(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 0xFFu) noexcept { *this = BLRgba32(r, g, b, a); }

  BL_INLINE void reset(const BLRgba32& rgba32) noexcept { value = rgba32.value; }
  BL_INLINE void reset(const BLRgba64& rgba64) noexcept;

  BL_INLINE bool equals(const BLRgba32& other) const noexcept { return blEquals(this->value, other.value); }

  //! \}

  //! \name Utilities
  //! \{

  //! Tests whether the color is fully-opaque (alpha equals 0xFFFF).
  BL_INLINE bool isOpaque() const noexcept { return this->value >= 0xFF000000u; }
  //! Tests whether the color is fully-transparent (alpha equals 0).
  BL_INLINE bool isTransparent() const noexcept { return this->value <= 0x00FFFFFFu; }

  //! \}
  #endif
  // --------------------------------------------------------------------------
};

#ifdef __cplusplus
static BL_INLINE BLRgba32 blMin(const BLRgba32& a, const BLRgba32& b) noexcept {
  return BLRgba32(blMin((a.value >> 16) & 0xFFu, (b.value >> 16) & 0xFFu),
                  blMin((a.value >>  8) & 0xFFu, (b.value >>  8) & 0xFFu),
                  blMin((a.value      ) & 0xFFu, (b.value      ) & 0xFFu),
                  blMin((a.value >> 24) & 0xFFu, (b.value >> 24) & 0xFFu));
}

static BL_INLINE BLRgba32 blMax(const BLRgba32& a, const BLRgba32& b) noexcept {
  return BLRgba32(blMax((a.value >> 16) & 0xFFu, (b.value >> 16) & 0xFFu),
                  blMax((a.value >>  8) & 0xFFu, (b.value >>  8) & 0xFFu),
                  blMax((a.value      ) & 0xFFu, (b.value      ) & 0xFFu),
                  blMax((a.value >> 24) & 0xFFu, (b.value >> 24) & 0xFFu));
}
#endif

// ============================================================================
// [BLRgba64]
// ============================================================================

//! 64-bit RGBA color (16-bit per component) stored as `0xAAAARRRRGGGGBBBB`.
struct BLRgba64 {
  union {
    uint64_t value;
    struct {
    #if BL_BYTE_ORDER == 1234 // LITTLE ENDIAN
      uint32_t b : 16;
      uint32_t g : 16;
      uint32_t r : 16;
      uint32_t a : 16;
    #else
      uint32_t a : 16;
      uint32_t r : 16;
      uint32_t g : 16;
      uint32_t b : 16;
    #endif
    };
  };

  // --------------------------------------------------------------------------
  #ifdef __cplusplus
  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLRgba64() noexcept = default;
  BL_INLINE BLRgba64(const BLRgba64&) noexcept = default;
  BL_INLINE explicit BLRgba64(uint64_t rgba64) noexcept : value(rgba64) {}

  BL_INLINE BLRgba64(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 0xFFFFu) noexcept
    : value(((uint64_t)a << 48) |
            ((uint64_t)r << 32) |
            ((uint64_t)g << 16) |
            ((uint64_t)b      ) ) {}

  BL_INLINE explicit BLRgba64(const BLRgba32& rgba32) noexcept { reset(rgba32); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return this->value != 0; }

  BL_INLINE bool operator==(const BLRgba64& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLRgba64& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE void reset() noexcept { this->value = 0u; }
  BL_INLINE void reset(uint64_t rgba64) noexcept { this->value = rgba64; }
  BL_INLINE void reset(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 0xFFFFu) noexcept { *this = BLRgba64(r, g, b, a); }

  BL_INLINE void reset(const BLRgba64& rgba64) noexcept { this->value = rgba64.value; }
  BL_INLINE void reset(const BLRgba32& rgba32) noexcept {
    reset(rgba32.r | (uint32_t(rgba32.r) << 8u),
          rgba32.g | (uint32_t(rgba32.g) << 8u),
          rgba32.b | (uint32_t(rgba32.b) << 8u),
          rgba32.a | (uint32_t(rgba32.a) << 8u));
  }

  BL_INLINE bool equals(const BLRgba64& other) const noexcept { return blEquals(this->value, other.value); }

  //! \}

  //! \name Utilities
  //! \{

  //! Tests whether the color is fully-opaque (alpha equals 0xFFFF).
  BL_INLINE bool isOpaque() const noexcept { return this->value >= 0xFFFF000000000000u; }
  //! Tests whether the color is fully-transparent (alpha equals 0).
  BL_INLINE bool isTransparent() const noexcept { return this->value <= 0x0000FFFFFFFFFFFFu; }

  //! \}
  #endif
  // --------------------------------------------------------------------------
};

#ifdef __cplusplus
static BL_INLINE BLRgba64 blMin(const BLRgba64& a, const BLRgba64& b) noexcept {
  return BLRgba64(blMin(uint32_t((a.value >> 32) & 0xFFFFu), uint32_t((b.value >> 32) & 0xFFFFu)),
                  blMin(uint32_t((a.value >> 16) & 0xFFFFu), uint32_t((b.value >> 16) & 0xFFFFu)),
                  blMin(uint32_t((a.value      ) & 0xFFFFu), uint32_t((b.value      ) & 0xFFFFu)),
                  blMin(uint32_t((a.value >> 48) & 0xFFFFu), uint32_t((b.value >> 48) & 0xFFFFu)));
}

static BL_INLINE BLRgba64 blMax(const BLRgba64& a, const BLRgba64& b) noexcept {
  return BLRgba64(blMax(uint32_t((a.value >> 32) & 0xFFFFu), uint32_t((b.value >> 32) & 0xFFFFu)),
                  blMax(uint32_t((a.value >> 16) & 0xFFFFu), uint32_t((b.value >> 16) & 0xFFFFu)),
                  blMax(uint32_t((a.value      ) & 0xFFFFu), uint32_t((b.value      ) & 0xFFFFu)),
                  blMax(uint32_t((a.value >> 48) & 0xFFFFu), uint32_t((b.value >> 48) & 0xFFFFu)));
}
#endif

// ============================================================================
// [BLRgba]
// ============================================================================

//! 128-bit RGBA color stored as 4 32-bit floating point values in [RGBA] order.
struct BLRgba {
  float r;
  float g;
  float b;
  float a;

  // --------------------------------------------------------------------------
  #ifdef __cplusplus
  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLRgba() noexcept = default;
  constexpr BLRgba(const BLRgba&) noexcept = default;

  constexpr BLRgba(float r, float g, float b, float a = 1.0f) noexcept
    : r(r),
      g(g),
      b(b),
      a(a) {}

  BL_INLINE BLRgba(const BLRgba32& rgba32) noexcept
    : r(float(int(rgba32.r)) * (1.0f / 255.0f)),
      g(float(int(rgba32.g)) * (1.0f / 255.0f)),
      b(float(int(rgba32.b)) * (1.0f / 255.0f)),
      a(float(int(rgba32.a)) * (1.0f / 255.0f)) {}

  BL_INLINE BLRgba(const BLRgba64& rgba64) noexcept
    : r(float(int(rgba64.r)) * (1.0f / 65535.0f)),
      g(float(int(rgba64.g)) * (1.0f / 65535.0f)),
      b(float(int(rgba64.b)) * (1.0f / 65535.0f)),
      a(float(int(rgba64.a)) * (1.0f / 65535.0f)) {}

  //! \}

  //! \name Overloaded Operators
  //! \{

  constexpr explicit operator bool() const noexcept {
    return (this->r == 0.0f) &
           (this->g == 0.0f) &
           (this->b == 0.0f) &
           (this->a == 0.0f) ;
  }

  BL_INLINE bool operator==(const BLRgba& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLRgba& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE void reset() noexcept {
    reset(0.0f, 0.0f, 0.0f, 0.0f);
  }

  BL_INLINE void reset(const BLRgba32& rgba32) noexcept {
    *this = BLRgba(rgba32);
  }

  BL_INLINE void reset(const BLRgba64& rgba64) noexcept {
    *this = BLRgba(rgba64);
  }

  BL_INLINE void reset(const BLRgba& other) noexcept {
    reset(other.r, other.g, other.b, other.a);
  }

  BL_INLINE void reset(float r, float g, float b, float a = 1.0f) noexcept {
    this->r = r;
    this->g = g;
    this->b = b;
    this->a = a;
  }

  BL_INLINE bool equals(const BLRgba32& rgba32) const noexcept {
    return equals(BLRgba(rgba32));
  }

  BL_INLINE bool equals(const BLRgba64& rgba64) const noexcept {
    return equals(BLRgba(rgba64));
  }

  BL_INLINE bool equals(const BLRgba& other) const noexcept {
    return blEquals(this->r, other.r) &
           blEquals(this->g, other.g) &
           blEquals(this->b, other.b) &
           blEquals(this->a, other.a) ;
  }

  BL_INLINE bool equals(float r, float g, float b, float a = 1.0f) const noexcept {
    return blEquals(this->r, r) &
           blEquals(this->g, g) &
           blEquals(this->b, b) &
           blEquals(this->a, a) ;
  }

  //! \}

  //! \name Utilities
  //! \{

  //! Tests whether the color is fully-opaque (alpha equals 1.0).
  constexpr bool isOpaque() const noexcept { return this->a >= 1.0; }
  //! Tests whether the color is fully-transparent (alpha equals 0.0).
  constexpr bool isTransparent() const noexcept { return this->a == 0.0; }

  //! \}
  #endif
  // --------------------------------------------------------------------------
};

#ifdef __cplusplus
template<>
constexpr BL_INLINE BLRgba blMin(const BLRgba& a, const BLRgba& b) noexcept {
  return BLRgba(blMin(a.r, b.r),
                blMin(a.g, b.g),
                blMin(a.b, b.b),
                blMin(a.a, b.a));
}

template<>
constexpr BL_INLINE BLRgba blMax(const BLRgba& a, const BLRgba& b) noexcept {
  return BLRgba(blMax(a.r, b.r),
                blMax(a.g, b.g),
                blMax(a.b, b.b),
                blMax(a.a, b.a));
}
#endif

// ============================================================================
// [Out of Class]
// ============================================================================

#ifdef __cplusplus
BL_INLINE void BLRgba32::reset(const BLRgba64& rgba64) noexcept {
  uint32_t hi = uint32_t(rgba64.value >> 32);
  uint32_t lo = uint32_t(rgba64.value & 0xFFFFFFFFu);

  this->value = ((hi & 0xFF000000)      ) +
                ((lo & 0xFF000000) >> 16) +
                ((hi & 0x0000FF00) <<  8) +
                ((lo & 0x0000FF00) >>  8) ;
}
#endif

// ============================================================================
// [Constraints]
// ============================================================================

#ifdef __cplusplus
static_assert(sizeof(BLRgba) == 16, "'BLRgba' struct must be exactly 16 bytes long");
static_assert(sizeof(BLRgba32) == 4, "'BLRgba32' struct must be exactly 4 bytes long");
static_assert(sizeof(BLRgba64) == 8, "'BLRgba64' struct must be exactly 8 bytes long");
#endif

//! \}

BL_DIAGNOSTIC_POP

#endif // BLEND2D_RGBA_H
