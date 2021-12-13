// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_REFERENCE_PIXELGENERIC_P_H_INCLUDED
#define BLEND2D_PIPELINE_REFERENCE_PIXELGENERIC_P_H_INCLUDED

#include "../../pipeline/pipedefs_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_reference
//! \{

namespace BLPipeline {
namespace Reference {
namespace Pixel {

template<typename Format>
struct P32_8888;

template<typename Format>
struct U32_8888;

enum class Component {
  R = 0,
  G = 1,
  B = 2,
  A = 3,
  X = 0xFFu
};

struct Repeat {
  uint32_t v;

  BL_INLINE constexpr uint32_t u16x2() const noexcept { return v | (v << 16); }
  BL_INLINE constexpr uint64_t u16x4() const noexcept { return uint64_t(u16x2()) | (uint64_t(u16x2()) << 32); }
};

template<uint32_t rShift, uint32_t gShift, uint32_t bShift, uint32_t aShift>
struct Format8888 {
  enum Sizes : uint32_t {
    kRSize = 8,
    kGSize = 8,
    kBSize = 8,
    kASize = 8
  };

  enum Shifts : uint32_t {
    kRShift = rShift,
    kGShift = gShift,
    kBShift = bShift,
    kAShift = aShift
  };

  static constexpr Component componentFromShift(uint32_t shift) noexcept {
    return (shift == kRShift) ? Component::R :
           (shift == kGShift) ? Component::G :
           (shift == kBShift) ? Component::B :
           (shift == kAShift) ? Component::A : Component::X;
  }

  static constexpr Component componentFromIndex(uint32_t index) noexcept {
    return componentFromShift(index * 8u);
  }

  static uint32_t shiftFromComponent(Component component) noexcept {
    return (component == Component::R) ? kRShift :
           (component == Component::G) ? kGShift :
           (component == Component::B) ? kBShift :
           (component == Component::A) ? kAShift : 0xFFFFFFFFu;
  }
};

typedef Format8888<16, 8, 0, 24> Format_A8R8G8B8;

//! Packed 32-bit pixel.
template<typename FormatT>
struct P32_8888 {
  //! Packed pixel value.
  uint32_t p;

  //! Pixel format information.
  typedef FormatT Format;

  //! Type of a packed compatible pixel.
  typedef P32_8888<Format> Packed;

  //! Type of an unpacked compatible pixel.
  typedef U32_8888<Format> Unpacked;

  static BL_INLINE Packed fromValue(uint32_t value) noexcept { return Packed { value }; }

  BL_INLINE uint32_t r() const noexcept { return (p >> Format::kRShift) & 0xFF; }
  BL_INLINE uint32_t g() const noexcept { return (p >> Format::kGShift) & 0xFF; }
  BL_INLINE uint32_t b() const noexcept { return (p >> Format::kBShift) & 0xFF; }
  BL_INLINE uint32_t a() const noexcept { return (p >> Format::kAShift) & 0xFF; }
  BL_INLINE uint32_t value() noexcept { return p; }

  BL_INLINE Packed pack() const noexcept { return *this; }
  BL_INLINE Unpacked unpack() const noexcept;

  BL_INLINE Packed operator&(uint32_t x) const noexcept { return Packed { p & x }; }
  BL_INLINE Packed operator|(uint32_t x) const noexcept { return Packed { p | x }; }
  BL_INLINE Packed operator^(uint32_t x) const noexcept { return Packed { p ^ x }; }
  BL_INLINE Packed operator+(uint32_t x) const noexcept { return Packed { p + x }; }
  BL_INLINE Packed operator-(uint32_t x) const noexcept { return Packed { p - x }; }
  BL_INLINE Packed operator*(uint32_t x) const noexcept { return Packed { p * x }; }
  BL_INLINE Packed operator>>(uint32_t x) const noexcept { return Packed { p >> x }; }
  BL_INLINE Packed operator<<(uint32_t x) const noexcept { return Packed { p << x }; }

  BL_INLINE Packed operator&(const Packed& x) const noexcept { return Packed { p & x.p }; }
  BL_INLINE Packed operator|(const Packed& x) const noexcept { return Packed { p | x.p }; }
  BL_INLINE Packed operator^(const Packed& x) const noexcept { return Packed { p ^ x.p }; }
  BL_INLINE Packed operator+(const Packed& x) const noexcept { return Packed { p + x.p }; }
  BL_INLINE Packed operator-(const Packed& x) const noexcept { return Packed { p - x.p }; }

  BL_INLINE Packed& operator&=(uint32_t x) noexcept { *this = *this & x; return *this; }
  BL_INLINE Packed& operator|=(uint32_t x) noexcept { *this = *this | x; return *this; }
  BL_INLINE Packed& operator^=(uint32_t x) noexcept { *this = *this ^ x; return *this; }
  BL_INLINE Packed& operator+=(uint32_t x) noexcept { *this = *this + x; return *this; }
  BL_INLINE Packed& operator-=(uint32_t x) noexcept { *this = *this - x; return *this; }
  BL_INLINE Packed& operator*=(uint32_t x) noexcept { *this = *this * x; return *this; }
  BL_INLINE Packed& operator>>=(uint32_t x) noexcept { *this = *this >> x; return *this; }
  BL_INLINE Packed& operator<<=(uint32_t x) noexcept { *this = *this << x; return *this; }

  BL_INLINE Packed& operator&=(const Packed& x) noexcept { *this = *this & x; return *this; }
  BL_INLINE Packed& operator|=(const Packed& x) noexcept { *this = *this | x; return *this; }
  BL_INLINE Packed& operator^=(const Packed& x) noexcept { *this = *this ^ x; return *this; }
  BL_INLINE Packed& operator+=(const Packed& x) noexcept { *this = *this + x; return *this; }
  BL_INLINE Packed& operator-=(const Packed& x) noexcept { *this = *this - x; return *this; }
  BL_INLINE Packed& operator*=(const Packed& x) noexcept { *this = *this * x; return *this; }

};

#if BL_TARGET_ARCH_BITS >= 64

//! Unpacked 32-bit pixel to 64-bit data type.
template<typename FormatT>
struct U32_8888 {
  //! Unpacked components [.3.1.2.0]
  uint64_t u3120;

  //! Pixel format information.
  typedef FormatT Format;

  //! Type of a packed compatible pixel.
  typedef P32_8888<Format> Packed;

  //! Type of an unpacked compatible pixel.
  typedef U32_8888<Format> Unpacked;

  template<uint32_t shift>
  BL_INLINE uint32_t valueByShiftT() const noexcept {
    BL_STATIC_ASSERT(shift == 0 || shift == 8 || shift == 16 || shift == 24);

    if (shift == 0)
      return uint32_t((u3120 >>  0) & 0xFFFFu);
    else if (shift == 8)
      return uint32_t((u3120 >> 32) & 0xFFFFu);
    else if (shift == 16)
      return uint32_t((u3120 >> 16) & 0xFFFFu);
    else
      return uint32_t((u3120 >> 48));
  }

  template<uint32_t index>
  BL_INLINE uint32_t valueByIndexT() const noexcept {
    return valueByShiftT<index * 8>();
  }

  template<Component component>
  BL_INLINE uint32_t valueByComponentT() const noexcept {
    return valueByShiftT<Format::shiftFromComponent(component)>();
  }

  BL_INLINE uint32_t r() const noexcept { return valueByComponentT<Component::R>(); }
  BL_INLINE uint32_t g() const noexcept { return valueByComponentT<Component::G>(); }
  BL_INLINE uint32_t b() const noexcept { return valueByComponentT<Component::B>(); }
  BL_INLINE uint32_t a() const noexcept { return valueByComponentT<Component::A>(); }

  BL_INLINE Packed pack() const noexcept { return Packed { (uint32_t)(((u3120 >> 24) | u3120) & 0xFFFFFFFFu) }; }
  BL_INLINE Unpacked unpack() const noexcept { return *this; }

  BL_INLINE Unpacked operator>>(uint32_t x) const noexcept { return Unpacked { u3120 >> x }; }
  BL_INLINE Unpacked operator<<(uint32_t x) const noexcept { return Unpacked { u3120 << x }; }

  BL_INLINE Unpacked operator&(const Repeat& x) const noexcept { return Unpacked { u3120 & x.u16x4() }; }
  BL_INLINE Unpacked operator|(const Repeat& x) const noexcept { return Unpacked { u3120 | x.u16x4() }; }
  BL_INLINE Unpacked operator^(const Repeat& x) const noexcept { return Unpacked { u3120 ^ x.u16x4() }; }
  BL_INLINE Unpacked operator+(const Repeat& x) const noexcept { return Unpacked { u3120 + x.u16x4() }; }
  BL_INLINE Unpacked operator-(const Repeat& x) const noexcept { return Unpacked { u3120 - x.u16x4() }; }
  BL_INLINE Unpacked operator*(const Repeat& x) const noexcept { return Unpacked { u3120 * x.v }; }
  BL_INLINE Unpacked operator>>(const Repeat& x) const noexcept { return Unpacked { u3120 >> x.v }; }
  BL_INLINE Unpacked operator<<(const Repeat& x) const noexcept { return Unpacked { u3120 << x.v }; }

  BL_INLINE Unpacked operator&(const Unpacked& x) const noexcept { return Unpacked { u3120 & x.u3120 }; }
  BL_INLINE Unpacked operator|(const Unpacked& x) const noexcept { return Unpacked { u3120 | x.u3120 }; }
  BL_INLINE Unpacked operator^(const Unpacked& x) const noexcept { return Unpacked { u3120 ^ x.u3120 }; }
  BL_INLINE Unpacked operator+(const Unpacked& x) const noexcept { return Unpacked { u3120 + x.u3120 }; }
  BL_INLINE Unpacked operator-(const Unpacked& x) const noexcept { return Unpacked { u3120 - x.u3120 }; }
  BL_INLINE Unpacked operator*(const Unpacked& x) const noexcept { return cmul(x); }

  BL_INLINE Unpacked& operator>>=(uint32_t x) noexcept { *this = *this >> x; return *this; }
  BL_INLINE Unpacked& operator<<=(uint32_t x) noexcept { *this = *this << x; return *this; }

  BL_INLINE Unpacked& operator&=(const Repeat& x) noexcept { *this = *this & x; return *this; }
  BL_INLINE Unpacked& operator|=(const Repeat& x) noexcept { *this = *this | x; return *this; }
  BL_INLINE Unpacked& operator^=(const Repeat& x) noexcept { *this = *this ^ x; return *this; }
  BL_INLINE Unpacked& operator+=(const Repeat& x) noexcept { *this = *this + x; return *this; }
  BL_INLINE Unpacked& operator-=(const Repeat& x) noexcept { *this = *this - x; return *this; }
  BL_INLINE Unpacked& operator*=(const Repeat& x) noexcept { *this = *this * x; return *this; }
  BL_INLINE Unpacked& operator>>=(const Repeat& x) noexcept { *this = *this >> x; return *this; }
  BL_INLINE Unpacked& operator<<=(const Repeat& x) noexcept { *this = *this << x; return *this; }

  BL_INLINE Unpacked& operator&=(const Unpacked& x) noexcept { *this = *this & x; return *this; }
  BL_INLINE Unpacked& operator|=(const Unpacked& x) noexcept { *this = *this | x; return *this; }
  BL_INLINE Unpacked& operator^=(const Unpacked& x) noexcept { *this = *this ^ x; return *this; }
  BL_INLINE Unpacked& operator+=(const Unpacked& x) noexcept { *this = *this + x; return *this; }
  BL_INLINE Unpacked& operator-=(const Unpacked& x) noexcept { *this = *this - x; return *this; }
  BL_INLINE Unpacked& operator*=(const Unpacked& x) noexcept { *this = *this * x; return *this; }

  template<typename XFormat>
  BL_INLINE Unpacked cmul(const U32_8888<XFormat>& x) const noexcept {
    uint32_t u0 = valueByIndexT<0>() * x.template valueByComponentT<Format::componentFromIndex(0)>();
    uint32_t u1 = valueByIndexT<1>() * x.template valueByComponentT<Format::componentFromIndex(1)>();
    uint32_t u2 = valueByIndexT<2>() * x.template valueByComponentT<Format::componentFromIndex(2)>();
    uint32_t u3 = valueByIndexT<3>() * x.template valueByComponentT<Format::componentFromIndex(3)>();

    return Unpacked { uint64_t(u0) | (uint64_t(u1) << 32) | (uint64_t(u2) << 16) | (uint64_t(u3) << 48) };
  }

  BL_INLINE Unpacked div255() const noexcept {
    return ((*this + ((*this >> 8) & Repeat{0xFFu})) >> 8) & Repeat{0xFFu};
  }

  BL_INLINE Unpacked addus8(const Unpacked& x) const noexcept {
    Unpacked val = (*this + x);
    Unpacked msk = ((val >> 8) & Repeat{0x1u}) * Repeat{0xFF};
    return (val | msk) & Repeat{0xFFu};
  }
};

template<typename Format>
BL_INLINE U32_8888<Format> P32_8888<Format>::unpack() const noexcept {
  return Unpacked { uint64_t(p & 0x00FF00FFu) | (uint64_t(p & 0xFF00FF00u) << 24) };
}

#else

//! Unpacked 32-bit pixel to 64-bit data type.
template<typename FormatT>
struct U32_8888 {
  //! Unpacked components [.2.0].
  uint32_t u20;
  //! Unpacked components [3.1.].
  uint32_t u31;

  //! Pixel format information.
  typedef FormatT Format;

  //! Type of a packed compatible pixel.
  typedef P32_8888<Format> Packed;

  //! Type of an unpacked compatible pixel.
  typedef U32_8888<Format> Unpacked;

  template<uint32_t shift>
  BL_INLINE uint32_t valueByShiftT() const noexcept {
    BL_STATIC_ASSERT(shift == 0 || shift == 8 || shift == 16 || shift == 24);

    if (shift == 0)
      return u20 & 0xFFFFu;
    else if (shift == 8)
      return u31 & 0xFFFFu;
    else if (shift == 16)
      return u20 >> 16;
    else
      return u31 >> 16;
  }

  template<uint32_t index>
  BL_INLINE uint32_t valueByIndexT() const noexcept {
    return valueByShiftT<index * 8>();
  }

  template<Component component>
  BL_INLINE uint32_t valueByComponentT() const noexcept {
    return valueByShiftT<Format::shiftFromComponent(component)>();
  }

  BL_INLINE uint32_t r() const noexcept { return valueByComponentT<Component::R>(); }
  BL_INLINE uint32_t g() const noexcept { return valueByComponentT<Component::G>(); }
  BL_INLINE uint32_t b() const noexcept { return valueByComponentT<Component::B>(); }
  BL_INLINE uint32_t a() const noexcept { return valueByComponentT<Component::A>(); }

  BL_INLINE Packed pack() const noexcept { return Packed { u20 | (u31 << 8) }; }
  BL_INLINE Unpacked unpack() const noexcept { return *this; }

  BL_INLINE Unpacked operator>>(uint32_t x) const noexcept { return Unpacked { u20 >> x, u31 >> x }; }
  BL_INLINE Unpacked operator<<(uint32_t x) const noexcept { return Unpacked { u20 << x, u31 << x }; }

  BL_INLINE Unpacked operator&(const Repeat& x) const noexcept { return Unpacked { u20 & x.u16x2(), u31 & x.u16x2() }; }
  BL_INLINE Unpacked operator|(const Repeat& x) const noexcept { return Unpacked { u20 | x.u16x2(), u31 | x.u16x2() }; }
  BL_INLINE Unpacked operator^(const Repeat& x) const noexcept { return Unpacked { u20 ^ x.u16x2(), u31 ^ x.u16x2() }; }
  BL_INLINE Unpacked operator+(const Repeat& x) const noexcept { return Unpacked { u20 + x.u16x2(), u31 + x.u16x2() }; }
  BL_INLINE Unpacked operator-(const Repeat& x) const noexcept { return Unpacked { u20 - x.u16x2(), u31 - x.u16x2() }; }
  BL_INLINE Unpacked operator*(const Repeat& x) const noexcept { return Unpacked { u20 * x.v, u31 * x.v }; }
  BL_INLINE Unpacked operator>>(const Repeat& x) const noexcept { return Unpacked { u20 >> x.v, u31 >> x.v }; }
  BL_INLINE Unpacked operator<<(const Repeat& x) const noexcept { return Unpacked { u20 << x.v, u31 << x.v }; }

  BL_INLINE Unpacked operator&(const Unpacked& x) const noexcept { return Unpacked { u20 & x.u20, u31 & x.u31 }; }
  BL_INLINE Unpacked operator|(const Unpacked& x) const noexcept { return Unpacked { u20 | x.u20, u31 | x.u31 }; }
  BL_INLINE Unpacked operator^(const Unpacked& x) const noexcept { return Unpacked { u20 ^ x.u20, u31 ^ x.u31 }; }
  BL_INLINE Unpacked operator+(const Unpacked& x) const noexcept { return Unpacked { u20 + x.u20, u31 + x.u31 }; }
  BL_INLINE Unpacked operator-(const Unpacked& x) const noexcept { return Unpacked { u20 - x.u20, u31 - x.u31 }; }
  BL_INLINE Unpacked operator*(const Unpacked& x) const noexcept { return cmul(x); }

  BL_INLINE Unpacked& operator>>=(uint32_t x) noexcept { *this = *this >> x; return *this; }
  BL_INLINE Unpacked& operator<<=(uint32_t x) noexcept { *this = *this << x; return *this; }

  BL_INLINE Unpacked& operator&=(const Repeat& x) noexcept { *this = *this & x; return *this; }
  BL_INLINE Unpacked& operator|=(const Repeat& x) noexcept { *this = *this | x; return *this; }
  BL_INLINE Unpacked& operator^=(const Repeat& x) noexcept { *this = *this ^ x; return *this; }
  BL_INLINE Unpacked& operator+=(const Repeat& x) noexcept { *this = *this + x; return *this; }
  BL_INLINE Unpacked& operator-=(const Repeat& x) noexcept { *this = *this - x; return *this; }
  BL_INLINE Unpacked& operator*=(const Repeat& x) noexcept { *this = *this * x; return *this; }
  BL_INLINE Unpacked& operator>>=(const Repeat& x) noexcept { *this = *this >> x; return *this; }
  BL_INLINE Unpacked& operator<<=(const Repeat& x) noexcept { *this = *this << x; return *this; }

  BL_INLINE Unpacked& operator&=(const Unpacked& x) noexcept { *this = *this & x; return *this; }
  BL_INLINE Unpacked& operator|=(const Unpacked& x) noexcept { *this = *this | x; return *this; }
  BL_INLINE Unpacked& operator^=(const Unpacked& x) noexcept { *this = *this ^ x; return *this; }
  BL_INLINE Unpacked& operator+=(const Unpacked& x) noexcept { *this = *this + x; return *this; }
  BL_INLINE Unpacked& operator-=(const Unpacked& x) noexcept { *this = *this - x; return *this; }
  BL_INLINE Unpacked& operator*=(const Unpacked& x) noexcept { *this = *this * x; return *this; }

  template<typename XFormat>
  BL_INLINE Unpacked cmul(const U32_8888<XFormat>& x) const noexcept {
    return Unpacked {
      ((u20 & 0xFFFF0000u) * x.template valueByComponentT<Format::componentFromIndex(2)>()) |
      ((u20 & 0x0000FFFFu) * x.template valueByComponentT<Format::componentFromIndex(0)>())
      ,
      ((u31 & 0xFFFF0000u) * x.template valueByComponentT<Format::componentFromIndex(3)>()) |
      ((u31 & 0x0000FFFFu) * x.template valueByComponentT<Format::componentFromIndex(1)>())
    };
  }

  BL_INLINE Unpacked div255() const noexcept {
    return ((*this + ((*this >> 8) & Repeat{0xFFu})) >> 8) & Repeat{0xFFu};
  }

  BL_INLINE Unpacked addus8(const Unpacked& x) const noexcept {
    Unpacked val = (*this + x);
    Unpacked msk = ((val >> 8) & Repeat{0x1u}) * Repeat{0xFF};
    return (val | msk) & Repeat{0xFFu};
  }
};

template<typename Format>
BL_INLINE U32_8888<Format> P32_8888<Format>::unpack() const noexcept {
  return Unpacked { p & 0x00FF00FFu, (p >> 8) & 0x00FF00FFu };
}

#endif

typedef P32_8888<Format_A8R8G8B8> P32_A8R8G8B8;
typedef U32_8888<Format_A8R8G8B8> U32_A8R8G8B8;

} // {Pixel}
} // {Reference}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_REFERENCE_PIXELGENERIC_P_H_INCLUDED
