// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RGBA_H_INCLUDED
#define BLEND2D_RGBA_H_INCLUDED

#include "api.h"

//! \addtogroup bl_styling
//! \{

//! 32-bit RGBA color (8-bit per component) stored as `0xAARRGGBB`.
struct BLRgba32 {
  //! Packed 32-bit RGBA value.
  uint32_t value;

#ifdef __cplusplus
  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLRgba32() noexcept = default;

  BL_INLINE_CONSTEXPR BLRgba32(const BLRgba32&) noexcept = default;

  BL_INLINE_CONSTEXPR explicit BLRgba32(uint32_t rgba32) noexcept : value(rgba32) {}

  BL_INLINE_NODEBUG explicit BLRgba32(const BLRgba64& rgba64) noexcept { reset(rgba64); }
  BL_INLINE_CONSTEXPR BLRgba32(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 0xFFu) noexcept
    : value((r << 16) | (g << 8) | b | (a << 24)) {}

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE_CONSTEXPR explicit operator bool() const noexcept { return value != 0; }

  BL_INLINE_NODEBUG BLRgba32& operator=(const BLRgba32& other) noexcept = default;

  [[nodiscard]]
  BL_INLINE_CONSTEXPR bool operator==(const BLRgba32& other) const noexcept { return  equals(other); }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR bool operator!=(const BLRgba32& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Accessors
  //! \{

  [[nodiscard]]
  BL_INLINE_CONSTEXPR uint32_t r() const noexcept { return (value >> 16) & 0xFFu; }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR uint32_t g() const noexcept { return (value >>  8) & 0xFFu; }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR uint32_t b() const noexcept { return (value >>  0) & 0xFFu; }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR uint32_t a() const noexcept { return (value >> 24); }

  BL_INLINE_NODEBUG void setR(uint32_t r) noexcept { value = (value & 0xFF00FFFFu) | (r << 16); }
  BL_INLINE_NODEBUG void setG(uint32_t g) noexcept { value = (value & 0xFFFF00FFu) | (g <<  8); }
  BL_INLINE_NODEBUG void setB(uint32_t b) noexcept { value = (value & 0xFFFFFF00u) | (b <<  0); }
  BL_INLINE_NODEBUG void setA(uint32_t a) noexcept { value = (value & 0x00FFFFFFu) | (a << 24); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG void reset() noexcept { value = 0u; }
  BL_INLINE_NODEBUG void reset(uint32_t rgba32) noexcept { value = rgba32;}

  BL_INLINE_NODEBUG void reset(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 0xFFu) noexcept {
    value = (r << 16) | (g << 8) | b | (a << 24);
  }

  BL_INLINE_NODEBUG void reset(const BLRgba32& rgba32) noexcept {
    value = rgba32.value;
  }

  BL_INLINE_NODEBUG void reset(const BLRgba64& rgba64) noexcept;

  [[nodiscard]]
  BL_INLINE_CONSTEXPR bool equals(const BLRgba32& other) const noexcept { return value == other.value; }

  //! \}

  //! \name Utilities
  //! \{

  //! Tests whether the color is fully-opaque (alpha equals 0xFFFF).
  [[nodiscard]]
  BL_INLINE_CONSTEXPR bool isOpaque() const noexcept { return value >= 0xFF000000u; }

  //! Tests whether the color is fully-transparent (alpha equals 0).
  [[nodiscard]]
  BL_INLINE_CONSTEXPR bool isTransparent() const noexcept { return value <= 0x00FFFFFFu; }

  //! \}
#endif
};

//! 64-bit RGBA color (16-bit per component) stored as `0xAAAARRRRGGGGBBBB`.
struct BLRgba64 {
  //! Packed 64-bit RGBA value.
  uint64_t value;

#ifdef __cplusplus
  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLRgba64() noexcept = default;

  BL_INLINE_CONSTEXPR BLRgba64(const BLRgba64&) noexcept = default;

  BL_INLINE_CONSTEXPR explicit BLRgba64(uint64_t rgba64) noexcept : value(rgba64) {}

  BL_INLINE_CONSTEXPR BLRgba64(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 0xFFFFu) noexcept
    : value((uint64_t(a) << 48) |
            (uint64_t(r) << 32) |
            (uint64_t(g) << 16) |
            (uint64_t(b) <<  0) ) {}

  BL_INLINE_CONSTEXPR explicit BLRgba64(const BLRgba32& rgba32) noexcept
    : value(((uint64_t(rgba32.r()) << 32) |
             (uint64_t(rgba32.g()) << 16) |
             (uint64_t(rgba32.b()) <<  0) |
             (uint64_t(rgba32.a()) << 48)) * 0x0101u) {}

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE_CONSTEXPR explicit operator bool() const noexcept { return value != 0; }

  BL_INLINE_NODEBUG BLRgba64& operator=(const BLRgba64& other) noexcept = default;

  [[nodiscard]]
  BL_INLINE_CONSTEXPR bool operator==(const BLRgba64& other) const noexcept { return  equals(other); }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR bool operator!=(const BLRgba64& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Accessors
  //! \{

  [[nodiscard]]
  BL_INLINE_CONSTEXPR uint32_t r() const noexcept { return uint32_t((value >> 32) & 0xFFFFu); }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR uint32_t g() const noexcept { return uint32_t((value >> 16) & 0xFFFFu); }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR uint32_t b() const noexcept { return uint32_t((value >>  0) & 0xFFFFu); }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR uint32_t a() const noexcept { return uint32_t((value >> 48)); }

  BL_INLINE_NODEBUG void setR(uint32_t r) noexcept { value = (value & 0xFFFF0000FFFFFFFFu) | (uint64_t(r) << 32); }
  BL_INLINE_NODEBUG void setG(uint32_t g) noexcept { value = (value & 0xFFFFFFFF0000FFFFu) | (uint64_t(g) << 16); }
  BL_INLINE_NODEBUG void setB(uint32_t b) noexcept { value = (value & 0xFFFFFFFFFFFF0000u) | (uint64_t(b) <<  0); }
  BL_INLINE_NODEBUG void setA(uint32_t a) noexcept { value = (value & 0x0000FFFFFFFFFFFFu) | (uint64_t(a) << 48); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG void reset() noexcept { value = 0u; }
  BL_INLINE_NODEBUG void reset(uint64_t rgba64) noexcept { value = rgba64; }

  BL_INLINE_NODEBUG void reset(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 0xFFFFu) noexcept {
    value = (uint64_t(r) << 32) |
            (uint64_t(g) << 16) |
            (uint64_t(b) <<  0) |
            (uint64_t(a) << 48);
  }

  BL_INLINE_NODEBUG void reset(const BLRgba64& rgba64) noexcept {
    value = rgba64.value;
  }

  BL_INLINE_NODEBUG void reset(const BLRgba32& rgba32) noexcept {
    value = ((uint64_t(rgba32.r()) << 32) |
             (uint64_t(rgba32.g()) << 16) |
             (uint64_t(rgba32.b()) <<  0) |
             (uint64_t(rgba32.a()) << 48)) * 0x0101u;
  }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR bool equals(const BLRgba64& other) const noexcept { return value == other.value; }

  //! \}

  //! \name Utilities
  //! \{

  //! Tests whether the color is fully-opaque (alpha equals 0xFFFF).
  [[nodiscard]]
  BL_INLINE_CONSTEXPR bool isOpaque() const noexcept { return value >= 0xFFFF000000000000u; }

  //! Tests whether the color is fully-transparent (alpha equals 0).
  [[nodiscard]]
  BL_INLINE_CONSTEXPR bool isTransparent() const noexcept { return value <= 0x0000FFFFFFFFFFFFu; }

  //! \}
#endif
};

//! 128-bit RGBA color stored as 4 32-bit floating point values in [RGBA] order.
struct BLRgba {
  //! Red component.
  float r;
  //! Green component.
  float g;
  //! Blur component.
  float b;
  //! Alpha component.
  float a;

#ifdef __cplusplus
  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLRgba() noexcept = default;

  BL_INLINE_CONSTEXPR BLRgba(const BLRgba&) noexcept = default;

  BL_INLINE_CONSTEXPR BLRgba(float rValue, float gValue, float bValue, float aValue = 1.0f) noexcept
    : r(rValue),
      g(gValue),
      b(bValue),
      a(aValue) {}

  BL_INLINE_CONSTEXPR BLRgba(const BLRgba32& rgba32) noexcept
    : r(float(int32_t(rgba32.r())) * (1.0f / 255.0f)),
      g(float(int32_t(rgba32.g())) * (1.0f / 255.0f)),
      b(float(int32_t(rgba32.b())) * (1.0f / 255.0f)),
      a(float(int32_t(rgba32.a())) * (1.0f / 255.0f)) {}

  BL_INLINE_CONSTEXPR BLRgba(const BLRgba64& rgba64) noexcept
    : r(float(int32_t(rgba64.r())) * (1.0f / 65535.0f)),
      g(float(int32_t(rgba64.g())) * (1.0f / 65535.0f)),
      b(float(int32_t(rgba64.b())) * (1.0f / 65535.0f)),
      a(float(int32_t(rgba64.a())) * (1.0f / 65535.0f)) {}

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE_CONSTEXPR explicit operator bool() const noexcept {
    return !((r == 0.0f) & (g == 0.0f) & (b == 0.0f) & (a == 0.0f));
  }

  BL_INLINE_NODEBUG BLRgba& operator=(const BLRgba& other) noexcept = default;

  [[nodiscard]]
  BL_INLINE_NODEBUG bool operator==(const BLRgba& other) const noexcept { return  equals(other); }

  [[nodiscard]]
  BL_INLINE_NODEBUG bool operator!=(const BLRgba& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG void reset() noexcept {
    reset(0.0f, 0.0f, 0.0f, 0.0f);
  }

  BL_INLINE_NODEBUG void reset(const BLRgba32& rgba32) noexcept {
    *this = BLRgba(rgba32);
  }

  BL_INLINE_NODEBUG void reset(const BLRgba64& rgba64) noexcept {
    *this = BLRgba(rgba64);
  }

  BL_INLINE_NODEBUG void reset(const BLRgba& other) noexcept {
    reset(other.r, other.g, other.b, other.a);
  }

  BL_INLINE_NODEBUG void reset(float rValue, float gValue, float bValue, float aValue = 1.0f) noexcept {
    r = rValue;
    g = gValue;
    b = bValue;
    a = aValue;
  }

  [[nodiscard]]
  BL_INLINE_NODEBUG bool equals(const BLRgba32& rgba32) const noexcept {
    return equals(BLRgba(rgba32));
  }

  [[nodiscard]]
  BL_INLINE_NODEBUG bool equals(const BLRgba64& rgba64) const noexcept {
    return equals(BLRgba(rgba64));
  }

  [[nodiscard]]
  BL_INLINE_NODEBUG bool equals(const BLRgba& other) const noexcept {
    return BLInternal::bool_and(blEquals(r, other.r),
                                blEquals(g, other.g),
                                blEquals(b, other.b),
                                blEquals(a, other.a));
  }

  [[nodiscard]]
  BL_INLINE_NODEBUG bool equals(float rValue, float gValue, float bValue, float aValue = 1.0f) const noexcept {
    return BLInternal::bool_and(blEquals(r, rValue),
                                blEquals(g, gValue),
                                blEquals(b, bValue),
                                blEquals(a, aValue));
  }

  //! \}

  //! \name Conversion
  //! \{

  [[nodiscard]]
  BL_INLINE_NODEBUG BLRgba32 toRgba32() const noexcept {
    return BLRgba32(uint32_t(int(blClamp(r, 0.0f, 1.0f) * 255.0f + 0.5f)),
                    uint32_t(int(blClamp(g, 0.0f, 1.0f) * 255.0f + 0.5f)),
                    uint32_t(int(blClamp(b, 0.0f, 1.0f) * 255.0f + 0.5f)),
                    uint32_t(int(blClamp(a, 0.0f, 1.0f) * 255.0f + 0.5f)));
  }

  [[nodiscard]]
  BL_INLINE_NODEBUG BLRgba64 toRgba64() const noexcept {
    return BLRgba64(uint32_t(int(blClamp(r, 0.0f, 1.0f) * 65535.0f + 0.5f)),
                    uint32_t(int(blClamp(g, 0.0f, 1.0f) * 65535.0f + 0.5f)),
                    uint32_t(int(blClamp(b, 0.0f, 1.0f) * 65535.0f + 0.5f)),
                    uint32_t(int(blClamp(a, 0.0f, 1.0f) * 65535.0f + 0.5f)));
  }

  //! \}

  //! \name Utilities
  //! \{

  //! Tests whether the color is fully-opaque (alpha equals 1.0).
  [[nodiscard]]
  BL_INLINE_CONSTEXPR bool isOpaque() const noexcept { return a >= 1.0; }

  //! Tests whether the color is fully-transparent (alpha equals 0.0).
  [[nodiscard]]
  BL_INLINE_CONSTEXPR bool isTransparent() const noexcept { return a <= 0.0; }

  //! \}
#endif
};

#ifdef __cplusplus
BL_INLINE_NODEBUG void BLRgba32::reset(const BLRgba64& rgba64) noexcept {
  reset(uint32_t((rgba64.value >> 40) & 0xFF),
        uint32_t((rgba64.value >> 24) & 0xFF),
        uint32_t((rgba64.value >>  8) & 0xFF),
        uint32_t((rgba64.value >> 56)       ));
}

[[nodiscard]]
static BL_INLINE_CONSTEXPR BLRgba32 blMin(const BLRgba32& a, const BLRgba32& b) noexcept {
  return BLRgba32(blMin(a.r(), b.r()), blMin(a.g(), b.g()), blMin(a.b(), b.b()), blMin(a.a(), b.a()));
}

[[nodiscard]]
static BL_INLINE_CONSTEXPR BLRgba32 blMax(const BLRgba32& a, const BLRgba32& b) noexcept {
  return BLRgba32(blMax(a.r(), b.r()), blMax(a.g(), b.g()), blMax(a.b(), b.b()), blMax(a.a(), b.a()));
}

[[nodiscard]]
static BL_INLINE_CONSTEXPR BLRgba64 blMin(const BLRgba64& a, const BLRgba64& b) noexcept {
  return BLRgba64(blMin(a.r(), b.r()), blMin(a.g(), b.g()), blMin(a.b(), b.b()), blMin(a.a(), b.a()));
}

[[nodiscard]]
static BL_INLINE_CONSTEXPR BLRgba64 blMax(const BLRgba64& a, const BLRgba64& b) noexcept {
  return BLRgba64(blMax(a.r(), b.r()), blMax(a.g(), b.g()), blMax(a.b(), b.b()), blMax(a.a(), b.a()));
}

[[nodiscard]]
static BL_INLINE_CONSTEXPR BLRgba blMin(const BLRgba& a, const BLRgba& b) noexcept {
  return BLRgba(blMin(a.r, b.r), blMin(a.g, b.g), blMin(a.b, b.b), blMin(a.a, b.a));
}

[[nodiscard]]
static BL_INLINE_CONSTEXPR BLRgba blMax(const BLRgba& a, const BLRgba& b) noexcept {
  return BLRgba(blMax(a.r, b.r), blMax(a.g, b.g), blMax(a.b, b.b), blMax(a.a, b.a));
}
#endif

#ifdef __cplusplus
static_assert(sizeof(BLRgba) == 16, "'BLRgba' struct must be exactly 16 bytes long");
static_assert(sizeof(BLRgba32) == 4, "'BLRgba32' struct must be exactly 4 bytes long");
static_assert(sizeof(BLRgba64) == 8, "'BLRgba64' struct must be exactly 8 bytes long");
#endif

//! \}

#endif // BLEND2D_RGBA_H_INCLUDED
