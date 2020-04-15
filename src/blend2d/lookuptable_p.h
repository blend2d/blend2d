// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_LOOKUPTABLE_P_H
#define BLEND2D_LOOKUPTABLE_P_H

#include "./support_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLLookupTable]
// ============================================================================

//! Struct that holds `N` items of `T` type - output of lookup table generators.
template<typename T, size_t N>
struct BLLookupTable {
  T data[N];

  constexpr size_t size() const noexcept { return N; }
  constexpr const T& operator[](size_t i) const noexcept { return data[i]; }
};

// NOTE: We only need `index_sequence` and `make_index_sequence` to generate
// our lookup tables. These were added in C++14 so we have to provide our own
// implementation for C++11 mode.
#if BL_STDCXX_VERSION >= 201402L
namespace BLInternal {
  // C++14 implementation uses `std::index_sequence` and `std::make_index_sequence`.
  using std::index_sequence;
  using std::make_index_sequence;
}
#else
namespace BLInternal {
  // C++11 implementation based on <https://stackoverflow.com/a/17426611/410767>.
  template <size_t... Ints>
  struct index_sequence {
    using type = index_sequence;
    using value_type = size_t;
    static constexpr std::size_t size() noexcept { return sizeof...(Ints); }
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
}
#endif

template<typename T, size_t N, class Gen, size_t... Indexes>
static constexpr BLLookupTable<T, N> blLookupTableImpl(BLInternal::index_sequence<Indexes...>) noexcept {
  return BLLookupTable<T, N> {{ T(Gen::value(Indexes))... }};
}

//! Creates a lookup table of `BLLookupTable<T[N]>` by using the generator `Gen`.
template<typename T, size_t N, class Gen>
static constexpr BLLookupTable<T, N> blLookupTable() noexcept {
  return blLookupTableImpl<T, N, Gen>(BLInternal::make_index_sequence<N>{});
}

//! \}
//! \endcond

#endif // BLEND2D_LOOKUPTABLE_P_H
