// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLRGBA_H
#define BLEND2D_BLRGBA_H

#include "./blapi.h"

//! \addtogroup blend2d_api_styles
//! \{

// ============================================================================
// [BLRgba32]
// ============================================================================

//! 32-bit RGBA color (8-bit per component) stored as `0xAARRGGBB`.
struct BLRgba32 {
  union {
    uint32_t value;
    struct {
    #if BL_BUILD_BYTE_ORDER == 1234
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

  BL_INLINE BLRgba32() noexcept = default;
  constexpr BLRgba32(const BLRgba32&) noexcept = default;
  constexpr explicit BLRgba32(uint32_t rgba32) noexcept : value(rgba32) {}

  BL_INLINE explicit BLRgba32(const BLRgba64& rgba64) noexcept { reset(rgba64); }
  constexpr BLRgba32(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 0xFFu) noexcept
    : value((a << 24) |
            (r << 16) |
            (g <<  8) |
            (b      ) ) {}

  BL_INLINE void reset() noexcept { this->value = 0u; }
  BL_INLINE void reset(uint32_t rgba32) noexcept { this->value = rgba32;}
  BL_INLINE void reset(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 0xFFu) noexcept { *this = BLRgba32(r, g, b, a); }

  BL_INLINE void reset(const BLRgba32& rgba32) noexcept { value = rgba32.value; }
  BL_INLINE void reset(const BLRgba64& rgba64) noexcept;

  BL_INLINE bool equals(const BLRgba32& other) const noexcept { return blEquals(this->value, other.value); }

  //! Get whether the color is fully-opaque (alpha equals 0xFFFF).
  constexpr bool isOpaque() const noexcept { return this->value >= 0xFF000000u; }
  //! Get whether the color is fully-transparent (alpha equals 0).
  constexpr bool isTransparent() const noexcept { return this->value <= 0x00FFFFFFu; }

  BL_INLINE bool operator==(const BLRgba32& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLRgba32& other) const noexcept { return !equals(other); }

  constexpr explicit operator bool() const noexcept { return this->value != 0; }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLRgba64]
// ============================================================================

//! 64-bit RGBA color (16-bit per component) stored as `0xAAAARRRRGGGGBBBB`.
struct BLRgba64 {
  union {
    uint64_t value;
    struct {
    #if BL_BUILD_BYTE_ORDER == 1234
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

  BL_INLINE BLRgba64() noexcept = default;
  constexpr BLRgba64(const BLRgba64&) noexcept = default;
  constexpr explicit BLRgba64(uint64_t rgba64) noexcept : value(rgba64) {}

  constexpr BLRgba64(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 0xFFFFu) noexcept
    : value(((uint64_t)a << 48) |
            ((uint64_t)r << 32) |
            ((uint64_t)g << 16) |
            ((uint64_t)b      ) ) {}

  BL_INLINE explicit BLRgba64(const BLRgba32& rgba32) noexcept { reset(rgba32); }

  BL_INLINE void reset() noexcept { this->value = 0u; }
  BL_INLINE void reset(uint64_t rgba64) noexcept { this->value = rgba64; }
  BL_INLINE void reset(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 0xFFFFu) noexcept { *this = BLRgba64(r, g, b, a); }

  BL_INLINE void reset(const BLRgba64& rgba64) noexcept { this->value = rgba64.value; }
  BL_INLINE void reset(const BLRgba32& rgba32) noexcept {
    reset(rgba32.r | (rgba32.r << 8),
          rgba32.g | (rgba32.g << 8),
          rgba32.b | (rgba32.b << 8),
          rgba32.a | (rgba32.a << 8));
  }

  BL_INLINE bool equals(const BLRgba64& other) const noexcept { return blEquals(this->value, other.value); }

  //! Get whether the color is fully-opaque (alpha equals 0xFFFF).
  constexpr bool isOpaque() const noexcept { return this->value >= 0xFFFF000000000000u; }
  //! Get whether the color is fully-transparent (alpha equals 0).
  constexpr bool isTransparent() const noexcept { return this->value <= 0x0000FFFFFFFFFFFFu; }

  BL_INLINE bool operator==(const BLRgba64& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLRgba64& other) const noexcept { return !equals(other); }

  constexpr explicit operator bool() const noexcept { return this->value != 0; }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLRgba128]
// ============================================================================

//! 128-bit RGBA color stored as 4 32-bit floating point values in [RGBA] order.
struct BLRgba128 {
  float r;
  float g;
  float b;
  float a;

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE BLRgba128() noexcept = default;
  constexpr BLRgba128(const BLRgba128&) noexcept = default;

  constexpr BLRgba128(float r, float g, float b, float a = 1.0f) noexcept
    : r(r),
      g(g),
      b(b),
      a(a) {}

  constexpr explicit operator bool() const noexcept {
    return (this->r == 0.0f) &
           (this->g == 0.0f) &
           (this->b == 0.0f) &
           (this->a == 0.0f) ;
  }

  BL_INLINE void reset() noexcept {
    reset(0.0f, 0.0f, 0.0f, 0.0f);
  }

  BL_INLINE void reset(float r, float g, float b, float a = 1.0f) noexcept {
    this->r = r;
    this->g = g;
    this->b = b;
    this->a = a;
  }

  BL_INLINE bool equals(const BLRgba128& other) const noexcept {
    return blEquals(this->r, other.r) &
           blEquals(this->g, other.g) &
           blEquals(this->b, other.b) &
           blEquals(this->a, other.a) ;
  }

  //! Get whether the color is fully-opaque (alpha equals 1.0).
  constexpr bool isOpaque() const noexcept { return this->a >= 1.0; }
  //! Get whether the color is fully-transparent (alpha equals 0.0).
  constexpr bool isTransparent() const noexcept { return this->a == 0.0; }

  BL_INLINE bool operator==(const BLRgba128& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLRgba128& other) const noexcept { return !equals(other); }

  #endif
  // --------------------------------------------------------------------------
};

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
static_assert(sizeof(BLRgba32) == 4, "'BLRgba32' struct must be exactly 4 bytes long");
static_assert(sizeof(BLRgba64) == 8, "'BLRgba64' struct must be exactly 8 bytes long");
static_assert(sizeof(BLRgba128) == 16, "'BLRgba128' struct must be exactly 16 bytes long");
#endif

//! \}

#endif // BLEND2D_BLRGBA_H
