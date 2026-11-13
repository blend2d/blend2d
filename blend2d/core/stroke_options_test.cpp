// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/core/stroke_options_p.h>

namespace bl::StrokeOptionsTests {

// StrokeOptions - Test Utilities
// ==============================

static constexpr float initial_dashes[] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f };
static constexpr float updated_dashes[] = { 9.5f, 8.5f, 7.5f, 6.5f, 5.5f, 4.5f, 3.5f, 2.5f, 1.5f, 33.0f };

static void expect_dashes(const BLStrokeOptions& options, const float* data, size_t size) noexcept {
  EXPECT_EQ(options._d.type(), BL_OBJECT_TYPE_STROKE_OPTIONS);
  EXPECT_EQ(options.dash_array_size(), size);
  EXPECT_GE(options.dash_array_capacity(), size);

  EXPECT_EQ(options.dash_array(), BLSpan<const float>(data, size));

  EXPECT_EQ(bl_stroke_options_get_dash_count(&options), size);
  EXPECT_EQ(bl_stroke_options_get_dash_array(&options), options.dash_array_data());

  if (options._d.is_sso()) {
    EXPECT_EQ(options.dash_array_capacity(), 4u);
    for (size_t i = size; i < 4; i++) {
      EXPECT_EQ(options.dash_array_data()[i], 0.0f);
    }
  }
}

static void expect_default(const BLStrokeOptions& options) noexcept {
  EXPECT_TRUE(options._d.is_sso());

  EXPECT_EQ(options.width(), 1.0f);
  EXPECT_EQ(options.miter_limit(), 4.0f);
  EXPECT_EQ(options.dash_offset(), 0.0f);
  EXPECT_EQ(options.start_cap(), BL_STROKE_CAP_BUTT);
  EXPECT_EQ(options.end_cap(), BL_STROKE_CAP_BUTT);
  EXPECT_EQ(options.join(), BL_STROKE_JOIN_MITER_CLIP);
  EXPECT_EQ(options.transform_order(), BL_STROKE_TRANSFORM_ORDER_AFTER);

  expect_dashes(options, nullptr, 0);
}

static void expect_non_default_attributes(const BLStrokeOptions& options) noexcept {
  EXPECT_EQ(options.width(), 2.5f);
  EXPECT_EQ(options.miter_limit(), 7.5f);
  EXPECT_EQ(options.dash_offset(), 3.5f);
  EXPECT_EQ(options.start_cap(), BL_STROKE_CAP_ROUND_REV);
  EXPECT_EQ(options.end_cap(), BL_STROKE_CAP_TRIANGLE_REV);
  EXPECT_EQ(options.join(), BL_STROKE_JOIN_MITER_ROUND);
  EXPECT_EQ(options.transform_order(), BL_STROKE_TRANSFORM_ORDER_BEFORE);
  EXPECT_EQ(options.cap_by_position(BL_STROKE_CAP_POSITION_START), options.start_cap());
  EXPECT_EQ(options.cap_by_position(BL_STROKE_CAP_POSITION_END), options.end_cap());
}

static BLStrokeOptions make_non_default_options(const float* data, size_t size) noexcept {
  BLStrokeOptions options;

  EXPECT_OK(options.set_width(2.5f));
  EXPECT_OK(options.set_miter_limit(7.5f));
  EXPECT_OK(options.set_dash_offset(3.5f));
  EXPECT_OK(options.set_caps(BL_STROKE_CAP_ROUND_REV, BL_STROKE_CAP_TRIANGLE_REV));
  EXPECT_OK(options.set_join(BL_STROKE_JOIN_MITER_ROUND));
  EXPECT_OK(options.set_transform_order(BL_STROKE_TRANSFORM_ORDER_BEFORE));
  EXPECT_OK(options.set_dash_array(data, size));

  return options;
}

// StrokeOptions - Tests
// =====================

TEST_CASE(core_stroke_options_dash_array_data) {
  auto test_dash_array_setter = [](auto set_dash_array) noexcept {
    // Include every SSO length, the first dynamic length, and BLArray's own SSO boundary.
    for (size_t initial_size = 0; initial_size <= BL_ARRAY_SIZE(initial_dashes); initial_size++) {
      for (size_t size = 0; size <= BL_ARRAY_SIZE(updated_dashes); size++) {
        for (uint32_t shared = 0; shared < 2; shared++) {
          BLStrokeOptions options = make_non_default_options(initial_dashes, initial_size);
          BLStrokeOptions copy;

          if (shared) {
            EXPECT_OK(copy.assign(options));
          }

          BLArray<float> array;
          EXPECT_OK(array.assign_data(updated_dashes, size));

          const float* old_data = options.dash_array_data();
          size_t old_capacity = options.dash_array_capacity();

          EXPECT_OK(set_dash_array(options, array));
          expect_dashes(options, updated_dashes, size);
          expect_non_default_attributes(options);

          if (initial_size <= 4 && size <= 4) {
            EXPECT_TRUE(options._d.is_sso());
          }

          if (initial_size > 4 && !shared && size <= old_capacity) {
            EXPECT_EQ(options.dash_array_data(), old_data);
          }

          if (shared) {
            expect_dashes(copy, initial_dashes, initial_size);
            expect_non_default_attributes(copy);
          }

          EXPECT_OK(options.reset_dash_array());
          EXPECT_TRUE(options._d.is_sso());
          expect_dashes(options, nullptr, 0);
          expect_non_default_attributes(options);

          if (shared) {
            expect_dashes(copy, initial_dashes, initial_size);
          }
        }
      }
    }
  };

  test_dash_array_setter([](BLStrokeOptions& options, const BLArray<float>& array) {
    return options.set_dash_array(array);
  });

  test_dash_array_setter([](BLStrokeOptions& options, const BLArray<float>& array) {
    return options.set_dash_array(array.span());
  });

  test_dash_array_setter([](BLStrokeOptions& options, const BLArray<float>& array) {
    return options.set_dash_array(array.data(), array.size());
  });

  // A short array can still own dynamic storage, even when its contents fit SSO.
  test_dash_array_setter([](BLStrokeOptions& options, const BLArray<float>& array) {
    BLArray<float> dynamic(array);

    EXPECT_OK(dynamic.reserve(16));
    EXPECT_TRUE(dynamic._d.is_dynamic());

    return options.set_dash_array(dynamic);
  });

  // External array.
  test_dash_array_setter([](BLStrokeOptions& options, const BLArray<float>& array) {
    BLArray<float> external;

    EXPECT_OK(external.assign_external_data_const(array.data(), array.size(), array.capacity()));
    EXPECT_OK(options.set_dash_array(external));
    EXPECT_NE(options.dash_array_data(), array.data());

    return BL_SUCCESS;
  });
}

TEST_CASE(core_stroke_options_dash_array_null) {
  for (size_t size = 0; size <= BL_ARRAY_SIZE(initial_dashes); size++) {
    BLStrokeOptions options = make_non_default_options(initial_dashes, size);
    BLStrokeOptions copy(options);

    EXPECT_OK(options.set_dash_array(nullptr, 0));
    expect_dashes(options, nullptr, 0);
    expect_non_default_attributes(options);
    expect_dashes(copy, initial_dashes, size);
  }
}

TEST_CASE(core_stroke_options_dash_array_invalid) {
  for (size_t size = 0; size <= BL_ARRAY_SIZE(initial_dashes); size++) {
    BLStrokeOptions options = make_non_default_options(initial_dashes, size);
    BLStrokeOptions copy(options);
    BLArray<double> wrong_type;

    EXPECT_EQ(bl_stroke_options_set_dash_array_object(&options, &wrong_type), BL_ERROR_INVALID_VALUE);
    EXPECT_EQ(options, copy);

    expect_non_default_attributes(options);
    expect_dashes(options, initial_dashes, size);
  }
}

TEST_CASE(core_stroke_options_dash_array_ownership) {
  const float values[] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };
  const float replacement[] = { 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f };
  constexpr size_t count = BL_ARRAY_SIZE(values);

  TEST_LOG("Testing dynamic BLArray ownership and reassignment");
  {
    BLArray<float> array;
    BLStrokeOptions options;

    EXPECT_OK(array.assign_data(values, count));
    EXPECT_TRUE(array._d.is_dynamic());

    EXPECT_OK(options.set_dash_array(array));
    EXPECT_EQ(options.dash_array_data(), array.data());
    EXPECT_EQ(options.dash_array(), BLSpan<const float>(values, count));
    EXPECT_EQ(ObjectLayer::get_impl_ref_count(options._d.impl<BLStrokeOptionsImpl>()), 2u * ObjectLayer::kImplRCScale);

    // Both objects already refer to this allocation. Retain before releasing the old value.
    EXPECT_OK(options.set_dash_array(array));
    EXPECT_EQ(options.dash_array_data(), array.data());
    EXPECT_EQ(ObjectLayer::get_impl_ref_count(options._d.impl<BLStrokeOptionsImpl>()), 2u * ObjectLayer::kImplRCScale);

    const float* data = options.dash_array_data();
    EXPECT_OK(array.reset());
    EXPECT_EQ(options.dash_array_data(), data);
    EXPECT_EQ(options.dash_array(), BLSpan<const float>(values, count));
    EXPECT_EQ(ObjectLayer::get_impl_ref_count(options._d.impl<BLStrokeOptionsImpl>()), ObjectLayer::kImplRCScale);
  }

  TEST_LOG("Testing BLArray detachment from shared stroke options");
  {
    BLArray<float> array;
    BLStrokeOptions options;

    EXPECT_OK(array.assign_data(values, count));
    EXPECT_OK(options.set_dash_array(array));
    EXPECT_EQ(options.dash_array_data(), array.data());

    EXPECT_OK(array.replace(0, 100.0f));
    EXPECT_EQ(array[0], 100.0f);
    EXPECT_NE(options.dash_array_data(), array.data());
    EXPECT_EQ(options.dash_array(), BLSpan<const float>(values, count));
  }

  TEST_LOG("Testing stroke options detachment from a shared BLArray");
  {
    BLArray<float> array;
    BLStrokeOptions options;

    EXPECT_OK(array.assign_data(values, count));
    EXPECT_OK(options.set_dash_array(array));
    EXPECT_EQ(options.dash_array_data(), array.data());

    EXPECT_OK(options.set_dash_array(replacement, count));
    EXPECT_NE(options.dash_array_data(), array.data());
    EXPECT_EQ(options.dash_array(), BLSpan<const float>(replacement, count));
    EXPECT_EQ(array.span(), BLSpan<const float>(values, count));
  }
}

TEST_CASE(core_stroke_options_equals) {
  for (size_t size = 0; size <= BL_ARRAY_SIZE(initial_dashes); size++) {
    BLStrokeOptions a = make_non_default_options(initial_dashes, size);
    BLStrokeOptions b = make_non_default_options(initial_dashes, size);

    EXPECT_TRUE(a.equals(a));
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);

    // Equality must ignore representation and unused inline elements after shrinking.
    BLStrokeOptions dynamic = make_non_default_options(updated_dashes, BL_ARRAY_SIZE(updated_dashes));
    EXPECT_OK(dynamic.set_dash_array(initial_dashes, size));
    EXPECT_FALSE(dynamic._d.is_sso());
    EXPECT_EQ(a, dynamic);
    EXPECT_EQ(dynamic, a);

    EXPECT_OK(b.set_dash_array(updated_dashes, BL_ARRAY_SIZE(updated_dashes)));
    EXPECT_OK(b.reset_dash_array());
    EXPECT_OK(b.set_dash_array(initial_dashes, size));
    EXPECT_EQ(a, b);

    if (size) {
      float data[BL_ARRAY_SIZE(initial_dashes)];
      memcpy(data, initial_dashes, sizeof(data));
      for (size_t i = 0; i < size; i++) {
        data[i] += 0.5f;
        EXPECT_OK(b.set_dash_array(data, size));
        EXPECT_NE(a, b);
        EXPECT_NE(b, a);
        data[i] = initial_dashes[i];
      }
    }

    EXPECT_OK(b.set_dash_array(initial_dashes, size ? size - 1 : 1));
    EXPECT_NE(a, b);

    const auto test_attribute = [&](auto change) {
      BLStrokeOptions changed(a);
      EXPECT_OK(change(changed));
      EXPECT_NE(a, changed);
      EXPECT_NE(changed, a);
      expect_dashes(changed, initial_dashes, size);
    };

    test_attribute([](BLStrokeOptions& o) { return o.set_width(3.5f); });
    test_attribute([](BLStrokeOptions& o) { return o.set_miter_limit(8.5f); });
    test_attribute([](BLStrokeOptions& o) { return o.set_dash_offset(4.5f); });
    test_attribute([](BLStrokeOptions& o) { return o.set_start_cap(BL_STROKE_CAP_SQUARE); });
    test_attribute([](BLStrokeOptions& o) { return o.set_end_cap(BL_STROKE_CAP_ROUND); });
    test_attribute([](BLStrokeOptions& o) { return o.set_join(BL_STROKE_JOIN_BEVEL); });
    test_attribute([](BLStrokeOptions& o) { return o.set_transform_order(BL_STROKE_TRANSFORM_ORDER_AFTER); });

    expect_non_default_attributes(a);
  }
}


TEST_CASE(core_stroke_options_lifetime) {
  expect_default(BLStrokeOptions());

  for (size_t size = 0; size <= BL_ARRAY_SIZE(initial_dashes); size++) {
    BLStrokeOptions original = make_non_default_options(initial_dashes, size);
    EXPECT_EQ(original._d.is_sso(), size <= 4);

    BLStrokeOptions copy(original);
    EXPECT_EQ(copy, original);

    BLStrokeOptions moved(axl::move(copy));
    EXPECT_EQ(moved, original);
    expect_default(copy);

    EXPECT_OK(copy.set_dash_array(updated_dashes, size));
    expect_dashes(original, initial_dashes, size);

    // Assign over both inline and allocated destination storage.
    const size_t dst_sizes[] = { 0, 4, 5, 9 };
    for (size_t dst_size : dst_sizes) {
      BLStrokeOptions assigned = make_non_default_options(updated_dashes, dst_size);
      EXPECT_OK(assigned.assign(original));
      EXPECT_EQ(assigned, original);
      EXPECT_OK(assigned.assign(assigned));
      EXPECT_EQ(assigned, original);
      EXPECT_OK(assigned.reset());
      expect_default(assigned);
      expect_dashes(original, initial_dashes, size);

      assigned = make_non_default_options(updated_dashes, dst_size);
      copy = original;
      assigned = axl::move(copy);

      EXPECT_EQ(assigned, original);
      expect_default(copy);

      EXPECT_OK(assigned.reset());
      expect_default(assigned);
    }

    // Exercise the C initialization paths as well as the inline C++ constructors.
    BLStrokeOptionsCore core;
    EXPECT_OK(bl_stroke_options_init(&core));
    expect_default(core.dcast());

    EXPECT_OK(bl_stroke_options_destroy(&core));
    EXPECT_OK(bl_stroke_options_init_weak(&core, &original));
    EXPECT_EQ(core.dcast(), original);

    EXPECT_OK(bl_stroke_options_destroy(&core));
    EXPECT_OK(bl_stroke_options_init_move(&core, &moved));
    EXPECT_EQ(core.dcast(), original);
    expect_default(moved);

    EXPECT_OK(bl_stroke_options_destroy(&core));
    EXPECT_OK(original.reset());
    expect_default(original);

    EXPECT_OK(original.reset());
    expect_default(original);
  }
}

TEST_CASE(core_stroke_options_c_api_vs_cxx_api) {
  BLStrokeOptions options;

  EXPECT_EQ(bl_stroke_options_get_width(&options), 1.0f);
  EXPECT_EQ(bl_stroke_options_get_miter_limit(&options), 4.0f);
  EXPECT_EQ(bl_stroke_options_get_dash_offset(&options), 0.0f);
  EXPECT_EQ(bl_stroke_options_get_start_cap(&options), BL_STROKE_CAP_BUTT);
  EXPECT_EQ(bl_stroke_options_get_end_cap(&options), BL_STROKE_CAP_BUTT);
  EXPECT_EQ(bl_stroke_options_get_join(&options), BL_STROKE_JOIN_MITER_CLIP);
  EXPECT_EQ(bl_stroke_options_get_transform_order(&options), BL_STROKE_TRANSFORM_ORDER_AFTER);

  EXPECT_EQ(bl_stroke_options_get_dash_array(&options), options.dash_array().data());
  EXPECT_EQ(bl_stroke_options_get_dash_count(&options), 0u);

  EXPECT_OK(bl_stroke_options_set_width(&options, 10.0f));
  EXPECT_EQ(bl_stroke_options_get_width(&options), 10.0f);

  EXPECT_OK(bl_stroke_options_set_miter_limit(&options, 12.0f));
  EXPECT_EQ(bl_stroke_options_get_miter_limit(&options), 12.0f);

  EXPECT_OK(bl_stroke_options_set_dash_offset(&options, 13.0f));
  EXPECT_EQ(bl_stroke_options_get_dash_offset(&options), 13.0f);

  EXPECT_OK(bl_stroke_options_set_start_cap(&options, BL_STROKE_CAP_ROUND));
  EXPECT_EQ(bl_stroke_options_get_start_cap(&options), BL_STROKE_CAP_ROUND);

  EXPECT_OK(bl_stroke_options_set_end_cap(&options, BL_STROKE_CAP_ROUND_REV));
  EXPECT_EQ(bl_stroke_options_get_end_cap(&options), BL_STROKE_CAP_ROUND_REV);

  EXPECT_OK(bl_stroke_options_set_join(&options, BL_STROKE_JOIN_ROUND));
  EXPECT_EQ(bl_stroke_options_get_join(&options), BL_STROKE_JOIN_ROUND);

  EXPECT_OK(bl_stroke_options_set_transform_order(&options, BL_STROKE_TRANSFORM_ORDER_BEFORE));
  EXPECT_EQ(bl_stroke_options_get_transform_order(&options), BL_STROKE_TRANSFORM_ORDER_BEFORE);

  constexpr float dashes[] { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };
  EXPECT_OK(bl_stroke_options_set_dash_array_data(&options, dashes, BL_ARRAY_SIZE(dashes)));
  EXPECT_EQ(bl_stroke_options_get_dash_array(&options), options.dash_array().data());
}

TEST_CASE(core_stroke_options_alloc_overflow) {
  constexpr size_t invalid_sizes[] = {SIZE_MAX - 1u, SIZE_MAX};

  for (size_t size : invalid_sizes) {
    BLStrokeOptions options;
    EXPECT_EQ(options.set_dash_array(nullptr, size), BL_ERROR_OUT_OF_MEMORY);
    expect_default(options);
  }
}

} // {bl::StrokeOptionsTests}

#endif // BL_TEST
