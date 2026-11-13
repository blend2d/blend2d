// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_BIT_ARRAY_H_INCLUDED
#define BLEND2D_CORE_BIT_ARRAY_H_INCLUDED

#include <blend2d/core/object_layer.h>

//! \addtogroup bl_c_api
//! \{

//! \name BLBitArray - C API
//! \{

//! \cond INTERNAL

#if defined(__cplusplus)
//! BitArray container [C API Impl].
struct BLBitArrayImpl {};
#endif // __cplusplus

//! \endcond

//! BitArray container [C API].
struct BLBitArrayCore BL_INHERITS_FROM(BLObjectCore) {
  BL_DEFINE_OBJECT(BLBitArray, bl::ObjectLayer::ContainerInfo<
    BL_OBJECT_TYPE_BIT_ARRAY,
    bl::ObjectLayer::ObjectMode::kAny,
    bl::ObjectLayer::ObjectLayout::kNative,
    (BLObjectDetail::kSSOCapacity - 6) * 8u, 0u, BLObjectDetail::kPL0ByteIndex>
  )

#ifdef __cplusplus
  static constexpr size_t kSSOCapacityInWords = Info::kSSOCapacity / (sizeof(BLBitWord) * 8u);
#endif
};

BL_EXTERN_C_BEGIN

BL_API BLResult BL_CDECL bl_bit_array_init(BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_init_move(BLBitArrayCore* self, BLBitArrayCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_init_weak(BLBitArrayCore* self, const BLBitArrayCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_destroy(BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_reset(BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_assign_move(BLBitArrayCore* self, BLBitArrayCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_assign_weak(BLBitArrayCore* self, const BLBitArrayCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_assign_words(BLBitArrayCore* self, const BLBitWord* word_data, size_t word_count) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_assign_zeroed(BLBitArrayCore* self, size_t n_bits, BLBitWord** data_out) BL_NOEXCEPT_C;
BL_API bool BL_CDECL bl_bit_array_is_empty(const BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL bl_bit_array_get_size(const BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL bl_bit_array_get_size_in_words(const BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL bl_bit_array_get_capacity(const BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL bl_bit_array_get_capacity_in_words(const BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API const BLBitWord* BL_CDECL bl_bit_array_get_data(const BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL bl_bit_array_get_cardinality(const BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL bl_bit_array_get_cardinality_in_range(const BLBitArrayCore* self, size_t start_bit, size_t end_bit) BL_NOEXCEPT_C;
BL_API bool BL_CDECL bl_bit_array_has_bit(const BLBitArrayCore* self, size_t bit_index) BL_NOEXCEPT_C;
BL_API bool BL_CDECL bl_bit_array_has_bits_in_range(const BLBitArrayCore* self, size_t start_bit, size_t end_bit) BL_NOEXCEPT_C;
BL_API bool BL_CDECL bl_bit_array_subsumes(const BLBitArrayCore* a, const BLBitArrayCore* b) BL_NOEXCEPT_C;
BL_API bool BL_CDECL bl_bit_array_intersects(const BLBitArrayCore* a, const BLBitArrayCore* b) BL_NOEXCEPT_C;
BL_API bool BL_CDECL bl_bit_array_get_range(const BLBitArrayCore* self, size_t* start_out, size_t* end_out) BL_NOEXCEPT_C;
BL_API bool BL_CDECL bl_bit_array_equals(const BLBitArrayCore* a, const BLBitArrayCore* b) BL_NOEXCEPT_C;
BL_API int BL_CDECL bl_bit_array_compare(const BLBitArrayCore* a, const BLBitArrayCore* b) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_clear(BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_resize(BLBitArrayCore* self, size_t n_bits) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_reserve(BLBitArrayCore* self, size_t min_size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_shrink(BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_fill_bit(BLBitArrayCore* self, size_t bit_index) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_fill_range(BLBitArrayCore* self, size_t start_bit, size_t end_bit) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_fill_word(BLBitArrayCore* self, size_t bit_index, BLBitWord word_value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_fill_words(BLBitArrayCore* self, size_t bit_index, const BLBitWord* word_data, size_t word_count) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_clear_bit(BLBitArrayCore* self, size_t bit_index) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_clear_range(BLBitArrayCore* self, size_t start_bit, size_t end_bit) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_clear_word(BLBitArrayCore* self, size_t bit_index, BLBitWord word_value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_clear_words(BLBitArrayCore* self, size_t bit_index, const BLBitWord* word_data, size_t word_count) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_replace_bit(BLBitArrayCore* self, size_t bit_index, bool bit_value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_replace_word(BLBitArrayCore* self, size_t bit_index, BLBitWord word_value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_replace_words(BLBitArrayCore* self, size_t bit_index, const BLBitWord* word_data, size_t word_count) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_append_bit(BLBitArrayCore* self, bool bit_value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_append_word(BLBitArrayCore* self, BLBitWord word_value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_bit_array_append_words(BLBitArrayCore* self, const BLBitWord* word_data, size_t word_count) BL_NOEXCEPT_C;

// TODO: Future API (BitArray).
/*
BL_API BLResult BL_CDECL bl_bit_array_combine(BLBitArrayCore* dst, const BLBitArrayCore* a, const BLBitArrayCore* b, BLBooleanOp op) BL_NOEXCEPT_C;
*/

BL_EXTERN_C_END

//! \}

//! \}

//! \addtogroup bl_containers
//! \{

//! \name BLBitArray - C++ API
//! \{
#ifdef __cplusplus

//! BitArray container [C++ API].
class BLBitArray final : public BLBitArrayCore {
public:
  //! \cond INTERNAL
  //! \name Internals
  //! \{

  //! Size of BLBitWord in bits.
  static constexpr size_t _bit_word_size = (sizeof(BLBitWord) * 8u);
  //! \}
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLBitArray() noexcept {
    bl::ObjectLayer::init_sso(this);
  }

  BL_INLINE_NODEBUG BLBitArray(BLBitArray&& other) noexcept {
    _d = other._d;
    bl::ObjectLayer::init_sso(&other);
  }

  BL_INLINE_NODEBUG BLBitArray(const BLBitArray& other) noexcept {
    bl_bit_array_init_weak(this, &other);
  }

  //! Destroys the BitArray.
  BL_INLINE_NODEBUG ~BLBitArray() noexcept {
    if (bl::ObjectLayer::requires_destroy(this)) {
      bl_bit_array_destroy(this);
    }
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  //! Tests whether the BitArray has a content.
  //!
  //! \note This is essentially the opposite of `is_empty()`.
  BL_INLINE_NODEBUG explicit operator bool() const noexcept { return !is_empty(); }

  //! Move assignment.
  //!
  //! \note The `other` BitArray is reset by move assignment, so its state after the move operation is the same as
  //! a default constructed BitArray.
  BL_INLINE_NODEBUG BLBitArray& operator=(BLBitArray&& other) noexcept { bl_bit_array_assign_move(this, &other); return *this; }
  //! Copy assignment, performs weak copy of the data held by the `other` BitArray.
  BL_INLINE_NODEBUG BLBitArray& operator=(const BLBitArray& other) noexcept { bl_bit_array_assign_weak(this, &other); return *this; }

  BL_INLINE_NODEBUG bool operator==(const BLBitArray& other) const noexcept { return equals(other); }
  BL_INLINE_NODEBUG bool operator!=(const BLBitArray& other) const noexcept { return !equals(other); }
  BL_INLINE_NODEBUG bool operator<(const BLBitArray& other) const noexcept { return compare(other) < 0; }
  BL_INLINE_NODEBUG bool operator<=(const BLBitArray& other) const noexcept { return compare(other) <= 0; }
  BL_INLINE_NODEBUG bool operator>(const BLBitArray& other) const noexcept { return compare(other) > 0; }
  BL_INLINE_NODEBUG bool operator>=(const BLBitArray& other) const noexcept { return compare(other) >= 0; }

  //! \}

  //! \name Common Functionality
  //! \{

  //! Clears the content of the BitArray and releases its data.
  //!
  //! After reset the BitArray content matches a default constructed instance.
  BL_INLINE_NODEBUG BLResult reset() noexcept {
    BLResult result = bl_bit_array_reset(this);

    // Reset operation always succeeds.
    BL_ASSUME(result == BL_SUCCESS);
    // Assume a default constructed BLBitArray after reset.
    BL_ASSUME(_d._sso_data[BLObjectDetail::kMetaByteIndex] == 0);

    return result;
  }

  //! Swaps the content of this bit-array with `other`.
  BL_INLINE_NODEBUG void swap(BLBitArrayCore& other) noexcept { _d.swap(other._d); }

  //! \name Accessors
  //! \{

  //! Tests whether the BitArray is empty.
  //!
  //! \remarks Only returns true if the bit-size is zero. The content is not considered, only the size of the container.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_empty() const noexcept { return bl::ObjectLayer::get_size(this) == 0u; }

  //! Returns the size of the BitArray in bits.
  [[nodiscard]] BL_INLINE_NODEBUG size_t size() const noexcept { return bl::ObjectLayer::get_size(this); }
  //! Returns the capacity of the BitArray in bits.
  [[nodiscard]] BL_INLINE_NODEBUG size_t capacity() const noexcept { return bl::ObjectLayer::get_capacity(this); }

  //! Returns number of BitWords this BitArray uses.
  [[nodiscard]] BL_INLINE_NODEBUG size_t size_in_words() const noexcept { return (size() + _bit_word_size - 1u) / _bit_word_size; }
  //! Returns the capacity of the BitArray in word units.
  [[nodiscard]] BL_INLINE_NODEBUG size_t capacity_in_words() const noexcept { return (capacity() + _bit_word_size - 1u) / _bit_word_size; }

  //! Returns data as 32-bit words.
  [[nodiscard]] BL_INLINE_NODEBUG const BLBitWord* data() const noexcept { return bl::ObjectLayer::get_data<BLBitWord>(this); }

  //! Returns the number of bits set in the BitArray.
  [[nodiscard]] BL_INLINE_NODEBUG size_t cardinality() const noexcept { return bl_bit_array_get_cardinality(this); }
  //! Returns the number of bits set in the given `[start_bit, end_bit)` range.
  [[nodiscard]] BL_INLINE_NODEBUG size_t cardinality_in_range(size_t start_bit, size_t end_bit) const noexcept { return bl_bit_array_get_cardinality_in_range(this, start_bit, end_bit); }
  //! Returns the number of bits set in the given `[range.start, range.end)` range.
  [[nodiscard]] BL_INLINE_NODEBUG size_t cardinality_in_range(BLRange range) const noexcept { return bl_bit_array_get_cardinality_in_range(this, range.start, range.end); }

  //! \}

  //! \name Test Operations
  //! \{

  //! Returns a bit-value at the given `bit_index`.
  [[nodiscard]] BL_INLINE_NODEBUG bool has_bit(size_t bit_index) const noexcept { return bl_bit_array_has_bit(this, bit_index); }
  //! Returns whether the bit-set has at least on bit in the given `[start_bit, end_bit)` range.
  [[nodiscard]] BL_INLINE_NODEBUG bool has_bits_in_range(size_t start_bit, size_t end_bit) const noexcept { return bl_bit_array_has_bits_in_range(this, start_bit, end_bit); }
  //! Returns whether the bit-set has at least on bit in the given `[range.start, range.end)` range.
  [[nodiscard]] BL_INLINE_NODEBUG bool has_bits_in_range(BLRange range) const noexcept { return bl_bit_array_has_bits_in_range(this, range.start, range.end); }

  //! Returns whether this BitArray subsumes `other`.
  [[nodiscard]] BL_INLINE_NODEBUG bool subsumes(const BLBitArrayCore& other) const noexcept { return bl_bit_array_subsumes(this, &other); }
  //! Returns whether this BitArray intersects with `other`.
  [[nodiscard]] BL_INLINE_NODEBUG bool intersects(const BLBitArrayCore& other) const noexcept { return bl_bit_array_intersects(this, &other); }

  BL_INLINE_NODEBUG bool get_range(BLRange* range_out) const noexcept { return bl_bit_array_get_range(this, &range_out->start, &range_out->end); }
  BL_INLINE_NODEBUG bool get_range(size_t* start_out, size_t* end_out) const noexcept { return bl_bit_array_get_range(this, start_out, end_out); }

  //! \}

  //! \name Equality & Comparison
  //! \{

  //! Returns whether this BitArray and `other` are bitwise equal.
  [[nodiscard]] BL_INLINE_NODEBUG bool equals(const BLBitArrayCore& other) const noexcept { return bl_bit_array_equals(this, &other); }
  //! Compares this BitArray with `other` and returns either `-1`, `0`, or `1`.
  [[nodiscard]] BL_INLINE_NODEBUG int compare(const BLBitArrayCore& other) const noexcept { return bl_bit_array_compare(this, &other); }

  //! \}

  //! \name Content Manipulation
  //! \{

  //! Move assignment, the same as `operator=`, but returns a `BLResult` instead of `this`.
  BL_INLINE_NODEBUG BLResult assign(BLBitArrayCore&& other) noexcept { return bl_bit_array_assign_move(this, &other); }

  //! Copy assignment, the same as `operator=`, but returns a `BLResult` instead of `this`.
  BL_INLINE_NODEBUG BLResult assign(const BLBitArrayCore& other) noexcept { return bl_bit_array_assign_weak(this, &other); }

  //! Replaces the content of the BitArray by bits specified by `word_data` of size `word_count` [the size is in BLBitWord units].
  BL_INLINE_NODEBUG BLResult assign_words(const BLBitWord* word_data, size_t word_count) noexcept { return bl_bit_array_assign_words(this, word_data, word_count); }

  //! Makes the BitArray mutable with the intention to replace all bits of it and sets its size to `n_bits`.
  //!
  //! \note All bits in the BitArray will be set to zero.
  BL_INLINE_NODEBUG BLResult assign_zeroed(size_t n_bits, BLBitWord** data_out) noexcept { return bl_bit_array_assign_zeroed(this, n_bits, data_out); }

  //! Clears the content of the BitArray without releasing its dynamically allocated data, if possible.
  BL_INLINE_NODEBUG BLResult clear() noexcept { return bl_bit_array_clear(this); }

  //! Resizes the BitArray so its size matches `n_bits`.
  BL_INLINE_NODEBUG BLResult resize(size_t n_bits) noexcept { return bl_bit_array_resize(this, n_bits); }

  //! Reserves `n_bits` in the BitArray (capacity would match `n_bits`) without changing its size.
  BL_INLINE_NODEBUG BLResult reserve(size_t min_size) noexcept { return bl_bit_array_reserve(this, min_size); }

  //! Shrinks the capacity of the BitArray to match the actual content with the intention to save memory.
  BL_INLINE_NODEBUG BLResult shrink() noexcept { return bl_bit_array_shrink(this); }

  //! Sets a bit to true at the given `bit_index`.
  BL_INLINE_NODEBUG BLResult fill_bit(size_t bit_index) noexcept { return bl_bit_array_fill_bit(this, bit_index); }

  //! Fills bits in `[start_bit, end_bit)` range to true.
  BL_INLINE_NODEBUG BLResult fill_range(size_t start_bit, size_t end_bit) noexcept { return bl_bit_array_fill_range(this, start_bit, end_bit); }
  //! Fills bits in `[range.start, range,end)` range to true.
  BL_INLINE_NODEBUG BLResult fill_range(BLRange range) noexcept { return bl_bit_array_fill_range(this, range.start, range.end); }

  //! Fills bits starting from `bit_index` specified by `word_value` to true (zeros in word_value are ignored).
  //!
  //! \note This operation uses an `OR` operator - bits in `word_value` are combined with OR operator with existing bits in BitArray.
  BL_INLINE_NODEBUG BLResult fill_word(size_t bit_index, BLBitWord word_value) noexcept { return bl_bit_array_fill_word(this, bit_index, word_value); }

  //! Fills bits starting from `bit_index` specified by `word_data` and `word_count` to true (zeros in word_data are ignored).
  //!
  //! \note This operation uses an `OR` operator - bits in `word_data` are combined with OR operator with existing bits in BitArray.
  BL_INLINE_NODEBUG BLResult fill_words(size_t bit_index, const BLBitWord* word_data, size_t word_count) noexcept { return bl_bit_array_fill_words(this, bit_index, word_data, word_count); }

  //! Sets a bit to false at the given `bit_index`.
  BL_INLINE_NODEBUG BLResult clear_bit(size_t bit_index) noexcept { return bl_bit_array_clear_bit(this, bit_index); }

  //! Sets bits in `[start_bit, end_bit)` range to false.
  BL_INLINE_NODEBUG BLResult clear_range(size_t start_bit, size_t end_bit) noexcept { return bl_bit_array_clear_range(this, start_bit, end_bit); }
  //! Sets bits in `[range.start, range.end)` range to false.
  BL_INLINE_NODEBUG BLResult clear_range(BLRange range) noexcept { return bl_bit_array_clear_range(this, range.start, range.end); }

  //! Sets bits starting from `bit_index` specified by `word_value` to false (zeros in word_value are ignored).
  //!
  //! \note This operation uses an `AND_NOT` operator - negated bits in `word_value` are combined with AND operator with existing bits in BitArray.
  BL_INLINE_NODEBUG BLResult clear_word(size_t bit_index, size_t word_value) noexcept { return bl_bit_array_clear_word(this, bit_index, word_value); }

  //! Sets bits starting from `bit_index` specified by `word_data` and `word_count` to false (zeros in word_data are ignored).
  //!
  //! \note This operation uses an `AND_NOT` operator - negated bits in `word_data` are combined with AND operator with existing bits in BitArray.
  BL_INLINE_NODEBUG BLResult clear_words(size_t bit_index, const BLBitWord* word_data, size_t word_count) noexcept { return bl_bit_array_clear_words(this, bit_index, word_data, word_count); }

  //! Replaces a bit in the BitArray at the given `bit_index` to match `bit_value`.
  BL_INLINE_NODEBUG BLResult replace_bit(size_t bit_index, bool bit_value) noexcept { return bl_bit_array_replace_bit(this, bit_index, bit_value); }

  //! Replaces bits starting from `bit_index` to match the bits specified by `word_value`.
  //!
  //! \note Replaced bits from BitArray are not combined by using any operator, `word_value` is copied as is, thus
  //! replaces fully the existing bits.
  BL_INLINE_NODEBUG BLResult replace_word(size_t bit_index, BLBitWord word_value) noexcept { return bl_bit_array_replace_word(this, bit_index, word_value); }

  //! Replaces bits starting from `bit_index` to match the bits specified by `word_data` and `word_count`.
  //!
  //! \note Replaced bits from BitArray are not combined by using any operator, `word_data` is copied as is, thus
  //! replaces fully the existing bits.
  BL_INLINE_NODEBUG BLResult replace_words(size_t bit_index, const BLBitWord* word_data, size_t word_count) noexcept { return bl_bit_array_replace_words(this, bit_index, word_data, word_count); }

  //! Appends a bit `bit_value` to the BitArray.
  BL_INLINE_NODEBUG BLResult append_bit(bool bit_value) noexcept { return bl_bit_array_append_bit(this, bit_value); }

  //! Appends a single word `word_value` to the BitArray.
  BL_INLINE_NODEBUG BLResult append_word(BLBitWord word_value) noexcept { return bl_bit_array_append_word(this, word_value); }

  //! Appends whole words to the BitArray.
  BL_INLINE_NODEBUG BLResult append_words(const BLBitWord* word_data, size_t word_count) noexcept { return bl_bit_array_append_words(this, word_data, word_count); }

  /*
  // TODO: Future API (BitArray).

  BL_INLINE_NODEBUG BLResult and_(const BLBitArrayCore& other) noexcept { return bl_bit_array_combine(this, this, &other, BL_BOOLEAN_OP_AND); }
  BL_INLINE_NODEBUG BLResult or_(const BLBitArrayCore& other) noexcept { return bl_bit_array_combine(this, this, &other, BL_BOOLEAN_OP_OR); }
  BL_INLINE_NODEBUG BLResult xor_(const BLBitArrayCore& other) noexcept { return bl_bit_array_combine(this, this, &other, BL_BOOLEAN_OP_XOR); }
  BL_INLINE_NODEBUG BLResult and_not(const BLBitArrayCore& other) noexcept { return bl_bit_array_combine(this, this, &other, BL_BOOLEAN_OP_AND_NOT); }
  BL_INLINE_NODEBUG BLResult not_and(const BLBitArrayCore& other) noexcept { return bl_bit_array_combine(this, this, &other, BL_BOOLEAN_OP_NOT_AND); }
  BL_INLINE_NODEBUG BLResult combine(const BLBitArrayCore& other, BLBooleanOp boolean_op) noexcept { return bl_bit_array_combine(this, this, &other, boolean_op); }

  static BL_INLINE_NODEBUG BLResult and_(BLBitArrayCore& dst, const BLBitArrayCore& a, const BLBitArrayCore& b) noexcept { return bl_bit_array_combine(&dst, &a, &b, BL_BOOLEAN_OP_AND); }
  static BL_INLINE_NODEBUG BLResult or_(BLBitArrayCore& dst, const BLBitArrayCore& a, const BLBitArrayCore& b) noexcept { return bl_bit_array_combine(&dst, &a, &b, BL_BOOLEAN_OP_OR); }
  static BL_INLINE_NODEBUG BLResult xor_(BLBitArrayCore& dst, const BLBitArrayCore& a, const BLBitArrayCore& b) noexcept { return bl_bit_array_combine(&dst, &a, &b, BL_BOOLEAN_OP_XOR); }
  static BL_INLINE_NODEBUG BLResult and_not(BLBitArrayCore& dst, const BLBitArrayCore& a, const BLBitArrayCore& b) noexcept { return bl_bit_array_combine(&dst, &a, &b, BL_BOOLEAN_OP_AND_NOT); }
  static BL_INLINE_NODEBUG BLResult not_and(BLBitArrayCore& dst, const BLBitArrayCore& a, const BLBitArrayCore& b) noexcept { return bl_bit_array_combine(&dst, &a, &b, BL_BOOLEAN_OP_NOT_AND); }
  static BL_INLINE_NODEBUG BLResult combine(BLBitArrayCore& dst, const BLBitArrayCore& a, const BLBitArrayCore& b, BLBooleanOp boolean_op) noexcept { return bl_bit_array_combine(&dst, &a, &b, boolean_op); }
  */

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_CORE_BIT_ARRAY_H_INCLUDED
