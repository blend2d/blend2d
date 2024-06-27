// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_SUPPORT_LOOKUPTABLE_P_H_INCLUDED
#define BLEND2D_SUPPORT_LOOKUPTABLE_P_H_INCLUDED

#include "../api-internal_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

//! \name Compile-Time Lookup Table
//! \{

//! Struct that holds `N` items of `T` type - output of lookup table generators.
template<typename T, size_t N>
struct LookupTable {
  T data[N];

  BL_INLINE constexpr size_t size() const noexcept { return N; }
  BL_INLINE constexpr const T& operator[](size_t i) const noexcept { return data[i]; }
};

// NOTE: We only need `index_sequence` and `make_index_sequence` to generate our lookup tables. These were
// introduced by C++14 so if we want to support C++11 we have to implement these on our own. In addition,
// we no longer want to depend on <utility> header, so we always provide our own implementation of these.
namespace Internal {

// C++11 implementation based on <https://stackoverflow.com/a/17426611/410767>.
template <size_t... Ints>
struct index_sequence {
  using type = index_sequence;
  using value_type = size_t;
  static BL_INLINE_NODEBUG constexpr std::size_t size() noexcept { return sizeof...(Ints); }
};

template <class S1, class S2>
struct _merge_and_renumber;

template <size_t... I1, size_t... I2>
struct _merge_and_renumber<index_sequence<I1...>, index_sequence<I2...>>
  : index_sequence<I1..., (sizeof...(I1)+I2)...> {};

template <size_t N>
struct make_index_sequence
  : _merge_and_renumber<typename make_index_sequence<N/2>::type,
                        typename make_index_sequence<N - N/2>::type> {};

template<> struct make_index_sequence<0> : index_sequence<> {};
template<> struct make_index_sequence<1> : index_sequence<0> {};

template<typename T, size_t N, class Gen, size_t... Indexes>
static BL_INLINE_NODEBUG constexpr LookupTable<T, N> makeLookupTableImpl(Internal::index_sequence<Indexes...>) noexcept {
  return LookupTable<T, N> {{ T(Gen::value(Indexes))... }};
}

template<typename Generator>
struct BitTableGeneratorAdapter {
  static BL_INLINE_NODEBUG constexpr BLBitWord gen8(size_t index) noexcept {
    return (BLBitWord(Generator::value(index + 0u)) << 0u) |
           (BLBitWord(Generator::value(index + 1u)) << 1u) |
           (BLBitWord(Generator::value(index + 2u)) << 2u) |
           (BLBitWord(Generator::value(index + 3u)) << 3u) |
           (BLBitWord(Generator::value(index + 4u)) << 4u) |
           (BLBitWord(Generator::value(index + 5u)) << 5u) |
           (BLBitWord(Generator::value(index + 6u)) << 6u) |
           (BLBitWord(Generator::value(index + 7u)) << 7u) ;
  }

  static BL_INLINE_NODEBUG constexpr BLBitWord gen32(size_t index) noexcept {
    return (uint32_t(gen8(index +  0u)) <<  0u) |
           (uint32_t(gen8(index +  8u)) <<  8u) |
           (uint32_t(gen8(index + 16u)) << 16u) |
           (uint32_t(gen8(index + 24u)) << 24u) ;
  }

  static BL_INLINE_NODEBUG constexpr BLBitWord gen64(size_t index) noexcept {
    return (uint64_t(gen8(index +  0u)) <<  0u) |
           (uint64_t(gen8(index +  8u)) <<  8u) |
           (uint64_t(gen8(index + 16u)) << 16u) |
           (uint64_t(gen8(index + 24u)) << 24u) |
           (uint64_t(gen8(index + 32u)) << 32u) |
           (uint64_t(gen8(index + 40u)) << 40u) |
           (uint64_t(gen8(index + 48u)) << 48u) |
           (uint64_t(gen8(index + 56u)) << 56u) ;
  }

  static BL_INLINE_NODEBUG constexpr BLBitWord value(size_t index) noexcept {
    return sizeof(BLBitWord) <= 4 ? BLBitWord(gen32(index * 32u) & ~BLBitWord(0))
                                  : BLBitWord(gen64(index * 64u) & ~BLBitWord(0));
  }
};

}

//! Creates a lookup table of `LookupTable<T[N]>` by using the generator `Gen`.
template<typename T, size_t N, class Gen>
static BL_INLINE_NODEBUG constexpr LookupTable<T, N> makeLookupTable() noexcept {
  return Internal::makeLookupTableImpl<T, N, Gen>(Internal::make_index_sequence<N>{});
}

template<size_t N>
struct BitLookupTable {
  enum : size_t {
    kBitsPerBitWord = sizeof(BLBitWord) * 8,

    kBitCount = N,
    kWordCount = (N + kBitsPerBitWord - 1) / (kBitsPerBitWord)
  };

  LookupTable<BLBitWord, kWordCount> data;

  BL_INLINE_NODEBUG constexpr size_t size() const noexcept { return N; }

  BL_INLINE bool operator[](size_t i) const noexcept {
    BL_ASSERT(i < N);
    return bool((data[i / kBitsPerBitWord] >> (i % kBitsPerBitWord)) & 0x1u);
  }
};

template<size_t N, class Generator>
static BL_INLINE_NODEBUG constexpr BitLookupTable<N> makeBitTable() noexcept {
  return BitLookupTable<N>{
    Internal::makeLookupTableImpl<BLBitWord, BitLookupTable<N>::kWordCount,
                                  Internal::BitTableGeneratorAdapter<Generator>>(Internal::make_index_sequence<BitLookupTable<N>::kWordCount>{})
  };
}

#define BL_CONSTEXPR_TABLE(Name, Generator, T, N) \
  static constexpr const LookupTable<T, N> Name##_constexpr = makeLookupTable<T, N, Generator>(); \
  const LookupTable<T, N> Name = Name##_constexpr

//! \}

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_SUPPORT_LOOKUPTABLE_P_H_INCLUDED
