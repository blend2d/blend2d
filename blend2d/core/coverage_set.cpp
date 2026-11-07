// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/bitops_p.h>
#include <blend2d/axl/bit_set_ops.h>
#include <blend2d/axl/commons.h>
#include <blend2d/axl/mem_ops.h>
#include <blend2d/core/coverage_set_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/simd/simd_p.h>
#include <blend2d/utils/popcnt_p.h>

namespace bl::CoverageSetDetail {

// CoverageSet - Private - Null Impl
// =================================

static consteval BLCoverageSetImpl make_eternal_coverage_set_impl() noexcept {
  BLCoverageSetImpl impl {};
  for (uint32_t i = 0; i < kMPCount; i++) {
    impl.mp_offsets[i] = kPageZero;
  }
  return impl;
}

// The shared null Impl - a coverage with zero capacity that reports every code-point as absent.
static constexpr ObjectLayer::EternalImpl<BLCoverageSetImpl> null_coverage_set {
  .padding{},
  .header{},
  .impl = make_eternal_coverage_set_impl()
};

// CoverageSet - Private - Impl Size Details
// =========================================

static BL_INLINE ObjectImplSize impl_size_from_sp_capacity(size_t page_capacity) noexcept {
  return ObjectImplSize{sizeof(BLCoverageSetImpl) + page_capacity * kPageSize};
}

// CoverageSet - Private - Tables
// ==============================

static consteval MasterPlane const_init_mp(size_t page_index) noexcept {
  MasterPlane mp {};
  uint32_t sp_cardinality = page_index == kPageZero ? 0u : kSPBitCount;

  for (uint32_t i = 0; i < kSPPerMP; i++) {
    mp.sp_page_hi[i] = static_cast<uint16_t>((page_index << 4) | (sp_cardinality >> 8));
    mp.sp_card_lo[i] = static_cast<uint8_t>(sp_cardinality & 0xFFu);
  }

  mp.mp_cardinality = sp_cardinality * kSPPerMP;
  mp.sp_filled_mask = page_index == kPageZero ? 0u : 0xFFFFFFFFu;

  return mp;
}

static consteval SubPlane const_init_sp(BLBitWord bits) noexcept {
  SubPlane sp {};
  for (uint32_t i = 0; i < kBWCount; i++) {
    sp.bits[i] = bits;
  }
  return sp;
}

static constexpr MasterPlane builtin_mp_table[2] = { const_init_mp(kPageZero), const_init_mp(kPageFull) };
static constexpr SubPlane builtin_sp_table[2] = { const_init_sp(0), const_init_sp(axl::bit_ones<BLBitWord>) };

// CoverageSet - Private - Bit/Range Ops
// =====================================

enum class UnicodeCoverageOp : uint32_t { kZero = 0, kFill = 1 };

template<UnicodeCoverageOp Op>
inline constexpr BLBitWord fill_pattern = (Op == UnicodeCoverageOp::kZero) ? BLBitWord(0) : axl::bit_ones<BLBitWord>;

template<UnicodeCoverageOp Op, typename T>
static BL_INLINE T invert_when_zeroing(const T& bits) noexcept { return (Op == UnicodeCoverageOp::kZero) ? ~bits : bits; }

template<UnicodeCoverageOp Op>
static BL_INLINE BLBitWord masked_op(BLBitWord a, BLBitWord b) noexcept { return (Op == UnicodeCoverageOp::kZero) ? a & b : a | b; }

template<typename T>
static BL_INLINE uint32_t count_up_to_2_bits(const T& bit_mask) noexcept {
  auto x = axl::as_std_uint(bit_mask);
  return (x != 0) + ((x & (x - 1)) != 0);
}

template<typename T>
static BL_INLINE uint32_t count_1_or_2_bits(const T& bit_mask) noexcept {
  return 1 + axl::has_at_least_2_bits_set(bit_mask);
}

template<typename T>
static BL_INLINE T bit_mask_from_to(auto start, auto end) noexcept {
  return T(axl::shl_wrap(axl::bit_ones<T>, start) &
           axl::shr_wrap(axl::bit_ones<T>, axl::neg(end)));
}

// CoverageSet - Private - Capacity
// ================================

static BL_INLINE_NODEBUG size_t expand_capacity(size_t count) noexcept {
  return axl::min<size_t>(count + (count / 2u) + 4u, kMaxPageCapacity);
}

static BL_INLINE_NODEBUG PageIndex opposite_special_offset(PageIndex special_offset) noexcept {
  return static_cast<PageIndex>(static_cast<size_t>(special_offset) ^ 1u);
}

// CoverageSet - Private - Page Utilities
// ======================================

static BL_INLINE_NODEBUG size_t get_page_count(const BLCoverageSetCore* self) noexcept { return self->_d.layout<Layout>().page_count; }
static BL_INLINE_NODEBUG size_t get_page_capacity(const BLCoverageSetCore* self) noexcept { return self->_d.layout<Layout>().page_capacity; }

static BL_INLINE_NODEBUG void set_page_count(BLCoverageSetCore* self, size_t page_count) noexcept {
  self->_d.layout<Layout>().page_count = static_cast<uint16_t>(page_count);
}


static BL_INLINE_NODEBUG size_t get_remaining_page_capacity(const BLCoverageSetCore* self) noexcept {
  const Layout& layout = self->_d.layout<Layout>();
  return static_cast<size_t>(
    static_cast<uint32_t>(layout.page_capacity) - static_cast<uint32_t>(layout.page_count));
}

template<typename T>
static BL_INLINE T* get_page_with_byte_offset(BLCoverageSetImpl* impl, size_t byte_offset) noexcept requires (sizeof(T) == kPageSize) {
  return axl::offset_ptr<T>(impl, sizeof(BLCoverageSetImpl) + byte_offset);
}

template<typename T>
static BL_INLINE const T* get_page_with_byte_offset(const BLCoverageSetImpl* impl, size_t byte_offset) noexcept requires (sizeof(T) == kPageSize) {
  return axl::offset_ptr<const T>(impl, sizeof(BLCoverageSetImpl) + byte_offset);
}

template<typename T>
static BL_INLINE T* get_page(BLCoverageSetImpl* impl, PageIndex page_index) noexcept requires (sizeof(T) == kPageSize) {
  BL_ASSERT(static_cast<size_t>(page_index) < kMaxPageCapacity);
  return get_page_with_byte_offset<T>(impl, static_cast<size_t>(page_index) * kPageSize);
}

template<typename T>
static BL_INLINE const T* get_page(const BLCoverageSetImpl* impl, PageIndex page_index) noexcept requires (sizeof(T) == kPageSize) {
  BL_ASSERT(static_cast<size_t>(page_index) < kMaxPageCapacity);
  return get_page_with_byte_offset<T>(impl, static_cast<size_t>(page_index) * kPageSize);
}

template<typename T>
static BL_INLINE void copy_page(T* dst, const T* src) noexcept requires (sizeof(T) == kPageSize) { memcpy(dst, src, sizeof(T)); }

// CoverageSet - Private - BitWord Utilities
// =========================================

//! Returns a bit-word index within a sub-plane.
static BL_INLINE_NODEBUG size_t bw_index_from_bit_index(uint32_t index) noexcept { return size_t(index / axl::bit_size_of<BLBitWord>) & kBWMask; }

// CoverageSet - Private - Sub-Plane Composition & Decomposition
// =============================================================

struct SubPlaneInfo {
  size_t page_hi;
  uint32_t card_lo;

  static BL_INLINE_NODEBUG SubPlaneInfo from_page_index_and_cardinality(PageIndex page_index, uint32_t cardinality) noexcept {
    return SubPlaneInfo{(static_cast<size_t>(page_index) << 4) | (cardinality >> 8), cardinality & 0xFFu};
  }

  static BL_INLINE_NODEBUG SubPlaneInfo from_page_index_and_cardinality(size_t page_index, uint32_t cardinality) noexcept {
    return SubPlaneInfo{(static_cast<size_t>(page_index) << 4) | (cardinality >> 8), cardinality & 0xFFu};
  }

  BL_INLINE_NODEBUG bool is_dense() const noexcept { return (page_hi & (kPageSpecial << 4)) == 0u; }
  BL_INLINE_NODEBUG bool is_special() const noexcept { return (page_hi & (kPageSpecial << 4)) != 0u; }
  BL_INLINE_NODEBUG bool is_zero_page() const noexcept { return page_hi == ((kPageZero << 4) | (0x0u)); }
  BL_INLINE_NODEBUG bool is_full_page() const noexcept { return page_hi == ((kPageFull << 4) | (kSPBitCount >> 8)); }

  template<PageIndex idx>
  BL_INLINE_NODEBUG bool is_special() const noexcept
    requires (static_cast<size_t>(idx) == kPageZero ||
              static_cast<size_t>(idx) == kPageFull)
  {
    if constexpr (idx == static_cast<PageIndex>(kPageZero)) {
      return is_zero_page();
    }
    else {
      return is_full_page();
    }
  }

  BL_INLINE_NODEBUG PageIndex page_index() const noexcept { return static_cast<PageIndex>(page_hi >> 4u); }

  BL_INLINE_NODEBUG bool has_cardinality() const noexcept { return (card_lo | (page_hi & 0xF)) != 0; }
  BL_INLINE_NODEBUG bool has_special_cardinality() const noexcept { return ((card_lo | (page_hi << 8)) & kSPBitMask) == 0u; }

  BL_INLINE_NODEBUG uint32_t cardinality() const noexcept { return (card_lo | (page_hi << 8)) & 0xFFFu; }
  BL_INLINE_NODEBUG uint32_t special_cardinality() const noexcept { return (uint32_t(page_hi & 0xFu) << 8); }

  BL_INLINE void flip_special() noexcept {
    BL_ASSERT(is_special());
    page_hi ^= (0x1 << 4) | (kSPBitCount >> 8);
  }

  BL_INLINE void set_page_index(PageIndex page_index) noexcept {
    page_hi = (page_hi & 0xFu) + (static_cast<size_t>(page_index) << 4);
  }

  BL_INLINE void set_page_index_and_cardinality(PageIndex page_index, uint32_t cardinality) noexcept {
    page_hi = (static_cast<size_t>(page_index) << 4) | (cardinality >> 8);
    card_lo = cardinality & 0xFFu;
  }

  BL_INLINE void add_cardinality(uint32_t value) noexcept {
    card_lo += value;
    page_hi += card_lo >> 8u;
    card_lo &= 0xFFu;
  }

  BL_INLINE void sub_cardinality(uint32_t value) noexcept {
    size_t tmp_lo = static_cast<size_t>(card_lo) - value;
    page_hi += axl::sar(tmp_lo, 8);
    card_lo = (tmp_lo & 0xFFu);
  }

  template<UnicodeCoverageOp Op>
  BL_INLINE void cardinality_op(uint32_t value) noexcept {
    if constexpr (Op == UnicodeCoverageOp::kFill) {
      add_cardinality(value);
    }
    else {
      sub_cardinality(value);
    }
  }
};

static BL_INLINE_NODEBUG SubPlaneInfo mp_load_sp_info(const MasterPlane* mp, size_t sp_index) noexcept {
  return {mp->sp_page_hi[sp_index], mp->sp_card_lo[sp_index]};
}

static BL_INLINE_NODEBUG void mp_store_sp_info(MasterPlane* mp, size_t sp_index, SubPlaneInfo info) noexcept {
  BL_ASSERT(static_cast<size_t>(info.page_index()) < kMaxPageCapacity || info.is_zero_page() || info.is_full_page());
  BL_ASSERT(info.card_lo <= 0xFFu);

  mp->sp_card_lo[sp_index] = uint8_t(info.card_lo);
  mp->sp_page_hi[sp_index] = uint16_t(info.page_hi);
}

// Can be used instead of `mp_store_sp_info()` in case in which cardinality didn't change - used
// by `copy_content()`, which allocates pages, but copies dense data without cardinality changes.
static BL_INLINE_NODEBUG void mp_store_sp_offset(MasterPlane* mp, size_t sp_index, SubPlaneInfo info) noexcept {
  BL_ASSERT(static_cast<size_t>(info.page_index()) < kMaxPageCapacity || info.is_zero_page() || info.is_full_page());
  mp->sp_page_hi[sp_index] = uint16_t(info.page_hi);
}

static BL_INLINE_NODEBUG size_t get_mp_count(const BLCoverageSetCore* self) noexcept { return self->_d.layout<Layout>().mp_count; }
static BL_INLINE_NODEBUG void set_mp_count(BLCoverageSetCore* self, size_t mp_count) noexcept { self->_d.layout<Layout>().mp_count = uint16_t(mp_count); }

static BL_INLINE_NODEBUG uint16_t* get_mp_offsets(BLCoverageSetCore* self) noexcept { return self->_d.layout<Layout>().impl.ptr->mp_offsets; }
static BL_INLINE_NODEBUG const uint16_t* get_mp_offsets(const BLCoverageSetCore* self) noexcept { return self->_d.layout<Layout>().impl.ptr->mp_offsets; }

// CoverageSet - Private - Cardinality Utilities
// =============================================

// Checks whether the given cardinality means that a master-plane/sub-plane is either empty or full, but not between.
static BL_INLINE_NODEBUG bool mp_is_cardinality_empty_or_full(uint32_t cardinality) noexcept { return cardinality - 1u >= kMPBitCount - 1u; }
static BL_INLINE_NODEBUG bool sp_is_cardinality_empty_or_full(uint32_t cardinality) noexcept { return cardinality - 1u >= kSPBitCount - 1u; }

// Returns a special offset based on `cardinality` - the result is only valid if cardinality is either zero or full, but not between.
static BL_INLINE_NODEBUG PageIndex mp_special_offset_from_cardinality(uint32_t cardinality) noexcept {
  return static_cast<PageIndex>((cardinality >> axl::ctz_const(kMPBitCount)) | kPageZero);
}

static BL_INLINE_NODEBUG PageIndex sp_special_offset_from_cardinality(uint32_t cardinality) noexcept {
  return static_cast<PageIndex>((cardinality >> axl::ctz_const(kSPBitCount)) | kPageZero);
}

static BL_INLINE bool sp_has_cardinality_in_range(const BLBitWord* bits, uint32_t range_start, uint32_t range_end) noexcept {
  size_t bw_index = bw_index_from_bit_index(range_start);
  size_t bw_index_last = bw_index_from_bit_index(range_end - 1u);

  // Must hold - responsible callers must provide a valid `range_start` and `range_end`.
  BL_ASSERT(bw_index <= bw_index_last);

  BLBitWord bw_mask_start = axl::shl_wrap(axl::bit_ones<BLBitWord>, range_start);
  BLBitWord bw_mask_end = axl::shr_wrap(axl::bit_ones<BLBitWord>, axl::neg(range_end));

  BLBitWord bw_bits = bits[bw_index] & bw_mask_start;
  BLBitWord mask = 0u;

  if (bw_index != bw_index_last) {
    for (;;) {
      mask |= bw_bits;
      bw_bits = bits[++bw_index];
      if (bw_index >= bw_index_last) {
        break;
      }
    }
  }

  mask |= bw_bits & bw_mask_end;
  return mask != 0u;
}

static BL_INLINE uint32_t sp_get_cardinality_in_range(const BLBitWord* bits, uint32_t range_start, uint32_t range_end, uint32_t acc) noexcept {
  size_t bw_index = bw_index_from_bit_index(range_start);
  size_t bw_index_last = bw_index_from_bit_index(range_end - 1u);

  // Must hold - responsible callers must provide a valid `range_start` and `range_end`.
  BL_ASSERT(bw_index <= bw_index_last);

  BLBitWord bw_mask_start = axl::shl_wrap(axl::bit_ones<BLBitWord>, range_start);
  BLBitWord bw_mask_end = axl::shr_wrap(axl::bit_ones<BLBitWord>, axl::neg(range_end));

  BLBitWord bw_bits = bits[bw_index] & bw_mask_start;
  if (bw_index != bw_index_last) {
    for (;;) {
      acc += axl::popcnt(bw_bits);
      bw_bits = bits[++bw_index];
      if (bw_index >= bw_index_last) {
        break;
      }
    }
  }

  acc += axl::popcnt(bw_bits & bw_mask_end);
  return acc;
}

// CoverageSet - Private - Master-Plane & Sub-Plane Utilities
// ==========================================================

// Master-plane/sub-plane index from bit `index`.
static BL_INLINE_NODEBUG size_t mp_index_from_bit_index(uint32_t index) noexcept { return static_cast<size_t>(index >> kMPBitShift); }
static BL_INLINE_NODEBUG size_t sp_index_from_bit_index(uint32_t index) noexcept { return static_cast<size_t>((index >> kSPShift) & kSPMask); }

static BL_INLINE MasterPlane* get_mp_dense(BLCoverageSetImpl* impl, PageIndex mp_offset) noexcept { return get_page<MasterPlane>(impl, mp_offset); }
static BL_INLINE SubPlane* get_sp_dense(BLCoverageSetImpl* impl, PageIndex sp_offset) noexcept { return get_page<SubPlane>(impl, sp_offset); }

static BL_INLINE SubPlane* get_sp_dense(BLCoverageSetImpl* impl, SubPlaneInfo sp_info) noexcept {
  size_t page_index_scaled = size_t(sp_info.page_hi) & ~size_t(0xF);
  return axl::offset_ptr<SubPlane>(impl, sizeof(BLCoverageSetImpl) + page_index_scaled * 8u);
}

static BL_INLINE const SubPlane* get_sp_dense(const BLCoverageSetImpl* impl, SubPlaneInfo sp_info) noexcept {
  size_t page_index_scaled = size_t(sp_info.page_hi) & ~size_t(0xF);
  return axl::offset_ptr<SubPlane>(impl, sizeof(BLCoverageSetImpl) + page_index_scaled * 8u);
}

static BL_INLINE const MasterPlane* get_mp_dense(const BLCoverageSetImpl* impl, PageIndex mp_offset) noexcept {
  return get_page<MasterPlane>(impl, mp_offset);
}

static BL_INLINE const SubPlane* get_sp_dense(const BLCoverageSetImpl* impl, PageIndex sp_offset) noexcept {
  return get_page<SubPlane>(impl, sp_offset);
}

static BL_INLINE MasterPlane* get_mp_or_builtin(BLCoverageSetImpl* impl, PageIndex mp_offset) noexcept {
  if (is_page_special(mp_offset)) {
    return const_cast<MasterPlane*>(&builtin_mp_table[static_cast<size_t>(mp_offset) - kPageSpecial]);
  }
  else {
    return get_mp_dense(impl, mp_offset);
  }
}

static BL_INLINE const MasterPlane* get_mp_or_builtin(const BLCoverageSetImpl* impl, PageIndex mp_offset) noexcept {
  if (is_page_special(mp_offset)) {
    return &builtin_mp_table[static_cast<size_t>(mp_offset) - kPageSpecial];
  }
  else {
    return get_mp_dense(impl, mp_offset);
  }
}

static BL_INLINE SubPlane* get_sp_or_builtin(BLCoverageSetImpl* impl, PageIndex sp_offset) noexcept {
  if (is_page_special(sp_offset)) {
    return const_cast<SubPlane*>(&builtin_sp_table[static_cast<size_t>(sp_offset) - kPageSpecial]);
  }
  else {
    return get_sp_dense(impl, sp_offset);
  }
}

static BL_INLINE const SubPlane* get_sp_or_builtin(const BLCoverageSetImpl* impl, PageIndex sp_offset) noexcept {
  if (is_page_special(sp_offset)) {
    return &builtin_sp_table[static_cast<size_t>(sp_offset) - kPageSpecial];
  }
  else {
    return get_sp_dense(impl, sp_offset);
  }
}

static BL_INLINE SubPlane* get_sp_or_builtin(BLCoverageSetImpl* impl, SubPlaneInfo sp_info) noexcept {
  return get_sp_or_builtin(impl, sp_info.page_index());
}

static BL_INLINE const SubPlane* get_sp_or_builtin(const BLCoverageSetImpl* impl, SubPlaneInfo sp_info) noexcept {
  return get_sp_or_builtin(impl, sp_info.page_index());
}

static BL_INLINE void sp_init_page(SubPlane* sp, BLBitWord pattern) noexcept { axl::fill_items(sp->bits, kBWCount, pattern); }

template<UnicodeCoverageOp Op>
static BL_INLINE uint32_t sp_init_full_range(SubPlane* sp, uint32_t range_start, uint32_t range_end) noexcept {
  constexpr BLBitWord kPattern = fill_pattern<Op>;

  size_t bw_index = 0;
  size_t bw_index_start = bw_index_from_bit_index(range_start);
  size_t bw_index_last = bw_index_from_bit_index(range_end - 1u);

  BLBitWord bw_mask = invert_when_zeroing<Op>(axl::shl_wrap(axl::bit_ones<BLBitWord>, range_start));
  BLBitWord bw_mask_end = invert_when_zeroing<Op>(axl::shr_wrap(axl::bit_ones<BLBitWord>, axl::neg(range_end)));

  BL_NOUNROLL
  while (bw_index < bw_index_start) {
    sp->bits[bw_index++] = ~kPattern;
  }

  if (bw_index < bw_index_last) {
    sp->bits[bw_index++] = bw_mask;
    bw_mask = kPattern;

    BL_NOUNROLL
    while (bw_index < bw_index_last) {
      sp->bits[bw_index++] = bw_mask;
    }
  }

  bw_mask = (Op == UnicodeCoverageOp::kZero) ? bw_mask | bw_mask_end : bw_mask & bw_mask_end;
  sp->bits[bw_index++] = bw_mask;

  BL_NOUNROLL
  while (bw_index < kBWCount) {
    sp->bits[bw_index++] = ~kPattern;
  }

  if constexpr (Op == UnicodeCoverageOp::kZero)
    return kSPBitCount - (range_end - range_start);
  else
    return range_end - range_start;
}

template<UnicodeCoverageOp Op>
static BL_INLINE uint32_t sp_fill_closed_range(SubPlane* sp, uint32_t range_start, uint32_t range_end) noexcept {
  constexpr BLBitWord kPattern = fill_pattern<Op>;

  size_t bw_index = bw_index_from_bit_index(range_start);
  size_t bw_index_last = bw_index_from_bit_index(range_end - 1u);
  uint32_t cardinality_diff = 0u;

  BLBitWord bw_mask = invert_when_zeroing<Op>(axl::shl_wrap(axl::bit_ones<BLBitWord>, range_start));
  BLBitWord bw_mask_end = invert_when_zeroing<Op>(axl::shr_wrap(axl::bit_ones<BLBitWord>, axl::neg(range_end)));

  if (bw_index < bw_index_last) {
    BLBitWord old_bits = sp->bits[bw_index];
    BLBitWord new_bits = masked_op<Op>(old_bits, bw_mask);

    bw_mask = kPattern;

    BL_NOUNROLL
    for (;;) {
      sp->bits[bw_index++] = new_bits;
      cardinality_diff += axl::popcnt(old_bits ^ new_bits);

      if (bw_index >= bw_index_last) {
        break;
      }

      old_bits = sp->bits[bw_index];
      new_bits = masked_op<Op>(old_bits, bw_mask);
    }
  }

  {
    bw_mask = (Op == UnicodeCoverageOp::kZero) ? bw_mask | bw_mask_end : bw_mask & bw_mask_end;

    BLBitWord old_bits = sp->bits[bw_index];
    BLBitWord new_bits = masked_op<Op>(old_bits, bw_mask);

    sp->bits[bw_index] = new_bits;
    cardinality_diff += axl::popcnt(old_bits ^ new_bits);
  }

  return cardinality_diff;
}

static BL_NOINLINE BLResult copy_content(BLCoverageSetCore* dst, const BLCoverageSetCore* src) noexcept {
  BLCoverageSetImpl* dst_impl = ObjectLayer::get_impl(dst);
  BLCoverageSetImpl* src_impl = ObjectLayer::get_impl(src);

  size_t mp_count = get_mp_count(src);
  size_t page_index = get_page_count(dst);

  uint16_t* dst_offsets = get_mp_offsets(dst);
  const uint16_t* src_offsets = get_mp_offsets(src);

  for (size_t mp_index = 0; mp_index < mp_count; mp_index++) {
    PageIndex src_mp_offset = static_cast<PageIndex>(src_offsets[mp_index]);
    PageIndex dst_mp_offset = src_mp_offset;

    if (!is_page_special(src_mp_offset)) {
      BL_ASSERT(page_index < dst->_d.layout<Layout>().page_capacity);
      dst_mp_offset = static_cast<PageIndex>(page_index++);

      MasterPlane* dst_mp = get_mp_dense(dst_impl, dst_mp_offset);
      const MasterPlane* src_mp = get_mp_dense(src_impl, src_mp_offset);

      copy_page(dst_mp, src_mp);

      axl::BitWordIterator<uint32_t> it(dst_mp->sp_dense_mask);
      while (it.has_next()) {
        size_t sp_index = it.next();
        SubPlaneInfo sp_info = mp_load_sp_info(src_mp, sp_index);

        BL_ASSERT(page_index < dst->_d.layout<Layout>().page_capacity);
        PageIndex dst_sp_offset = static_cast<PageIndex>(page_index++);

        copy_page(get_sp_dense(dst_impl, dst_sp_offset), get_sp_dense(src_impl, sp_info));

        sp_info.set_page_index(dst_sp_offset);
        mp_store_sp_offset(dst_mp, sp_index, sp_info);
      }
    }

    dst_offsets[mp_index] = uint16_t(dst_mp_offset);
  }

  set_mp_count(dst, mp_count);
  set_page_count(dst, page_index);

  return BL_SUCCESS;
}

// CoverageSet - Private - Purge
// =============================

static BL_INLINE BLResult purge(BLCoverageSetCore* self) noexcept {
  Layout& layout = self->_d.layout<Layout>();

  axl::fill_items(get_mp_offsets(self), layout.mp_count, kPageZero);
  layout.mp_count = uint16_t(0);
  layout.page_count = uint16_t(0);
  layout.page_free_list = kUnassignedPage;

  return BL_SUCCESS;
}

// CoverageSet - Private - Alloc
// =============================

static BL_NOINLINE BLResult alloc(BLCoverageSetCore* self, size_t page_capacity) noexcept {
  BL_ASSERT(page_capacity <= kMaxPageCapacity);

  ObjectImplSize impl_size = impl_size_from_sp_capacity(page_capacity);
  BL_PROPAGATE(ObjectLayer::alloc_impl_aligned_t<BLCoverageSetImpl>(self, BL_OBJECT_TYPE_COVERAGE_SET, impl_size, 64u));

  Layout& layout = self->_d.layout<Layout>();
  layout.mp_count = uint16_t(0);
  layout.page_count = uint16_t(0);
  layout.page_capacity = uint16_t(page_capacity);
  layout.page_free_list = kUnassignedPage;
  memset(layout.reserved, 0, sizeof(layout.reserved));

  BLCoverageSetImpl* impl = layout.impl.ptr;
  axl::fill_items(impl->mp_offsets, kMPCount, kPageZero);
  memset(impl->reserved, 0, sizeof(impl->reserved));

  return BL_SUCCESS;
}

static BL_NOINLINE BLResult alloc_copy(BLCoverageSetCore* self, const BLCoverageSetCore* src, size_t page_capacity) noexcept {
  BL_ASSERT(get_page_count(src) <= page_capacity);

  BL_PROPAGATE(alloc(self, page_capacity));
  return copy_content(self, src);
}

// CoverageSet - Private - Memory Management
// =========================================

// Next page offset either uses a brand new page or reuses an existing page that is in a free-list of pages.
static BL_INLINE PageIndex next_page_index(BLCoverageSetCore* self, BLCoverageSetImpl* impl) noexcept {
  Layout& layout = self->_d.layout<Layout>();
  size_t page_index = layout.page_count;

  // Cannot be called when there are no pages available.
  BL_ASSERT(page_index < layout.page_capacity);
  layout.page_count = uint16_t(page_index + 1u);

  if (layout.page_free_list != kUnassignedPage) {
    page_index = layout.page_free_list;
    layout.page_free_list = get_page<FreePlane>(impl, static_cast<PageIndex>(page_index))->next_page;
  }

  return static_cast<PageIndex>(page_index);
}

static BL_INLINE void free_page(BLCoverageSetCore* self, PageIndex page_index) noexcept {
  Layout& layout = self->_d.layout<Layout>();

  // Make sure we are not freeing built-in or out-of-range offset.
  BL_ASSERT(static_cast<size_t>(page_index) < layout.page_capacity);

  FreePlane* free_page = get_page<FreePlane>(layout.impl.ptr, page_index);
  free_page->next_page = layout.page_free_list;

  layout.page_free_list = static_cast<uint16_t>(page_index);
  layout.page_count--;
}

static BL_INLINE void free_page_if_not_special(BLCoverageSetCore* self, PageIndex page_index) noexcept {
  if (!is_page_special(page_index)) {
    free_page(self, page_index);
  }
}

static BL_NOINLINE BLResultOr<MasterPlane*> sp_ensure_page(BLCoverageSetCore* self, size_t mp_index, size_t sp_index) noexcept {
  // Let's not repeat a basic-error handling callers already did to get here...
  BL_ASSERT(mp_index < kMPCount);
  BL_ASSERT(sp_index <= kSPMask);

  BLCoverageSetCore tmp;

  // Ensure Master-Plane & Sub-Plane Exist
  // -------------------------------------

  BLCoverageSetImpl* impl = ObjectLayer::get_impl(self);
  uint16_t* mp_offsets = get_mp_offsets(self);

  PageIndex mp_offset = static_cast<PageIndex>(mp_offsets[mp_index]);
  size_t mp_count = axl::max(get_mp_count(self), mp_index + 1u);
  size_t remaining_capacity = get_remaining_page_capacity(self);

  if (BL_UNLIKELY(is_page_special(mp_offset))) {
    // Need 2 more pages - one for master-plane and another for sub-plane.
    if (BL_UNLIKELY(!ObjectLayer::check_dynamic_capacity_and_mutability_raw(self, 2u, remaining_capacity))) {
      BL_PROPAGATE(alloc_copy(&tmp, self, expand_capacity(get_page_count(self))));
      ObjectLayer::replace_object(self, &tmp);

      impl = ObjectLayer::get_impl(self);
      mp_offsets = get_mp_offsets(self);
    }

    const MasterPlane* mp_ref = &builtin_mp_table[static_cast<size_t>(mp_offset) - kPageSpecial];
    BLBitWord sp_bits = is_page_zero(mp_offset) ? BLBitWord(0) : axl::bit_ones<BLBitWord>;

    mp_offset = next_page_index(self, impl);
    PageIndex sp_offset = next_page_index(self, impl);

    MasterPlane* mp = get_mp_dense(impl, mp_offset);
    SubPlane* sp = get_sp_dense(impl, sp_offset);

    mp_offsets[mp_index] = uint16_t(mp_offset);
    copy_page(mp, mp_ref);

    mp_store_sp_offset(mp, sp_index, SubPlaneInfo::from_page_index_and_cardinality(sp_offset, sp_bits == 0 ? 0u : kSPBitCount));
    mp->sp_dense_mask |= axl::bit_mask<uint32_t>(sp_index);
    mp->sp_filled_mask &= ~axl::bit_mask<uint32_t>(sp_index);
    axl::fill_items(sp->bits, kBWCount, sp_bits);

    set_mp_count(self, mp_count);
    return {BL_SUCCESS, mp};
  }

  MasterPlane* mp = get_mp_dense(impl, mp_offset);
  SubPlaneInfo sp_info = mp_load_sp_info(mp, sp_index);

  // Ensure Sub-Plane Exists
  // -----------------------

  if (BL_UNLIKELY(sp_info.is_special())) {
    // To add a new sub-plane, the container has to be mutable and have at least a single page that can be used.
    if (BL_UNLIKELY(!ObjectLayer::check_dynamic_capacity_and_mutability_raw(self, 1u, remaining_capacity))) {
      BL_PROPAGATE(alloc_copy(&tmp, self, expand_capacity(get_page_count(self))));
      ObjectLayer::replace_object(self, &tmp);

      // Indexes and pointers to the old Impl were invalidated - `alloc_copy()` reorders pages.
      impl = ObjectLayer::get_impl(self);
      mp_offsets = get_mp_offsets(self);

      mp_offset = static_cast<PageIndex>(mp_offsets[mp_index]);
      BL_ASSERT(!is_page_special(mp_offset));

      mp = get_mp_dense(impl, mp_offset);
    }

    mp->sp_dense_mask |= axl::bit_mask<uint32_t>(sp_index);
    mp->sp_filled_mask &= ~axl::bit_mask<uint32_t>(sp_index);

    PageIndex sp_offset = next_page_index(self, impl);
    BLBitWord sp_bits = sp_info.is_zero_page() ? BLBitWord(0) : axl::bit_ones<BLBitWord>;

    sp_init_page(get_sp_dense(impl, sp_offset), sp_bits);
    sp_info.set_page_index(sp_offset);
    mp_store_sp_offset(mp, sp_index, sp_info);
  }
  else {
    if (BL_UNLIKELY(!ObjectLayer::is_impl_mutable(impl))) {
      BL_PROPAGATE(alloc_copy(&tmp, self, expand_capacity(get_page_count(self))));
      ObjectLayer::replace_object(self, &tmp);

      // Indexes and pointers to the old Impl were invalidated - `alloc_copy()` reorders pages.
      impl = ObjectLayer::get_impl(self);
      mp_offsets = get_mp_offsets(self);

      mp_offset = static_cast<PageIndex>(mp_offsets[mp_index]);
      BL_ASSERT(!is_page_special(mp_offset));

      mp = get_mp_dense(impl, mp_offset);
      BL_ASSERT(mp_load_sp_info(mp, sp_index).is_dense());
    }
  }

  // Update mp_count here, in case it changed.
  set_mp_count(self, mp_count);
  return {BL_SUCCESS, mp};
}

// CoverageSet - Private - Master-Plane & Sub-Plane Utilities
// ==========================================================

static BL_INLINE bool sp_subsumes(const SubPlane& a, const SubPlane& b) noexcept {
  BLBitWord mask = 0u;
  for (uint32_t i = 0; i < kBWCount; i++) {
    // A & B -> Contains B when A fully subsumes B - XOR clears the B part.
    mask |= (a.bits[i] & b.bits[i]) ^ b.bits[i];
  }
  return mask == 0u;
}

static BL_INLINE bool sp_intersects(const SubPlane& a, const SubPlane& b) noexcept {
  BLBitWord mask = 0u;
  for (uint32_t i = 0; i < kBWCount; i++) {
    mask |= a.bits[i] & b.bits[i];
  }
  return mask != 0u;
}

// CoverageSet - Private - Range Analysis
// ======================================

struct UnicodeCoverageRangeAnalysis {
  uint32_t cp_min = 0xFFFFFFFFu;
  uint32_t cp_max = 0u;
  uint32_t not_sorted = 0u;

  uint32_t sp_full_mask[kMPCount] {};
  uint32_t sp_dense_mask[kMPCount] {};
};

//! Performs analysis of code-point ranges:
//!   - Ranges that don't start or end exactly at a sub-plane boundary mark sp_dense_mask bits.
//!   - Ranges that span across multiple sub-pages mark sp_full_mask - these regions don't need dense sub-planes.
static BL_NOINLINE bool analyze_ranges(UnicodeCoverageRangeAnalysis& analysis, const BLCoverageSetRange* range_data, size_t range_count, uint32_t range_end_limit) noexcept {
  using BitOps = ParametrizedBitOps<BitOrder::kLSB, uint32_t>;

  uint32_t cp_min = analysis.cp_min;
  uint32_t cp_max = analysis.cp_max;
  uint32_t cp_prev_end = 0;
  uint32_t not_sorted = analysis.not_sorted;

  for (size_t i = 0; i < range_count; i++) {
    uint32_t range_start = range_data[i].start;
    uint32_t range_end = axl::min(range_data[i].end, range_end_limit);

    if (range_start >= range_end) {
      continue;
    }

    cp_min = axl::min(cp_min, range_start);
    cp_max = axl::max(cp_max, range_end);
    not_sorted |= uint32_t(range_start < cp_prev_end);
    cp_prev_end = range_end;

    size_t sp_index_a = (range_start   ) / kSPBitCount;
    size_t sp_index_b = (range_end - 1u) / kSPBitCount;

    BitOps::bit_array_or_bit(analysis.sp_dense_mask, sp_index_a, (range_start % kSPBitCount) != 0u);
    BitOps::bit_array_or_bit(analysis.sp_dense_mask, sp_index_b, (range_end   % kSPBitCount) != 0u);

    sp_index_a = (range_start + kSPBitCount - 1u) / kSPBitCount;
    sp_index_b = (range_end / kSPBitCount);

    if (sp_index_a < sp_index_b) {
      BitOps::bit_array_fill(analysis.sp_full_mask, sp_index_a, sp_index_b - sp_index_a);
    }
  }

  analysis.cp_min = cp_min;
  analysis.cp_max = cp_max;
  analysis.not_sorted = not_sorted;

  return cp_max > 0u;
}

static BL_NOINLINE BLResult reserve_pages_marked_by_analysis(BLCoverageSetCore* self, UnicodeCoverageRangeAnalysis& analysis, PageIndex special_offset_nop) noexcept {
  using BitOps = ParametrizedBitOps<BitOrder::kLSB, uint32_t>;

  // Checked before - makes no sense to reserve pages from invalid or empty ranges as it's expected to end
  // up with a mutable container that has enough reserved pages if the function succeeds.
  BL_ASSERT(analysis.cp_max > 0u);
  BL_ASSERT(analysis.cp_max <= kMaxSize);
  BL_ASSERT(is_page_special(special_offset_nop));

  BLCoverageSetImpl* impl = ObjectLayer::get_impl(self);
  PageIndex special_offset_upgrade = opposite_special_offset(special_offset_nop);

  // Find How Many Pages to Alloc
  // ----------------------------

  size_t mp_index_start = analysis.cp_min / kMPBitCount;
  size_t mp_index_end = ((analysis.cp_max - 1u) / kMPBitCount) + 1u;

  size_t mp_count = get_mp_count(self);
  // Removal must be analyzed with the existing coverage's end as its limit.
  BL_ASSERT(is_page_full(special_offset_nop) || mp_index_end <= mp_count);

  uint32_t pages_required = 0u;
  uint16_t* mp_offsets = get_mp_offsets(self);

  for (size_t mp_index = mp_index_start; mp_index < mp_index_end; mp_index++) {
    uint32_t sp_full_bits = analysis.sp_full_mask[mp_index];
    uint32_t sp_alloc_bits = analysis.sp_dense_mask[mp_index] & ~sp_full_bits;
    PageIndex mp_offset = static_cast<PageIndex>(mp_index < mp_count ? mp_offsets[mp_index] : kPageZero);

    // Untouched, already uniform in the desired state, or replaced in full -> no pages to materialize.
    if ((sp_full_bits | sp_alloc_bits) == 0u || mp_offset == special_offset_nop || sp_full_bits == 0xFFFFFFFFu) {
      continue;
    }

    if (mp_offset == special_offset_upgrade) {
      pages_required++;
    }
    else {
      const MasterPlane* mp = get_mp_dense(impl, mp_offset);
      uint32_t sp_upgrade_mask = is_page_full(special_offset_upgrade) ? mp->sp_filled_mask : ~(mp->sp_dense_mask | mp->sp_filled_mask);
      sp_alloc_bits &= sp_upgrade_mask;
    }

    pages_required += axl::popcnt(sp_alloc_bits);
  }

  // Ensure the Storage of Required Pages
  // ------------------------------------

  if (BL_UNLIKELY(!ObjectLayer::check_dynamic_capacity_and_mutability_raw(self, pages_required, get_remaining_page_capacity(self)))) {
    BLCoverageSetCore tmp;
    BL_PROPAGATE(alloc_copy(&tmp, self, expand_capacity(get_page_count(self) + pages_required)));

    ObjectLayer::replace_object(self, &tmp);
    impl = ObjectLayer::get_impl(self);
    mp_offsets = get_mp_offsets(self);
  }

  // Allocate and Link Required Pages
  // --------------------------------

  set_mp_count(self, axl::max(mp_count, mp_index_end));
  BLBitWord sp_pattern = is_page_zero(special_offset_upgrade) ? BLBitWord(0) : axl::bit_ones<BLBitWord>;

  for (size_t mp_index = mp_index_start; mp_index < mp_index_end; mp_index++) {
    uint32_t sp_full_bits = analysis.sp_full_mask[mp_index];
    uint32_t sp_alloc_bits = analysis.sp_dense_mask[mp_index] & ~sp_full_bits;
    PageIndex mp_offset = static_cast<PageIndex>(mp_offsets[mp_index]);

    if ((sp_full_bits | sp_alloc_bits) == 0u || mp_offset == special_offset_nop || sp_full_bits == 0xFFFFFFFFu) {
      continue;
    }

    MasterPlane* mp;
    if (mp_offset == special_offset_upgrade) {
      mp_offset = next_page_index(self, impl);
      mp_offsets[mp_index] = uint16_t(mp_offset);

      mp = get_mp_dense(impl, mp_offset);
      copy_page(mp, &builtin_mp_table[static_cast<size_t>(special_offset_upgrade) - kPageSpecial]);
    }
    else {
      mp = get_mp_dense(impl, mp_offset);
    }

    // TODO: Consider also what's in master-plane (dense + filled mask).
    if (sp_alloc_bits) {
      uint32_t sp_dense_mask = mp->sp_dense_mask;
      uint32_t sp_filled_mask = mp->sp_filled_mask;

      BitOps::BitIterator it(sp_alloc_bits);
      do {
        size_t sp_index = it.next();
        SubPlaneInfo sp_info = mp_load_sp_info(mp, sp_index);
        if (sp_info.page_index() == special_offset_upgrade) {
          sp_info.set_page_index(next_page_index(self, impl));
          sp_init_page(get_sp_dense(impl, sp_info), sp_pattern);

          mp_store_sp_offset(mp, sp_index, sp_info);
          sp_dense_mask |= axl::bit_mask<uint32_t>(sp_index);
        }
      } while (it.has_next());

      sp_filled_mask &= ~sp_dense_mask;

      mp->sp_dense_mask = sp_dense_mask;
      mp->sp_filled_mask = sp_filled_mask;
    }
  }

  return BL_SUCCESS;
}

// CoverageSet - Private - Range Operations
// ========================================

template<UnicodeCoverageOp Op>
static uint32_t BL_INLINE cardinality_op(uint32_t cardinality, uint32_t diff) noexcept {
  if constexpr (Op == UnicodeCoverageOp::kZero) {
    return cardinality - diff;
  }
  else {
    return cardinality + diff;
  }
}

template<UnicodeCoverageOp Op>
static BL_INLINE BLResult range_op(BLCoverageSetCore* self, uint32_t range_start, uint32_t range_end) noexcept {
  constexpr bool kIsFill = Op == UnicodeCoverageOp::kFill;

  constexpr uint16_t kOffsetAll = kIsFill ? kPageFull : kPageZero;
  constexpr uint16_t kCardinalityAll = kIsFill ? kSPBitCount : 0u;

  BLCoverageSetImpl* impl = ObjectLayer::get_impl(self);
  size_t mp_count = get_mp_count(self);
  uint32_t range_end_limit = kIsFill ? kMaxSize : static_cast<uint32_t>(mp_count) * kMPBitCount;

  range_end = axl::min(range_end, range_end_limit);
  if (range_start >= range_end) {
    return BL_SUCCESS;
  }

  BLCoverageSetRange range{range_start, range_end};

  bool is_mutable = ObjectLayer::is_impl_mutable(impl);
  size_t mp_index = mp_index_from_bit_index(range_start);
  size_t mp_index_last = mp_index_from_bit_index(range_end - 1u);

  // We can access mp_index as it's guaranteed `mp_index < kMPCount`.
  PageIndex mp_offset = static_cast<PageIndex>(impl->mp_offsets[mp_index]);

  // PERFORMANCE: Optimize operations that happen within a single master-plane and delegate more complex
  // operations to add_ranges() or remove_ranges(). This makes this function fast for the most common cases.
  do {
    if (axl::bool_and(is_mutable, mp_index == mp_index_last)) {
      if (is_page_special(mp_offset)) {
        if (mp_offset == static_cast<PageIndex>(kOffsetAll)) {
          return BL_SUCCESS;
        }
        break;
      }

      // Drop master-plane contribution from input ranges to make some other calculations simpler.
      // We know we are dealing with a single master-plane, thus we don't need the scale of it.
      range_start &= kMPBitMask;
      range_end = ((range_end - 1u) & kMPBitMask) + 1u;

      MasterPlane* mp = get_mp_dense(impl, mp_offset);
      uint32_t mp_cardinality = mp->mp_cardinality;

      size_t sp_index_first = range_start >> kSPShift;
      size_t sp_index_last = (range_end - 1u) >> kSPShift;

      uint32_t sp_dense_mask = mp->sp_dense_mask;
      uint32_t sp_endpoint_mask = uint32_t((range_start & kSPBitMask) != 0u) << sp_index_first;

      // FASTEST PATH: If the range is within a single sub-plane, which is already dense, fill it in place.
      if (axl::bool_and(sp_index_first == sp_index_last, axl::test(sp_dense_mask, sp_endpoint_mask))) {
        SubPlaneInfo sp_info = mp_load_sp_info(mp, sp_index_first);
        BL_ASSERT(sp_info.is_dense());

        SubPlane* sp = get_sp_dense(impl, sp_info.page_index());
        uint32_t diff = sp_fill_closed_range<Op>(sp, range_start, range_end);

        mp_cardinality = cardinality_op<Op>(mp_cardinality, diff);
        sp_info.cardinality_op<Op>(diff);

        mp->mp_cardinality = mp_cardinality;
        mp_store_sp_info(mp, sp_index_first, sp_info);
        return BL_SUCCESS;
      }

      // Contains sub-planes, which are filled entirely (special sub-planes only).
      uint32_t sp_filled_mask = mp->sp_filled_mask;
      // Contains sub-planes we won't touch, because they are already in the desired state (special sub-planes).
      uint32_t sp_uniform_mask = kIsFill ? sp_filled_mask : (~sp_dense_mask & ~sp_filled_mask);

      // Contains end-points, which are not aligned to sub-plane boundaries - if not already uniform, bits needs to change in these.
      sp_endpoint_mask = (sp_endpoint_mask | uint32_t((range_end & kSPBitMask) != 0u) << sp_index_last) & ~sp_uniform_mask;

      // Contains sub-planes that need allocation - these are end-points that don't have dense representation yet, or aren't uniform.
      uint32_t sp_alloc_mask = sp_endpoint_mask & ~sp_dense_mask;
      if (sp_alloc_mask) {
        size_t page_capacity = get_remaining_page_capacity(self);
        size_t pages_required = count_1_or_2_bits(sp_alloc_mask);

        if (pages_required > page_capacity) {
          break;
        }

        sp_dense_mask |= sp_alloc_mask;
        mp->sp_dense_mask = sp_dense_mask;

        sp_filled_mask &= ~sp_alloc_mask;
        sp_endpoint_mask &= ~sp_alloc_mask;
        sp_uniform_mask |= sp_alloc_mask;

        axl::BitWordIterator<uint32_t> it(sp_alloc_mask);
        do {
          size_t sp_index = it.next();
          PageIndex sp_offset = next_page_index(self, impl);
          SubPlane* sp = get_sp_dense(impl, sp_offset);

          uint32_t sp_range_start = axl::max(range_start, static_cast<uint32_t>(sp_index) * kSPBitCount);
          uint32_t sp_range_end = axl::min(range_end, static_cast<uint32_t>(sp_index) * kSPBitCount + kSPBitCount);
          uint32_t sp_cardinality = sp_init_full_range<Op>(sp, sp_range_start, sp_range_end);

          if (Op == UnicodeCoverageOp::kZero) {
            mp_cardinality = mp_cardinality - kSPBitCount + sp_cardinality;
          }
          else {
            mp_cardinality += sp_cardinality;
          }

          mp_store_sp_info(mp, sp_index, SubPlaneInfo::from_page_index_and_cardinality(sp_offset, sp_cardinality));
        } while (it.has_next());
      }

      // Contains sub-planes this range spans across - initially with uniform sub-planes already excluded.
      uint32_t sp_range_mask = bit_mask_from_to<uint32_t>(sp_index_first, sp_index_last + 1u) & ~(sp_alloc_mask | sp_uniform_mask);

      // Process end-points (first, last, or both).
      if (sp_endpoint_mask) {
        sp_range_mask &= ~sp_endpoint_mask;

        axl::BitWordIterator<uint32_t> it(sp_endpoint_mask);
        do {
          size_t sp_index = it.next();

          SubPlaneInfo sp_info = mp_load_sp_info(mp, sp_index);
          BL_ASSERT(sp_info.is_dense());

          SubPlane* sp = get_sp_dense(impl, sp_info.page_index());
          uint32_t sp_range_start = axl::max(range_start, static_cast<uint32_t>(sp_index) * kSPBitCount);
          uint32_t sp_range_end = axl::min(range_end, static_cast<uint32_t>(sp_index) * kSPBitCount + kSPBitCount);

          uint32_t diff = sp_fill_closed_range<Op>(sp, sp_range_start, sp_range_end);
          mp_cardinality = cardinality_op<Op>(mp_cardinality, diff);
          sp_info.cardinality_op<Op>(diff);

          mp_store_sp_info(mp, sp_index, sp_info);
        } while (it.has_next());
      }

      // Process sub-planes that must be freed - freed sub-planes are turned into either special zero or filled planes.
      uint32_t sp_free_mask = sp_range_mask & sp_dense_mask;
      if (sp_free_mask) {
        sp_range_mask &= ~sp_free_mask;
        mp->sp_dense_mask &= ~sp_free_mask;

        if (Op == UnicodeCoverageOp::kFill) {
          sp_filled_mask |= sp_free_mask;
        }

        axl::BitWordIterator<uint32_t> it(sp_free_mask);
        do {
          size_t sp_index = it.next();

          SubPlaneInfo sp_info = mp_load_sp_info(mp, sp_index);
          BL_ASSERT(sp_info.is_dense());

          free_page(self, sp_info.page_index());
          mp_cardinality -= sp_info.cardinality();

          mp_store_sp_info(mp, sp_index, SubPlaneInfo::from_page_index_and_cardinality(kOffsetAll, kCardinalityAll));
          mp_cardinality += kCardinalityAll;
        } while (it.has_next());
      }

      // Process remaining sub-planes, which need to be flipped. They are already special, but not in the desired state.
      if (sp_range_mask) {
        sp_filled_mask ^= sp_range_mask;

        axl::BitWordIterator<uint32_t> it(sp_range_mask);
        do {
          size_t sp_index = it.next();
          BL_ASSERT(mp_load_sp_info(mp, sp_index).is_special<static_cast<PageIndex>(kIsFill ? kPageZero : kPageFull)>());

          mp_cardinality = cardinality_op<Op>(mp_cardinality, kSPBitCount);
          mp_store_sp_offset(mp, sp_index, SubPlaneInfo::from_page_index_and_cardinality(kOffsetAll, kCardinalityAll));
        } while (it.has_next());
      }

      mp->sp_filled_mask = sp_filled_mask;
      mp->mp_cardinality = mp_cardinality;

      return BL_SUCCESS;
    }
  } while (false);

  // Delegate allocation, copy-on-write, and ranges spanning multiple master-planes to the batch implementation.
  if constexpr (kIsFill) {
    return bl_coverage_set_add_ranges(self, &range, 1u);
  }
  else {
    return bl_coverage_set_remove_ranges(self, &range, 1u);
  }
}

template<UnicodeCoverageOp Op>
static BL_INLINE BLResult ranges_op(BLCoverageSetCore* self, const BLCoverageSetRange* range_data, size_t range_count) noexcept {
  constexpr bool kIsFill = Op == UnicodeCoverageOp::kFill;
  constexpr PageIndex kOffsetAll = static_cast<PageIndex>(kIsFill ? kPageFull : kPageZero);
  // constexpr uint16_t kOffsetNone = kIsFill ? kPageZero : kPageFull;
  constexpr uint16_t kCardinalityAll = kIsFill ? kSPBitCount : 0u;

  UnicodeCoverageRangeAnalysis analysis;
  uint32_t range_end_limit = kIsFill ? kMaxSize : static_cast<uint32_t>(get_mp_count(self)) * kMPBitCount;

  if (!analyze_ranges(analysis, range_data, range_count, range_end_limit)) {
    return BL_SUCCESS;
  }

  BL_PROPAGATE(reserve_pages_marked_by_analysis(self, analysis, static_cast<PageIndex>(kOffsetAll)));

  BLCoverageSetImpl* impl = ObjectLayer::get_impl(self);
  uint16_t* mp_offsets = get_mp_offsets(self);

  // Replace Whole Master-Planes and Sub-Planes
  // ------------------------------------------

  size_t mp_index_start = mp_index_from_bit_index(analysis.cp_min);
  size_t mp_index_end = mp_index_from_bit_index(analysis.cp_max - 1u) + 1u;

  for (size_t mp_index = mp_index_start; mp_index < mp_index_end; mp_index++) {
    uint32_t sp_full_bits = analysis.sp_full_mask[mp_index];
    PageIndex mp_offset = static_cast<PageIndex>(mp_offsets[mp_index]);

    if (axl::bool_or(sp_full_bits == 0u, mp_offset == kOffsetAll)) {
      continue;
    }

    // A uniform master-plane can remain implicit when the entire plane is replaced.
    if (is_page_special(mp_offset)) {
      BL_ASSERT(sp_full_bits == 0xFFFFFFFFu);
      mp_offsets[mp_index] = static_cast<uint16_t>(kOffsetAll);
      continue;
    }

    MasterPlane* mp = get_mp_dense(impl, mp_offset);
    uint32_t sp_dense_mask = mp->sp_dense_mask;
    uint32_t sp_filled_mask = mp->sp_filled_mask;
    uint32_t mp_cardinality = mp->mp_cardinality;

    axl::BitWordIterator<uint32_t> it(~sp_dense_mask & invert_when_zeroing<Op>(~sp_filled_mask) & sp_full_bits);
    sp_filled_mask ^= it.bit_word();

    while (it.has_next()) {
      size_t sp_index = it.next();

      mp_cardinality = cardinality_op<Op>(mp_cardinality, kSPBitCount);
      mp_store_sp_info(mp, sp_index, SubPlaneInfo::from_page_index_and_cardinality(kOffsetAll, kCardinalityAll));
    }

    it.init(sp_dense_mask & sp_full_bits);
    sp_dense_mask &= ~it.bit_word();

    if (Op == UnicodeCoverageOp::kFill) {
      sp_filled_mask |= it.bit_word();
    }

    while (it.has_next()) {
      size_t sp_index = it.next();
      SubPlaneInfo sp_info = mp_load_sp_info(mp, sp_index);

      mp_cardinality -= sp_info.cardinality();
      free_page(self, sp_info.page_index());

      mp_cardinality += kCardinalityAll;
      mp_store_sp_info(mp, sp_index, SubPlaneInfo::from_page_index_and_cardinality(kOffsetAll, kCardinalityAll));
    }

    // TODO: Is this the right check? Check dense_mask and filled_mask instead? Or maybe the best, cardinality?
    if (sp_full_bits == 0xFFFFFFFFu) {
      free_page(self, mp_offset);
      mp_offsets[mp_index] = static_cast<uint16_t>(kOffsetAll);
    }
    else {
      mp->sp_dense_mask = sp_dense_mask;
      mp->sp_filled_mask = sp_filled_mask;
      mp->mp_cardinality = mp_cardinality;
    }
  }

  // Modify Partial Sub-Planes
  // -------------------------

  // Whole sub-planes were handled above, so each range has at most two remaining fragments.
  auto modify_partial_range = [&](uint32_t range_start, uint32_t range_end) noexcept {
    size_t mp_index = mp_index_from_bit_index(range_start);
    size_t sp_index = sp_index_from_bit_index(range_start);
    PageIndex mp_offset = static_cast<PageIndex>(mp_offsets[mp_index]);

    if (mp_offset == kOffsetAll) {
      return;
    }

    MasterPlane* mp = get_mp_dense(impl, mp_offset);
    SubPlaneInfo sp_info = mp_load_sp_info(mp, sp_index);

    if (sp_info.is_special<kOffsetAll>()) {
      return;
    }

    SubPlane* sp = get_sp_dense(impl, sp_info);
    size_t bw_index = bw_index_from_bit_index(range_start);
    size_t bw_index_last = bw_index_from_bit_index(range_end - 1u);
    BLBitWord bw_mask = axl::shl_wrap(axl::bit_ones<BLBitWord>, range_start);
    BLBitWord bw_mask_end = axl::shr_wrap(axl::bit_ones<BLBitWord>, axl::neg(range_end));
    uint32_t cardinality_diff = 0u;

    for (; bw_index <= bw_index_last; bw_index++) {
      if (bw_index == bw_index_last) {
        bw_mask &= bw_mask_end;
      }

      BLBitWord old_bits = sp->bits[bw_index];
      BLBitWord new_bits = kIsFill ? old_bits | bw_mask : old_bits & ~bw_mask;
      sp->bits[bw_index] = new_bits;
      cardinality_diff += axl::popcnt(new_bits ^ old_bits);
      bw_mask = axl::bit_ones<BLBitWord>;
    }

    mp->mp_cardinality = cardinality_op<Op>(mp->mp_cardinality, cardinality_diff);
    sp_info.cardinality_op<Op>(cardinality_diff);

    mp_store_sp_info(mp, sp_index, sp_info);
  };

  for (size_t i = 0; i < range_count; i++) {
    uint32_t range_start = range_data[i].start;
    uint32_t range_end = axl::min(range_data[i].end, range_end_limit);

    if (range_start >= range_end) {
      continue;
    }

    if ((range_start & kSPBitMask) != 0u) {
      uint32_t fragment_end = axl::min(range_end, (range_start | kSPBitMask) + 1u);
      modify_partial_range(range_start, fragment_end);
      range_start = fragment_end;
    }

    if (range_start < range_end && (range_end & kSPBitMask) != 0u) {
      modify_partial_range(range_end & ~kSPBitMask, range_end);
    }
  }

  return BL_SUCCESS;
}

// CoverageSet - Private - Boolean Operations
// ==========================================

struct CombinePlane {
  uint32_t sp_dense_mask;
  uint32_t sp_filled_mask;

  BL_INLINE bool is_uniform() const noexcept {
    return sp_dense_mask == 0u && (sp_filled_mask == 0u || sp_filled_mask == 0xFFFFFFFFu);
  }

  BL_INLINE PageIndex special_offset() const noexcept {
    BL_ASSERT(is_uniform());
    return static_cast<PageIndex>(kPageZero + uint32_t(sp_filled_mask != 0u));
  }
};

static BL_INLINE const MasterPlane* get_combine_mp(const BLCoverageSetImpl* impl, PageIndex mp_offset) noexcept {
  const MasterPlane* mp = get_mp_or_builtin(impl, mp_offset);
  if (mp_is_cardinality_empty_or_full(mp->mp_cardinality)) {
    return &builtin_mp_table[mp->mp_cardinality >> kMPBitShift];
  }
  return mp;
}

template<typename Op>
static BL_INLINE CombinePlane analyze_combine_mp(const MasterPlane* a, const MasterPlane* b) noexcept {
  uint32_t a_dense = a->sp_dense_mask;
  uint32_t b_dense = b->sp_dense_mask;
  uint32_t a_filled = a->sp_filled_mask;
  uint32_t b_filled = b->sp_filled_mask;

  // Complementing an operand exchanges its zero and full sub-planes; dense sub-planes stay dense.
  if constexpr (std::is_same_v<Op, BitOperator::NotAnd>) {
    a_filled = ~(a_dense | a_filled);
  }
  if constexpr (std::is_same_v<Op, BitOperator::AndNot>) {
    b_filled = ~(b_dense | b_filled);
  }

  if constexpr (std::is_same_v<Op, BitOperator::Xor>) {
    uint32_t dense = a_dense | b_dense;
    return {dense, (a_filled ^ b_filled) & ~dense};
  }
  else if constexpr (std::is_same_v<Op, BitOperator::Or>) {
    uint32_t filled = a_filled | b_filled;
    return {(a_dense | b_dense) & ~filled, filled};
  }
  else {
    uint32_t filled = a_filled & b_filled;
    return {((a_dense | a_filled) & (b_dense | b_filled)) & ~filled, filled};
  }
}

// Release all pages known to become special before allocating any result pages. This lets an earlier
// master-plane reuse pages from a later one, so the analyzed result size also bounds peak page usage.
static void free_combine_pages(BLCoverageSetCore* dst, const CombinePlane* planes) noexcept {
  BLCoverageSetImpl* impl = ObjectLayer::get_impl(dst);
  uint16_t* offsets = get_mp_offsets(dst);
  size_t mp_count = get_mp_count(dst);

  for (size_t mp_index = 0; mp_index < mp_count; mp_index++) {
    PageIndex mp_offset = static_cast<PageIndex>(offsets[mp_index]);
    if (is_page_special(mp_offset)) {
      continue;
    }

    const CombinePlane& plane = planes[mp_index];
    MasterPlane* mp = get_mp_dense(impl, mp_offset);
    axl::BitWordIterator<uint32_t> it(mp->sp_dense_mask & ~plane.sp_dense_mask);

    while (it.has_next()) {
      free_page(dst, mp_load_sp_info(mp, it.next()).page_index());
    }

    if (plane.is_uniform()) {
      free_page(dst, mp_offset);
      offsets[mp_index] = uint16_t(plane.special_offset());
    }

    // Retain the original metadata until combine_mp() has consumed the aliased input. Released
    // sub-planes cannot contribute to dense results and their entries will be replaced without reads.
  }
}

template<typename Op>
static BL_INLINE PageIndex combine_mp(
  BLCoverageSetCore* dst, PageIndex dst_offset, CombinePlane plane,
  const BLCoverageSetImpl* a_impl, const MasterPlane* a_mp,
  const BLCoverageSetImpl* b_impl, const MasterPlane* b_mp
) noexcept {
  BL_ASSERT(!plane.is_uniform());
  BLCoverageSetImpl* dst_impl = ObjectLayer::get_impl(dst);

  uint32_t a_dense = a_mp->sp_dense_mask;
  uint32_t b_dense = b_mp->sp_dense_mask;
  // With one dense operand, the result at its zero bits tells whether to copy or complement it.
  uint32_t invert_mask = Op::op(a_mp->sp_filled_mask, b_mp->sp_filled_mask);

  if (is_page_special(dst_offset)) {
    dst_offset = next_page_index(dst, dst_impl);
    copy_page(get_mp_dense(dst_impl, dst_offset), &builtin_mp_table[0]);
  }

  MasterPlane* dst_mp = get_mp_dense(dst_impl, dst_offset);
  uint32_t retained_dense = dst_mp->sp_dense_mask & plane.sp_dense_mask;
  uint32_t mp_cardinality = axl::popcnt(plane.sp_filled_mask) * kSPBitCount;

  // Only special entries that change need stores. The masks determine their state without decoding
  // per-sub-plane metadata, including entries whose bitmap was released by free_combine_pages().
  axl::BitWordIterator<uint32_t> it((dst_mp->sp_dense_mask | (dst_mp->sp_filled_mask ^ plane.sp_filled_mask)) & ~plane.sp_dense_mask);
  while (it.has_next()) {
    size_t sp_index = it.next();
    uint32_t filled = (plane.sp_filled_mask >> sp_index) & 1u;
    mp_store_sp_info(dst_mp, sp_index, SubPlaneInfo::from_page_index_and_cardinality(kPageZero + filled, filled * kSPBitCount));
  }

  it.init(plane.sp_dense_mask);
  while (it.has_next()) {
    size_t sp_index = it.next();
    uint32_t sp_bit = axl::bit_mask<uint32_t>(sp_index);
    PageIndex sp_offset = static_cast<PageIndex>(kPageZero);
    if (retained_dense & sp_bit) {
      sp_offset = mp_load_sp_info(dst_mp, sp_index).page_index();
    }

    uint32_t cardinality;
    if (a_dense & b_dense & sp_bit) {
      const SubPlane* a_sp = get_sp_dense(a_impl, mp_load_sp_info(a_mp, sp_index));
      const SubPlane* b_sp = get_sp_dense(b_impl, mp_load_sp_info(b_mp, sp_index));
      if (is_page_special(sp_offset)) {
        sp_offset = next_page_index(dst, dst_impl);
      }

      SubPlane* dst_sp = get_sp_dense(dst_impl, sp_offset);
      for (size_t i = 0; i < kBWCount; i++) {
        dst_sp->bits[i] = Op::op(a_sp->bits[i], b_sp->bits[i]);
      }
      Utils::PopCounter<BLBitWord> counter;
      counter.add_array(dst_sp->bits, kBWCount);
      cardinality = counter.get();
    }
    else {
      bool use_a = (a_dense & sp_bit) != 0u;
      bool invert = (invert_mask & sp_bit) != 0u;
      SubPlaneInfo src_info = mp_load_sp_info(use_a ? a_mp : b_mp, sp_index);
      BL_ASSERT(src_info.is_dense());
      cardinality = src_info.cardinality();
      if (invert) {
        cardinality = kSPBitCount - cardinality;
      }

      // An allocated input bitmap can already be uniform. Copying or complementing it needs no scan.
      if (!sp_is_cardinality_empty_or_full(cardinality)) {
        const SubPlane* src_sp = get_sp_dense(use_a ? a_impl : b_impl, src_info);
        if (is_page_special(sp_offset)) {
          sp_offset = next_page_index(dst, dst_impl);
        }

        SubPlane* dst_sp = get_sp_dense(dst_impl, sp_offset);
        if (invert) {
          for (size_t i = 0; i < kBWCount; i++) {
            dst_sp->bits[i] = ~src_sp->bits[i];
          }
        }
        else if (dst_sp != src_sp) {
          copy_page(dst_sp, src_sp);
        }
      }
    }

    if (sp_is_cardinality_empty_or_full(cardinality)) {
      free_page_if_not_special(dst, sp_offset);
      sp_offset = sp_special_offset_from_cardinality(cardinality);
      plane.sp_dense_mask &= ~sp_bit;
      plane.sp_filled_mask |= uint32_t(cardinality != 0u) << sp_index;
    }

    mp_store_sp_info(dst_mp, sp_index, SubPlaneInfo::from_page_index_and_cardinality(sp_offset, cardinality));
    mp_cardinality += cardinality;
  }

  if (mp_is_cardinality_empty_or_full(mp_cardinality)) {
    BL_ASSERT(plane.sp_dense_mask == 0u);
    free_page(dst, dst_offset);
    return mp_special_offset_from_cardinality(mp_cardinality);
  }

  dst_mp->sp_dense_mask = plane.sp_dense_mask;
  dst_mp->sp_filled_mask = plane.sp_filled_mask;
  dst_mp->mp_cardinality = mp_cardinality;
  return dst_offset;
}

template<typename Op>
static BLResult combine_op(BLCoverageSetCore* dst, const BLCoverageSetCore* a, const BLCoverageSetCore* b) noexcept {
  // The public entry point handles identical inputs and makes `a` the only possible aliased operand.
  BL_ASSERT(dst != b);
  constexpr bool kPreserveATail = Op::op(1u, 0u) != 0u;
  constexpr bool kPreserveBTail = Op::op(0u, 1u) != 0u;

  size_t a_count = get_mp_count(a);
  size_t b_count = get_mp_count(b);
  if (a_count == 0u) {
    return kPreserveBTail ? bl_coverage_set_assign_weak(dst, b) : bl_coverage_set_clear(dst);
  }
  if (b_count == 0u) {
    if constexpr (kPreserveATail) {
      return dst == a ? BL_SUCCESS : bl_coverage_set_assign_weak(dst, a);
    }
    else {
      return bl_coverage_set_clear(dst);
    }
  }

  size_t mp_count = axl::min(a_count, b_count);
  if constexpr (kPreserveATail) {
    mp_count = a_count;
  }
  if constexpr (kPreserveBTail) {
    mp_count = axl::max(mp_count, b_count);
  }

  const BLCoverageSetImpl* a_impl = ObjectLayer::get_impl(a);
  const BLCoverageSetImpl* b_impl = ObjectLayer::get_impl(b);
  const uint16_t* a_offsets = get_mp_offsets(a);
  const uint16_t* b_offsets = get_mp_offsets(b);

  CombinePlane planes[kMPCount] {};
  size_t pages_required = 0u;
  size_t result_count = 0u;

  // Analyze only master-plane metadata. Dense pairs are counted even if their eventual result is
  // uniform; dense/special pairs whose result is special contribute no pages to the bound.
  for (size_t mp_index = 0; mp_index < mp_count; mp_index++) {
    const MasterPlane* a_mp = get_combine_mp(a_impl, static_cast<PageIndex>(a_offsets[mp_index]));
    const MasterPlane* b_mp = get_combine_mp(b_impl, static_cast<PageIndex>(b_offsets[mp_index]));
    CombinePlane plane = analyze_combine_mp<Op>(a_mp, b_mp);
    planes[mp_index] = plane;
    pages_required += uint32_t(!plane.is_uniform()) + axl::popcnt(plane.sp_dense_mask);
    if (plane.sp_dense_mask | plane.sp_filled_mask) {
      result_count = mp_index + 1u;
    }
  }

  if (result_count == 0u) {
    return bl_coverage_set_clear(dst);
  }

  BLCoverageSetCore tmp;
  BLCoverageSetCore* out = dst;
  if (!ObjectLayer::is_object_mutable(dst) || pages_required > get_page_capacity(dst)) {
    // Even a result with zero bitmap pages needs its own writable master-plane directory.
    BL_PROPAGATE(alloc(&tmp, pages_required ? expand_capacity(pages_required) : 0u));
    out = &tmp;
  }
  else if (dst == a) {
    free_combine_pages(dst, planes);
  }
  else {
    purge(dst);
  }

  // No fallible work remains. An aliased input's surviving pages stay at their original offsets.
  uint16_t* out_offsets = get_mp_offsets(out);
  result_count = 0u;
  for (size_t mp_index = 0; mp_index < mp_count; mp_index++) {
    CombinePlane plane = planes[mp_index];
    PageIndex offset;
    if (plane.is_uniform()) {
      offset = plane.special_offset();
    }
    else {
      const MasterPlane* a_mp = get_combine_mp(a_impl, static_cast<PageIndex>(a_offsets[mp_index]));
      const MasterPlane* b_mp = get_combine_mp(b_impl, static_cast<PageIndex>(b_offsets[mp_index]));
      offset = combine_mp<Op>(out, static_cast<PageIndex>(out_offsets[mp_index]), plane, a_impl, a_mp, b_impl, b_mp);
    }
    out_offsets[mp_index] = uint16_t(offset);
    if (!is_page_zero(offset)) {
      result_count = mp_index + 1u;
    }
  }

  axl::fill_items(out_offsets + mp_count, kMPCount - mp_count, kPageZero);
  set_mp_count(out, result_count);
  return out == dst ? BL_SUCCESS : ObjectLayer::replace_object(dst, out);
}

} // {bl::CoverageSetDetail}

// CoverageSet - API - Construction & Destruction
// ==============================================

BL_API_IMPL BLResult bl_coverage_set_init(BLCoverageSetCore* self) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  return ObjectLayer::init_object(self);
}

BL_API_IMPL BLResult bl_coverage_set_init_move(BLCoverageSetCore* self, BLCoverageSetCore* other) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::move_object(self, other);
}

BL_API_IMPL BLResult bl_coverage_set_init_weak(BLCoverageSetCore* self, const BLCoverageSetCore* other) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::copy_object(self, other);
}

BL_API_IMPL BLResult bl_coverage_set_init_range(BLCoverageSetCore* self, uint32_t start_cp, uint32_t end_cp) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  ObjectLayer::init_object(self);

  return bl_coverage_set_add_range(self, start_cp, end_cp);
}

BL_API_IMPL BLResult bl_coverage_set_destroy(BLCoverageSetCore* self) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::release_object(self);
}

BL_API_IMPL BLResult bl_coverage_set_reset(BLCoverageSetCore* self) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::reset_object(self);
}

// CoverageSet - API - Assignment
// ==============================

BL_API_IMPL BLResult bl_coverage_set_assign_move(BLCoverageSetCore* self, BLCoverageSetCore* other) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_move(self, other);
}

BL_API_IMPL BLResult bl_coverage_set_assign_weak(BLCoverageSetCore* self, const BLCoverageSetCore* other) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_weak(self, other);
}

BL_API_IMPL BLResult bl_coverage_set_assign_deep(BLCoverageSetCore* self, const BLCoverageSetCore* other) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  Layout& self_layout = self->_d.layout<Layout>();
  const Layout& other_layout = other->_d.layout<Layout>();

  if (self == other) {
    if (ObjectLayer::is_object_mutable(self)) {
      return BL_SUCCESS;
    }
  }
  else {
    if (ObjectLayer::check_dynamic_capacity_and_mutability_raw(self, other_layout.page_count, self_layout.page_capacity)) {
      purge(self);
      return copy_content(self, other);
    }
  }

  BLCoverageSetCore tmp;
  BL_PROPAGATE(alloc_copy(&tmp, other, other_layout.page_count));
  return ObjectLayer::replace_object(self, &tmp);
}

BL_API_IMPL BLResult bl_coverage_set_assign_range(BLCoverageSetCore* self, uint32_t start_cp, uint32_t end_cp) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  // TODO: Not transactional when `bl_coverage_set_add_range()` fails.
  BL_PROPAGATE(bl_coverage_set_clear(self));
  return bl_coverage_set_add_range(self, start_cp, end_cp);
}

// CoverageSet - API - Accessors
// =============================

BL_API_IMPL bool bl_coverage_set_is_empty(const BLCoverageSetCore* self) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLCoverageSetImpl* impl = ObjectLayer::get_impl(self);
  const uint16_t* mp_offsets = CoverageSetDetail::get_mp_offsets(self);

  size_t mp_count = get_mp_count(self);
  for (size_t mp_index = 0; mp_index < mp_count; mp_index++) {
    PageIndex mp_offset = static_cast<PageIndex>(mp_offsets[mp_index]);
    if (CoverageSetDetail::get_mp_or_builtin(impl, mp_offset)->mp_cardinality != 0u) {
      return false;
    }
  }

  return true;
}


BL_API_IMPL size_t bl_coverage_set_get_page_count(const BLCoverageSetCore* self) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return get_page_count(self);
}

BL_API_IMPL size_t bl_coverage_set_get_page_capacity(const BLCoverageSetCore* self) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return get_page_capacity(self);
}

BL_API_IMPL bool bl_coverage_set_get_range(const BLCoverageSetCore* self, uint32_t* range_start_out, uint32_t* range_end_out) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLCoverageSetImpl* impl = ObjectLayer::get_impl(self);
  const uint16_t* mp_offsets = get_mp_offsets(self);

  size_t mp_count = get_mp_count(self);
  uint32_t range_start = 0;
  uint32_t range_end = 0;

  // Find range start.
  for (size_t mp_index = 0; mp_index < mp_count; mp_index++, range_start += kMPBitCount) {
    PageIndex mp_offset = static_cast<PageIndex>(mp_offsets[mp_index]);

    if (is_page_special(mp_offset)) {
      if (is_page_zero(mp_offset)) {
        continue;
      }
      goto FindRangeEnd;
    }

    const MasterPlane* mp = CoverageSetDetail::get_mp_dense(impl, mp_offset);
    axl::BitWordIterator<uint32_t> it(mp->sp_dense_mask | mp->sp_filled_mask);

    while (it.has_next()) {
      size_t sp_index = it.next();
      SubPlaneInfo sp_info = mp_load_sp_info(mp, sp_index);

      if (sp_info.is_special()) {
        // We are iterating over dense or filled planes - we cannot face a zero-plane here.
        BL_ASSERT(!sp_info.is_zero_page());

        range_start += static_cast<uint32_t>(sp_index) * kSPBitCount;
        goto FindRangeEnd;
      }
      else if (sp_info.has_cardinality()) {
        const SubPlane* sp = get_sp_dense(impl, sp_info);
        range_start += static_cast<uint32_t>(sp_index) * kSPBitCount;

        for (size_t bw_index = 0; bw_index < kBWCount; bw_index++, range_start += axl::bit_size_of<BLBitWord>) {
          BLBitWord bw = sp->bits[bw_index];
          if (bw != 0u) {
            range_start += axl::ctz(bw);
            goto FindRangeEnd;
          }
        }

        // Impossible - if cardinality != 0 we have to find at least one bit set.
        BL_NOT_REACHED();
      }
    }
  }

  // If we reached here the container is empty - set it both outputs zero (range_end is correct for both).
  *range_start_out = range_end;
  *range_end_out = range_end;
  return false;

  // Find range end.
FindRangeEnd:
  for (size_t mp_index = mp_count; mp_index != 0; mp_index--) {
    PageIndex mp_offset = static_cast<PageIndex>(mp_offsets[mp_index - 1u]);

    if (is_page_special(mp_offset)) {
      if (is_page_zero(mp_offset)) {
        continue;
      }

      range_end = uint32_t(mp_index) * kMPBitCount;
      goto Done;
    }

    const MasterPlane* mp = get_mp_dense(impl, mp_offset);
    for (size_t sp_index = kSPPerMP; sp_index != 0; sp_index--) {
      SubPlaneInfo sp_info = mp_load_sp_info(mp, sp_index - 1u);
      if (sp_info.has_cardinality()) {
        range_end = uint32_t(mp_index - 1u) * kMPBitCount + uint32_t(sp_index) * kSPBitCount;

        if (sp_info.is_special()) {
          // Cannot have empty offset if cardinality is non-zero.
          BL_ASSERT(!sp_info.is_zero_page());
        }
        else {
          const SubPlane* sp = get_sp_dense(impl, sp_info);
          for (size_t bw_index = kBWCount; bw_index != 0u; bw_index--, range_end -= axl::bit_size_of<BLBitWord>) {
            BLBitWord bw = sp->bits[bw_index - 1u];
            if (bw != 0u) {
              range_end -= axl::clz(bw);
              break;
            }
          }
        }
        goto Done;
      }
    }
  }

Done:
  *range_start_out = range_start;
  *range_end_out = range_end;
  return true;
}

BL_API_IMPL uint32_t bl_coverage_set_get_cardinality(const BLCoverageSetCore* self) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLCoverageSetImpl* impl = ObjectLayer::get_impl(self);
  const uint16_t* mp_offsets = get_mp_offsets(self);

  size_t mp_count = get_mp_count(self);
  uint32_t cardinality_sum = 0u;

  for (size_t mp_index = 0; mp_index < mp_count; mp_index++) {
    cardinality_sum += get_mp_or_builtin(impl, static_cast<PageIndex>(mp_offsets[mp_index]))->mp_cardinality;
  }

  return cardinality_sum;
}

BL_API_IMPL uint32_t bl_coverage_set_get_cardinality_in_range(const BLCoverageSetCore* self, uint32_t range_start, uint32_t range_end) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLCoverageSetImpl* impl = ObjectLayer::get_impl(self);
  const uint16_t* mp_offsets = get_mp_offsets(self);

  size_t mp_count = get_mp_count(self);
  uint32_t cardinality = 0u;

  range_end = axl::min<uint32_t>(range_end, static_cast<uint32_t>(mp_count) * kMPBitCount);
  if (BL_UNLIKELY(range_start >= range_end)) {
    return cardinality;
  }

  size_t mp_index = mp_index_from_bit_index(range_start);
  size_t sp_index = sp_index_from_bit_index(range_start);

  for (;;) {
    BL_ASSERT(mp_index < mp_count);
    PageIndex mp_offset = static_cast<PageIndex>(mp_offsets[mp_index]);

    if (is_page_special(mp_offset)) {
      sp_index = 0;
      mp_index++;

      uint32_t mp_range_end = axl::min(static_cast<uint32_t>(mp_index) * kMPBitCount, range_end);
      uint32_t mp_range_size = mp_range_end - range_start;
      cardinality += is_page_zero(mp_offset) ? 0u : mp_range_size;

      range_start = mp_range_end;
      if (range_start >= range_end) {
        return cardinality;
      }
      continue;
    }

    const MasterPlane* mp = get_mp_dense(impl, mp_offset);
    for (;;) {
      SubPlaneInfo sp_info = mp_load_sp_info(mp, sp_index);
      uint32_t sp_range_end = axl::min(static_cast<uint32_t>(mp_index) * kMPBitCount + static_cast<uint32_t>(sp_index + 1u) * kSPBitCount, range_end);

      // Handle the first sub-plane - it could be the beginning or end of the range, which could need a partial
      // cardinality calculations.
      if (sp_info.is_special()) {
        uint32_t sp_range_size = sp_range_end - range_start;
        cardinality += sp_info.is_zero_page() ? 0u : sp_range_size;
      }
      else if (((range_start | sp_range_end) & kSPBitMask) == 0u) {
        cardinality += sp_info.cardinality();
      }
      else {
        const SubPlane* sp = get_sp_dense(impl, sp_info);
        cardinality = sp_get_cardinality_in_range(sp->bits, range_start, sp_range_end, cardinality);
      }

      range_start = sp_range_end;
      if (range_start >= range_end) {
        return cardinality;
      }

      if (++sp_index == kSPPerMP) {
        break;
      }

      // Handle the middle sub-planes - these don't require partial calculations, so we can use precalculated
      // sub-plane cardinalities, which we decode from SubPlaneInfo.
      uint32_t sp_end = axl::min(static_cast<uint32_t>(sp_index) + (range_end - range_start) / kSPBitCount, kSPPerMP);
      while (sp_index < sp_end) {
        sp_info = mp_load_sp_info(mp, sp_index++);
        cardinality += sp_info.cardinality();
        range_start += kSPBitCount;
      }

      if (range_start >= range_end) {
        return cardinality;
      }

      if (sp_index >= kSPPerMP) {
        break;
      }
    }

    mp_index++;
    sp_index = 0;
  }
}

// CoverageSet - API - Equality
// ============================

BL_API_IMPL bool bl_coverage_set_equals(const BLCoverageSetCore* a, const BLCoverageSetCore* b) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  using namespace bl::SIMD;

  BL_ASSERT(ObjectLayer::is_object_valid(a));
  BL_ASSERT(ObjectLayer::is_object_valid(b));

  // The order of equals() doesn't matter, so prefer `mp_count(A)` <= `mp_count(B)`. Differing `mp_count` doesn't really
  // mean the containers don't have equal bits - it just means that there are more master-planes, but they can be empty.
  size_t mp_count_a = get_mp_count(a);
  size_t mp_count_b = get_mp_count(b);

  if (mp_count_a > mp_count_b) {
    axl::swap(a, b);
    axl::swap(mp_count_a, mp_count_b);
  }

  const BLCoverageSetImpl* a_impl = ObjectLayer::get_impl(a);
  const BLCoverageSetImpl* b_impl = ObjectLayer::get_impl(b);

  // Quick check if both containers hold the same data.
  if (a_impl == b_impl) {
    return true;
  }

  // First compare master-planes both containers have in common.
  const uint16_t* a_mp_offsets = get_mp_offsets(a);
  const uint16_t* b_mp_offsets = get_mp_offsets(b);

  size_t mp_index = 0u;

#if BL_SIMD_WIDTH_I >= 256
  Vec16xU16 v_card_hi_mask = make256_u16(0x0Fu);
#elif BL_SIMD_WIDTH_I >= 128
  Vec8xU16 v_card_hi_mask = make128_u16(0x0Fu);
#endif

  for (; mp_index < mp_count_a; mp_index++) {
    PageIndex a_mp_offset = static_cast<PageIndex>(a_mp_offsets[mp_index]);
    PageIndex b_mp_offset = static_cast<PageIndex>(b_mp_offsets[mp_index]);

    // Quick skip if both master-planes share the same built-in master-plane.
    if (a_mp_offset == b_mp_offset && is_page_special(a_mp_offset)) {
      continue;
    }

    const MasterPlane* a_mp = get_mp_or_builtin(a_impl, a_mp_offset);
    const MasterPlane* b_mp = get_mp_or_builtin(b_impl, b_mp_offset);

    uint32_t a_dense_mask = a_mp->sp_dense_mask;
    uint32_t b_dense_mask = b_mp->sp_dense_mask;

    // SIMD enabled implementation first compares all cardinalities, before going for dense vs dense sub-plane check.
    // We don't have to compare dense sub-planes vs special sub-planes, because it's guaranteed that once cardinalities
    // match, dense vs special would also match (dense sub-plane with 0 cardinality matches built-in kPageZero sub-plane).
#if BL_SIMD_WIDTH_I >= 256
    Vec32xU8 a_card_lo = loadu<Vec32xU8>(&a_mp->sp_card_lo[0]);
    Vec32xU8 b_card_lo = loadu<Vec32xU8>(&b_mp->sp_card_lo[0]);
    Vec32xU8 eq_all = cmp_eq_u8(a_card_lo, b_card_lo);

    Vec16xU16 a_page_hi_0 = loadu<Vec16xU16>(&a_mp->sp_page_hi[0]);
    Vec16xU16 a_page_hi_1 = loadu<Vec16xU16>(&a_mp->sp_page_hi[16]);
    Vec16xU16 b_page_hi_0 = loadu<Vec16xU16>(&b_mp->sp_page_hi[0]);
    Vec16xU16 b_page_hi_1 = loadu<Vec16xU16>(&b_mp->sp_page_hi[16]);

    Vec32xU8 a_card_hi_0_1 = packz_128_u16_u8(vec_cast<Vec32xU8>(a_page_hi_0 & v_card_hi_mask), vec_cast<Vec32xU8>(a_page_hi_1 & v_card_hi_mask));
    Vec32xU8 b_card_hi_0_1 = packz_128_u16_u8(vec_cast<Vec32xU8>(b_page_hi_0 & v_card_hi_mask), vec_cast<Vec32xU8>(b_page_hi_1 & v_card_hi_mask));

    eq_all &= cmp_eq_u8(a_card_hi_0_1, b_card_hi_0_1);
    uint32_t ab_dense_mask = a_dense_mask & b_dense_mask;

    if (!test_mask_ones_i8(eq_all)) {
      return false;
    }

    axl::BitWordIterator it(ab_dense_mask);
    while (it.has_next()) {
      size_t sp_index = it.next();

      const SubPlane* a_sp = get_sp_dense(a_impl, static_cast<PageIndex>(a_mp->sp_page_hi[sp_index] >> 4));
      const SubPlane* b_sp = get_sp_dense(b_impl, static_cast<PageIndex>(b_mp->sp_page_hi[sp_index] >> 4));

      if (!simd_mem_eq<kPageSize>(a_sp->bits, b_sp->bits)) {
        return false;
      }
    }
#elif BL_SIMD_WIDTH_I >= 128
    Vec16xU8 a_card_lo_0 = loada<Vec16xU8>(&a_mp->sp_card_lo[0]);
    Vec16xU8 a_card_lo_1 = loada<Vec16xU8>(&a_mp->sp_card_lo[16]);
    Vec16xU8 b_card_lo_0 = loada<Vec16xU8>(&b_mp->sp_card_lo[0]);
    Vec16xU8 b_card_lo_1 = loada<Vec16xU8>(&b_mp->sp_card_lo[16]);
    Vec16xU8 eq_all = cmp_eq_u8(a_card_lo_0, b_card_lo_0) & cmp_eq_u8(a_card_lo_1, b_card_lo_1);

    Vec8xU16 a_page_hi_0 = loada<Vec8xU16>(&a_mp->sp_page_hi[0]);
    Vec8xU16 a_page_hi_1 = loada<Vec8xU16>(&a_mp->sp_page_hi[8]);
    Vec8xU16 b_page_hi_0 = loada<Vec8xU16>(&b_mp->sp_page_hi[0]);
    Vec8xU16 b_page_hi_1 = loada<Vec8xU16>(&b_mp->sp_page_hi[8]);

    Vec16xU8 a_card_hi_0_1 = packz_128_u16_u8(vec_cast<Vec16xU8>(a_page_hi_0 & v_card_hi_mask), vec_cast<Vec16xU8>(a_page_hi_1 & v_card_hi_mask));
    Vec16xU8 b_card_hi_0_1 = packz_128_u16_u8(vec_cast<Vec16xU8>(b_page_hi_0 & v_card_hi_mask), vec_cast<Vec16xU8>(b_page_hi_1 & v_card_hi_mask));
    eq_all &= cmp_eq_u8(a_card_hi_0_1, b_card_hi_0_1);

    Vec8xU16 a_page_hi_2 = loada<Vec8xU16>(&a_mp->sp_page_hi[16]);
    Vec8xU16 a_page_hi_3 = loada<Vec8xU16>(&a_mp->sp_page_hi[24]);
    Vec8xU16 b_page_hi_2 = loada<Vec8xU16>(&b_mp->sp_page_hi[16]);
    Vec8xU16 b_page_hi_3 = loada<Vec8xU16>(&b_mp->sp_page_hi[24]);

    Vec16xU8 a_card_hi_2_3 = packz_128_u16_u8(vec_cast<Vec16xU8>(a_page_hi_2 & v_card_hi_mask), vec_cast<Vec16xU8>(a_page_hi_3 & v_card_hi_mask));
    Vec16xU8 b_card_hi_2_3 = packz_128_u16_u8(vec_cast<Vec16xU8>(b_page_hi_2 & v_card_hi_mask), vec_cast<Vec16xU8>(b_page_hi_3 & v_card_hi_mask));

    eq_all &= cmp_eq_u8(a_card_hi_2_3, b_card_hi_2_3);
    uint32_t ab_dense_mask = a_dense_mask & b_dense_mask;

    if (!test_mask_ones_i8(eq_all)) {
      return false;
    }

    axl::BitWordIterator it(ab_dense_mask);
    while (it.has_next()) {
      size_t sp_index = it.next();

      const SubPlane* a_sp = get_sp_dense(a_impl, static_cast<PageIndex>(a_mp->sp_page_hi[sp_index] >> 4));
      const SubPlane* b_sp = get_sp_dense(b_impl, static_cast<PageIndex>(b_mp->sp_page_hi[sp_index] >> 4));

      if (!simd_mem_eq<kPageSize>(a_sp->bits, b_sp->bits)) {
        return false;
      }
    }
#else
    uint32_t ab_dense_mask = a_dense_mask | b_dense_mask;
    uint32_t ab_filled_mask_diff = a_mp->sp_filled_mask ^ b_mp->sp_filled_mask;

    // Quick check LO cardinality values and the difference of `sp_filled_mask(A ^ B)` outside of `sp_dense_mask(A | B)`.
    if (axl::bool_or(memcmp(a_mp->sp_card_lo, b_mp->sp_card_lo, sizeof(a_mp->sp_card_lo)) != 0, (ab_filled_mask_diff & ~ab_dense_mask) != 0u)) {
      return false;
    }

    // Fallback version that doesn't use SIMD.
    axl::BitWordIterator it(ab_dense_mask);
    while (it.has_next()) {
      size_t sp_index = it.next();

      SubPlaneInfo a_info = mp_load_sp_info(a_mp, sp_index);
      SubPlaneInfo b_info = mp_load_sp_info(b_mp, sp_index);

      size_t ab_info_hi = a_info.page_hi ^ b_info.page_hi;

      // Cardinality mismatch -> not equal.
      if (ab_info_hi & 0xFu) {
        return false;
      }

      // If cardinality matches, dense page vs special page would also match - so avoid comparison between
      // dense and special sub-planes, which would not just burn cycles, but also touch cache lines.
      if (ab_info_hi & 0x8000u) {
        continue;
      }

      BL_ASSERT(a_info.is_dense());
      BL_ASSERT(b_info.is_dense());

      const SubPlane* a_sp = get_sp_dense(a_impl, a_info);
      const SubPlane* b_sp = get_sp_dense(b_impl, b_info);

      if (memcmp(a_sp->bits, b_sp->bits, kPageSize) != 0) {
        return false;
      }
    }
#endif
  }

  // Compare the remaining master-planes - they must all be ALL empty.
  for (; mp_index < mp_count_b; mp_index++) {
    PageIndex mp_offset = static_cast<PageIndex>(b_mp_offsets[mp_index]);
    if (get_mp_or_builtin(b_impl, mp_offset)->mp_cardinality != 0u) {
      return false;
    }
  }

  return true;
}

// CoverageSet - API - Test Operations
// ===================================

BL_API_IMPL bool bl_coverage_set_contains(const BLCoverageSetCore* self, uint32_t index) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  size_t mp_index = mp_index_from_bit_index(index);
  size_t sp_index = sp_index_from_bit_index(index);

  const uint16_t* mp_offsets = get_mp_offsets(self);
  const BLCoverageSetImpl* impl = ObjectLayer::get_impl(self);

  if (mp_index >= kMPCount) {
    return false;
  }

  PageIndex mp_offset = static_cast<PageIndex>(mp_offsets[mp_index]);
  const MasterPlane* mp = get_mp_or_builtin(impl, mp_offset);

  SubPlaneInfo sp_info = mp_load_sp_info(mp, sp_index);
  if (sp_info.is_special()) {
    return sp_info.is_full_page();
  }

  const SubPlane* sp = get_sp_dense(impl, sp_info);
  size_t bw_index = bw_index_from_bit_index(index);
  BLBitWord bw_mask = axl::shl_wrap<BLBitWord>(1u, index);

  return (sp->bits[bw_index] & bw_mask) != 0u;
}

BL_API_IMPL bool bl_coverage_set_contains_in_range(const BLCoverageSetCore* self, uint32_t range_start, uint32_t range_end) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLCoverageSetImpl* impl = ObjectLayer::get_impl(self);
  const uint16_t* mp_offsets = get_mp_offsets(self);

  size_t mp_count = get_mp_count(self);
  uint32_t cardinality = 0u;

  range_end = axl::min<uint32_t>(range_end, static_cast<uint32_t>(mp_count) * kMPBitCount);
  if (BL_UNLIKELY(range_start >= range_end)) {
    return cardinality;
  }

  size_t mp_index = mp_index_from_bit_index(range_start);
  size_t sp_index = sp_index_from_bit_index(range_start);

  for (;;) {
    BL_ASSERT(mp_index < mp_count);
    PageIndex mp_offset = static_cast<PageIndex>(mp_offsets[mp_index]);

    if (is_page_special(mp_offset)) {
      if (is_page_full(mp_offset)) {
        return true;
      }

      sp_index = 0;
      mp_index++;
      range_start = axl::min(static_cast<uint32_t>(mp_index) * kMPBitCount, range_end);
      if (range_start >= range_end) {
        return false;
      }
      continue;
    }

    const MasterPlane* mp = get_mp_dense(impl, mp_offset);
    for (;;) {
      SubPlaneInfo sp_info = mp_load_sp_info(mp, sp_index);
      uint32_t sp_range_end = axl::min(static_cast<uint32_t>(mp_index) * kMPBitCount + static_cast<uint32_t>(sp_index + 1u) * kSPBitCount, range_end);

      // Handle the first sub-plane - it could be the beginning or end of the range, which could need a partial
      // cardinality calculations.
      if (sp_info.is_special()) {
        if (sp_info.is_full_page()) {
          return true;
        }
      }
      else {
        const SubPlane* sp = get_sp_dense(impl, sp_info);
        if (sp_has_cardinality_in_range(sp->bits, range_start, sp_range_end)) {
          return true;
        }
      }

      range_start = sp_range_end;
      if (range_start >= range_end) {
        return false;
      }

      if (++sp_index == kSPPerMP) {
        break;
      }

      // Handle the middle sub-planes - these don't require partial calculations, so we can use precalculated
      // sub-plane cardinalities, which we decode from SubPlaneInfo.
      uint32_t sp_end = axl::min(static_cast<uint32_t>(sp_index) + (range_end - range_start) / kSPBitCount, kSPPerMP);
      while (sp_index < sp_end) {
        sp_info = mp_load_sp_info(mp, sp_index++);
        if (sp_info.has_cardinality()) {
          return true;
        }
        range_start += kSPBitCount;
      }

      if (range_start >= range_end) {
        return false;
      }

      if (sp_index >= kSPPerMP) {
        break;
      }
    }

    mp_index++;
    sp_index = 0;
  }
}

BL_API_IMPL bool bl_coverage_set_subsumes(const BLCoverageSetCore* a, const BLCoverageSetCore* b) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(a));
  BL_ASSERT(ObjectLayer::is_object_valid(b));

  const BLCoverageSetImpl* a_impl = ObjectLayer::get_impl(a);
  const BLCoverageSetImpl* b_impl = ObjectLayer::get_impl(b);

  const uint16_t* a_mp_offsets = get_mp_offsets(a);
  const uint16_t* b_mp_offsets = get_mp_offsets(b);

  // First iterate over master planes both containers have in common.
  size_t mp_index = 0u;
  size_t mp_count = axl::min(get_mp_count(a), get_mp_count(b));

  for (; mp_index < mp_count; mp_index++) {
    PageIndex a_mp_offset = static_cast<PageIndex>(a_mp_offsets[mp_index]);
    PageIndex b_mp_offset = static_cast<PageIndex>(b_mp_offsets[mp_index]);

    // A always subsumes B if A is filled entirely or B is completely empty.
    if (axl::bool_or(is_page_full(a_mp_offset), is_page_zero(b_mp_offset))) {
      continue;
    }

    const MasterPlane* a_mp = get_mp_or_builtin(a_impl, a_mp_offset);
    const MasterPlane* b_mp = get_mp_or_builtin(b_impl, b_mp_offset);

    // First check cardinalities - if cardinality of A is less than B then A cannot subsume B.
    if (a_mp->mp_cardinality < b_mp->mp_cardinality) {
      return false;
    }

    // Filled A always subsumes B; zero B is always subsumed by A.
    uint32_t skip_mask = a_mp->sp_filled_mask | (~b_mp->sp_dense_mask & ~b_mp->sp_filled_mask);

    axl::BitWordIterator<uint32_t> it(~skip_mask);
    while (it.has_next()) {
      size_t sp_index = it.next();

      SubPlaneInfo a_info = mp_load_sp_info(a_mp, sp_index);
      SubPlaneInfo b_info = mp_load_sp_info(b_mp, sp_index);

      const SubPlane* a_sp = get_sp_or_builtin(a_impl, a_info);
      const SubPlane* b_sp = get_sp_or_builtin(b_impl, b_info);

      if (!sp_subsumes(*a_sp, *b_sp)) {
        return false;
      }
    }
  }

  // Next verify that ALL remaining master planes of `b` are empty.
  mp_count = get_mp_count(b);
  for (; mp_index < mp_count; mp_index++) {
    PageIndex b_mp_offset = static_cast<PageIndex>(b_mp_offsets[mp_index]);
    if (get_mp_or_builtin(b_impl, b_mp_offset)->mp_cardinality != 0u) {
      return false;
    }
  }

  return true;
}

BL_API_IMPL bool bl_coverage_set_intersects(const BLCoverageSetCore* a, const BLCoverageSetCore* b) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(a));
  BL_ASSERT(ObjectLayer::is_object_valid(b));

  const BLCoverageSetImpl* a_impl = ObjectLayer::get_impl(a);
  const BLCoverageSetImpl* b_impl = ObjectLayer::get_impl(b);

  const uint16_t* a_mp_offsets = get_mp_offsets(a);
  const uint16_t* b_mp_offsets = get_mp_offsets(b);

  // Only iterate over master planes both containers have in common.
  size_t mp_count = axl::min(get_mp_count(a), get_mp_count(b));

  for (size_t mp_index = 0u; mp_index < mp_count; mp_index++) {
    PageIndex a_mp_offset = static_cast<PageIndex>(a_mp_offsets[mp_index]);
    PageIndex b_mp_offset = static_cast<PageIndex>(b_mp_offsets[mp_index]);

    if (is_page_zero(a_mp_offset) || is_page_zero(b_mp_offset)) {
      // No intersection if any of A or B is built-in zero plane.
      continue;
    }

    const MasterPlane* a_mp = get_mp_or_builtin(a_impl, a_mp_offset);
    const MasterPlane* b_mp = get_mp_or_builtin(b_impl, b_mp_offset);

    uint32_t a_dense_mask = a_mp->sp_dense_mask;
    uint32_t b_dense_mask = b_mp->sp_dense_mask;

    uint32_t a_filled_mask = a_mp->sp_filled_mask;
    uint32_t b_filled_mask = b_mp->sp_filled_mask;

    // If A and B share sub-planes both completely filled, it's guaranteed that there is an intersection.
    if (a_filled_mask & b_filled_mask) {
      return true;
    }

    axl::BitWordIterator<uint32_t> it((a_dense_mask | a_filled_mask) & (b_dense_mask | b_filled_mask));
    while (it.has_next()) {
      size_t sp_index = it.next();

      SubPlaneInfo a_info = mp_load_sp_info(a_mp, sp_index);
      SubPlaneInfo b_info = mp_load_sp_info(b_mp, sp_index);

      const SubPlane* a_sp = get_sp_or_builtin(a_impl, a_info);
      const SubPlane* b_sp = get_sp_or_builtin(b_impl, b_info);

      if (sp_intersects(*a_sp, *b_sp)) {
        return true;
      }
    }
  }

  return false;
}

// CoverageSet - API - Manipulation
// ================================

BL_API_IMPL BLResult bl_coverage_set_clear(BLCoverageSetCore* self) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (self->_d.is_ref_counted()) {
    if (ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kRC>(self)) {
      return purge(self);
    }

    ObjectLayer::release_object<ObjectLayer::ObjectMode::kRC>(self);
  }

  return ObjectLayer::init_object(self);
}

BL_API_IMPL BLResult bl_coverage_set_shrink(BLCoverageSetCore* self) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLCoverageSetImpl* self_impl = ObjectLayer::get_impl(self);
  const uint16_t* mp_offsets = get_mp_offsets(self);

  size_t mp_count = get_mp_count(self);
  size_t mp_optimized_count = 0;

  size_t pages_required = get_page_count(self);
  uint16_t mp_optimized_offsets[kMPCount];

  // The difference between shrink() loop and optimize() loop is that we need to do an analysis here to calculate
  // how many pages will be required by an optimized representation, but the pages are not freed in this loop.
  for (size_t mp_index = 0; mp_index < mp_count; mp_index++) {
    PageIndex mp_offset = static_cast<PageIndex>(mp_offsets[mp_index]);
    mp_optimized_offsets[mp_index] = uint16_t(mp_offset);

    if (is_page_special(mp_offset)) {
      if (is_page_full(mp_offset)) {
        mp_optimized_count = mp_index + 1u;
      }
      continue;
    }

    const MasterPlane* mp = get_mp_dense(self_impl, mp_offset);
    uint32_t mp_cardinality = mp->mp_cardinality;

    if (mp_is_cardinality_empty_or_full(mp_cardinality)) {
      // Turn the whole master-plane into a special built-in plane, which effectively frees all associated sub-planes.
      pages_required -= axl::popcnt(mp->sp_dense_mask) + 1u;
      mp_optimized_offsets[mp_index] = uint16_t(mp_special_offset_from_cardinality(mp_cardinality));

      if (mp_cardinality == 0u) {
        continue;
      }
    }
    else {
      axl::BitWordIterator<uint32_t> it(mp->sp_dense_mask);
      while (it.has_next()) {
        size_t sp_index = it.next();
        SubPlaneInfo sp_info = mp_load_sp_info(mp, sp_index);
        pages_required -= uint32_t(sp_info.has_special_cardinality());
      }
    }

    mp_optimized_count = mp_index + 1u;
  }

  if (mp_optimized_count == 0u) {
    // The container is empty, ideal representation of an empty container is a default-constructed one, which needs `reset()`.
    BL_ASSERT(pages_required == 0u);
    return bl_coverage_set_reset(self);
  }

  if (pages_required == get_page_capacity(self)) {
    // The container representation is optimal.
    return BL_SUCCESS;
  }

  BLCoverageSetCore tmp;
  BL_PROPAGATE(alloc(&tmp, pages_required));

  BLCoverageSetImpl* dst_impl = ObjectLayer::get_impl(&tmp);
  uint16_t* mp_dst_offsets = get_mp_offsets(&tmp);
  size_t page_index = get_page_count(&tmp);

  for (size_t mp_index = 0; mp_index < mp_optimized_count; mp_index++) {
    PageIndex mp_offset = static_cast<PageIndex>(mp_optimized_offsets[mp_index]);
    mp_dst_offsets[mp_index] = uint16_t(mp_offset);

    if (is_page_special(mp_offset)) {
      continue;
    }

    const MasterPlane* mp_src = get_mp_dense(self_impl, mp_offset);
    uint32_t sp_dense_mask = mp_src->sp_dense_mask;
    uint32_t sp_filled_mask = mp_src->sp_filled_mask;

    BL_ASSERT(page_index < get_page_capacity(&tmp));
    mp_offset = static_cast<PageIndex>(page_index++);
    mp_dst_offsets[mp_index] = uint16_t(mp_offset);

    MasterPlane* mp = get_mp_dense(dst_impl, mp_offset);
    copy_page(mp, mp_src);

    axl::BitWordIterator it(sp_dense_mask);
    while (it.has_next()) {
      size_t sp_index = it.next();
      SubPlaneInfo sp_info = mp_load_sp_info(mp, sp_index);
      BL_ASSERT(sp_info.is_dense());

      if (sp_info.has_special_cardinality()) {
        sp_info.set_page_index(sp_special_offset_from_cardinality(sp_info.special_cardinality()));
        sp_dense_mask &= ~axl::bit_mask<uint32_t>(sp_index);
        sp_filled_mask |= uint32_t(sp_info.has_cardinality()) << sp_index;
      }
      else {
        BL_ASSERT(page_index < get_page_capacity(&tmp));
        PageIndex sp_offset = static_cast<PageIndex>(page_index++);

        const SubPlane* sp_src = get_sp_dense(self_impl, sp_info);
        copy_page(get_sp_dense(dst_impl, sp_offset), sp_src);

        sp_info.set_page_index(sp_offset);
      }

      mp_store_sp_offset(mp, sp_index, sp_info);
    }

    mp->sp_dense_mask = sp_dense_mask;
    mp->sp_filled_mask = sp_filled_mask;
  }

  BL_ASSERT(page_index == get_page_capacity(&tmp));

  set_mp_count(&tmp, mp_optimized_count);
  set_page_count(&tmp, page_index);

  return ObjectLayer::replace_object(self, &tmp);
}

BL_API_IMPL BLResult bl_coverage_set_optimize(BLCoverageSetCore* self) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  // Delegate to shrink if we cannot mutate the existing container.
  if (!ObjectLayer::is_object_mutable(self)) {
    return bl_coverage_set_shrink(self);
  }

  BLCoverageSetImpl* impl = ObjectLayer::get_impl(self);
  uint16_t* mp_offsets = get_mp_offsets(self);

  size_t mp_count = get_mp_count(self);
  size_t mp_optimized_count = 0;

  for (size_t mp_index = 0; mp_index < mp_count; mp_index++) {
    PageIndex mp_offset = static_cast<PageIndex>(mp_offsets[mp_index]);
    if (is_page_special(mp_offset)) {
      if (is_page_full(mp_offset)) {
        mp_optimized_count = mp_index + 1u;
      }
      continue;
    }

    MasterPlane* mp = get_mp_dense(impl, mp_offset);
    uint32_t mp_cardinality = mp->mp_cardinality;
    uint32_t sp_dense_mask = mp->sp_dense_mask;
    uint32_t sp_filled_mask = mp->sp_filled_mask;

    if (mp_is_cardinality_empty_or_full(mp_cardinality)) {
      axl::BitWordIterator<uint32_t> it(sp_dense_mask);
      while (it.has_next()) {
        size_t sp_index = it.next();
        SubPlaneInfo sp_info = mp_load_sp_info(mp, sp_index);

        BL_ASSERT(sp_info.is_dense());
        free_page(self, sp_info.page_index());
      }

      free_page(self, mp_offset);
      mp_offsets[mp_index] = uint16_t(mp_special_offset_from_cardinality(mp_cardinality));

      if (mp_cardinality == 0) {
        continue;
      }
    }
    else {
      axl::BitWordIterator<uint32_t> it(sp_dense_mask);
      while (it.has_next()) {
        size_t sp_index = it.next();
        SubPlaneInfo sp_info = mp_load_sp_info(mp, sp_index);

        BL_ASSERT(sp_info.is_dense());

        if (sp_info.has_special_cardinality()) {
          free_page(self, sp_info.page_index());
          sp_dense_mask &= ~axl::bit_mask<uint32_t>(sp_index);
          sp_filled_mask |= uint32_t(sp_info.has_cardinality()) << sp_index;

          sp_info.set_page_index(sp_special_offset_from_cardinality(sp_info.special_cardinality()));
          mp_store_sp_offset(mp, sp_index, sp_info);
        }
      }

      mp->sp_dense_mask = sp_dense_mask;
      mp->sp_filled_mask = sp_filled_mask;
    }

    mp_optimized_count = mp_index + 1u;
  }

  set_mp_count(self, mp_optimized_count);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_coverage_set_chop(BLCoverageSetCore* self, uint32_t range_start, uint32_t range_end) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  // If the range is invalid or contains nothing, clear the whole container.
  if (BL_UNLIKELY(range_start >= range_end)) {
    return bl_coverage_set_clear(self);
  }

  if (range_start > 0u) {
    BLCoverageSetRange ranges[2] = {{0, range_start}, {range_end, kMaxSize}};
    return bl_coverage_set_remove_ranges(self, ranges, 2u);
  }
  else {
    return bl_coverage_set_remove_range(self, range_end, kMaxSize);
  }
}

BL_API_IMPL BLResult bl_coverage_set_add(BLCoverageSetCore* self, uint32_t index) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  size_t mp_index = mp_index_from_bit_index(index);
  uint16_t* mp_offsets = get_mp_offsets(self);
  BLCoverageSetImpl* impl = ObjectLayer::get_impl(self);

  if (BL_UNLIKELY(index >= kMaxSize)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  bool is_mutable = ObjectLayer::is_impl_mutable(impl);
  size_t sp_index = sp_index_from_bit_index(index);

  MasterPlane* mp;
  SubPlaneInfo sp_info;

  do {
    PageIndex mp_offset = static_cast<PageIndex>(mp_offsets[mp_index]);
    if (BL_LIKELY(!is_page_special(mp_offset))) {
      mp = get_mp_dense(impl, mp_offset);
      sp_info = mp_load_sp_info(mp, sp_index);

      if (BL_LIKELY(axl::bool_and(is_mutable, sp_info.is_dense()))) {
        break;
      }

      if (sp_info.is_full_page()) {
        return BL_SUCCESS;
      }
    }
    else if (is_page_full(mp_offset)) {
      return BL_SUCCESS;
    }

    // The container either doesn't contain the required master-plane/sub-plane or is not mutable.
    BLResultOr<MasterPlane*> wrapped = sp_ensure_page(self, mp_index, sp_index);
    BL_PROPAGATE(wrapped.result());

    impl = ObjectLayer::get_impl(self);
    mp = wrapped.value();
    sp_info = mp_load_sp_info(mp, sp_index);
  } while (false);

  // Machinery to set a single bit and keep cardinality caches synchronized.
  SubPlane* sp = get_sp_dense(impl, sp_info);
  size_t bw_index = bw_index_from_bit_index(index);

  BLBitWord bw_bits = sp->bits[bw_index];
  BLBitWord bw_mask = axl::bit_mask_wrap<BLBitWord>(index);

  uint32_t cardinality_diff = uint32_t((bw_bits & bw_mask) == 0u);
  sp->bits[bw_index] = bw_bits | bw_mask;
  sp_info.card_lo += cardinality_diff;

  mp->mp_cardinality += cardinality_diff;
  mp->sp_card_lo[sp_index] = uint8_t(sp_info.card_lo & 0xFFu);

  // PERFORMANCE: Don't touch `sp_page_hi` if not necessary - this improves performance a lot.
  if (sp_info.card_lo <= 0xFFu) {
    return BL_SUCCESS;
  }
  else {
    mp->sp_page_hi[sp_index] = uint16_t((sp_info.page_hi + 1u) & 0xFFFFu);
    return BL_SUCCESS;
  }
}

BL_API_IMPL BLResult bl_coverage_set_add_range(BLCoverageSetCore* self, uint32_t range_start, uint32_t range_end) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return range_op<UnicodeCoverageOp::kFill>(self, range_start, range_end);
}

BL_API_IMPL BLResult bl_coverage_set_add_ranges(BLCoverageSetCore* self, const BLCoverageSetRange* range_data, size_t range_count) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ranges_op<UnicodeCoverageOp::kFill>(self, range_data, range_count);
}

BL_API_IMPL BLResult bl_coverage_set_remove(BLCoverageSetCore* self, uint32_t index) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  size_t mp_index = mp_index_from_bit_index(index);
  uint16_t* mp_offsets = get_mp_offsets(self);
  BLCoverageSetImpl* impl = ObjectLayer::get_impl(self);

  if (BL_UNLIKELY(index >= kMaxSize)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  bool is_mutable = ObjectLayer::is_impl_mutable(impl);
  size_t sp_index = sp_index_from_bit_index(index);

  MasterPlane* mp;
  SubPlaneInfo sp_info;

  do {
    PageIndex mp_offset = static_cast<PageIndex>(mp_offsets[mp_index]);
    if (BL_LIKELY(!is_page_special(mp_offset))) {
      mp = get_mp_dense(impl, mp_offset);
      sp_info = mp_load_sp_info(mp, sp_index);
      if (BL_LIKELY(axl::bool_and(is_mutable, sp_info.is_dense()))) {
        break;
      }

      if (sp_info.is_zero_page()) {
        return BL_SUCCESS;
      }
    }
    else if (is_page_zero(mp_offset)) {
      return BL_SUCCESS;
    }

    // The container either doesn't contain the required master-plane/sub-plane or is not mutable.
    BLResultOr<MasterPlane*> wrapped = sp_ensure_page(self, mp_index, sp_index);
    BL_PROPAGATE(wrapped.result());

    impl = ObjectLayer::get_impl(self);
    mp = wrapped.value();
    sp_info = mp_load_sp_info(mp, sp_index);
  } while (false);

  // Machinery to clear a single bit and keep cardinality caches synchronized.
  SubPlane* sp = get_sp_dense(impl, sp_info);
  size_t bw_index = bw_index_from_bit_index(index);

  BLBitWord bw_bits = sp->bits[bw_index];
  BLBitWord bw_mask = axl::bit_mask_wrap<BLBitWord>(index);

  uint32_t cardinality_diff = uint32_t((bw_bits & bw_mask) != 0u);
  sp->bits[bw_index] = bw_bits & ~bw_mask;
  sp_info.card_lo -= cardinality_diff;

  mp->mp_cardinality -= cardinality_diff;
  mp->sp_card_lo[sp_index] = uint8_t(sp_info.card_lo & 0xFFu);

  // PERFORMANCE: Don't touch `sp_page_hi` if not necessary - this improves performance a lot.
  if (sp_info.card_lo <= 0xFFu) {
    return BL_SUCCESS;
  }
  else {
    mp->sp_page_hi[sp_index] = uint16_t((sp_info.page_hi - 1u) & 0xFFFFu);
    return BL_SUCCESS;
  }
}

BL_API_IMPL BLResult bl_coverage_set_remove_range(BLCoverageSetCore* self, uint32_t range_start, uint32_t range_end) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return range_op<UnicodeCoverageOp::kZero>(self, range_start, range_end);
}

BL_API_IMPL BLResult bl_coverage_set_remove_ranges(BLCoverageSetCore* self, const BLCoverageSetRange* range_data, size_t range_count) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ranges_op<UnicodeCoverageOp::kZero>(self, range_data, range_count);
}

BL_API_IMPL BLResult bl_coverage_set_combine(BLCoverageSetCore* dst, const BLCoverageSetCore* a, const BLCoverageSetCore* b, BLBooleanOp op) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(dst));
  BL_ASSERT(ObjectLayer::is_object_valid(a));
  BL_ASSERT(ObjectLayer::is_object_valid(b));

  BLCoverageSetImpl* a_impl = a->_impl();
  BLCoverageSetImpl* b_impl = b->_impl();

  switch (op) {
    case BL_BOOLEAN_OP_COPY:
      // TODO: Maybe not the best semantics?
      return dst == b ? BL_SUCCESS : bl_coverage_set_assign_weak(dst, b);

    case BL_BOOLEAN_OP_AND:
      if (a_impl == b_impl) {
        return dst == a ? BL_SUCCESS : bl_coverage_set_assign_weak(dst, a);
      }

      if (dst == b) {
        axl::swap(a, b);
      }

      return combine_op<BitOperator::And>(dst, a, b);

    case BL_BOOLEAN_OP_OR:
      if (a_impl == b_impl) {
        return dst == a ? BL_SUCCESS : bl_coverage_set_assign_weak(dst, a);
      }

      if (dst == b) {
        axl::swap(a, b);
      }

      return combine_op<BitOperator::Or>(dst, a, b);

    case BL_BOOLEAN_OP_XOR:
      if (a_impl == b_impl) {
        return bl_coverage_set_clear(dst);
      }

      if (dst == b) {
        axl::swap(a, b);
      }

      return combine_op<BitOperator::Xor>(dst, a, b);

    case BL_BOOLEAN_OP_NOT_AND:
      axl::swap(a, b);
      [[fallthrough]];

    case BL_BOOLEAN_OP_AND_NOT:
      if (a_impl == b_impl) {
        return bl_coverage_set_clear(dst);
      }

      if (dst == b) {
        return combine_op<BitOperator::NotAnd>(dst, b, a);
      }
      else {
        return combine_op<BitOperator::AndNot>(dst, a, b);
      }

    default:
      return bl_make_error(BL_ERROR_INVALID_VALUE);
  }
}

// CoverageSet - Runtime Registration
// ==================================

void bl_coverage_set_rt_init([[maybe_unused]] BLRuntimeContext* rt) noexcept {
  BL_USING_DETAIL(bl::CoverageSetDetail);
  static constexpr BLObjectType object_type = BLCoverageSet::Info::kType;

  BLCoverageSetImpl* impl = const_cast<BLCoverageSetImpl*>(&null_coverage_set.impl);
  ObjectLayer::init_dyn(&bl_object_defaults[object_type], object_type, BLObjectDetail::kMetaFlagD, impl, nullptr);

  Layout& layout = bl_object_defaults[object_type]._d.layout<Layout>();
  layout.page_free_list = kUnassignedPage;
}
