// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_AXL_MEM_OPS_H_INCLUDED
#define BLEND2D_AXL_MEM_OPS_H_INCLUDED

#include <blend2d/axl/build_integration.h>
#include <blend2d/axl/commons.h>

#if defined(__SANITIZE_MEMORY__)
  #define AXL_BUILD_MSAN
#elif defined(__clang__)
  #if __has_feature(memory_sanitizer)
    #define AXL_BUILD_MSAN
  #endif
#endif

namespace bl {

//! \addtogroup blend2d_axl
//! \{

// Memory Utilities - Aligned / Unaligned Memory Access
// ====================================================

namespace axl {

//! True if unaligned memory access is supported by the target platform.
inline constexpr bool kHasUnalignedMemoryAccess = (BL_TARGET_ARCH_X86 != 0) ||
                                                  (BL_TARGET_ARCH_ARM == 64) ||
                                                  (BL_TARGET_ARCH_WASM != 0);

namespace {

#if defined(__GNUC__) || defined(_MSC_VER)
#if defined(__GNUC__)
  #define BL_CUSTOM_ALIGNED_TYPE(T, Alignment) T __attribute__((__may_alias__, __aligned__(Alignment)))
#else
  #define BL_CUSTOM_ALIGNED_TYPE(T, Alignment) T __declspec(align(Alignment))
#endif

template<ByteOrder BO, size_t Alignment, typename T>
[[nodiscard]]
BL_INLINE_NODEBUG T load(const void* p) noexcept {
  using U = std_uint_t<sizeof(T)>;
  if constexpr (Alignment >= sizeof(T)) {
    U uv = *static_cast<const U*>(p);
    return std::bit_cast<T>(byteswap<BO>(uv));
  }
  else {
    typedef BL_CUSTOM_ALIGNED_TYPE(U, Alignment) UnalignedU;
    U uv = *static_cast<const UnalignedU*>(p);
    return std::bit_cast<T>(byteswap<BO>(uv));
  }
}

template<ByteOrder BO, size_t Alignment, typename T>
BL_INLINE_NODEBUG void store(void* p, const T& v) noexcept {
  using U = std_uint_t<sizeof(T)>;
  U uv = byteswap<BO>(std::bit_cast<U>(v));

  if constexpr (Alignment >= sizeof(T)) {
    *static_cast<U*>(p) = uv;
  }
  else {
    typedef BL_CUSTOM_ALIGNED_TYPE(U, Alignment) UnalignedU;
    *static_cast<UnalignedU*>(p) = uv;
  }
}
#undef BL_CUSTOM_ALIGNED_TYPE
#else
template<ByteOrder BO, size_t Alignment, typename T>
[[nodiscard]]
BL_INLINE_NODEBUG T load(const void* p) noexcept {
  using U = std_uint_t<sizeof(T)>;
  if constexpr (Alignment >= sizeof(T)) {
    return std::bit_cast<T>(byteswap<BO>(*static_cast<const U*>(p)));
  }
  else {
    U uv;
    memcpy(&uv, p, sizeof(U));
    return std::bit_cast<T>(byteswap<BO>(uv));
  }
}

template<ByteOrder BO, size_t Alignment, typename T>
BL_INLINE_NODEBUG void store(void* p, const T& v) noexcept {
  using U = std_uint_t<sizeof(T)>;
  if constexpr (Alignment >= sizeof(T)) {
    *static_cast<U*>(p) = std::bit_cast<T>(byteswap<BO>(std::bit_cast<U>(v)));
  }
  else {
    U uv = byteswap<BO>(std::bit_cast<U>(v));
    memcpy(p, &uv, sizeof(U));
  }
}
#endif

template<typename T> [[nodiscard]] BL_INLINE_NODEBUG T loada(const void* p) noexcept { return *static_cast<const T*>(p); }
template<typename T> [[nodiscard]] BL_INLINE_NODEBUG T loadu(const void* p) noexcept { return load<ByteOrder::kNative, 1, T>(p); }

template<ByteOrder BO, typename T> [[nodiscard]] BL_INLINE_NODEBUG T loada(const void* p) noexcept { return load<BO, sizeof(T), T>(p); }
template<ByteOrder BO, typename T> [[nodiscard]] BL_INLINE_NODEBUG T loadu(const void* p) noexcept { return load<BO, 1, T>(p); }

template<typename T> [[nodiscard]] BL_INLINE_NODEBUG T loada_le(const void* p) noexcept { return loada<ByteOrder::kLE, T>(p); }
template<typename T> [[nodiscard]] BL_INLINE_NODEBUG T loadu_le(const void* p) noexcept { return loadu<ByteOrder::kLE, T>(p); }

template<typename T> [[nodiscard]] BL_INLINE_NODEBUG T loada_be(const void* p) noexcept { return loada<ByteOrder::kBE, T>(p); }
template<typename T> [[nodiscard]] BL_INLINE_NODEBUG T loadu_be(const void* p) noexcept { return loadu<ByteOrder::kBE, T>(p); }

[[nodiscard]] BL_INLINE_NODEBUG int8_t load_i8(const void* p) noexcept { return *static_cast<const int8_t*>(p); }
[[nodiscard]] BL_INLINE_NODEBUG uint8_t load_u8(const void* p) noexcept { return *static_cast<const uint8_t*>(p); }

template<ByteOrder BO = ByteOrder::kNative> [[nodiscard]] BL_INLINE_NODEBUG int16_t loada_i16(const void* p) noexcept { return loada<BO, int16_t>(p); }
template<ByteOrder BO = ByteOrder::kNative> [[nodiscard]] BL_INLINE_NODEBUG uint16_t loada_u16(const void* p) noexcept { return loada<BO, uint16_t>(p); }
template<ByteOrder BO = ByteOrder::kNative> [[nodiscard]] BL_INLINE_NODEBUG int32_t loada_i32(const void* p) noexcept { return loada<BO, int32_t>(p); }
template<ByteOrder BO = ByteOrder::kNative> [[nodiscard]] BL_INLINE_NODEBUG uint32_t loada_u32(const void* p) noexcept { return loada<BO, uint32_t>(p); }
template<ByteOrder BO = ByteOrder::kNative> [[nodiscard]] BL_INLINE_NODEBUG int64_t loada_i64(const void* p) noexcept { return loada<BO, int64_t>(p); }
template<ByteOrder BO = ByteOrder::kNative> [[nodiscard]] BL_INLINE_NODEBUG uint64_t loada_u64(const void* p) noexcept { return loada<BO, uint64_t>(p); }

template<ByteOrder BO = ByteOrder::kNative> [[nodiscard]] BL_INLINE_NODEBUG int16_t loadu_i16(const void* p) noexcept { return loadu<BO, int16_t>(p); }
template<ByteOrder BO = ByteOrder::kNative> [[nodiscard]] BL_INLINE_NODEBUG uint16_t loadu_u16(const void* p) noexcept { return loadu<BO, uint16_t>(p); }
template<ByteOrder BO = ByteOrder::kNative> [[nodiscard]] BL_INLINE_NODEBUG int32_t loadu_i32(const void* p) noexcept { return loadu<BO, int32_t>(p); }
template<ByteOrder BO = ByteOrder::kNative> [[nodiscard]] BL_INLINE_NODEBUG uint32_t loadu_u32(const void* p) noexcept { return loadu<BO, uint32_t>(p); }
template<ByteOrder BO = ByteOrder::kNative> [[nodiscard]] BL_INLINE_NODEBUG int64_t loadu_i64(const void* p) noexcept { return loadu<BO, int64_t>(p); }
template<ByteOrder BO = ByteOrder::kNative> [[nodiscard]] BL_INLINE_NODEBUG uint64_t loadu_u64(const void* p) noexcept { return loadu<BO, uint64_t>(p); }

template<ByteOrder BO = ByteOrder::kNative>
[[nodiscard]]
BL_INLINE_NODEBUG uint32_t loadu_u24(const void* p) noexcept {
  uint32_t b0 = static_cast<const uint8_t*>(p)[BO == ByteOrder::kLE ? 2 : 0];
  uint32_t b1 = static_cast<const uint8_t*>(p)[1];
  uint32_t b2 = static_cast<const uint8_t*>(p)[BO == ByteOrder::kLE ? 0 : 2];
  return (b0 << 16) | (b1 << 8) | b2;
}

[[nodiscard]] BL_INLINE_NODEBUG int16_t loada_i16_le(const void* p) noexcept { return loada_i16<ByteOrder::kLE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG uint16_t loada_u16_le(const void* p) noexcept { return loada_u16<ByteOrder::kLE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG int32_t loada_i32_le(const void* p) noexcept { return loada_i32<ByteOrder::kLE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG uint32_t loada_u32_le(const void* p) noexcept { return loada_u32<ByteOrder::kLE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG int64_t loada_i64_le(const void* p) noexcept { return loada_i64<ByteOrder::kLE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG uint64_t loada_u64_le(const void* p) noexcept { return loada_u64<ByteOrder::kLE>(p); }

[[nodiscard]] BL_INLINE_NODEBUG int16_t loadu_i16_le(const void* p) noexcept { return loadu_i16<ByteOrder::kLE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG uint16_t loadu_u16_le(const void* p) noexcept { return loadu_u16<ByteOrder::kLE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG uint32_t loadu_u24_le(const void* p) noexcept { return loadu_u24<ByteOrder::kLE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG int32_t loadu_i32_le(const void* p) noexcept { return loadu_i32<ByteOrder::kLE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG uint32_t loadu_u32_le(const void* p) noexcept { return loadu_u32<ByteOrder::kLE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG int64_t loadu_i64_le(const void* p) noexcept { return loadu_i64<ByteOrder::kLE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG uint64_t loadu_u64_le(const void* p) noexcept { return loadu_u64<ByteOrder::kLE>(p); }

[[nodiscard]] BL_INLINE_NODEBUG int16_t loada_i16_be(const void* p) noexcept { return loada_i16<ByteOrder::kBE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG uint16_t loada_u16_be(const void* p) noexcept { return loada_u16<ByteOrder::kBE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG int32_t loada_i32_be(const void* p) noexcept { return loada_i32<ByteOrder::kBE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG uint32_t loada_u32_be(const void* p) noexcept { return loada_u32<ByteOrder::kBE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG int64_t loada_i64_be(const void* p) noexcept { return loada_i64<ByteOrder::kBE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG uint64_t loada_u64_be(const void* p) noexcept { return loada_u64<ByteOrder::kBE>(p); }

[[nodiscard]] BL_INLINE_NODEBUG int16_t loadu_i16_be(const void* p) noexcept { return loadu_i16<ByteOrder::kBE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG uint16_t loadu_u16_be(const void* p) noexcept { return loadu_u16<ByteOrder::kBE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG uint32_t loadu_u24_be(const void* p) noexcept { return loadu_u24<ByteOrder::kBE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG int32_t loadu_i32_be(const void* p) noexcept { return loadu_i32<ByteOrder::kBE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG uint32_t loadu_u32_be(const void* p) noexcept { return loadu_u32<ByteOrder::kBE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG int64_t loadu_i64_be(const void* p) noexcept { return loadu_i64<ByteOrder::kBE>(p); }
[[nodiscard]] BL_INLINE_NODEBUG uint64_t loadu_u64_be(const void* p) noexcept { return loadu_u64<ByteOrder::kBE>(p); }

template<typename T> BL_INLINE_NODEBUG void storea(void* p, const T& v) noexcept { *static_cast<T*>(p) = v; }
template<typename T> BL_INLINE_NODEBUG void storeu(void* p, const T& v) noexcept { store<ByteOrder::kNative, 1>(p, v); }

template<ByteOrder BO, typename T> BL_INLINE_NODEBUG void storea(void* p, const T& v) noexcept { store<BO, sizeof(T)>(p, v); }
template<ByteOrder BO, typename T> BL_INLINE_NODEBUG void storeu(void* p, const T& v) noexcept { store<BO, 1>(p, v); }

template<typename T> BL_INLINE_NODEBUG void storea_le(void* p, const T& v) noexcept { storea<ByteOrder::kLE>(p, v); }
template<typename T> BL_INLINE_NODEBUG void storeu_le(void* p, const T& v) noexcept { storeu<ByteOrder::kLE>(p, v); }

template<typename T> BL_INLINE_NODEBUG void storea_be(void* p, const T& v) noexcept { storea<ByteOrder::kBE>(p, v); }
template<typename T> BL_INLINE_NODEBUG void storeu_be(void* p, const T& v) noexcept { storeu<ByteOrder::kBE>(p, v); }

BL_INLINE_NODEBUG void store_i8(void* p, int8_t v) noexcept { *static_cast<int8_t*>(p) = v; }
BL_INLINE_NODEBUG void store_u8(void* p, uint8_t v) noexcept { *static_cast<uint8_t*>(p) = v; }

template<ByteOrder BO = ByteOrder::kNative> BL_INLINE_NODEBUG void storea_i16(void* p, int16_t v) noexcept { storea<BO>(p, v); }
template<ByteOrder BO = ByteOrder::kNative> BL_INLINE_NODEBUG void storea_u16(void* p, uint16_t v) noexcept { storea<BO>(p, v); }
template<ByteOrder BO = ByteOrder::kNative> BL_INLINE_NODEBUG void storea_i32(void* p, int32_t v) noexcept { storea<BO>(p, v); }
template<ByteOrder BO = ByteOrder::kNative> BL_INLINE_NODEBUG void storea_u32(void* p, uint32_t v) noexcept { storea<BO>(p, v); }
template<ByteOrder BO = ByteOrder::kNative> BL_INLINE_NODEBUG void storea_i64(void* p, int64_t v) noexcept { storea<BO>(p, v); }
template<ByteOrder BO = ByteOrder::kNative> BL_INLINE_NODEBUG void storea_u64(void* p, uint64_t v) noexcept { storea<BO>(p, v); }

template<ByteOrder BO = ByteOrder::kNative> BL_INLINE_NODEBUG void storeu_i16(void* p, int16_t v) noexcept { storeu<BO>(p, v); }
template<ByteOrder BO = ByteOrder::kNative> BL_INLINE_NODEBUG void storeu_u16(void* p, uint16_t v) noexcept { storeu<BO>(p, v); }
template<ByteOrder BO = ByteOrder::kNative> BL_INLINE_NODEBUG void storeu_i32(void* p, int32_t v) noexcept { storeu<BO>(p, v); }
template<ByteOrder BO = ByteOrder::kNative> BL_INLINE_NODEBUG void storeu_u32(void* p, uint32_t v) noexcept { storeu<BO>(p, v); }
template<ByteOrder BO = ByteOrder::kNative> BL_INLINE_NODEBUG void storeu_i64(void* p, int64_t v) noexcept { storeu<BO>(p, v); }
template<ByteOrder BO = ByteOrder::kNative> BL_INLINE_NODEBUG void storeu_u64(void* p, uint64_t v) noexcept { storeu<BO>(p, v); }

template<ByteOrder BO = ByteOrder::kNative>
BL_INLINE_NODEBUG void storeu_u24(void* p, uint32_t v) noexcept {
  static_cast<uint8_t*>(p)[0] = uint8_t((v >> (BO == ByteOrder::kLE ?  0 : 16)) & 0xFFu);
  static_cast<uint8_t*>(p)[1] = uint8_t((v >> 8) & 0xFFu);
  static_cast<uint8_t*>(p)[2] = uint8_t((v >> (BO == ByteOrder::kLE ? 16 :  0)) & 0xFFu);
}

BL_INLINE_NODEBUG void storea_i16_le(void* p, int16_t v) noexcept { storea<ByteOrder::kLE>(p, v); }
BL_INLINE_NODEBUG void storea_u16_le(void* p, uint16_t v) noexcept { storea<ByteOrder::kLE>(p, v); }
BL_INLINE_NODEBUG void storea_i32_le(void* p, int32_t v) noexcept { storea<ByteOrder::kLE>(p, v); }
BL_INLINE_NODEBUG void storea_u32_le(void* p, uint32_t v) noexcept { storea<ByteOrder::kLE>(p, v); }
BL_INLINE_NODEBUG void storea_i64_le(void* p, int64_t v) noexcept { storea<ByteOrder::kLE>(p, v); }
BL_INLINE_NODEBUG void storea_u64_le(void* p, uint64_t v) noexcept { storea<ByteOrder::kLE>(p, v); }

BL_INLINE_NODEBUG void storeu_i16_le(void* p, int16_t v) noexcept { storeu<ByteOrder::kLE>(p, v); }
BL_INLINE_NODEBUG void storeu_u16_le(void* p, uint16_t v) noexcept { storeu<ByteOrder::kLE>(p, v); }
BL_INLINE_NODEBUG void storeu_u24_le(void* p, uint32_t v) noexcept { storeu_u24<ByteOrder::kLE>(p, v); }
BL_INLINE_NODEBUG void storeu_i32_le(void* p, int32_t v) noexcept { storeu<ByteOrder::kLE>(p, v); }
BL_INLINE_NODEBUG void storeu_u32_le(void* p, uint32_t v) noexcept { storeu<ByteOrder::kLE>(p, v); }
BL_INLINE_NODEBUG void storeu_i64_le(void* p, int64_t v) noexcept { storeu<ByteOrder::kLE>(p, v); }
BL_INLINE_NODEBUG void storeu_u64_le(void* p, uint64_t v) noexcept { storeu<ByteOrder::kLE>(p, v); }

BL_INLINE_NODEBUG void storea_i16_be(void* p, int16_t v) noexcept { storea<ByteOrder::kBE>(p, v); }
BL_INLINE_NODEBUG void storea_u16_be(void* p, uint16_t v) noexcept { storea<ByteOrder::kBE>(p, v); }
BL_INLINE_NODEBUG void storea_i32_be(void* p, int32_t v) noexcept { storea<ByteOrder::kBE>(p, v); }
BL_INLINE_NODEBUG void storea_u32_be(void* p, uint32_t v) noexcept { storea<ByteOrder::kBE>(p, v); }
BL_INLINE_NODEBUG void storea_i64_be(void* p, int64_t v) noexcept { storea<ByteOrder::kBE>(p, v); }
BL_INLINE_NODEBUG void storea_u64_be(void* p, uint64_t v) noexcept { storea<ByteOrder::kBE>(p, v); }

BL_INLINE_NODEBUG void storeu_i16_be(void* p, int16_t v) noexcept { storeu<ByteOrder::kBE>(p, v); }
BL_INLINE_NODEBUG void storeu_u16_be(void* p, uint16_t v) noexcept { storeu<ByteOrder::kBE>(p, v); }
BL_INLINE_NODEBUG void storeu_u24_be(void* p, uint32_t v) noexcept { storeu_u24<ByteOrder::kBE>(p, v); }
BL_INLINE_NODEBUG void storeu_i32_be(void* p, int32_t v) noexcept { storeu<ByteOrder::kBE>(p, v); }
BL_INLINE_NODEBUG void storeu_u32_be(void* p, uint32_t v) noexcept { storeu<ByteOrder::kBE>(p, v); }
BL_INLINE_NODEBUG void storeu_i64_be(void* p, int64_t v) noexcept { storeu<ByteOrder::kBE>(p, v); }
BL_INLINE_NODEBUG void storeu_u64_be(void* p, uint64_t v) noexcept { storeu<ByteOrder::kBE>(p, v); }

// Memory Utilities - Fill
// =======================

BL_INLINE void fill_bytes_small(void* array, size_t size, uint8_t value) noexcept {
#if defined(__GNUC__)
  if (!__builtin_constant_p(size)) {
#if BL_TARGET_ARCH_X86 && !defined(AXL_BUILD_MSAN)
    size_t unused0, unused1;
    __asm__ __volatile__(
      "rep stosb" : "=&c"(unused0), "=&D"(unused1)
                  : "0"(size), "a"(value), "1"(array)
                  : "memory");
#else
    BL_NOUNROLL
    for (size_t i = 0; i < size; i++) {
      static_cast<uint8_t*>(array)[i] = value;
    }
#endif
  }
  else {
    for (size_t i = 0; i < size; i++) {
      static_cast<uint8_t*>(array)[i] = value;
    }
  }
#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
  __stosb(static_cast<unsigned char *>(array), static_cast<unsigned char>(value), size);
#else
  memset(array, value, size);
#endif
}

BL_INLINE void fill_bytes(void* array, size_t size, uint8_t value) noexcept {
  for (size_t i = 0; i < size; i++) {
    static_cast<uint8_t*>(array)[i] = value;
  }
}

template<typename T>
BL_INLINE void fill_items_small(T* array, size_t size, T value) noexcept {
  if constexpr (sizeof(T) == 1) {
    fill_bytes_small(array, size, std::bit_cast<uint8_t>(value));
  }
  else {
    BL_NOUNROLL
    for (size_t i = 0; i < size; i++) {
      array[i] = value;
    }
  }
}

template<typename T>
BL_INLINE void fill_items(T* array, size_t size, T value) noexcept {
  if constexpr (sizeof(T) == 1) {
    fill_bytes(array, size, std::bit_cast<uint8_t>(value));
  }
  else {
    for (size_t i = 0; i < size; i++) {
      array[i] = value;
    }
  }
}

// Memory Utilities - Memory Copy
// ==============================

//! Copies `n` bytes from `src` to `dst` - optimized for small buffers.
BL_INLINE void copy_bytes_small(void* dst, const void* src, size_t n) noexcept {
#if defined(__GNUC__)
  if (!__builtin_constant_p(n)) {
#if BL_TARGET_ARCH_X86 && !defined(AXL_BUILD_MSAN)
    size_t unused;
    __asm__ __volatile__(
      "rep movsb" : "=&D"(dst), "=&S"(src), "=&c"(unused)
                  : "0"(dst), "1"(src), "2"(n)
                  : "memory"
    );
#else
    BL_NOUNROLL
    for (size_t i = 0; i < n; i++) {
      static_cast<uint8_t*>(dst)[i] = static_cast<const uint8_t*>(src)[i];
    }
#endif
  }
  else {
    for (size_t i = 0; i < n; i++) {
      static_cast<uint8_t*>(dst)[i] = static_cast<const uint8_t*>(src)[i];
    }
  }
#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
  __movsb(static_cast<unsigned char *>(dst), static_cast<const unsigned char *>(src), n);
#else
  BL_NOUNROLL
  for (size_t i = 0; i < n; i++) {
    static_cast<uint8_t*>(dst)[i] = static_cast<const uint8_t*>(src)[i];
  }
#endif
}

template<typename T>
BL_INLINE void copy_forward(T* dst, const T* src, size_t n) noexcept {
  for (size_t i = 0; i < n; i++) {
    dst[i] = src[i];
  }
}

template<typename T>
BL_INLINE void copy_reverse(T* dst, const T* src, size_t n) noexcept {
  size_t i = n;
  while (i) {
    i--;
    dst[i] = src[i];
  }
}

} // {anonymous}
} // {axl}

//! \}

} // {bl}

#if defined(AXL_BUILD_MSAN)
  #undef AXL_BUILD_MSAN
#endif

#endif // BLEND2D_AXL_MEM_OPS_H_INCLUDED
