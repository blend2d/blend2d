// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/axl/bitops_p.h>
#include <blend2d/axl/commons.h>
#include <blend2d/axl/fixedbit_array_p.h>
#include <blend2d/core/coverage_set_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/random.h>
#include <blend2d/utils/popcnt_p.h>

// CoverageSet - Tests
// ===================

namespace bl::UnicodeCoverageTests {

// A dense reference model using a dense bit-set that the sparse container is verified against.
class CoverageReference {
  BL_NONCOPYABLE(CoverageReference)

public:
  using BitArray = FixedBitArray<BLBitWord, CoverageSetDetail::kMaxSize>;
  using BitOps = ParametrizedBitOps<BitOrder::kLSB, BLBitWord>;

  BitArray* _bits {};

  CoverageReference() noexcept { _bits = axl::mem_alloc_zeroed<BitArray>(sizeof(BitArray)); }
  ~CoverageReference() noexcept { axl::mem_free(_bits); }

  void set(uint32_t cp) noexcept { _bits->set_at(cp); }
  void clear(uint32_t cp) noexcept { _bits->clear_at(cp); }
  bool contains(uint32_t cp) const noexcept { return _bits->bit_at(cp); }

  void set_range(uint32_t start, uint32_t end) noexcept {
    end = axl::min(end, CoverageSetDetail::kMaxSize);
    if (start >= end) {
      return;
    }
    BitOps::bit_array_fill(_bits->data, start, end - start);
  }

  void remove_range(uint32_t start, uint32_t end) noexcept {
    end = axl::min(end, CoverageSetDetail::kMaxSize);
    if (start >= end) {
      return;
    }
    BitOps::bit_array_clear(_bits->data, start, end - start);
  }

  uint32_t cardinality() const noexcept {
    Utils::PopCounter<BLBitWord> counter;
    counter.add_array(_bits->data, BitArray::kFixedArraySize);
    return static_cast<uint32_t>(counter.get());
  }

  uint32_t cardinality_in_range(uint32_t s, uint32_t e) const noexcept {
    e = axl::min(e, CoverageSetDetail::kMaxSize);
    uint32_t count = 0u;
    for (uint32_t cp = s; cp < e; cp++) {
      count += uint32_t(contains(cp));
    }
    return count;
  }
};

static void verify_cardinality_in_range(const BLCoverageSet& cov, const CoverageReference& ref, uint32_t start, uint32_t end) noexcept {
  uint32_t expected = ref.cardinality_in_range(start, end);
  EXPECT_EQ(cov.cardinality_in_range(start, end), expected)
    .message("Cardinality mismatch in [U+%06X, U+%06X)", start, end);
  EXPECT_EQ(cov.contains_in_range(start, end), expected != 0u)
    .message("Contains mismatch in [U+%06X, U+%06X)", start, end);
}

// Verifies every code-point of `cov` against `ref` - the strong (exhaustive) check.
static void verify_full(const BLCoverageSet& cov, const CoverageReference& ref) noexcept {
  uint32_t mismatch_count = 0;
  uint32_t mismatch_first = 0;

  for (uint32_t cp = 0; cp < CoverageSetDetail::kMaxSize; cp++) {
    if (cov.contains(cp) != ref.contains(cp)) {
      if (++mismatch_count == 1u) {
        mismatch_first = cp;
      }
    }
  }

  EXPECT_EQ(mismatch_count, 0u)
    .message("Coverage mismatch: %u code-points differ (first at U+%06X, cov=%u ref=%u)",
             mismatch_count, mismatch_first,
             int(cov.contains(mismatch_first)),
             int(ref.contains(mismatch_first)));

  EXPECT_EQ(cov.cardinality(), ref.cardinality())
    .message("Cardinality mismatch: cov=%u ref=%u", cov.cardinality(), ref.cardinality());
}

static void verify_master_plane_metadata(const BLCoverageSet& cov, uint32_t mp_index) noexcept {
  using namespace CoverageSetDetail;

  const BLCoverageSetImpl* impl = cov._d.layout<BLCoverageSet::Layout>().impl.ptr;
  uint32_t mp_offset = impl->mp_offsets[mp_index];
  EXPECT_LT(mp_offset, kPageSpecial);

  const MasterPlane* mp = reinterpret_cast<const MasterPlane*>(
    reinterpret_cast<const uint8_t*>(impl) + sizeof(*impl) + mp_offset * kPageSize);
  uint32_t cardinality = 0u;

  EXPECT_EQ(mp->sp_dense_mask & mp->sp_filled_mask, 0u);
  for (uint32_t sp_index = 0; sp_index < kSPPerMP; sp_index++) {
    uint32_t sp_offset = mp->sp_page_hi[sp_index] >> 4;
    EXPECT_EQ((mp->sp_dense_mask >> sp_index) & 1u, uint32_t(sp_offset < kPageSpecial));
    EXPECT_EQ((mp->sp_filled_mask >> sp_index) & 1u, uint32_t(sp_offset == kPageFull));
    cardinality += mp->sp_card_lo[sp_index] + ((mp->sp_page_hi[sp_index] & 0xFu) << 8);
  }
  EXPECT_EQ(mp->mp_cardinality, cardinality);
}

TEST_CASE(core_coverage_set_empty) {
  BLCoverageSet cov1;
  BLCoverageSet cov2;

  EXPECT_TRUE(cov1.is_empty());
  EXPECT_TRUE(cov1.equals(cov2));
  EXPECT_EQ(cov1.cardinality(), 0u);
  EXPECT_FALSE(cov1.contains(0u));
  EXPECT_FALSE(cov1.contains(0x41u));
  EXPECT_FALSE(cov1.contains(CoverageSetDetail::kMaxSize - 1u));
  // Invalid code point must return false.
  EXPECT_FALSE(cov1.contains(0xFFFFFFFFu));

  uint32_t start = 123, end = 456;
  EXPECT_FALSE(cov1.get_range(&start, &end));
  EXPECT_EQ(start, 0u);
  EXPECT_EQ(end, 0u);
}

TEST_CASE(core_coverage_set_full) {
  BLCoverageSet cov;

  EXPECT_OK(cov.add_range(0u, CoverageSetDetail::kMaxSize));
  EXPECT_EQ(cov.cardinality(), CoverageSetDetail::kMaxSize);
  EXPECT_FALSE(cov.is_empty());
}

TEST_CASE(core_coverage_set_single_ops) {
  BLCoverageSet cov1;
  BLCoverageSet cov2;
  CoverageReference ref;

  static const uint32_t code_points[] = {
    0x00u, 0x41u, 0x7Fu, 0x1FFu, 0x200u, 0x201u,   // Sub-group boundaries around 512.
    0x7FFFu, 0x8000u, 0x8001u,                     // Half-plane boundary at 32768.
    0x4E00u, 0x9FFFu,                              // CJK code-points.
    0x10000u, 0xFFFFu,                             // Supplementary code-points.
    0x10FFFFu                                      // Last code-point in Unicode.
  };

  uint32_t cardinality = 0;
  for (uint32_t cp : code_points) {
    EXPECT_OK(cov1.add(cp));
    EXPECT_OK(cov2.add(cp));

    ref.set(cp);
    cardinality++;

    EXPECT_FALSE(cov1.is_empty());
    EXPECT_FALSE(cov2.is_empty());
    EXPECT_TRUE(cov1.contains(cp));
    EXPECT_TRUE(cov2.contains(cp));

    EXPECT_EQ(cov1.cardinality(), cardinality);
    EXPECT_EQ(cov2.cardinality(), cardinality);

    verify_full(cov1, ref);
    verify_full(cov2, ref);
    EXPECT_TRUE(cov1.equals(cov2));
  }

  // Re-adding code-points does nothing.
  for (uint32_t cp : code_points) {
    EXPECT_OK(cov1.add(cp));
    EXPECT_EQ(cov1.cardinality(), cardinality);
  }

  verify_full(cov1, ref);
  EXPECT_TRUE(cov1.equals(cov2));

  // get_range reflects [min, max+1).
  uint32_t start = 0, end = 0;
  EXPECT_TRUE(cov1.get_range(&start, &end));
  EXPECT_EQ(start, 0x000000u);
  EXPECT_EQ(end, 0x110000u);

  // Clear each point.
  for (uint32_t cp : code_points) {
    EXPECT_OK(cov1.remove(cp));

    ref.clear(cp);
    cardinality--;

    EXPECT_FALSE(cov1.contains(cp));
    EXPECT_EQ(cov1.cardinality(), cardinality);
  }

  verify_full(cov1, ref);
  EXPECT_TRUE(cov1.is_empty());
  EXPECT_FALSE(cov1.equals(cov2));

  for (uint32_t cp : code_points) {
    EXPECT_OK(cov2.remove(cp));
    EXPECT_FALSE(cov2.contains(cp));
  }

  EXPECT_TRUE(cov1.equals(cov2));
  EXPECT_EQ(cov2.cardinality(), cardinality);

  // Adding invalid code-point is reported as error.
  EXPECT_EQ(cov1.add(CoverageSetDetail::kMaxSize), BL_ERROR_INVALID_VALUE);
  EXPECT_EQ(cov1.add(0xFFFFFFFFu), BL_ERROR_INVALID_VALUE);
  EXPECT_EQ(cov1.cardinality(), 0u);
  EXPECT_TRUE(cov1.is_empty());
}

TEST_CASE(core_coverage_set_copy_and_equals) {
  BLCoverageSet a;
  CoverageReference ref;

  EXPECT_OK(a.add_range(0x0041u, 0x005Bu));
  EXPECT_OK(a.add_range(0x4E00u, 0x5000u));
  EXPECT_OK(a.add(0x10FFFFu));

  ref.set_range(0x0041u, 0x005Bu);
  ref.set_range(0x4E00u, 0x5000u);
  ref.set(0x10FFFFu);

  // Weak copy shares data and compares equal.
  BLCoverageSet b(a);
  EXPECT_TRUE(a.equals(b));
  EXPECT_TRUE(b.equals(a));
  verify_full(b, ref);

  // Deep copy is independent - mutating the copy must not touch the original.
  BLCoverageSet c;
  EXPECT_OK(c.assign_deep(a));
  EXPECT_TRUE(c.equals(a));

  EXPECT_OK(c.add(0x30u));
  EXPECT_FALSE(c.equals(a));
  EXPECT_FALSE(a.contains(0x30u)); // Original untouched.
  verify_full(a, ref);

  // Mutating a weak copy must copy-on-write and not disturb the source.
  EXPECT_OK(b.add(0x31u));
  EXPECT_FALSE(a.contains(0x31u));
  verify_full(a, ref);

  // Move leaves the source empty.
  BLCoverageSet d;
  EXPECT_OK(d.assign(axl::move(a)));
  EXPECT_TRUE(a.is_empty());
  verify_full(d, ref);

  // Full and empty implicit planes have identical low cardinality bytes.
  BLCoverageSet whole;
  BLCoverageSet upper;
  EXPECT_OK(whole.add_range(0u, 2u * CoverageSetDetail::kMPBitCount));
  EXPECT_OK(upper.add_range(CoverageSetDetail::kMPBitCount, 2u * CoverageSetDetail::kMPBitCount));
  EXPECT_FALSE(whole.equals(upper));
  EXPECT_FALSE(upper.equals(whole));
}

template<bool Remove>
static void test_coverage_set_modify_ranges_boundaries() {
  constexpr uint32_t kWordBits = axl::bit_size_of<BLBitWord>;
  static const BLCoverageSetRange ranges[] = {
    { 0u, 1u },
    { 1u, kWordBits },
    { kWordBits - 1u, kWordBits + 1u },
    { kWordBits, kWordBits * 3u },
    { 0x3FFu, 0x401u },
    { 0x400u, 0x800u },
    { 0x801u, 0xC00u },
    { 0x1000u, 0x1234u },
    { 0x7FFFu, 0x8001u },
    { 0x8000u, 0x10000u },
    { 0x37FFFu, 0x38001u },
    { 0x40000u, 0x48000u },
    { 0x10FFFFu, 0x110000u },
    { 0x10FFF0u, 0xFFFFFFFFu },
    { 0u, 0u },
    { 100u, 10u },
    { 0x110000u, 0xFFFFFFFFu },
    { 0xFFFFFFFFu, 0xFFFFFFFFu }
  };

  // Verify each single-range call independently, including exclusive word and page boundaries.
  for (const BLCoverageSetRange& range : ranges) {
    BLCoverageSet cov;
    CoverageReference ref;
    if constexpr (Remove) {
      EXPECT_OK(cov.add_range(0u, CoverageSetDetail::kMaxSize));
      ref.set_range(0u, CoverageSetDetail::kMaxSize);
      EXPECT_OK(cov.remove_range(range));
      ref.remove_range(range.start, range.end);
    }
    else {
      EXPECT_OK(cov.add_range(range));
      ref.set_range(range.start, range.end);
    }
    verify_full(cov, ref);
  }

  BLCoverageSet cov;
  CoverageReference ref;
  EXPECT_OK(Remove ? cov.remove_ranges(nullptr, 0u) : cov.add_ranges(nullptr, 0u));
  EXPECT_EQ(cov.page_count(), 0u);

  if constexpr (Remove) {
    EXPECT_OK(cov.add_range(0u, CoverageSetDetail::kMaxSize));
    ref.set_range(0u, CoverageSetDetail::kMaxSize);
  }
  EXPECT_OK(Remove ? cov.remove_ranges(ranges, BL_ARRAY_SIZE(ranges)) : cov.add_ranges(ranges, BL_ARRAY_SIZE(ranges)));
  for (const BLCoverageSetRange& range : ranges) {
    if constexpr (Remove) {
      ref.remove_range(range.start, range.end);
    }
    else {
      ref.set_range(range.start, range.end);
    }
  }
  verify_full(cov, ref);

  // Repeat the batch in reverse: ordering, overlaps, and duplicates must not change the result.
  BLCoverageSetRange reversed[BL_ARRAY_SIZE(ranges)];
  for (size_t i = 0; i < BL_ARRAY_SIZE(ranges); i++) {
    reversed[i] = ranges[BL_ARRAY_SIZE(ranges) - 1u - i];
  }
  EXPECT_OK(Remove ? cov.remove_ranges(reversed, BL_ARRAY_SIZE(reversed)) : cov.add_ranges(reversed, BL_ARRAY_SIZE(reversed)));
  verify_full(cov, ref);
}

TEST_CASE(core_coverage_set_ranges_out_of_bounds) {
  BLCoverageSet cov;

  // Ranges that overflow the limit are clamped - then processed.
  EXPECT_OK(cov.add_range(0x10FFF0u, 0x200000u));

  EXPECT_EQ(cov.cardinality(), CoverageSetDetail::kMaxSize - 0x10FFF0u);
  EXPECT_TRUE(cov.contains(0x10FFFFu));

  // Invalid ranges are clamped and declined if they are empty.
  EXPECT_OK(cov.add_range(0x20FFF0u, 0x200000u));
  EXPECT_FALSE(cov.contains(CoverageSetDetail::kMaxSize));
}

TEST_CASE(core_coverage_set_ranges_add_at_boundaries) {
  test_coverage_set_modify_ranges_boundaries<false>();
}

TEST_CASE(core_coverage_set_ranges_remove_at_boundaries) {
  test_coverage_set_modify_ranges_boundaries<true>();
}

TEST_CASE(core_coverage_set_ranges_add_in_place) {
  static const BLCoverageSetRange ranges[] = {
    { 1u, 63u },                // Existing dense sub-plane.
    { 0x401u, 0x7FFu },         // Both endpoints need the same dense sub-plane.
    { 0x400u, 0x401u },         // Only the end is partial.
    { 0x7FFu, 0x800u },         // Only the start is partial.
    { 0x7FFu, 0xC01u },         // Two new dense sub-planes and a whole sub-plane.
    { 0u, 0x800u },             // Replace dense and empty sub-planes.
    { 0x7FFFu, 0x8001u },       // Cross a master-plane boundary.
    { 0x401u, 0x10401u },       // Replace an entire intermediate master-plane.
    { 0x37FFFu, 0x38001u },     // Cross a supplementary master-plane boundary.
    { 0u, CoverageSetDetail::kMaxSize },
    { 0x10FFF0u, 0xFFFFFFFFu }  // Clamp the final partial sub-plane.
  };

  for (const BLCoverageSetRange& range : ranges) {
    BLCoverageSet cov;
    CoverageReference ref;
    for (uint32_t cp = 0u; cp < CoverageSetDetail::kMaxSize; cp += CoverageSetDetail::kMPBitCount) {
      EXPECT_OK(cov.add(cp));
      ref.set(cp);
    }

    BLCoverageSet shared(cov);
    EXPECT_OK(shared.add_range(range));
    verify_full(cov, ref);

    BLCoverageSetImpl* impl = cov._d.layout<BLCoverageSet::Layout>().impl.ptr;
    EXPECT_OK(cov.add_range(range));
    EXPECT_EQ(cov._d.layout<BLCoverageSet::Layout>().impl.ptr, impl);
    ref.set_range(range.start, range.end);
    verify_full(cov, ref);
    EXPECT_TRUE(cov.equals(shared));

    // Repeating the range also exercises already filled master-planes and sub-planes.
    EXPECT_OK(cov.add_range(range));
    EXPECT_EQ(cov._d.layout<BLCoverageSet::Layout>().impl.ptr, impl);
    verify_full(cov, ref);
  }

  BLCoverageSet cov;
  CoverageReference ref;
  EXPECT_OK(cov.add(0u));
  ref.set(0u);

  // Leave exactly one free page to detect counting the same partial sub-plane twice.
  uint32_t cp = CoverageSetDetail::kSPBitCount;
  while (cov.page_count() + 1u < cov.page_capacity()) {
    EXPECT_OK(cov.add(cp));
    ref.set(cp);
    cp += CoverageSetDetail::kSPBitCount;
  }

  BLCoverageSetImpl* impl = cov._d.layout<BLCoverageSet::Layout>().impl.ptr;
  EXPECT_OK(cov.add_range(cp + 1u, cp + 10u));
  EXPECT_EQ(cov._d.layout<BLCoverageSet::Layout>().impl.ptr, impl);
  EXPECT_EQ(cov.page_count(), cov.page_capacity());
  ref.set_range(cp + 1u, cp + 10u);
  verify_full(cov, ref);

  // A second new partial sub-plane must fall back to growing the allocation.
  cp += CoverageSetDetail::kSPBitCount;
  EXPECT_OK(cov.add_range(cp + 1u, cp + 10u));
  EXPECT_NE(cov._d.layout<BLCoverageSet::Layout>().impl.ptr, impl);
  ref.set_range(cp + 1u, cp + 10u);
  verify_full(cov, ref);
}

TEST_CASE(core_coverage_set_ranges_at_boundaries) {
  using namespace CoverageSetDetail;

  static constexpr uint32_t endpoints[] = {
    0u, 1u, 63u, 64u, 65u, 0x3FFu, 0x400u, 0x401u, 0x800u, 0x801u, 0xC00u, 0x7FFFu, 0x8000u
  };

  BLCoverageSet seed;
  CoverageReference seed_ref;
  for (uint32_t sp_index = 0; sp_index < kSPPerMP; sp_index++) {
    uint32_t start = sp_index * kSPBitCount;
    switch (sp_index % 4u) {
      case 0:
        EXPECT_OK(seed.add_range(start + 63u, start + 65u));
        seed_ref.set_range(start + 63u, start + 65u);
        break;
      case 1:
        EXPECT_OK(seed.add_range(start, start + kSPBitCount));
        seed_ref.set_range(start, start + kSPBitCount);
        break;
      case 2:
        break;
      case 3:
        // An allocated bitmap can also be full, so it must still be released when replaced in full.
        EXPECT_OK(seed.add_range(start + 1u, start + kSPBitCount));
        EXPECT_OK(seed.add(start));
        seed_ref.set_range(start, start + kSPBitCount);
        break;
    }
  }

  BLCoverageSet cov;
  CoverageReference ref;
  // Retain enough capacity to exercise in-place endpoint materialization as well as whole-plane replacement.
  for (uint32_t sp_index = 0; sp_index < kSPPerMP; sp_index++) {
    EXPECT_OK(cov.add(sp_index * kSPBitCount));
  }

  for (uint32_t remove = 0; remove < 2u; remove++) {
    for (uint32_t start : endpoints) {
      for (uint32_t end : endpoints) {
        if (start >= end) {
          continue;
        }

        EXPECT_OK(cov.assign_deep(seed));
        memcpy(ref._bits, seed_ref._bits, sizeof(CoverageReference::BitArray));
        BLCoverageSetImpl* impl = cov._d.layout<BLCoverageSet::Layout>().impl.ptr;

        EXPECT_OK(remove ? cov.remove_range(start, end) : cov.add_range(start, end));
        EXPECT_EQ(cov._d.layout<BLCoverageSet::Layout>().impl.ptr, impl);

        if (remove)
          ref.remove_range(start, end);
        else
          ref.set_range(start, end);

        verify_full(cov, ref);
        verify_master_plane_metadata(cov, 0u);

        // The copy-on-write fallback must produce the same result without modifying the shared source.
        BLCoverageSet shared(seed);
        EXPECT_OK(remove ? shared.remove_range(start, end) : shared.add_range(start, end));
        EXPECT_TRUE(shared.equals(cov));
      }
    }
  }

  verify_full(seed, seed_ref);
}

TEST_CASE(core_coverage_set_ranges_metadata) {
  static constexpr BLCoverageSetRange ranges[] = {
    { 1u, 0xC01u },      // Existing opening bitmap, two interior planes, and a closing fragment.
    { 0x401u, 0x1001u }  // Both endpoint planes may require materialization.
  };

  for (uint32_t remove = 0; remove < 2u; remove++) {
    for (uint32_t dense_last = 0; dense_last < 2u; dense_last++) {
      for (const BLCoverageSetRange& range : ranges) {
        BLCoverageSet cov;
        CoverageReference ref;

        if (remove) {
          EXPECT_OK(cov.add_range(0u, CoverageSetDetail::kMPBitCount));
          ref.set_range(0u, CoverageSetDetail::kMPBitCount);
          EXPECT_OK(cov.remove(0u));
          ref.clear(0u);
        }
        else {
          EXPECT_OK(cov.add(0u));
          ref.set(0u);
        }

        if (dense_last) {
          uint32_t cp = range.end - 1u;
          EXPECT_OK(remove ? cov.remove(cp) : cov.add(cp));
          if (remove)
            ref.clear(cp);
          else
            ref.set(cp);
        }

        BLCoverageSetImpl* impl = cov._d.layout<BLCoverageSet::Layout>().impl.ptr;
        for (uint32_t repeat = 0; repeat < 2u; repeat++) {
          EXPECT_OK(remove ? cov.remove_range(range) : cov.add_range(range));
          EXPECT_EQ(cov._d.layout<BLCoverageSet::Layout>().impl.ptr, impl);
          if (remove)
            ref.remove_range(range.start, range.end);
          else
            ref.set_range(range.start, range.end);

          verify_full(cov, ref);
          verify_master_plane_metadata(cov, 0u);

          BLCoverageSet copy;
          EXPECT_OK(copy.assign_deep(cov));
          verify_full(copy, ref);
        }
      }
    }
  }
}

TEST_CASE(core_coverage_set_ranges_reuse_dense_endpoint) {
  using namespace CoverageSetDetail;

  static constexpr uint32_t plane_starts[] = {0u, 0x38000u, kMaxSize - kMPBitCount};

  for (uint32_t plane_start : plane_starts) {
    uint32_t plane_end = plane_start + kMPBitCount;
    for (uint32_t remove = 0; remove < 2u; remove++) {
      for (uint32_t shrink = 0; shrink < 2u; shrink++) {
        BLCoverageSet cov;
        CoverageReference ref;
        if (remove) {
          EXPECT_OK(cov.add_range(plane_start, plane_end));
          ref.set_range(plane_start, plane_end);
          EXPECT_OK(cov.remove(plane_end - 3u));
          ref.clear(plane_end - 3u);
        }
        else {
          EXPECT_OK(cov.add(plane_end - 3u));
          ref.set(plane_end - 3u);
        }

        if (shrink) {
          EXPECT_OK(cov.shrink());
          EXPECT_EQ(cov.page_capacity(), 2u);
        }
        BLCoverageSetImpl* impl = cov._d.layout<BLCoverageSet::Layout>().impl.ptr;

        // Reuse the endpoint bitmap, preserve its preceding bit, and keep the exclusive plane end.
        for (uint32_t repeat = 0; repeat < 2u; repeat++) {
          EXPECT_OK(remove ? cov.remove_range(plane_end - 2u, plane_end) : cov.add_range(plane_end - 2u, plane_end));
          EXPECT_EQ(cov._d.layout<BLCoverageSet::Layout>().impl.ptr, impl);
          EXPECT_EQ(cov.page_count(), 2u);
          if (remove)
            ref.remove_range(plane_end - 2u, plane_end);
          else
            ref.set_range(plane_end - 2u, plane_end);

          verify_full(cov, ref);
          verify_master_plane_metadata(cov, plane_start / kMPBitCount);
        }

        // A fully covered opening bitmap must be released just like an interior bitmap.
        EXPECT_OK(remove ? cov.remove_range(plane_end - kSPBitCount, plane_end) : cov.add_range(plane_end - kSPBitCount, plane_end));
        EXPECT_EQ(cov._d.layout<BLCoverageSet::Layout>().impl.ptr, impl);
        EXPECT_EQ(cov.page_count(), 1u);
        if (remove)
          ref.remove_range(plane_end - kSPBitCount, plane_end);
        else
          ref.set_range(plane_end - kSPBitCount, plane_end);

        verify_full(cov, ref);
        verify_master_plane_metadata(cov, plane_start / kMPBitCount);
      }
    }
  }
}

TEST_CASE(core_coverage_set_ranges_nop_capacity) {
  using namespace CoverageSetDetail;

  for (uint32_t remove = 0; remove < 2u; remove++) {
    BLCoverageSet cov;
    if (remove) {
      EXPECT_OK(cov.add(0u));
    }
    else {
      EXPECT_OK(cov.add_range(0u, kMPBitCount));
      EXPECT_OK(cov.remove(0u));
    }

    EXPECT_OK(cov.shrink());
    EXPECT_EQ(cov.page_count(), 2u);
    EXPECT_EQ(cov.page_capacity(), 2u);
    BLCoverageSetImpl* impl = cov._d.layout<BLCoverageSet::Layout>().impl.ptr;

    BLCoverageSetRange range{0x401u, 0x402u};
    EXPECT_OK(remove ? cov.remove_ranges(&range, 1u) : cov.add_ranges(&range, 1u));
    EXPECT_EQ(cov._d.layout<BLCoverageSet::Layout>().impl.ptr, impl);
    EXPECT_EQ(cov.page_count(), 2u);
    EXPECT_EQ(cov.page_capacity(), 2u);
    EXPECT_EQ(cov.cardinality(), remove ? 1u : kMPBitCount - 1u);
    verify_master_plane_metadata(cov, 0u);
  }
}

TEST_CASE(core_coverage_set_ranges_add_storage) {
  BLCoverageSet cov;
  CoverageReference ref;

  EXPECT_OK(cov.add_range(1u, 2u));
  ref.set(1u);
  EXPECT_EQ(cov.page_count(), 2u);

  // An implicit supplementary master-plane doesn't require another page.
  size_t capacity = cov.page_capacity();
  EXPECT_OK(cov.add_range(0x38000u, 0x40000u));
  ref.set_range(0x38000u, 0x40000u);
  EXPECT_EQ(cov.page_capacity(), capacity);
  EXPECT_EQ(cov.page_count(), 2u);
  verify_full(cov, ref);

  BLCoverageSet shared(cov);

  // Copy-on-write must preserve the original coverage when replacing the remaining dense master-plane.
  EXPECT_OK(cov.add_range(0u, 0x8000u));
  verify_full(shared, ref);
  ref.set_range(0u, 0x8000u);
  EXPECT_EQ(cov.page_count(), 0u);

  // Reuse the released master-plane and bitmap, then allocate beyond the recycled pages.
  EXPECT_OK(cov.add_range(0x8001u, 0x8002u));
  ref.set(0x8001u);
  EXPECT_EQ(cov.page_count(), 2u);
  EXPECT_OK(cov.add_range(0x8401u, 0x8402u));
  ref.set(0x8401u);
  EXPECT_EQ(cov.page_count(), 3u);

  // Replacing one bitmap by a filled sub-plane releases exactly one page.
  EXPECT_OK(cov.add_range(0x8000u, 0x8400u));
  ref.set_range(0x8000u, 0x8400u);
  EXPECT_EQ(cov.page_count(), 2u);
  verify_full(cov, ref);

  // A batch can fill an entire master-plane through separate sub-plane ranges.
  BLCoverageSetRange ranges[CoverageSetDetail::kSPPerMP];
  for (uint32_t i = 0; i < CoverageSetDetail::kSPPerMP; i++) {
    uint32_t start = 0x8000u + (CoverageSetDetail::kSPPerMP - 1u - i) * CoverageSetDetail::kSPBitCount;
    ranges[i] = { start, start + CoverageSetDetail::kSPBitCount };
  }
  EXPECT_OK(cov.add_ranges(ranges, BL_ARRAY_SIZE(ranges)));
  ref.set_range(0x8000u, 0x10000u);
  EXPECT_EQ(cov.page_count(), 0u);
  verify_full(cov, ref);

  EXPECT_OK(cov.add_range(0u, CoverageSetDetail::kMaxSize));
  ref.set_range(0u, CoverageSetDetail::kMaxSize);
  EXPECT_EQ(cov.page_count(), 0u);
  verify_full(cov, ref);
}

TEST_CASE(core_coverage_set_ranges_add_max_capacity) {
  BLCoverageSet cov;
  CoverageReference ref;
  BLCoverageSetRange ranges[CoverageSetDetail::kMPCount * CoverageSetDetail::kSPPerMP];

  // One bit in every sub-plane requires the maximum number of pages.
  for (uint32_t i = 0; i < BL_ARRAY_SIZE(ranges); i++) {
    uint32_t cp = i * CoverageSetDetail::kSPBitCount;
    ranges[i] = { cp, cp + 1u };
    ref.set(cp);
  }
  EXPECT_OK(cov.add_ranges(ranges, BL_ARRAY_SIZE(ranges)));
  EXPECT_EQ(cov.page_count(), CoverageSetDetail::kMaxPageCapacity);
  EXPECT_EQ(cov.page_capacity(), CoverageSetDetail::kMaxPageCapacity);
  verify_full(cov, ref);

  // Detach and update existing bitmaps without exceeding the capacity limit.
  BLCoverageSet shared(cov);
  for (BLCoverageSetRange& range : ranges) {
    range.end++;
  }
  EXPECT_OK(cov.add_ranges(ranges, BL_ARRAY_SIZE(ranges)));
  verify_full(shared, ref);
  for (const BLCoverageSetRange& range : ranges) {
    ref.set_range(range.start, range.end);
  }
  verify_full(cov, ref);

  EXPECT_OK(cov.add_range(0u, CoverageSetDetail::kMaxSize));
  ref.set_range(0u, CoverageSetDetail::kMaxSize);
  EXPECT_EQ(cov.page_count(), 0u);
  verify_full(cov, ref);
}

template<bool Mixed>
static void test_coverage_set_modify_ranges_random() {
  BLRandom rnd(0xADD123u);

  for (uint32_t round = 0; round < 12u; round++) {
    BLCoverageSet cov;
    CoverageReference ref;

    if (Mixed && (round & 1u)) {
      EXPECT_OK(cov.add_range(0u, CoverageSetDetail::kMaxSize));
      ref.set_range(0u, CoverageSetDetail::kMaxSize);
    }

    for (uint32_t iter = 0; iter < 32u; iter++) {
      BLCoverageSetRange ranges[16];
      for (BLCoverageSetRange& range : ranges) {
        uint32_t start = rnd.next_uint32() % CoverageSetDetail::kMaxSize;
        uint32_t length = rnd.next_uint32() % 0x2000u;
        switch (rnd.next_uint32() % 4u) {
          case 0:
            start &= ~CoverageSetDetail::kSPBitMask;
            length = CoverageSetDetail::kSPBitCount;
            break;
          case 1:
            start &= ~(CoverageSetDetail::kMPBitCount - 1u);
            length = CoverageSetDetail::kMPBitCount;
            break;
          default:
            break;
        }
        range = { start, start + length };
      }

      // Force detachment periodically, and verify that page replacement preserves the shared snapshot.
      BLCoverageSet snapshot;
      bool shared = (iter % 8u) == 0u;
      if (shared) {
        EXPECT_OK(snapshot.assign(cov));
      }

      bool remove = Mixed && (rnd.next_uint32() & 1u);
      EXPECT_OK(remove ? cov.remove_ranges(ranges, BL_ARRAY_SIZE(ranges)) : cov.add_ranges(ranges, BL_ARRAY_SIZE(ranges)));
      if (shared) {
        verify_full(snapshot, ref);
      }

      for (const BLCoverageSetRange& range : ranges) {
        if (remove) {
          ref.remove_range(range.start, range.end);
        }
        else {
          ref.set_range(range.start, range.end);
        }
      }
      EXPECT_EQ(cov.cardinality(), ref.cardinality());
    }

    verify_full(cov, ref);
  }
}

TEST_CASE(core_coverage_set_ranges_add_random) {
  test_coverage_set_modify_ranges_random<false>();
}

TEST_CASE(core_coverage_set_ranges_modify_random) {
  test_coverage_set_modify_ranges_random<true>();
}

TEST_CASE(core_coverage_set_ranges_remove_storage) {
  BLCoverageSet cov;
  CoverageReference ref;

  EXPECT_OK(cov.remove_range(0u, 0xFFFFFFFFu));
  EXPECT_EQ(cov.page_capacity(), 0u);
  EXPECT_OK(cov.add_range(0u, 0x8000u));
  ref.set_range(0u, 0x8000u);

  // Out-of-span removal must not allocate pages or detach shared storage.
  BLCoverageSet shared(cov);
  BLCoverageSetImpl* impl = cov._d.layout<BLCoverageSet::Layout>().impl.ptr;
  EXPECT_OK(cov.remove_range(0x8000u, 0xFFFFFFFFu));
  EXPECT_EQ(cov._d.layout<BLCoverageSet::Layout>().impl.ptr, impl);

  // A full master-plane needs a header and a bitmap to represent a partial removal.
  EXPECT_OK(cov.remove_range(1u, 2u));
  verify_full(shared, ref);
  ref.clear(1u);
  EXPECT_EQ(cov.page_count(), 2u);

  // Detaching an existing header must preserve its filled sub-plane markers.
  EXPECT_OK(shared.assign(cov));
  EXPECT_OK(cov.remove_range(0x401u, 0x402u));
  verify_full(shared, ref);
  ref.clear(0x401u);
  EXPECT_EQ(cov.page_count(), 3u);

  EXPECT_OK(shared.assign(cov));
  EXPECT_OK(cov.remove(0x801u));
  verify_full(shared, ref);
  ref.clear(0x801u);

  EXPECT_OK(cov.remove_range(0u, 0x400u));
  ref.remove_range(0u, 0x400u);
  EXPECT_EQ(cov.page_count(), 3u);
  EXPECT_OK(cov.remove_range(0xC01u, 0xC02u));
  ref.clear(0xC01u);
  EXPECT_EQ(cov.page_count(), 4u);
  verify_full(cov, ref);

  // Clip at the represented end and release all bitmaps; the fast path may retain an empty master-plane.
  EXPECT_OK(cov.remove_range(0u, 0xFFFFFFFFu));
  EXPECT_TRUE(cov.is_empty());
  EXPECT_LE(cov.page_count(), 1u);
  EXPECT_OK(cov.add_range(0x38001u, 0x38002u));
  ref.remove_range(0u, CoverageSetDetail::kMaxSize);
  ref.set(0x38001u);
  verify_full(cov, ref);
}

TEST_CASE(core_coverage_set_ranges_remove_max_capacity) {
  BLCoverageSet cov;
  CoverageReference ref;
  EXPECT_OK(cov.add_range(0u, CoverageSetDetail::kMaxSize));
  ref.set_range(0u, CoverageSetDetail::kMaxSize);

  BLCoverageSetRange ranges[CoverageSetDetail::kMPCount * CoverageSetDetail::kSPPerMP];
  for (uint32_t i = 0; i < BL_ARRAY_SIZE(ranges); i++) {
    uint32_t cp = i * CoverageSetDetail::kSPBitCount;
    ranges[i] = { cp, cp + 1u };
    ref.clear(cp);
  }
  EXPECT_OK(cov.remove_ranges(ranges, BL_ARRAY_SIZE(ranges)));
  EXPECT_EQ(cov.page_count(), CoverageSetDetail::kMaxPageCapacity);
  verify_full(cov, ref);

  BLCoverageSet shared(cov);
  for (BLCoverageSetRange& range : ranges) {
    range.end++;
  }
  EXPECT_OK(cov.remove_ranges(ranges, BL_ARRAY_SIZE(ranges)));
  verify_full(shared, ref);
  for (const BLCoverageSetRange& range : ranges) {
    ref.remove_range(range.start, range.end);
  }
  verify_full(cov, ref);

  EXPECT_OK(cov.remove_range(0u, CoverageSetDetail::kMaxSize));
  EXPECT_TRUE(cov.is_empty());
  EXPECT_EQ(cov.page_count(), 0u);
}

TEST_CASE(core_coverage_set_subsumes_intersects) {
  BLCoverageSet big;
  BLCoverageSet small;
  BLCoverageSet other;

  EXPECT_OK(big.add_range(0x0000u, 0x1000u));
  EXPECT_OK(small.add_range(0x0100u, 0x0200u));
  EXPECT_OK(other.add_range(0x2000u, 0x2100u));

  EXPECT_TRUE(big.subsumes(small));
  EXPECT_FALSE(small.subsumes(big));
  EXPECT_TRUE(big.intersects(small));
  EXPECT_FALSE(big.intersects(other));

  // Empty is subsumed by everything and intersects nothing.
  BLCoverageSet empty;
  EXPECT_TRUE(big.subsumes(empty));
  EXPECT_FALSE(big.intersects(empty));
}

TEST_CASE(core_coverage_set_optimize_roundtrip) {
  BLCoverageSet cov;
  CoverageReference ref;

  // Build something with a mix of full sub-groups, partial sub-groups, and singletons.
  EXPECT_OK(cov.add_range(0x0000u, 0x0800u)); ref.set_range(0x0000u, 0x0800u);
  EXPECT_OK(cov.add_range(0x1234u, 0x1240u)); ref.set_range(0x1234u, 0x1240u);
  for (uint32_t cp = 0x20000u; cp < 0x20000u + 4096u; cp += 37u) {
    EXPECT_OK(cov.add(cp));
    ref.set(cp);
  }
  verify_full(cov, ref);

  size_t capacity_before = bl_coverage_set_get_page_capacity(&cov);
  EXPECT_OK(cov.optimize());
  verify_full(cov, ref);

  EXPECT_OK(cov.shrink());
  verify_full(cov, ref);
  EXPECT_LE(bl_coverage_set_get_page_capacity(&cov), capacity_before);

  // chop keeps only the middle window.
  EXPECT_OK(cov.chop(0x0400u, 0x20000u));
  ref.remove_range(0x0000u, 0x0400u);
  ref.remove_range(0x20000u, CoverageSetDetail::kMaxSize);
  verify_full(cov, ref);
}

TEST_CASE(core_coverage_set_optimize_uniform_planes) {
  using namespace CoverageSetDetail;

  for (uint32_t shrink = 0; shrink < 2u; shrink++) {
    for (uint32_t fill = 0; fill < 2u; fill++) {
      BLCoverageSet cov;
      EXPECT_OK(cov.add(0u));
      EXPECT_OK(fill ? cov.add_range(0u, kMPBitCount) : cov.remove(0u));
      EXPECT_OK(shrink ? cov.shrink() : cov.optimize());
      EXPECT_EQ(cov.page_count(), 0u);
      EXPECT_EQ(cov.cardinality(), fill ? kMPBitCount : 0u);
      EXPECT_EQ(cov.contains(0u), bool(fill));
      EXPECT_EQ(cov.contains(kMPBitCount - 1u), bool(fill));
      EXPECT_FALSE(cov.contains(kMPBitCount));

      // A uniform bitmap must also become implicit when its master-plane remains mixed.
      EXPECT_OK(cov.reset());
      EXPECT_OK(cov.add(0u));
      EXPECT_OK(cov.add(kSPBitCount));
      if (fill) {
        EXPECT_OK(cov.add_range(0u, kSPBitCount - 1u));
        EXPECT_OK(cov.add(kSPBitCount - 1u));
      }
      else {
        EXPECT_OK(cov.remove(0u));
      }

      EXPECT_OK(shrink ? cov.shrink() : cov.optimize());
      EXPECT_EQ(cov.page_count(), 2u);
      EXPECT_EQ(cov.cardinality(), fill ? kSPBitCount + 1u : 1u);
      EXPECT_EQ(cov.contains(0u), bool(fill));
      EXPECT_EQ(cov.contains(kSPBitCount - 1u), bool(fill));
      EXPECT_TRUE(cov.contains(kSPBitCount));
      verify_master_plane_metadata(cov, 0u);
    }
  }
}

TEST_CASE(core_coverage_set_cardinality_in_range_uniform) {
  using namespace CoverageSetDetail;

  static const BLCoverageSetRange ranges[] = {
    { 0u, 0u },
    { 1u, 1u },
    { 100u, 10u },
    { 0u, 1u },
    { 1u, kSPBitCount - 1u },
    { kSPBitCount - 1u, kSPBitCount + 1u },
    { kMPBitCount - 1u, kMPBitCount + 1u },
    { 123u, 3u * kMPBitCount + 456u },
    { 0u, kMaxSize },
    { kMaxSize - 1u, kMaxSize },
    { kMaxSize - 17u, 0xFFFFFFFFu },
    { 0u, 0xFFFFFFFFu },
    { kMaxSize, 0xFFFFFFFFu },
    { kMaxSize + 1u, 0xFFFFFFFFu },
    { 0xFFFFFFFFu, 0u },
    { 0xFFFFFFFFu, 0xFFFFFFFFu }
  };

  for (uint32_t fill = 0; fill < 2u; fill++) {
    BLCoverageSet cov;
    if (fill) {
      EXPECT_OK(cov.add_range(0u, kMaxSize));
    }
    for (const BLCoverageSetRange& range : ranges) {
      uint32_t end = axl::min(range.end, kMaxSize);
      uint32_t expected = fill && range.start < end ? end - range.start : 0u;
      EXPECT_EQ(cov.cardinality_in_range(range.start, range.end), expected)
        .message("fill=%u, range=[U+%06X, U+%06X)", fill, range.start, range.end);
      EXPECT_EQ(cov.contains_in_range(range.start, range.end), expected != 0u)
        .message("fill=%u, range=[U+%06X, U+%06X)", fill, range.start, range.end);
    }
  }
}

TEST_CASE(core_coverage_set_cardinality_in_range_words) {
  using namespace CoverageSetDetail;
  constexpr uint32_t kWordBits = axl::bit_size_of<BLBitWord>;

  // Place the same bitmap at the beginning, at a nonzero sub-plane, and at the Unicode limit.
  static const uint32_t bases[] = { 0u, kMPBitCount + kSPBitCount, kMaxSize - kSPBitCount };
  for (uint32_t base : bases) {
    BLCoverageSet cov;
    CoverageReference ref;

    // Adjacent words have different populations and bit positions, exposing skipped or repeated words.
    for (uint32_t i = 0; i < kSPBitCount; i++) {
      if ((i * 13u + i / kWordBits) % 19u < 7u) {
        EXPECT_OK(cov.add(base + i));
        ref.set(base + i);
      }
    }

    // Exhaust all endpoint offsets within one word and across two or three words.
    for (uint32_t start = 0; start <= kWordBits; start++) {
      for (uint32_t end = start; end <= 3u * kWordBits; end++) {
        verify_cardinality_in_range(cov, ref, base + start, base + end);
      }
    }

    // Include the last word and an end exactly on the sub-plane boundary.
    for (uint32_t offset = 0; offset <= kWordBits; offset++) {
      verify_cardinality_in_range(cov, ref, base + offset, base + kSPBitCount);
      verify_cardinality_in_range(cov, ref, base + kSPBitCount - kWordBits + offset, base + kSPBitCount);
    }
  }
}

TEST_CASE(core_coverage_set_cardinality_in_range_planes) {
  using namespace CoverageSetDetail;

  BLCoverageSet cov;
  CoverageReference ref;
  static const BLCoverageSetRange ranges[] = {
    { 1u, 17u },
    { kSPBitCount, 2u * kSPBitCount },
    { 3u * kSPBitCount + 5u, 3u * kSPBitCount + 300u },
    { kMPBitCount - 7u, kMPBitCount },
    // An empty master-plane separates the first mixed plane from two full ones.
    { 2u * kMPBitCount, 4u * kMPBitCount },
    { 4u * kMPBitCount, 4u * kMPBitCount + kSPBitCount },
    { 4u * kMPBitCount + 2u * kSPBitCount + 3u, 4u * kMPBitCount + 2u * kSPBitCount + 511u }
  };
  EXPECT_OK(cov.add_ranges(ranges, BL_ARRAY_SIZE(ranges)));
  for (const BLCoverageSetRange& range : ranges) {
    ref.set_range(range.start, range.end);
  }

  // Leave allocated empty and full bitmaps, then repeat queries after canonicalizing them.
  EXPECT_OK(cov.add(5u * kMPBitCount + 1u));
  EXPECT_OK(cov.remove(5u * kMPBitCount + 1u));
  EXPECT_OK(cov.remove(kSPBitCount + 1u));
  EXPECT_OK(cov.add(kSPBitCount + 1u));

  static const uint32_t endpoints[] = {
    0u, 1u, 16u, 17u, 18u,
    kSPBitCount - 1u, kSPBitCount, kSPBitCount + 1u,
    2u * kSPBitCount - 1u, 2u * kSPBitCount, 2u * kSPBitCount + 1u,
    3u * kSPBitCount + 5u, 3u * kSPBitCount + 300u,
    kMPBitCount - 1u, kMPBitCount, kMPBitCount + 1u,
    2u * kMPBitCount - 1u, 2u * kMPBitCount, 2u * kMPBitCount + 1u,
    3u * kMPBitCount, 4u * kMPBitCount - 1u, 4u * kMPBitCount, 4u * kMPBitCount + 1u,
    4u * kMPBitCount + kSPBitCount, 4u * kMPBitCount + 2u * kSPBitCount + 19u,
    5u * kMPBitCount, 5u * kMPBitCount + 1u, 6u * kMPBitCount,
    kMaxSize, 0xFFFFFFFFu
  };
  for (uint32_t pass = 0; pass < 3u; pass++) {
    if (pass == 1u) {
      EXPECT_OK(cov.optimize());
    }
    else if (pass == 2u) {
      EXPECT_OK(cov.shrink());
    }
    for (uint32_t start : endpoints) {
      for (uint32_t end : endpoints) {
        verify_cardinality_in_range(cov, ref, start, end);
      }
    }
  }
}

TEST_CASE(core_coverage_set_cardinality_in_range_random) {
  using namespace CoverageSetDetail;
  BLRandom rnd(0xCA4D123u);

  for (uint32_t fill = 0; fill < 2u; fill++) {
    BLCoverageSet cov;
    CoverageReference ref;
    if (fill) {
      EXPECT_OK(cov.add_range(0u, kMaxSize));
      ref.set_range(0u, kMaxSize);
    }

    for (uint32_t iter = 0; iter < 128u; iter++) {
      uint32_t start = rnd.next_uint32() % kMaxSize;
      uint32_t end = start + 1u + rnd.next_uint32() % (2u * kMPBitCount);
      bool remove = (rnd.next_uint32() & 1u) != 0u;
      EXPECT_OK(remove ? cov.remove_range(start, end) : cov.add_range(start, end));
      if (remove)
        ref.remove_range(start, end);
      else
        ref.set_range(start, end);

      verify_cardinality_in_range(cov, ref, start, end);
      for (uint32_t query = 0; query < 8u; query++) {
        uint32_t a = rnd.next_uint32() % (kMaxSize + kMPBitCount);
        uint32_t b = rnd.next_uint32() % (kMaxSize + kMPBitCount);
        verify_cardinality_in_range(cov, ref, axl::min(a, b), axl::max(a, b));
      }
    }
    verify_cardinality_in_range(cov, ref, 0u, 0xFFFFFFFFu);
  }
}

TEST_CASE(core_coverage_set_ranges) {
  BLCoverageSet cov;
  CoverageReference ref;

  static const BLCoverageSetRange ranges[] = {
    { 0x000041u, 0x00005Bu }, // Partial sub-group (A-Z).
    { 0x000200u, 0x000400u }, // Exactly one whole sub-group.
    { 0x000600u, 0x000A37u }, // Spans whole + partial sub-groups.
    { 0x008000u, 0x010000u }, // Exactly one whole master-plane.
    { 0x004E00u, 0x009FB0u }, // Large CJK block (whole + partial).
    { 0x01F300u, 0x01FAFFu }, // Emoji-ish supplementary.
    { 0x000000u, 0x110000u }  // Absolutely everything.
  };

  for (const BLCoverageSetRange& range : ranges) {
    BLCoverageSet local_cov;
    CoverageReference local_ref;

    EXPECT_OK(local_cov.add_range(range.start, range.end));
    local_ref.set_range(range.start, range.end);
    verify_full(local_cov, local_ref);

    // Accumulate into the running coverage as well.
    EXPECT_OK(cov.add_range(range.start, range.end));
    ref.set_range(range.start, range.end);
  }
  verify_full(cov, ref);

  // The "everything" op means the whole thing is now full.
  EXPECT_EQ(cov.cardinality(), CoverageSetDetail::kMaxSize);

  // Clearing sub-ranges out of a full coverage (materializes full master-planes).
  EXPECT_OK(cov.remove_range(0x0100u, 0x0180u));   // Partial sub-group.
  ref.remove_range(0x0100u, 0x0180u);
  EXPECT_OK(cov.remove_range(0x0800u, 0x1000u));   // Whole sub-groups.
  ref.remove_range(0x0800u, 0x1000u);
  EXPECT_OK(cov.remove_range(0x18000u, 0x20000u)); // Whole master-plane out of a full plane.
  ref.remove_range(0x18000u, 0x20000u);
  verify_full(cov, ref);
}

static void verify_combine(const BLCoverageSet& cov, const CoverageReference& ref) noexcept {
  using namespace CoverageSetDetail;
  verify_full(cov, ref);

  const auto& layout = cov._d.layout<BLCoverageSet::Layout>();
  const uint8_t* pages = reinterpret_cast<const uint8_t*>(layout.impl.ptr) + sizeof(BLCoverageSetImpl);
  bool used[kMaxPageCapacity] {};
  uint32_t page_count = 0u;
  auto use_page = [&](uint32_t offset) noexcept {
    EXPECT_LT(offset, layout.page_capacity);
    EXPECT_FALSE(used[offset]);
    used[offset] = true;
    page_count++;
  };

  for (uint32_t mp_index = 0; mp_index < kMPCount; mp_index++) {
    uint32_t mp_offset = layout.impl.ptr->mp_offsets[mp_index];
    if (mp_index >= layout.mp_count) {
      EXPECT_EQ(mp_offset, kPageZero);
    }
    if (mp_offset >= kPageSpecial) {
      EXPECT_TRUE(mp_offset == kPageZero || mp_offset == kPageFull);
      continue;
    }

    use_page(mp_offset);
    verify_master_plane_metadata(cov, mp_index);
    const MasterPlane* mp = reinterpret_cast<const MasterPlane*>(pages + mp_offset * kPageSize);
    for (uint32_t sp_index = 0; sp_index < kSPPerMP; sp_index++) {
      uint32_t sp_offset = mp->sp_page_hi[sp_index] >> 4;
      uint32_t cardinality = mp->sp_card_lo[sp_index] + ((mp->sp_page_hi[sp_index] & 0xFu) << 8);
      if (sp_offset < kPageSpecial) {
        use_page(sp_offset);
        const SubPlane* sp = reinterpret_cast<const SubPlane*>(pages + sp_offset * kPageSize);
        Utils::PopCounter<BLBitWord> counter;
        counter.add_array(sp->bits, kBWCount);
        EXPECT_EQ(cardinality, counter.get());
      }
      else {
        EXPECT_TRUE(sp_offset == kPageZero || sp_offset == kPageFull);
        EXPECT_EQ(cardinality, sp_offset == kPageFull ? kSPBitCount : 0u);
      }
    }
  }
  EXPECT_EQ(page_count, cov.page_count());

  uint32_t free_offset = layout.page_free_list;
  while (free_offset != kUnassignedPage) {
    use_page(free_offset);
    free_offset = reinterpret_cast<const FreePlane*>(pages + free_offset * kPageSize)->next_page;
  }
  EXPECT_LE(page_count, cov.page_capacity());
  for (uint32_t i = 0; i < page_count; i++) {
    EXPECT_TRUE(used[i]);
  }
}

static void coverage_set_reference_combine(CoverageReference& dst, const CoverageReference& a, const CoverageReference& b, BLBooleanOp op) noexcept {
  for (size_t i = 0; i < CoverageReference::BitArray::kFixedArraySize; i++) {
    BLBitWord x = a._bits->data[i];
    BLBitWord y = b._bits->data[i];
    switch (op) {
      case BL_BOOLEAN_OP_AND: dst._bits->data[i] = x & y; break;
      case BL_BOOLEAN_OP_OR: dst._bits->data[i] = x | y; break;
      case BL_BOOLEAN_OP_XOR: dst._bits->data[i] = x ^ y; break;
      case BL_BOOLEAN_OP_AND_NOT: dst._bits->data[i] = x & ~y; break;
      case BL_BOOLEAN_OP_NOT_AND: dst._bits->data[i] = ~x & y; break;
      default: BL_NOT_REACHED();
    }
  }
}

static constexpr BLBooleanOp coverage_set_combine_ops[] = {
  BL_BOOLEAN_OP_AND, BL_BOOLEAN_OP_OR, BL_BOOLEAN_OP_XOR, BL_BOOLEAN_OP_AND_NOT, BL_BOOLEAN_OP_NOT_AND
};

// Empty, full, partial, allocated empty, and allocated full planes.
static void coverage_set_test_plane(BLCoverageSet& cov, CoverageReference& ref, uint32_t cp, uint32_t size, uint32_t state) {
  switch (state) {
    case 0:
      break;
    case 1:
      EXPECT_OK(cov.add_range(cp, cp + size));
      ref.set_range(cp, cp + size);
      break;
    case 2:
      EXPECT_OK(cov.add_range(cp + 3u, cp + size / 2u));
      ref.set_range(cp + 3u, cp + size / 2u);
      break;
    case 3:
      EXPECT_OK(cov.add(cp));
      EXPECT_OK(cov.remove(cp));
      break;
    case 4:
      EXPECT_OK(cov.add_range(cp, cp + size));
      EXPECT_OK(cov.remove(cp));
      EXPECT_OK(cov.add(cp));
      ref.set_range(cp, cp + size);
      break;
  }
}

TEST_CASE(core_coverage_set_combine) {
  BLCoverageSet a;
  BLCoverageSet b;
  CoverageReference a_ref;
  CoverageReference b_ref;
  CoverageReference result_ref;

  // Exercise all pairs of master-plane representations and all pairs of sub-plane representations.
  for (uint32_t i = 0; i < 25u; i++) {
    coverage_set_test_plane(a, a_ref, i * CoverageSetDetail::kMPBitCount, CoverageSetDetail::kMPBitCount, i / 5u);
    coverage_set_test_plane(b, b_ref, i * CoverageSetDetail::kMPBitCount, CoverageSetDetail::kMPBitCount, i % 5u);
    coverage_set_test_plane(a, a_ref, 25u * CoverageSetDetail::kMPBitCount + i * CoverageSetDetail::kSPBitCount, CoverageSetDetail::kSPBitCount, i / 5u);
    coverage_set_test_plane(b, b_ref, 25u * CoverageSetDetail::kMPBitCount + i * CoverageSetDetail::kSPBitCount, CoverageSetDetail::kSPBitCount, i % 5u);
  }

  for (BLBooleanOp op : coverage_set_combine_ops) {
    coverage_set_reference_combine(result_ref, a_ref, b_ref, op);

    BLCoverageSet dst;
    EXPECT_OK(bl_coverage_set_combine(&dst, &a, &b, op));
    verify_combine(dst, result_ref);

    // Both operand positions support mutable in-place updates.
    EXPECT_OK(dst.assign_deep(a));
    EXPECT_OK(bl_coverage_set_combine(&dst, &dst, &b, op));
    verify_combine(dst, result_ref);
    EXPECT_OK(dst.assign_deep(b));
    EXPECT_OK(bl_coverage_set_combine(&dst, &a, &dst, op));
    verify_combine(dst, result_ref);

    // Detaching an aliased destination must preserve its shared source.
    EXPECT_OK(dst.assign(a));
    EXPECT_OK(bl_coverage_set_combine(&dst, &dst, &b, op));
    verify_combine(dst, result_ref);
    verify_combine(a, a_ref);
    EXPECT_OK(dst.assign(b));
    EXPECT_OK(bl_coverage_set_combine(&dst, &a, &dst, op));
    verify_combine(dst, result_ref);
    verify_combine(b, b_ref);

    coverage_set_reference_combine(result_ref, a_ref, a_ref, op);
    EXPECT_OK(dst.assign(a));
    EXPECT_OK(bl_coverage_set_combine(&dst, &dst, &dst, op));
    verify_combine(dst, result_ref);
    EXPECT_OK(dst.assign(a));
    EXPECT_OK(bl_coverage_set_combine(&dst, &a, &dst, op));
    verify_combine(dst, result_ref);
  }
}

TEST_CASE(core_coverage_set_combine_tails) {
  const uint32_t counts[][2] = {{0u, 1u}, {1u, 2u}, {7u, 8u}, {8u, 34u}};
  for (const auto& pair : counts) {
    for (uint32_t reverse = 0; reverse < 2u; reverse++) {
      BLCoverageSet a;
      BLCoverageSet b;
      CoverageReference a_ref;
      CoverageReference b_ref;
      CoverageReference result_ref;

      for (uint32_t i = 0; i < pair[reverse]; i++) {
        coverage_set_test_plane(a, a_ref, i * CoverageSetDetail::kMPBitCount, CoverageSetDetail::kMPBitCount, i % 4u + 1u);
      }
      for (uint32_t i = 0; i < pair[reverse ^ 1u]; i++) {
        coverage_set_test_plane(b, b_ref, i * CoverageSetDetail::kMPBitCount, CoverageSetDetail::kMPBitCount, (i + 1u) % 4u + 1u);
      }

      for (BLBooleanOp op : coverage_set_combine_ops) {
        coverage_set_reference_combine(result_ref, a_ref, b_ref, op);
        BLCoverageSet dst;
        EXPECT_OK(bl_coverage_set_combine(&dst, &a, &b, op));
        verify_combine(dst, result_ref);
        EXPECT_OK(dst.assign_deep(a));
        EXPECT_OK(bl_coverage_set_combine(&dst, &dst, &b, op));
        verify_combine(dst, result_ref);
        EXPECT_OK(dst.assign_deep(b));
        EXPECT_OK(bl_coverage_set_combine(&dst, &a, &dst, op));
        verify_combine(dst, result_ref);
      }
    }
  }
}

TEST_CASE(core_coverage_set_combine_capacity) {
  BLCoverageSet a;
  BLCoverageSet b;
  BLCoverageSet dst;
  CoverageReference ref;

  EXPECT_OK(a.add(1u));
  EXPECT_OK(b.add(0x8001u));
  EXPECT_EQ(a.page_count(), 2u);
  EXPECT_EQ(b.page_count(), 2u);
  EXPECT_OK(dst.add(2u));
  EXPECT_EQ(dst.page_capacity(), 4u);
  BLCoverageSetImpl* impl = dst._d.layout<BLCoverageSet::Layout>().impl.ptr;

  // The sum of input data pages fits: combine directly, even with an unrelated nonempty destination.
  EXPECT_OK(bl_coverage_set_combine(&dst, &a, &b, BL_BOOLEAN_OP_OR));
  EXPECT_EQ(dst._d.layout<BLCoverageSet::Layout>().impl.ptr, impl);
  EXPECT_EQ(dst.page_count(), 4u);
  ref.set(1u);
  ref.set(0x8001u);
  verify_combine(dst, ref);

  impl = a._d.layout<BLCoverageSet::Layout>().impl.ptr;
  EXPECT_OK(bl_coverage_set_combine(&a, &a, &b, BL_BOOLEAN_OP_XOR));
  EXPECT_EQ(a._d.layout<BLCoverageSet::Layout>().impl.ptr, impl);
  verify_combine(a, ref);

  // The conservative bound doesn't fit, but metadata analysis proves that the existing two pages suffice.
  EXPECT_OK(a.reset());
  EXPECT_OK(b.reset());
  EXPECT_OK(a.add(1u));
  EXPECT_OK(a.shrink());
  EXPECT_OK(b.add(2u));
  EXPECT_EQ(a.page_capacity(), 2u);
  impl = a._d.layout<BLCoverageSet::Layout>().impl.ptr;
  EXPECT_OK(bl_coverage_set_combine(&a, &a, &b, BL_BOOLEAN_OP_OR));
  EXPECT_EQ(a._d.layout<BLCoverageSet::Layout>().impl.ptr, impl);
  ref.clear(0x8001u);
  ref.set(2u);
  verify_combine(a, ref);

  // A surviving high tail of full master-planes needs no additional pages.
  EXPECT_OK(a.reset());
  EXPECT_OK(b.reset());
  EXPECT_OK(a.add(1u));
  EXPECT_OK(b.add_range(0x38000u, 0x40000u));
  impl = a._d.layout<BLCoverageSet::Layout>().impl.ptr;
  EXPECT_OK(bl_coverage_set_combine(&a, &a, &b, BL_BOOLEAN_OP_OR));
  EXPECT_EQ(a._d.layout<BLCoverageSet::Layout>().impl.ptr, impl);
  EXPECT_EQ(a.page_count(), 2u);
  ref.clear(2u);
  ref.set_range(0x38000u, 0x40000u);
  verify_combine(a, ref);

  // Recycle the original data pages, then reuse the freed slots.
  EXPECT_OK(bl_coverage_set_combine(&a, &a, &b, BL_BOOLEAN_OP_AND));
  EXPECT_EQ(a.page_count(), 0u);
  EXPECT_OK(b.reset());
  EXPECT_OK(b.add(0x8001u));
  EXPECT_OK(bl_coverage_set_combine(&a, &a, &b, BL_BOOLEAN_OP_OR));
  EXPECT_EQ(a._d.layout<BLCoverageSet::Layout>().impl.ptr, impl);
  EXPECT_EQ(a.page_count(), 2u);
  ref.clear(1u);
  ref.set(0x8001u);
  verify_combine(a, ref);
}

TEST_CASE(core_coverage_set_combine_max_capacity) {
  BLCoverageSet a;
  BLCoverageSet b;
  CoverageReference a_ref;
  CoverageReference b_ref;
  CoverageReference result_ref;

  for (uint32_t cp = 0; cp < CoverageSetDetail::kMaxSize; cp += CoverageSetDetail::kSPBitCount) {
    EXPECT_OK(a.add(cp));
    EXPECT_OK(b.add(cp + 1u));
    a_ref.set(cp);
    b_ref.set(cp + 1u);
  }
  EXPECT_EQ(a.page_count(), CoverageSetDetail::kMaxPageCapacity);
  EXPECT_EQ(b.page_count(), CoverageSetDetail::kMaxPageCapacity);

  for (BLBooleanOp op : coverage_set_combine_ops) {
    BLCoverageSet dst;
    EXPECT_OK(dst.assign_deep(a));
    BLCoverageSetImpl* impl = dst._d.layout<BLCoverageSet::Layout>().impl.ptr;
    coverage_set_reference_combine(result_ref, a_ref, b_ref, op);
    EXPECT_OK(bl_coverage_set_combine(&dst, &dst, &b, op));
    EXPECT_EQ(dst._d.layout<BLCoverageSet::Layout>().impl.ptr, impl);
    verify_combine(dst, result_ref);

    // Collapsed pages must remain reusable after a dense result becomes empty.
    EXPECT_OK(dst.add(CoverageSetDetail::kMaxSize - 1u));
    result_ref.set(CoverageSetDetail::kMaxSize - 1u);
    verify_combine(dst, result_ref);
  }
}

TEST_CASE(core_coverage_set_combine_uniform) {
  BLCoverageSet a;
  BLCoverageSet b;
  CoverageReference a_ref;
  CoverageReference b_ref;
  CoverageReference result_ref;
  EXPECT_OK(a.add_range(0u, CoverageSetDetail::kMPBitCount));
  EXPECT_OK(b.add_range(CoverageSetDetail::kMPBitCount, 2u * CoverageSetDetail::kMPBitCount));
  a_ref.set_range(0u, CoverageSetDetail::kMPBitCount);
  b_ref.set_range(CoverageSetDetail::kMPBitCount, 2u * CoverageSetDetail::kMPBitCount);

  for (BLBooleanOp op : coverage_set_combine_ops) {
    BLCoverageSet dst;
    coverage_set_reference_combine(result_ref, a_ref, b_ref, op);
    EXPECT_OK(bl_coverage_set_combine(&dst, &a, &b, op));
    EXPECT_EQ(dst.page_capacity(), 0u);
    verify_combine(dst, result_ref);
  }
}

TEST_CASE(core_coverage_set_combine_random) {
  BLRandom rnd(0xAC19E520u);
  for (uint32_t iteration = 0; iteration < 24u; iteration++) {
    BLCoverageSet a;
    BLCoverageSet b;
    CoverageReference a_ref;
    CoverageReference b_ref;
    CoverageReference result_ref;

    for (uint32_t i = 0; i < 100u; i++) {
      BLCoverageSet& cov = i & 1u ? a : b;
      CoverageReference& ref = i & 1u ? a_ref : b_ref;
      uint32_t limit = (i % 3u) ? 0x10000u : CoverageSetDetail::kMaxSize;
      uint32_t start = rnd.next_uint32() % limit;
      uint32_t end = axl::min(start + rnd.next_uint32() % 0x10000u, CoverageSetDetail::kMaxSize);
      if (rnd.next_uint32() % 4u) {
        EXPECT_OK(cov.add_range(start, end));
        ref.set_range(start, end);
      }
      else {
        EXPECT_OK(cov.remove_range(start, end));
        ref.remove_range(start, end);
      }
    }

    if (iteration % 2u) {
      EXPECT_OK(a.shrink());
      EXPECT_OK(b.optimize());
    }

    BLCoverageSet dst;
    for (BLBooleanOp op : coverage_set_combine_ops) {
      coverage_set_reference_combine(result_ref, a_ref, b_ref, op);
      switch (iteration % 3u) {
        case 0:
          EXPECT_OK(bl_coverage_set_combine(&dst, &a, &b, op));
          break;
        case 1:
          EXPECT_OK(dst.assign_deep(a));
          EXPECT_OK(bl_coverage_set_combine(&dst, &dst, &b, op));
          break;
        case 2:
          EXPECT_OK(dst.assign_deep(b));
          EXPECT_OK(bl_coverage_set_combine(&dst, &a, &dst, op));
          break;
      }
      verify_combine(dst, result_ref);
    }
    verify_combine(a, a_ref);
    verify_combine(b, b_ref);
  }
}

TEST_CASE(core_coverage_set_combine_reuse_pages) {
  using namespace CoverageSetDetail;
  const BLBooleanOp ops[] = {BL_BOOLEAN_OP_AND, BL_BOOLEAN_OP_OR, BL_BOOLEAN_OP_AND_NOT, BL_BOOLEAN_OP_NOT_AND};
  const uint32_t late_code_points[] = {31u * kSPBitCount, kMaxSize - kMPBitCount};

  for (BLBooleanOp op : ops) {
    for (uint32_t late_cp : late_code_points) {
      for (uint32_t reverse = 0; reverse < 2u; reverse++) {
        BLCoverageSet a;
        BLCoverageSet b;
        CoverageReference a_ref;
        CoverageReference b_ref;
        CoverageReference result_ref;

        // A dense result at the beginning must reuse storage released later, even in another master-plane.
        if (op == BL_BOOLEAN_OP_AND || op == BL_BOOLEAN_OP_AND_NOT) {
          EXPECT_OK(a.add_range(0u, late_cp < kMPBitCount ? kSPBitCount : kMPBitCount));
          a_ref.set_range(0u, late_cp < kMPBitCount ? kSPBitCount : kMPBitCount);
        }
        EXPECT_OK(a.add(late_cp + 1u));
        a_ref.set(late_cp + 1u);
        EXPECT_OK(a.shrink());
        EXPECT_EQ(a.page_capacity(), 2u);

        EXPECT_OK(b.add(3u));
        b_ref.set(3u);
        if (op == BL_BOOLEAN_OP_OR || op == BL_BOOLEAN_OP_AND_NOT) {
          uint32_t end_cp = late_cp + (late_cp < kMPBitCount ? kSPBitCount : kMPBitCount);
          EXPECT_OK(b.add_range(late_cp, end_cp));
          b_ref.set_range(late_cp, end_cp);
        }

        coverage_set_reference_combine(result_ref, a_ref, b_ref, op);
        BLCoverageSetImpl* impl = a._d.layout<BLCoverageSet::Layout>().impl.ptr;
        if (reverse) {
          BLBooleanOp reversed_op = op;
          if (op == BL_BOOLEAN_OP_AND_NOT) reversed_op = BL_BOOLEAN_OP_NOT_AND;
          if (op == BL_BOOLEAN_OP_NOT_AND) reversed_op = BL_BOOLEAN_OP_AND_NOT;
          EXPECT_OK(bl_coverage_set_combine(&a, &b, &a, reversed_op));
        }
        else {
          EXPECT_OK(bl_coverage_set_combine(&a, &a, &b, op));
        }
        EXPECT_EQ(a._d.layout<BLCoverageSet::Layout>().impl.ptr, impl);
        EXPECT_EQ(a.page_count(), 2u);
        verify_combine(a, result_ref);
        verify_combine(b, b_ref);

        // Exercise allocation again after recycling both master-plane and sub-plane slots.
        EXPECT_OK(a.add(kMPBitCount + 7u));
        result_ref.set(kMPBitCount + 7u);
        verify_combine(a, result_ref);
      }
    }
  }
}

TEST_CASE(core_coverage_set_combine_dense_uniform) {
  using namespace CoverageSetDetail;
  for (BLBooleanOp op : coverage_set_combine_ops) {
    BLCoverageSet a;
    BLCoverageSet b;
    CoverageReference a_ref;
    CoverageReference b_ref;
    CoverageReference result_ref;
    for (uint32_t cp = 0; cp < kMPBitCount; cp += kSPBitCount) {
      EXPECT_OK(a.add_range(cp, cp + kSPBitCount / 2u));
      a_ref.set_range(cp, cp + kSPBitCount / 2u);
      uint32_t b_start = cp + (op == BL_BOOLEAN_OP_AND_NOT || op == BL_BOOLEAN_OP_NOT_AND ? 0u : kSPBitCount / 2u);
      EXPECT_OK(b.add_range(b_start, b_start + kSPBitCount / 2u));
      b_ref.set_range(b_start, b_start + kSPBitCount / 2u);
    }
    EXPECT_EQ(a.page_count(), 1u + kSPPerMP);
    EXPECT_EQ(b.page_count(), 1u + kSPPerMP);
    coverage_set_reference_combine(result_ref, a_ref, b_ref, op);

    BLCoverageSet dst;
    for (uint32_t alias = 0; alias < 3u; alias++) {
      if (alias == 0u) {
        EXPECT_OK(BLCoverageSet::combine(dst, a, b, op));
      }
      else if (alias == 1u) {
        EXPECT_OK(dst.assign_deep(a));
        EXPECT_OK(BLCoverageSet::combine(dst, dst, b, op));
      }
      else {
        EXPECT_OK(dst.assign_deep(b));
        EXPECT_OK(BLCoverageSet::combine(dst, a, dst, op));
      }
      EXPECT_EQ(dst.page_count(), 0u);
      verify_combine(dst, result_ref);
    }
  }
}

TEST_CASE(core_coverage_set_combine_copy_and_invalid) {
  BLCoverageSet a;
  BLCoverageSet b;
  BLCoverageSet dst;
  EXPECT_OK(a.add(1u));
  EXPECT_OK(b.add(2u));

  EXPECT_OK(BLCoverageSet::combine(dst, a, b, BL_BOOLEAN_OP_COPY));
  EXPECT_TRUE(dst.equals(b));
  EXPECT_OK(BLCoverageSet::combine(b, a, b, BL_BOOLEAN_OP_COPY));
  EXPECT_TRUE(dst.equals(b));
  EXPECT_OK(BLCoverageSet::combine(a, a, b, BL_BOOLEAN_OP_COPY));
  EXPECT_TRUE(a.equals(b));
  EXPECT_EQ(BLCoverageSet::combine(dst, a, b, static_cast<BLBooleanOp>(0xFFFFFFFFu)), BL_ERROR_INVALID_VALUE);
  EXPECT_TRUE(dst.equals(b));
}

TEST_CASE(core_coverage_set_stress) {
  BLCoverageSet cov;
  CoverageReference ref;

  BLRandom rnd(0x12345678u);

  // Universe biased towards low code-points, but reaching into supplementary planes.
  auto rand_cp = [&]() -> uint32_t {
    switch (rnd.next_uint32() % 4u) {
      case  0: return rnd.next_uint32() % 0x00200u;                        // Dense low.
      case  1: return rnd.next_uint32() % 0x10000u;                        // BMP.
      case  2: return rnd.next_uint32() % 0x30000u;                        // Low supplementary.
      default: return rnd.next_uint32() % CoverageSetDetail::kMaxSize; // Anywhere.
    }
  };

  for (uint32_t iter = 0; iter < 1000u; iter++) {
    uint32_t op = rnd.next_uint32() % 10u;
    if (op < 7) {
      uint32_t cp = rand_cp();
      EXPECT_OK(cov.add(cp));
      ref.set(cp);
    }
    else if (op == 7) {
      uint32_t cp = rand_cp();
      EXPECT_OK(cov.remove(cp));
      ref.clear(cp);
    }
    else if (op == 8) {
      uint32_t s = rand_cp();
      uint32_t len = rnd.next_uint32() % 0x4000u;
      EXPECT_OK(cov.add_range(s, s + len));
      ref.set_range(s, s + len);
    }
    else if (op == 9) {
      uint32_t s = rand_cp();
      uint32_t len = rnd.next_uint32() % 0x4000u;
      EXPECT_OK(cov.remove_range(s, s + len));
      ref.remove_range(s, s + len);
    }

    if ((iter % 512u) == 0) {
      if (iter % 1024u)
        EXPECT_OK(cov.optimize());
      else
        EXPECT_OK(cov.shrink());
    }

    verify_full(cov, ref);
  }

  EXPECT_OK(cov.optimize());
  verify_full(cov, ref);
}

} // {bl::UnicodeCoverageTests}

#endif // BL_TEST
