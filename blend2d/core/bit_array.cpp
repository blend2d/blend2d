// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/bit_set_ops.h>
#include <blend2d/axl/commons.h>
#include <blend2d/axl/mem_ops.h>
#include <blend2d/core/api_build_p.h>
#include <blend2d/core/bit_array_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/utils/popcnt_p.h>

namespace bl::BitArrayDetail {

// BitArray - Private - Commons
// ============================

static constexpr size_t bit_word_size = axl::bit_size_of<BLBitWord>;

[[nodiscard]] static BL_INLINE_CONSTEXPR size_t bit_index_of(size_t word_index) noexcept { return word_index * bit_word_size; }
[[nodiscard]] static BL_INLINE_CONSTEXPR size_t word_index_of(size_t bit_index) noexcept { return bit_index / bit_word_size; }

[[nodiscard]] static BL_INLINE_CONSTEXPR size_t bit_count_from_word_count(size_t word_count) noexcept { return word_count * bit_word_size; }
[[nodiscard]] static BL_INLINE_CONSTEXPR size_t word_count_from_bit_count(size_t bit_count) noexcept { return (bit_count + (bit_word_size - 1u)) / bit_word_size; }

[[nodiscard]] static BL_INLINE_CONSTEXPR ObjectImplSize impl_size_from_capacity_in_words(size_t capacity_in_words) noexcept {
  return ObjectImplSize(axl::align_up(ObjectLayer::data_offset_of<BLBitArrayCore> + capacity_in_words * sizeof(BLBitWord), ObjectLayer::kMinImplAlignment));
}

[[nodiscard]] static BL_INLINE_CONSTEXPR ObjectImplSize impl_size_from_capacity(size_t capacity) noexcept {
  return impl_size_from_capacity_in_words(word_count_from_bit_count(capacity));
}

[[nodiscard]] static BL_INLINE_CONSTEXPR size_t capacity_in_words_from_impl_size(ObjectImplSize impl_size) noexcept {
  return (size_t(impl_size) - ObjectLayer::data_offset_of<BLBitArrayCore>) / sizeof(BLBitWord);
}

[[nodiscard]] static BL_INLINE_CONSTEXPR size_t capacity_in_bits_from_impl_size(ObjectImplSize impl_size) noexcept {
  return ((size_t(impl_size) - ObjectLayer::data_offset_of<BLBitArrayCore>) / sizeof(BLBitWord)) * bit_word_size;
}

template<typename T>
struct BitSpan {
  T* data;
  size_t size;
};

[[nodiscard]] static BL_INLINE BitSpan<BLBitWord> get_sso_span(BLBitArrayCore* self) noexcept {
  return {ObjectLayer::sso_data<BLBitWord>(self), ObjectLayer::sso_size(self)};
}

[[nodiscard]] static BL_INLINE BitSpan<const BLBitWord> get_sso_span(const BLBitArrayCore* self) noexcept {
  return {ObjectLayer::sso_data<BLBitWord>(self), ObjectLayer::sso_size(self)};
}

[[nodiscard]] static BL_INLINE BitSpan<BLBitWord> get_dynamic_span(BLBitArrayCore* self) noexcept {
  return {ObjectLayer::dyn_data<BLBitWord>(self), ObjectLayer::dyn_size(self)};
}

[[nodiscard]] static BL_INLINE BitSpan<const BLBitWord> get_dynamic_span(const BLBitArrayCore* self) noexcept {
  return {ObjectLayer::dyn_data<BLBitWord>(self), ObjectLayer::dyn_size(self)};
}

[[nodiscard]] static BL_INLINE BitSpan<BLBitWord> get_container_span(BLBitArrayCore* self) noexcept {
  return {ObjectLayer::get_data<BLBitWord>(self), ObjectLayer::get_size(self)};
}

[[nodiscard]] static BL_INLINE BitSpan<const BLBitWord> get_container_span(const BLBitArrayCore* self) noexcept {
  return {ObjectLayer::get_data<BLBitWord>(self), ObjectLayer::get_size(self)};
}

[[nodiscard]] static BL_INLINE BLBitWord get_last_word_zero_mask(size_t size_in_bits) noexcept {
  return axl::shr_wrap(axl::bit_ones<BLBitWord>, axl::neg(size_in_bits));
}

// BitArray - Private - Init
// =========================

static BL_INLINE BLResult init_dynamic(BLBitArrayCore* self, ObjectImplSize impl_size, size_t size = 0u) noexcept {
  size_t capacity = capacity_in_bits_from_impl_size(impl_size);
  return ObjectLayer::alloc_container_impl(self, impl_size, {
    .object_type    = BL_OBJECT_TYPE_BIT_ARRAY,
    .data_offset    = ObjectLayer::data_offset_of<BLBitArrayCore>,
    .size           = size,
    .capacity       = capacity
  });
}

static BL_INLINE BLResult init_dynamic_with_data(BLBitArrayCore* self, ObjectImplSize impl_size, size_t size_in_bits, BLSpan<const BLBitWord> data) noexcept {
  size_t capacity_in_words = bit_count_from_word_count(capacity_in_words_from_impl_size(impl_size));
  return ObjectLayer::alloc_container_impl_with_data(self, impl_size, {
    .object_type    = BL_OBJECT_TYPE_BIT_ARRAY,
    .data_offset    = ObjectLayer::data_offset_of<BLBitArrayCore>,
    .size           = size_in_bits,
    .capacity       = capacity_in_words,
    .src_data       = data.data(),
    .src_data_size  = data.size() * sizeof(BLBitWord)
  });
}

// BitArray - Private - Modify Op
// ==============================

// A helper function that makes the BitArray mutable, but only if `from` is within its bounds.
static BL_NOINLINE BLResult make_mutable_for_modify_op(BLBitArrayCore* self, size_t from, BitSpan<BLBitWord>* out) noexcept {
  if (self->_d.is_sso()) {
    BitSpan content = get_sso_span(self);
    if (from >= content.size) {
      return bl_make_error(BL_ERROR_INVALID_VALUE);
    }
    *out = content;
    return BL_SUCCESS;
  }
  else {
    BitSpan content = get_dynamic_span(self);
    if (from >= content.size) {
      return bl_make_error(BL_ERROR_INVALID_VALUE);
    }

    if (ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kDyn>(self)) {
      *out = content;
      return BL_SUCCESS;
    }

    BLBitArrayCore tmp;
    BLSpan<const BLBitWord> d_span{content.data, word_count_from_bit_count(content.size)};

    if (content.size <= BLBitArray::Info::kSSOCapacity) {
      ObjectLayer::init_sso_with_data(&tmp, content.size, d_span);
      *out = {ObjectLayer::sso_data<BLBitWord>(self), content.size};
    }
    else {
      ObjectImplSize impl_size = impl_size_from_capacity(content.size);
      BL_PROPAGATE(init_dynamic_with_data(&tmp, impl_size, content.size, d_span));
      *out = {ObjectLayer::dyn_data<BLBitWord>(&tmp), content.size};
    }

    return ObjectLayer::replace_object(self, &tmp);
  }
}

// Returns the original size of the BitArray when passed to this function (basically it returns the index where to append the bits).
static BL_NOINLINE BLResult make_mutable_for_append_op(BLBitArrayCore* self, size_t append_bit_count, size_t* bit_index, BitSpan<BLBitWord>* out) noexcept {
  BL_ASSERT(append_bit_count > 0u);

  BitSpan<BLBitWord> content;
  if (self->_d.is_sso()) {
    content = get_sso_span(self);
    *bit_index = content.size;

    if (ObjectLayer::check_sso_capacity(self, append_bit_count, content.size)) {
      size_t result_size = content.size + append_bit_count;
      ObjectLayer::set_sso_size(self, result_size);

      *out = {content.data, result_size};
      return BL_SUCCESS;
    }
  }
  else {
    content = get_dynamic_span(self);
    *bit_index = content.size;

    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, append_bit_count, content.size)) {
      size_t result_size = content.size + append_bit_count;
      size_t from_word = word_index_of(content.size + BitOps::kBitMask);
      size_t last_word = word_index_of(result_size - 1u);

      axl::fill_items(content.data + from_word, last_word - from_word + 1, BLBitWord(0));
      ObjectLayer::set_dyn_size(self, result_size);

      *out = {content.data, result_size};
      return BL_SUCCESS;
    }
  }

  axl::OverflowFlag of{};
  size_t new_size = axl::add_overflow(content.size, append_bit_count, of);

  if (BL_UNLIKELY(axl::did_overflow(of))) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  size_t old_word_count = word_count_from_bit_count(content.size);
  size_t new_word_count = word_count_from_bit_count(new_size);
  ObjectImplSize impl_size = impl_size_from_capacity_in_words(new_word_count);

  BLBitArrayCore tmp;
  BL_PROPAGATE(init_dynamic(&tmp, ObjectLayer::expand_impl_size(impl_size), new_size));

  BLBitWord* dst = ObjectLayer::dyn_data<BLBitWord>(&tmp);
  axl::copy_forward(dst, content.data, old_word_count);
  axl::fill_items(dst + old_word_count, new_word_count - old_word_count, BLBitWord(0));

  *out = BitSpan<BLBitWord>{dst, new_size};
  return ObjectLayer::replace_object(self, &tmp);
}

// BitArray - Private - Combine Op
// ===============================

template<typename BitOp>
static BL_INLINE BLResult combine_word_data(BitSpan<BLBitWord> d, size_t bit_index, const BLBitWord* word_data, size_t word_count) noexcept {
  size_t bit_end = bit_index + axl::min(bit_index_of(word_count), d.size - bit_index);
  size_t bit_count = bit_end - bit_index;

  size_t word_index = word_index_of(bit_index);
  BLBitWord* dst = d.data + word_index;
  size_t bit_shift = bit_index & BitOps::kBitMask;

  // Special case - if `word_data` is aligned to a word boundary, we don't have to shift the input BitWords.
  if (bit_shift == 0u) {
    word_count = axl::min(word_count_from_bit_count(bit_count), word_count);
    size_t end_bit_count = bit_end & BitOps::kBitMask;

    size_t end = word_count - size_t(end_bit_count != 0);
    BitOps::bit_array_combine_words<BitOp>(dst, word_data, end);

    if (end_bit_count) {
      dst[end] = BitOp::op_masked(dst[end], word_data[end], BitOps::non_zero_start_mask(end_bit_count));
    }

    return BL_SUCCESS;
  }

  BLBitWord w = word_data[0];
  size_t bit_shift_inv = BitOps::kNumBits - bit_shift;

  // Special case - if the number of processed bits is less than number of the remaining bits in the current BitWord.
  if (bit_count <= bit_shift_inv) {
    BLBitWord mask = BitOps::non_zero_start_mask(bit_count, bit_shift);
    dst[0] = BitOp::op_masked(dst[0], BitOps::shift_to_end(w, bit_shift), mask);
    return BL_SUCCESS;
  }

  // Process the first BitWord, which is not fully combined (must combine under a write-mask).
  dst[0] = BitOp::op_masked(dst[0], BitOps::shift_to_end(w, bit_shift), BitOps::non_zero_end_mask(bit_shift_inv));
  bit_count -= bit_shift_inv;

  // Process guaranteed BitWord quantities.
  size_t i = 1;
  size_t n = word_index_of(bit_count);

  while (i <= n) {
    size_t prev_word_bits = BitOps::shift_to_start(w, bit_shift_inv);
    w = word_data[i];
    dst[i] = BitOp::op(dst[i], prev_word_bits | BitOps::shift_to_end(w, bit_shift));

    i++;
  }

  bit_count &= BitOps::kBitMask;
  if (bit_count == 0) {
    return BL_SUCCESS;
  }

  size_t last_word_bits = BitOps::shift_to_start(w, bit_shift_inv);
  if (bit_shift_inv < bit_count) {
    last_word_bits |= BitOps::shift_to_end(word_data[i], bit_shift);
  }

  dst[i] = BitOp::op_masked(dst[i], last_word_bits, BitOps::non_zero_start_mask(bit_count));
  return BL_SUCCESS;
}

} // {bl::BitArrayDetail}

// BitArray - API - Init & Destroy
// ===============================

BL_API_IMPL BLResult bl_bit_array_init(BLBitArrayCore* self) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  return ObjectLayer::init_object(self);
}

BL_API_IMPL BLResult bl_bit_array_init_move(BLBitArrayCore* self, BLBitArrayCore* other) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::move_object(self, other);
}

BL_API_IMPL BLResult bl_bit_array_init_weak(BLBitArrayCore* self, const BLBitArrayCore* other) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::copy_object(self, other);
}

BL_API_IMPL BLResult bl_bit_array_destroy(BLBitArrayCore* self) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::release_object(self);
}

// BitArray - API - Reset
// ======================

BL_API_IMPL BLResult bl_bit_array_reset(BLBitArrayCore* self) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::reset_object(self);
}

// BitArray - API - Assign
// =======================

BL_API_IMPL BLResult bl_bit_array_assign_move(BLBitArrayCore* self, BLBitArrayCore* other) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_move(self, other);
}

BL_API_IMPL BLResult bl_bit_array_assign_weak(BLBitArrayCore* self, const BLBitArrayCore* other) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_weak(self, other);
}

BL_API_IMPL BLResult bl_bit_array_assign_words(BLBitArrayCore* self, const BLBitWord* word_data, size_t word_count) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  size_t size_in_bits = word_count * bit_word_size;
  BLBitArrayCore tmp;

  if (self->_d.is_sso()) {
    if (word_count <= BLBitArray::kSSOCapacityInWords) {
      ObjectLayer::init_sso_with_data(&tmp, size_in_bits, BLSpan<const BLBitWord>{word_data, word_count});
      self->_d = tmp._d;
      return BL_SUCCESS;
    }
  }
  else {
    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, size_in_bits)) {
      memmove(ObjectLayer::dyn_data<void>(self), word_data, word_count * sizeof(BLBitWord));
      ObjectLayer::set_dyn_size(self, size_in_bits);
      return BL_SUCCESS;
    }

    if (word_count <= BLBitArray::kSSOCapacityInWords) {
      ObjectLayer::init_sso_with_data(&tmp, size_in_bits, word_data, word_count * sizeof(BLBitWord));
      return ObjectLayer::replace_object<ObjectLayer::ObjectMode::kDyn>(self, &tmp);
    }
  }

  ObjectImplSize impl_size = impl_size_from_capacity_in_words(word_count);
  BL_PROPAGATE(init_dynamic_with_data(&tmp, impl_size, size_in_bits, {word_data, word_count}));
  return ObjectLayer::replace_object<ObjectLayer::ObjectMode::kAny>(self, &tmp);
}

BL_API_IMPL BLResult bl_bit_array_assign_zeroed(BLBitArrayCore* self, size_t n_bits, BLBitWord** data_out) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLBitWord* content_ptr;

  if (self->_d.is_sso()) {
    content_ptr = ObjectLayer::sso_data<BLBitWord>(self);
    if (n_bits <= BLBitArray::Info::kSSOCapacity) {
      *data_out = content_ptr;
      ObjectLayer::set_sso_size(self, n_bits);
      memset(content_ptr, 0, BLBitArray::kSSOCapacityInWords * sizeof(BLBitWord));
      return BL_SUCCESS;
    }
  }
  else {
    content_ptr = ObjectLayer::dyn_data<BLBitWord>(self);
    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, n_bits)) {
      *data_out = content_ptr;
      ObjectLayer::set_dyn_size(self, n_bits);

      size_t n_words = word_count_from_bit_count(n_bits);
      axl::fill_items(content_ptr, BLBitWord(0), n_words);
      return BL_SUCCESS;
    }

    if (n_bits <= BLBitArray::Info::kSSOCapacity) {
      ObjectLayer::release_object(self);
      ObjectLayer::init_sso_with_size(self, n_bits);
      *data_out = ObjectLayer::sso_data<BLBitWord>(self);
      return BL_SUCCESS;
    }
  }

  BL_ASSERT(n_bits > 0u);

  BLBitArrayCore tmp;
  ObjectImplSize impl_size = impl_size_from_capacity(n_bits);
  BL_PROPAGATE(init_dynamic(&tmp, impl_size, n_bits));

  content_ptr = ObjectLayer::dyn_data<BLBitWord>(&tmp);
  *data_out = content_ptr;

  size_t n_words = word_count_from_bit_count(n_bits);
  axl::fill_items(content_ptr, BLBitWord(0), n_words);
  return ObjectLayer::replace_object(self, &tmp);
}

// BitArray - API - Accessors
// ==========================

BL_API_IMPL bool bl_bit_array_is_empty(const BLBitArrayCore* self) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_size(self) == 0u;
}

BL_API_IMPL size_t bl_bit_array_get_size(const BLBitArrayCore* self) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_size(self);
}

BL_API_IMPL size_t bl_bit_array_get_size_in_words(const BLBitArrayCore* self) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return word_count_from_bit_count(ObjectLayer::get_size(self));
}

BL_API_IMPL size_t bl_bit_array_get_capacity(const BLBitArrayCore* self) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_capacity(self);
}

BL_API_IMPL size_t bl_bit_array_get_capacity_in_words(const BLBitArrayCore* self) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return word_count_from_bit_count(ObjectLayer::get_capacity(self));
}

BL_API_IMPL const BLBitWord* bl_bit_array_get_data(const BLBitArrayCore* self) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_data<BLBitWord>(self);
}

BL_API_IMPL size_t bl_bit_array_get_cardinality(const BLBitArrayCore* self) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BitSpan content = get_container_span(self);
  if (!content.size) {
    return 0u;
  }

  Utils::PopCounter<BLBitWord> counter;
  counter.add_array(content.data, word_count_from_bit_count(content.size));
  return counter.get();
}

BL_API_IMPL size_t bl_bit_array_get_cardinality_in_range(const BLBitArrayCore* self, size_t start_bit, size_t end_bit) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BitSpan content = get_container_span(self);
  size_t start = start_bit;
  size_t end = axl::min<size_t>(end_bit, content.size);

  if (start >= end) {
    return 0u;
  }

  size_t start_word = word_index_of(start);
  size_t last_word = word_index_of(end - 1u);
  Utils::PopCounter<size_t> counter;

  if (start_word == last_word) {
    // Special case - the range is within a single BitWord.
    BLBitWord mask = BitOps::non_zero_start_mask(end - start, start & BitOps::kBitMask);
    counter.add_item(content.data[start_word] & mask);
  }
  else {
    BLBitWord start_mask = BitOps::non_zero_end_mask(BitOps::kNumBits - BLBitWord(start & BitOps::kBitMask));
    BLBitWord end_mask = BitOps::non_zero_start_mask(((end - 1u) & BitOps::kBitMask) + 1u);

    counter.add_item(content.data[start_word] & start_mask);
    counter.add_array(content.data + start_word + 1, last_word - start_word - 1);
    counter.add_item(content.data[last_word] & end_mask);
  }

  return counter.get();
}

BL_API_IMPL bool bl_bit_array_has_bit(const BLBitArrayCore* self, size_t bit_index) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BitSpan content = get_container_span(self);
  if (bit_index >= content.size) {
    return false;
  }
  return BitOps::bit_array_test_bit(content.data, bit_index);
}

BL_API_IMPL bool bl_bit_array_has_bits_in_range(const BLBitArrayCore* self, size_t start_bit, size_t end_bit) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BitSpan d = get_container_span(self);
  size_t start = start_bit;
  size_t end = axl::min(d.size, end_bit);

  if (start >= end)
    return false;

  size_t start_word = word_index_of(start);
  size_t end_word = word_index_of(end - 1u);

  if (start_word == end_word) {
    // Special case - the range is within a single BitWord.
    BLBitWord mask = BitOps::non_zero_start_mask(end - start, start & BitOps::kBitMask);
    return (d.data[start_word] & mask) != 0u;
  }

  BLBitWord start_mask = BitOps::non_zero_end_mask(BitOps::kNumBits - (start & BitOps::kBitMask));
  if (d.data[start_word] & start_mask) {
    return true;
  }

  for (size_t i = start_word + 1; i < end_word; i++) {
    if (d.data[i]) {
      return true;
    }
  }

  BLBitWord end_mask = BitOps::non_zero_start_mask(((end - 1u) & BitOps::kBitMask) + 1u);
  return (d.data[end_word] & end_mask) != 0u;
}

// BitArray - API - Testing
// ========================

BL_API_IMPL bool bl_bit_array_subsumes(const BLBitArrayCore* a, const BLBitArrayCore* b) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(a));
  BL_ASSERT(ObjectLayer::is_object_valid(b));

  BitSpan a_content = get_container_span(a);
  BitSpan b_content = get_container_span(b);
  size_t shared_word_count = word_count_from_bit_count(axl::min(a_content.size, b_content.size));

  for (size_t i = 0; i < shared_word_count; i++) {
    if ((a_content.data[i] & b_content.data[i]) != b_content.data[i]) {
      return false;
    }
  }

  size_t b_word_count = word_count_from_bit_count(b_content.size);
  for (size_t i = shared_word_count; i < b_word_count; i++) {
    if (b_content.data[i]) {
      return false;
    }
  }

  return true;
}

BL_API_IMPL bool bl_bit_array_intersects(const BLBitArrayCore* a, const BLBitArrayCore* b) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(a));
  BL_ASSERT(ObjectLayer::is_object_valid(b));

  BitSpan a_content = get_container_span(a);
  BitSpan b_content = get_container_span(b);
  size_t shared_word_count = word_count_from_bit_count(axl::min(a_content.size, b_content.size));

  for (size_t i = 0; i < shared_word_count; i++) {
    if ((a_content.data[i] & b_content.data[i]) != 0u) {
      return true;
    }
  }

  return false;
}

BL_API_IMPL bool bl_bit_array_get_range(const BLBitArrayCore* self, size_t* start_out, size_t* end_out) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BitSpan content = get_container_span(self);
  size_t word_count = word_count_from_bit_count(content.size);

  for (size_t i = 0; i < word_count; i++) {
    BLBitWord bits = content.data[i];
    if (bits) {
      size_t start = bit_index_of(i) + BitOps::count_zeros_from_start(bits);
      for (size_t j = word_count; j != 0; j--) {
        bits = content.data[j - 1];
        if (bits) {
          size_t end = bit_index_of(j) - BitOps::count_zeros_from_end(bits);
          *start_out = uint32_t(start);
          *end_out = uint32_t(end);
          return true;
        }
      }
    }
  }

  // There are no bits set in this BitArray.
  *start_out = 0u;
  *end_out = 0u;
  return false;
}

// BitArray - API - Equality & Comparison
// ======================================

BL_API_IMPL bool bl_bit_array_equals(const BLBitArrayCore* a, const BLBitArrayCore* b) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(a));
  BL_ASSERT(ObjectLayer::is_object_valid(b));

  BitSpan<const BLBitWord> a_content = get_container_span(a);
  BitSpan<const BLBitWord> b_content = get_container_span(b);

  if (a_content.size != b_content.size) {
    return false;
  }

  size_t word_count = word_count_from_bit_count(a_content.size);
  for (size_t i = 0; i < word_count; i++) {
    if (a_content.data[i] != b_content.data[i]) {
      return false;
    }
  }

  return true;
}

BL_API_IMPL int bl_bit_array_compare(const BLBitArrayCore* a, const BLBitArrayCore* b) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(a));
  BL_ASSERT(ObjectLayer::is_object_valid(b));

  BitSpan<const BLBitWord> a_content = get_container_span(a);
  BitSpan<const BLBitWord> b_content = get_container_span(b);

  size_t min_size = axl::min(a_content.size, b_content.size);
  size_t word_count = word_count_from_bit_count(min_size);

  // We don't need any masking here - bits in a BitWord that are outside of a BitArray range must be zero. If one
  // of the BitArray has a greater size and any bit not used by the other filled, it's would compare as greater.
  for (size_t i = 0; i < word_count; i++) {
    if (a_content.data[i] != b_content.data[i]) {
      return BitOps::compare(a_content.data[i], b_content.data[i]);
    }
  }

  return a_content.size < b_content.size ? -1 : int(a_content.size > b_content.size);
}

// BitArray - API - Clear
// ======================

BL_API_IMPL BLResult bl_bit_array_clear(BLBitArrayCore* self) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (self->_d.is_ref_counted()) {
    if (ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kRC>(self)) {
      ObjectLayer::set_dyn_size(self, 0u);
      return BL_SUCCESS;
    }

    ObjectLayer::release_object<ObjectLayer::ObjectMode::kRC>(self);
  }

  return ObjectLayer::init_sso(self);
}

// BitArray - API - Storage Management
// ===================================

BL_API_IMPL BLResult bl_bit_array_resize(BLBitArrayCore* self, size_t n_bits) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  // Special case so we can assume `n_bits >= 1` in all of the core logic here.
  if (n_bits == 0u) {
    return bl_bit_array_clear(self);
  }

  BLBitArrayCore tmp;
  BitSpan<BLBitWord> content;
  size_t word_count = word_count_from_bit_count(n_bits);
  size_t copy_word_count;
  size_t zero_word_count;

  BLBitWord last_word_mask = get_last_word_zero_mask(n_bits);

  if (self->_d.is_sso()) {
    content = get_sso_span(self);
    copy_word_count = word_count_from_bit_count(axl::min(n_bits, content.size));
    zero_word_count = word_count - copy_word_count;

    if (ObjectLayer::check_sso_capacity(self, n_bits)) {
      axl::fill_items(content.data + copy_word_count, BLBitArray::kSSOCapacityInWords - copy_word_count, BLBitWord(0));
      content.data[word_count - 1u] &= last_word_mask;
      ObjectLayer::set_sso_size(self, n_bits);
      return BL_SUCCESS;
    }
  }
  else {
    content = get_dynamic_span(self);
    copy_word_count = word_count_from_bit_count(axl::min(n_bits, content.size));
    zero_word_count = word_count - copy_word_count;

    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, n_bits)) {
      axl::fill_items(content.data + copy_word_count, zero_word_count, BLBitWord(0));
      content.data[word_count - 1u] &= last_word_mask;
      ObjectLayer::set_dyn_size(self, n_bits);
      return BL_SUCCESS;
    }

    if (n_bits <= BLBitArray::Info::kSSOCapacity) {
      tmp._d = self->_d;
      ObjectLayer::init_sso_with_data(self, n_bits, content.data, copy_word_count * sizeof(BLBitWord));
      ObjectLayer::sso_data<BLBitWord>(self)[word_count - 1u] &= last_word_mask;
      return ObjectLayer::release_object(&tmp);
    }
  }

  ObjectImplSize impl_size = impl_size_from_capacity(n_bits);
  BL_PROPAGATE(init_dynamic_with_data(&tmp, impl_size, n_bits, {content.data, copy_word_count}));

  BLBitWord* dst = ObjectLayer::dyn_data<BLBitWord>(&tmp);
  axl::fill_items(dst + copy_word_count, zero_word_count, BLBitWord(0));
  dst[word_count - 1u] &= last_word_mask;
  return ObjectLayer::replace_object(self, &tmp);
}

BL_API_IMPL BLResult bl_bit_array_reserve(BLBitArrayCore* self, size_t min_size) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLBitArrayCore tmp;
  BitSpan<BLBitWord> content;

  if (self->_d.is_sso()) {
    if (ObjectLayer::check_sso_capacity(self, min_size)) {
      return BL_SUCCESS;
    }
    content = get_sso_span(self);
  }
  else {
    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, min_size)) {
      return BL_SUCCESS;
    }
    content = get_dynamic_span(self);
  }

  min_size = axl::max(min_size, content.size);
  size_t content_size_in_words = word_count_from_bit_count(content.size);

  ObjectImplSize impl_size = impl_size_from_capacity_in_words(word_count_from_bit_count(min_size));
  BL_PROPAGATE(init_dynamic_with_data(&tmp, impl_size, content.size, {content.data, content_size_in_words}));
  return ObjectLayer::replace_object(self, &tmp);
}

BL_API_IMPL BLResult bl_bit_array_shrink(BLBitArrayCore* self) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (!self->_d.is_ref_counted()) {
    return BL_SUCCESS;
  }

  BLBitArrayCore tmp;
  BitSpan<BLBitWord> content = get_dynamic_span(self);
  size_t content_size_in_words = word_count_from_bit_count(content.size);

  if (content.size <= BLBitArray::Info::kSSOCapacity) {
    ObjectLayer::init_sso_with_data(&tmp, content.size, content.data, content_size_in_words * sizeof(BLBitWord));
  }
  else {
    ObjectImplSize fitting_impl_size = impl_size_from_capacity(content.size);
    ObjectImplSize current_impl_size = impl_size_from_capacity(ObjectLayer::dyn_capacity(self));

    if (fitting_impl_size == current_impl_size) {
      return BL_SUCCESS;
    }

    BL_PROPAGATE(init_dynamic_with_data(&tmp, fitting_impl_size, content.size, {content.data, content_size_in_words}));
  }

  return ObjectLayer::replace_object<ObjectLayer::ObjectMode::kRC>(self, &tmp);
}

// BitArray - API - Set / Fill
// ===========================

BL_API_IMPL BLResult bl_bit_array_fill_bit(BLBitArrayCore* self, size_t bit_index) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BitSpan<BLBitWord> content;
  BL_PROPAGATE(make_mutable_for_modify_op(self, bit_index, &content));

  BitOps::bit_array_set_bit(content.data, bit_index);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_bit_array_fill_range(BLBitArrayCore* self, size_t start_bit, size_t end_bit) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(start_bit >= end_bit)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  BitSpan<BLBitWord> content;
  BL_PROPAGATE(make_mutable_for_modify_op(self, start_bit, &content));

  size_t end = axl::min(end_bit, content.size);
  BitOps::bit_array_fill(content.data, start_bit, end - start_bit);

  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_bit_array_fill_word(BLBitArrayCore* self, size_t bit_index, BLBitWord word_value) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return bl_bit_array_fill_words(self, bit_index, &word_value, 1u);
}

BL_API_IMPL BLResult bl_bit_array_fill_words(BLBitArrayCore* self, size_t bit_index, const BLBitWord* word_data, size_t word_count) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(word_count == 0u)) {
    return BL_SUCCESS;
  }

  BitSpan<BLBitWord> content;
  BL_PROPAGATE(make_mutable_for_modify_op(self, bit_index, &content));

  combine_word_data<BitOperator::Or>(content, bit_index, word_data, word_count);
  return BL_SUCCESS;
}

// BitArray - API - Clear
// ======================

BL_API_IMPL BLResult bl_bit_array_clear_bit(BLBitArrayCore* self, size_t bit_index) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BitSpan<BLBitWord> content;
  BL_PROPAGATE(make_mutable_for_modify_op(self, bit_index, &content));

  BitOps::bit_array_clear_bit(content.data, bit_index);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_bit_array_clear_range(BLBitArrayCore* self, size_t start_bit, size_t end_bit) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(start_bit >= end_bit))
    return bl_make_error(BL_ERROR_INVALID_VALUE);

  BitSpan<BLBitWord> content;
  BL_PROPAGATE(make_mutable_for_modify_op(self, start_bit, &content));

  size_t end = axl::min(end_bit, content.size);
  BitOps::bit_array_clear(content.data, start_bit, end - start_bit);

  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_bit_array_clear_word(BLBitArrayCore* self, size_t bit_index, BLBitWord word_value) noexcept {
  return bl_bit_array_clear_words(self, bit_index, &word_value, 1u);
}

BL_API_IMPL BLResult bl_bit_array_clear_words(BLBitArrayCore* self, size_t bit_index, const BLBitWord* word_data, size_t word_count) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(word_count == 0u)) {
    return BL_SUCCESS;
  }

  BitSpan<BLBitWord> content;
  BL_PROPAGATE(make_mutable_for_modify_op(self, bit_index, &content));

  combine_word_data<BitOperator::AndNot>(content, bit_index, word_data, word_count);
  return BL_SUCCESS;
}

// BitArray - API - Assign
// =======================

BL_API_IMPL BLResult bl_bit_array_replace_bit(BLBitArrayCore* self, size_t bit_index, bool bit_value) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (bit_value)
    return bl_bit_array_fill_bit(self, bit_index);
  else
    return bl_bit_array_clear_bit(self, bit_index);
}

BL_API_IMPL BLResult bl_bit_array_replace_word(BLBitArrayCore* self, size_t bit_index, BLBitWord word_value) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return bl_bit_array_replace_words(self, bit_index, &word_value, 1u);
}

BL_API_IMPL BLResult bl_bit_array_replace_words(BLBitArrayCore* self, size_t bit_index, const BLBitWord* word_data, size_t word_count) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(word_count == 0u)) {
    return BL_SUCCESS;
  }

  BitSpan<BLBitWord> content;
  BL_PROPAGATE(make_mutable_for_modify_op(self, bit_index, &content));

  combine_word_data<BitOperator::Assign>(content, bit_index, word_data, word_count);
  return BL_SUCCESS;
}

// BitArray - API - Append
// =======================

BL_API_IMPL BLResult bl_bit_array_append_bit(BLBitArrayCore* self, bool bit_value) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BitSpan<BLBitWord> content;
  size_t bit_index;
  BL_PROPAGATE(make_mutable_for_append_op(self, 1u, &bit_index, &content));

  BitOps::bit_array_or_bit(content.data, bit_index, bit_value);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_bit_array_append_word(BLBitArrayCore* self, BLBitWord word_value) noexcept {
  return bl_bit_array_append_words(self, &word_value, 1);
}

BL_API_IMPL BLResult bl_bit_array_append_words(BLBitArrayCore* self, const BLBitWord* word_data, size_t word_count) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(word_count == 0u)) {
    return BL_SUCCESS;
  }

  BitSpan<BLBitWord> content;
  size_t bit_index;
  BL_PROPAGATE(make_mutable_for_append_op(self, word_count * BitOps::kNumBits, &bit_index, &content));

  combine_word_data<BitOperator::Or>(content, bit_index, word_data, word_count);
  return BL_SUCCESS;
}

// BitArray - Runtime Registration
// ===============================

void bl_bit_array_rt_init([[maybe_unused]] BLRuntimeContext* rt) noexcept {
  BL_USING_DETAIL(bl::BitArrayDetail);
  static constexpr BLObjectType object_type = BLBitArray::Info::kType;

  ObjectLayer::init_sso(&bl_object_defaults[object_type], object_type);
}
