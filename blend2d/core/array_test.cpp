// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/core/array_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/string_p.h>
#include <blend2d/core/var_p.h>

#include <initializer_list>

// Array - Tests
// =============

namespace bl::ArrayTests {

template<typename T>
struct array_test_values {
  static constexpr size_t kSize = 32;

  static constexpr T data[kSize] = {
    T(42), T( 1), T( 2), T( 3), T( 4), T( 5), T( 6), T( 7),
    T( 8), T( 9), T(10), T(11), T(12), T(13), T(14), T(15),
    T(16), T(17), T(18), T(19), T(20), T(21), T(22), T(23),
    T(24), T(25), T(26), T(27), T(28), T(29), T(30), T(31)
  };
};

template<typename T>
static void verify_sso_storage(const Array<T>& array) noexcept {
  if (array._d.is_sso()) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(array.data());
    for (size_t i = array.size() * sizeof(T); i < BLObjectDetail::kSSOCapacity; i++) {
      EXPECT_EQ(data[i], uint8_t(0))
        .message("SSO storage not zeroed beyond active data - sso_data[%zu]==0x%02X", i, data[i]);
    }
  }
}

template<typename T>
static void verify_elements(const Array<T>& array, const T* expected_elements, size_t expected_size) noexcept {
  EXPECT_EQ(array.size(), expected_size);
  EXPECT_GE(array.capacity(), array.size());
  EXPECT_EQ(array.is_empty(), expected_size == 0u);
  EXPECT_EQ(array._d.type(), BLArray<T>::Info::kType);

  const T* data = array.data();

  for (size_t i = 0; i < expected_size; i++) {
    EXPECT_EQ(data[i], expected_elements[i])
      .message("Array elements don't match at [%zu]", i);
  }
}

TEST_DEFINE_TYPES(array_test_types,
  int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float, double);

TEST_CASE_T(core_array_constructors, array_test_types) {
  BLArray<T> a;
  EXPECT_EQ(a.size(), 0u);
  EXPECT_GT(a.capacity(), 0u);
  EXPECT_TRUE(a._d.is_sso());

  for (size_t size : {size_t(1u), array_test_values<T>::kSize}) {
    EXPECT_OK(a.assign_data(array_test_values<T>::data, size));
    BLArray<T> copy(a);
    BLArray<T> moved(axl::move(a));

    verify_elements(copy, array_test_values<T>::data, size);
    verify_elements(moved, array_test_values<T>::data, size);
    verify_elements(a, array_test_values<T>::data, 0u);

    EXPECT_TRUE(a._d.is_sso());
    if (moved._d.is_dynamic()) {
      EXPECT_EQ(moved.data(), copy.data());
    }
  }
}

TEST_CASE_T(core_array_assign, array_test_types) {
  BLArray<T> a;
  BLArray<T> b;

  EXPECT_OK(a.assign_v(T(0), T(1), T(2)));
  EXPECT_OK(b.assign_v(T(0), T(1), T(2)));
  EXPECT_TRUE(a.equals(b));

  EXPECT_OK(a.assign_v(T( 0), T( 1), T( 2), T( 3), T( 4), T( 5), T( 6), T( 7),
                       T( 8), T( 9), T(10), T(11), T(12), T(13), T(14), T(15),
                       T(16), T(17), T(18), T(19), T(20), T(21), T(22), T(23),
                       T(24), T(25), T(26), T(27), T(28), T(29), T(30), T(31)));
  EXPECT_OK(b.assign_data(a.data(), a.size()));
  EXPECT_TRUE(a.equals(b));

  // Implicit data sharing and copy-on-write.
  EXPECT_OK(a.assign(b));
  EXPECT_EQ(a.data(), b.data());

  // Replacing a single element in `b` should not influence data in `a`.
  EXPECT_OK(b.replace(10, T(123)));
  EXPECT_EQ(a[10], T(10));
  EXPECT_EQ(b[10], T(123));

  // It's expected that `assign_deep()` never uses COW and makes a deep copy instead.
  EXPECT_OK(a.reset());
  EXPECT_OK(b.reset());

  EXPECT_OK(a.assign_data(array_test_values<T>::data, array_test_values<T>::kSize));
  EXPECT_OK(b.assign_deep(a));

  EXPECT_NE(a.data(), b.data());
  EXPECT_TRUE(a.equals(b));

  // Replace populated or shared destinations, including self-assignment.
  for (size_t size : {size_t(0u), size_t(1u), array_test_values<T>::kSize}) {
    EXPECT_OK(b.reset());
    EXPECT_OK(b.assign_data(array_test_values<T>::data, size));
    BLArray<T> previous(b);

    EXPECT_OK(b.assign(a));
    EXPECT_EQ(b.data(), a.data());

    EXPECT_OK(b.assign(b));
    EXPECT_OK(b.assign_deep(b));
    EXPECT_NE(b.data(), a.data());
    verify_elements(b, array_test_values<T>::data, array_test_values<T>::kSize);

    EXPECT_OK(b.assign_deep(a));
    EXPECT_NE(b.data(), a.data());
    verify_elements(b, array_test_values<T>::data, array_test_values<T>::kSize);

    BLArray<T> source(a);
    EXPECT_OK(b.assign(axl::move(source)));
    EXPECT_EQ(b.data(), a.data());
    EXPECT_TRUE(source._d.is_sso());
    verify_elements(source, array_test_values<T>::data, 0u);

    EXPECT_OK(b.assign(axl::move(b)));
    verify_elements(b, array_test_values<T>::data, array_test_values<T>::kSize);
    verify_elements(previous, array_test_values<T>::data, size);
  }

  // Uninitialized API variations make space in the array for N items, without initialization.
  T* ptr;
  EXPECT_OK(a.assign_uninitialized(100u, &ptr));
  EXPECT_EQ(a.data(), ptr);
  EXPECT_EQ(a.size(), 100u);
  EXPECT_GE(a.capacity(), 100u);

  EXPECT_OK(b.reset());
  EXPECT_OK(b.assign_uninitialized(3u, &ptr));
  EXPECT_EQ(b.data(), ptr);
  EXPECT_EQ(b.size(), 3u);
  EXPECT_GE(b.capacity(), 3u);
  EXPECT_TRUE(b._d.is_sso());
}

TEST_CASE(core_array_assign_string) {
  BLArray<BLString> a;

  EXPECT_EQ(a.size(), 0u);
  EXPECT_TRUE(a.is_empty());
  EXPECT_TRUE(a._d.is_dynamic());

  a.assign_v(BLString("Hello"), BLString("Blend2D"));
  EXPECT_EQ(a.size(), 2u);
  EXPECT_TRUE(a[0].equals("Hello"));
  EXPECT_TRUE(a[1].equals("Blend2D"));

  BLString sso_s("SSO STRING");
  BLString dyn_s("TESTING NON-SSO STRING REPRESENTATION");

  a.assign_v(sso_s, dyn_s);
  EXPECT_EQ(a.size(), 2u);
  EXPECT_TRUE(a[0].equals(sso_s));
  EXPECT_TRUE(a[1].equals(dyn_s));
  EXPECT_NE(a[0].data(), sso_s.data());
  EXPECT_EQ(a[1].data(), dyn_s.data());

  EXPECT_OK(dyn_s.assign("This is another string that doesn't fit SSO representation"));
  EXPECT_NE(a[1].data(), dyn_s.data());

  BLArray<BLString> b(a);
  EXPECT_NE(a[1].data(), dyn_s.data());
  EXPECT_TRUE(dyn_s.equals("This is another string that doesn't fit SSO representation"));

  const BLString expected[] = {sso_s, BLString("TESTING NON-SSO STRING REPRESENTATION")};
  const BLString* data_before = a.data();

  BLArray<BLString> moved(axl::move(b));
  EXPECT_TRUE(b._d.is_dynamic());
  verify_elements(b, expected, 0u);
  verify_elements(moved, expected, BL_ARRAY_SIZE(expected));

  EXPECT_OK(moved.assign(axl::move(a)));
  EXPECT_EQ(moved.data(), data_before);

  EXPECT_TRUE(ObjectLayer::is_object_mutable(&moved));
  verify_elements(moved, expected, BL_ARRAY_SIZE(expected));

  EXPECT_TRUE(a._d.is_dynamic());
  verify_elements(a, expected, 0u);

  EXPECT_OK(a.append(dyn_s));
  verify_elements(a, &dyn_s, 1u);
}

TEST_CASE_T(core_array_append, array_test_types) {
  constexpr size_t kSmallCount = 1000u;
  constexpr size_t kLargeCount = 100000u;

  BLArray<T> a;

  EXPECT_OK(a.append(T(42)));
  EXPECT_EQ(a.size(), 1u);
  EXPECT_EQ(a.capacity(), BLArray<T>::Info::kSSOCapacity);
  EXPECT_TRUE(a.equals(BLSpan<const T>(array_test_values<T>::data, a.size())));
  EXPECT_TRUE(a._d.is_sso());

  EXPECT_OK(a.append(T(1), T(2)));
  EXPECT_EQ(a.size(), 3u);
  EXPECT_EQ(a.capacity(), BLArray<T>::Info::kSSOCapacity);
  EXPECT_TRUE(a.equals(BLSpan<const T>(array_test_values<T>::data, a.size())));
  EXPECT_TRUE(a._d.is_sso());

  EXPECT_OK(a.append(T(3), T(4), T(5), T(6), T(7)));
  EXPECT_EQ(a.size(), 8u);
  EXPECT_TRUE(a.equals(BLSpan<const T>(array_test_values<T>::data, a.size())));

  EXPECT_OK(a.append(T( 8), T( 9), T(10), T(11), T(12), T(13), T(14), T(15),
                     T(16), T(17), T(18), T(19), T(20), T(21), T(22), T(23),
                     T(24), T(25), T(26), T(27), T(28), T(29), T(30), T(31)));
  EXPECT_EQ(a.size(), 32u);
  EXPECT_TRUE(a.equals(BLSpan<const T>(array_test_values<T>::data, a.size())));

  EXPECT_OK(a.append_data(array_test_values<T>::data, array_test_values<T>::kSize));
  T expected_elements[2u * array_test_values<T>::kSize];
  for (size_t i = 0; i < BL_ARRAY_SIZE(expected_elements); i++) {
    expected_elements[i] = array_test_values<T>::data[i % array_test_values<T>::kSize];
  }
  verify_elements(a, expected_elements, BL_ARRAY_SIZE(expected_elements));

  // Uninitialized API variations make space in the array for additional N items, without initialization.
  T* ptr;
  EXPECT_OK(a.append_uninitialized(100u, &ptr));
  EXPECT_EQ(a.data() + 64, ptr);
  EXPECT_EQ(a.size(), 164u);
  EXPECT_GE(a.capacity(), 164u);

  // Implicit data sharing and copy-on-write.
  EXPECT_OK(a.reset());
  for (size_t i = 0; i < kSmallCount; i++) {
    BLArray copy(a);
    T value = T(i % 127);
    EXPECT_OK(a.append(value));
    EXPECT_EQ(a.size(), i + 1u);
    EXPECT_EQ(a[i], value);
    EXPECT_EQ(copy.size(), i);
  }

  for (size_t i = 0; i < kSmallCount; i++) {
    T value = T(i % 127);
    EXPECT_EQ(a[i], value);
  }

  // Appending many values.
  EXPECT_OK(a.reset());
  for (size_t i = 0; i < kLargeCount; i++) {
    T value = T(i % 127);
    EXPECT_OK(a.append(value));
    EXPECT_EQ(a.size(), i + 1u);
  }

  for (size_t i = 0; i < kLargeCount; i++) {
    T value = T(i % 127);
    EXPECT_EQ(a[i], value);
  }

  // bl_array_append_item is never called by C++ API as it calls
  // a different function, so verify the functionality explicitly.
  EXPECT_OK(a.reset());
  for (size_t i = 0; i < kLargeCount; i++) {
    T value = T(i % 127);
    EXPECT_OK(bl_array_append_item(&a, &value));
    EXPECT_EQ(a.size(), i + 1u);
  }

  for (size_t i = 0; i < kLargeCount; i++) {
    T value = T(i % 127);
    EXPECT_EQ(a[i], value);
  }
}

TEST_CASE(core_array_append_string) {
  BLArray<BLString> a;

  EXPECT_EQ(a.size(), 0u);
  EXPECT_TRUE(a.is_empty());
  EXPECT_TRUE(a._d.is_dynamic());

  EXPECT_OK(a.append(BLString("Hello"), BLString("Blend2D")));
  EXPECT_EQ(a.size(), 2u);
  EXPECT_TRUE(a[0].equals("Hello"));
  EXPECT_TRUE(a[1].equals("Blend2D"));

  BLString s("TESTING NON-SSO STRING REPRESENTATION");
  EXPECT_OK(a.append(s));
  EXPECT_EQ(a.size(), 3u);
  EXPECT_TRUE(a[0].equals("Hello"));
  EXPECT_TRUE(a[1].equals("Blend2D"));
  EXPECT_TRUE(a[2].equals("TESTING NON-SSO STRING REPRESENTATION"));

  EXPECT_OK(a.append(s));
  EXPECT_TRUE(a[3].equals("TESTING NON-SSO STRING REPRESENTATION"));

  {
    BLArray<BLString> copy(a);
    EXPECT_OK(a.append(BLString("x"), BLString("y")));
    EXPECT_OK(copy.reset());
    EXPECT_FALSE(ObjectLayer::is_object_mutable(&s));
    const BLString expected[] = {
      BLString("Hello"), BLString("Blend2D"), s, s, BLString("x"), BLString("y")
    };
    verify_elements(a, expected, BL_ARRAY_SIZE(expected));
  }
  EXPECT_OK(a.reset());
  EXPECT_TRUE(ObjectLayer::is_object_mutable(&s));
}

TEST_CASE_T(core_array_prepend, array_test_types) {
  constexpr size_t kSmallCount = 1000u;

  BLArray<T> a;

  for (size_t i = 0; i < array_test_values<T>::kSize; i++) {
    EXPECT_OK(a.prepend(array_test_values<T>::data[31 - i]));
    EXPECT_EQ(a.size(), i + 1u);
    EXPECT_TRUE(a.equals(BLSpan<const T>(array_test_values<T>::data + 32 - a.size(), a.size())));
  }

  // Prepend using multi-item overload.
  EXPECT_OK(a.reset());
  EXPECT_OK(a.prepend(T(100)));
  EXPECT_EQ(a.size(), 1u);
  EXPECT_TRUE(a._d.is_sso());

  EXPECT_OK(a.prepend(T(80), T(90)));
  EXPECT_EQ(a.size(), 3u);
  EXPECT_TRUE(a._d.is_sso());

  EXPECT_OK(a.prepend(T(40), T(50), T(60), T(70)));
  EXPECT_EQ(a.size(), 7u);

  const T some_data[3] = {T(10), T(20), T(30)};
  EXPECT_OK(a.prepend_data(some_data, 3));
  EXPECT_EQ(a.size(), 10u);

  EXPECT_OK(a.prepend(T(0)));
  EXPECT_EQ(a.size(), 11u);

  EXPECT_EQ(a[0], T(0));
  EXPECT_EQ(a[1], T(10));
  EXPECT_EQ(a[2], T(20));
  EXPECT_EQ(a[3], T(30));
  EXPECT_EQ(a[4], T(40));
  EXPECT_EQ(a[5], T(50));
  EXPECT_EQ(a[6], T(60));
  EXPECT_EQ(a[7], T(70));
  EXPECT_EQ(a[8], T(80));
  EXPECT_EQ(a[9], T(90));
  EXPECT_EQ(a[10], T(100));

  // Implicit data sharing and copy-on-write.
  EXPECT_OK(a.reset());
  for (size_t i = 0; i < kSmallCount; i++) {
    BLArray copy(a);
    T value = T(i % 127);
    EXPECT_OK(a.prepend(value));
    EXPECT_EQ(a.size(), i + 1u);
    EXPECT_EQ(a[0], value);
    EXPECT_EQ(copy.size(), i);
  }

  for (size_t i = 0; i < kSmallCount; i++) {
    T value = T(i % 127);
    EXPECT_EQ(a[a.size() - i - 1u], value);
  }
}

TEST_CASE_T(core_array_insert, array_test_types) {
  BLArray<T> a;

  EXPECT_OK(a.assign_v(T(100), T(127)));
  EXPECT_EQ(a.size(), 2u);
  EXPECT_EQ(a[0], T(100));
  EXPECT_EQ(a[1], T(127));

  EXPECT_OK(a.insert(1, T(101)));
  EXPECT_EQ(a.size(), 3u);
  EXPECT_EQ(a[0], T(100));
  EXPECT_EQ(a[1], T(101));
  EXPECT_EQ(a[2], T(127));

  EXPECT_OK(a.insert(1, T(103)));
  EXPECT_EQ(a.size(), 4u);
  EXPECT_EQ(a[0], T(100));
  EXPECT_EQ(a[1], T(103));
  EXPECT_EQ(a[2], T(101));
  EXPECT_EQ(a[3], T(127));

  T expected_data[4u + array_test_values<T>::kSize] = {T(100), T(103)};
  T expected_v[8u + array_test_values<T>::kSize] = {T(100), T(103), T(40), T(50), T(60), T(70)};
  for (size_t i = 0; i < array_test_values<T>::kSize; i++) {
    expected_data[2u + i] = array_test_values<T>::data[i];
    expected_v[6u + i] = array_test_values<T>::data[i];
  }
  expected_data[BL_ARRAY_SIZE(expected_data) - 2u] = T(101);
  expected_data[BL_ARRAY_SIZE(expected_data) - 1u] = T(127);
  expected_v[BL_ARRAY_SIZE(expected_v) - 2u] = T(101);
  expected_v[BL_ARRAY_SIZE(expected_v) - 1u] = T(127);

  EXPECT_OK(a.insert_data(2, array_test_values<T>::data, array_test_values<T>::kSize));
  verify_elements(a, expected_data, BL_ARRAY_SIZE(expected_data));

  EXPECT_OK(a.insert(2, T(40), T(50), T(60), T(70)));
  verify_elements(a, expected_v, BL_ARRAY_SIZE(expected_v));
}

TEST_CASE(core_array_insert_string) {
  BLArray<BLString> a;

  a.insert(0, BLString("Blend2D"));
  a.insert(0, BLString("Hello"));
  EXPECT_EQ(a.size(), 2u);
  EXPECT_TRUE(a[0].equals("Hello"));
  EXPECT_TRUE(a[1].equals("Blend2D"));

  a.insert(2, BLString("World!"));
  EXPECT_EQ(a.size(), 3u);
  EXPECT_TRUE(a[0].equals("Hello"));
  EXPECT_TRUE(a[1].equals("Blend2D"));
  EXPECT_TRUE(a[2].equals("World!"));

  a.insert_data(1, a.span());
  EXPECT_EQ(a.size(), 6u);
  EXPECT_TRUE(a[0].equals("Hello"));
  EXPECT_TRUE(a[1].equals("Hello"));
  EXPECT_TRUE(a[2].equals("Blend2D"));
  EXPECT_TRUE(a[3].equals("World!"));
  EXPECT_TRUE(a[4].equals("Blend2D"));
  EXPECT_TRUE(a[5].equals("World!"));

  BLString s("TESTING NON-SSO STRING REPRESENTATION");
  EXPECT_OK(a.reset());
  EXPECT_OK(a.append(BLString("a"), BLString("b")));
  while (a.size() < a.capacity()) {
    EXPECT_OK(a.append(BLString("b")));
  }
  EXPECT_OK(a.insert(1u, s));
  EXPECT_FALSE(ObjectLayer::is_object_mutable(&s));
  EXPECT_TRUE(a[1].equals(s));
  EXPECT_OK(a.reset());
  EXPECT_TRUE(ObjectLayer::is_object_mutable(&s));
}

TEST_CASE_T(core_array_remove, array_test_types) {
  BLArray<T> a;

  EXPECT_OK(a.assign_v(T(0), T(1), T(2)));
  EXPECT_EQ(a.size(), 3u);

  EXPECT_OK(a.remove(BLRange{1, 2}));
  EXPECT_EQ(a.size(), 2u);
  EXPECT_EQ(a[0], T(0));
  EXPECT_EQ(a[1], T(2));

  EXPECT_OK(a.assign_data(array_test_values<T>::data, array_test_values<T>::kSize));
  EXPECT_EQ(a.size(), array_test_values<T>::kSize);

  EXPECT_OK(a.remove(BLRange{1, 30}));
  EXPECT_EQ(a.size(), 3u);
  EXPECT_EQ(a[0], T(42));
  EXPECT_EQ(a[1], T(30));
  EXPECT_EQ(a[2], T(31));

  BLArray<T> b(a);

  EXPECT_OK(a.remove(1, 3));
  EXPECT_EQ(a.size(), 1u);
  EXPECT_EQ(a[0], T(42));
  EXPECT_EQ(b.size(), 3u);

  constexpr size_t kArraySize = 600u;
  T expected[kArraySize];
  for (size_t i = 0; i < kArraySize; i++) {
    expected[i] = T(i & 0x7Fu);
  }

  for (size_t size = 0; size < kArraySize; size++) {
    for (size_t remove_from = 0; remove_from < size; remove_from++) {
      EXPECT_OK(a.reset());
      EXPECT_OK(a.assign_data(expected, size));
      verify_elements(a, expected, size);

      BLArray<T> copy(a);
      EXPECT_OK(a.remove(BLRange{remove_from, size}));
      verify_elements(a, expected, remove_from);
      verify_sso_storage(a);
      verify_elements(copy, expected, size);

      EXPECT_OK(copy.remove(BLRange{remove_from, size}));
      verify_elements(copy, expected, remove_from);
      verify_sso_storage(copy);
    }
  }
}

// Reference implementation of `replace_data()` used to verify the real one. Builds the expected array by concatenating
// the unchanged leading part `[0, index)`, the replacement `ins`, and the unchanged trailing part `[end, size)`. The
// range is clamped the same way the implementation clamps it (`end` to size, `start` to `end`).
template<typename T>
static BLArray<T> array_replace_data_reference(BLSpan<const T> src, size_t r_start, size_t r_end, BLSpan<const T> ins) noexcept {
  size_t end = bl_min(r_end, src.size());
  size_t index = bl_min(r_start, end);

  BLArray<T> out;
  EXPECT_OK(out.append_data(src.data(), index));
  EXPECT_OK(out.append_data(ins.data(), ins.size()));
  EXPECT_OK(out.append_data(src.data() + end, src.size() - end));
  return out;
}

TEST_CASE_T(core_array_self_alias, array_test_types) {
  for (size_t size : {size_t(1u), BLArray<T>::Info::kSSOCapacity, array_test_values<T>::kSize}) {
    BLSpan<const T> base{array_test_values<T>::data, size};
    for (unsigned mode = 0; mode < 3u; mode++) {
      for (size_t offset : {size_t(0u), size_t(1u)}) {
        for (unsigned operation = 0; operation < 4u; operation++) {
          BLArray<T> a;
          EXPECT_OK(a.assign_data(base));

          if (mode == 1u) {
            EXPECT_OK(a.reserve(2u * array_test_values<T>::kSize));
          }

          BLArray<T> copy;

          if (mode == 2u) {
            EXPECT_OK(copy.assign(a));
          }

          // Assign, append, prepend, or insert into the middle.
          size_t index = operation == 1u ? size : operation == 3u ? size / 2u : 0u;
          size_t end = operation == 0u ? size : index;
          BLSpan<const T> source{a.data() + offset, size - offset};
          BLArray<T> expected = array_replace_data_reference<T>(base, index, end, {base.data() + offset, source.size()});
          const T* data_before = a.data();

          switch (operation) {
            case 0: EXPECT_OK(a.assign_data(source)); break;
            case 1: EXPECT_OK(a.append_data(source)); break;
            case 2: EXPECT_OK(a.prepend_data(source)); break;
            case 3: EXPECT_OK(a.insert_data(index, source)); break;
          }

          verify_elements(a, expected.data(), expected.size());
          verify_sso_storage(a);
          if (mode == 1u) {
            EXPECT_EQ(a.data(), data_before);
          }

          if (mode == 2u) {
            verify_elements(copy, base.data(), base.size());
          }
        }
      }
    }
  }
}

// Verifies `replace_data()` with a non-aliased replacement buffer against the reference.
template<typename T>
static void test_array_replace_data_non_aliased(BLSpan<const T> base, size_t r_start, size_t r_end, BLSpan<const T> ins) noexcept {
  BLArray<T> a;
  EXPECT_OK(a.assign_data(base.data(), base.size()));

  BLArray<T> expected = array_replace_data_reference<T>(base, r_start, r_end, ins);
  EXPECT_OK(a.replace_data(BLRange{r_start, r_end}, ins));

  EXPECT_EQ(a.size(), expected.size());
  EXPECT_TRUE(a.equals(expected));
}

// Verifies `replace_data()` where the replacement span aliases the array's own data. The array is made mutable (and
// optionally reserves `min_capacity`) so that the in-place code path - which has to cope with the aliasing - is taken.
template<typename T>
static void test_array_replace_data_aliased(BLSpan<const T> base, size_t r_start, size_t r_end, size_t src_off, size_t src_len, size_t min_capacity = 0u) noexcept {
  BLArray<T> a;
  EXPECT_OK(a.assign_data(base.data(), base.size()));

  if (min_capacity) {
    EXPECT_OK(a.reserve(min_capacity));
  }

  // Make sure the array is uniquely owned & mutable so the in-place (aliasing) path is taken when capacity allows.
  T* mutable_data;
  EXPECT_OK(a.make_mutable(&mutable_data));

  // The replacement points into the array's own data - the implementation must read it before overwriting.
  BLSpan<const T> aliased_ins{a.data() + src_off, src_len};
  BLArray<T> expected = array_replace_data_reference<T>(base, r_start, r_end, BLSpan<const T>{base.data() + src_off, src_len});

  EXPECT_OK(a.replace_data(BLRange{r_start, r_end}, aliased_ins));
  EXPECT_EQ(a.size(), expected.size());
  EXPECT_TRUE(a.equals(expected));
}

TEST_CASE_T(core_array_replace_data, array_test_types) {
  BLSpan<const T> full{array_test_values<T>::data, array_test_values<T>::kSize};

  const T small[] = { T(10), T(11), T(12), T(13), T(14), T(15) };
  BLSpan<const T> small_span{small, BL_ARRAY_SIZE(small)};

  const T ins0[] = { T(0) };
  const T ins2[] = { T(90), T(91) };
  const T ins4[] = { T(90), T(91), T(92), T(93) };
  const T ins10[] = { T(90), T(91), T(92), T(93), T(94), T(95), T(96), T(97), T(98), T(99) };

  BLSpan<const T> empty{ins0, 0u};
  BLSpan<const T> r2{ins2, BL_ARRAY_SIZE(ins2)};
  BLSpan<const T> r4{ins4, BL_ARRAY_SIZE(ins4)};
  BLSpan<const T> r10{ins10, BL_ARRAY_SIZE(ins10)};

  // Non-aliased replacements exercising every relation between range size and replacement size.
  test_array_replace_data_non_aliased<T>(full, 4, 8, r4);          // Same size (in-place assign).
  test_array_replace_data_non_aliased<T>(full, 4, 12, r2);         // Shrink.
  test_array_replace_data_non_aliased<T>(full, 4, 6, r10);         // Grow.
  test_array_replace_data_non_aliased<T>(full, 10, 20, empty);     // Remove (empty replacement).
  test_array_replace_data_non_aliased<T>(full, 5, 5, r4);          // Insert (empty range).
  test_array_replace_data_non_aliased<T>(full, 0, 5, r2);          // Replace a prefix.
  test_array_replace_data_non_aliased<T>(full, 28, 32, r2);        // Replace a suffix.
  test_array_replace_data_non_aliased<T>(full, 0, SIZE_MAX, r4);   // Replace everything (BLRange::everything()).
  test_array_replace_data_non_aliased<T>(full, 0, 0, empty);       // No-op.

  // Range clamping.
  test_array_replace_data_non_aliased<T>(full, 30, 100, r4);       // r_end clamped to size.
  test_array_replace_data_non_aliased<T>(full, 100, 200, r4);      // r_start beyond size -> append.
  test_array_replace_data_non_aliased<T>(full, 10, 5, r4);         // r_start > r_end -> insert at clamped position.

  // Small (SSO) arrays.
  test_array_replace_data_non_aliased<T>(small_span, 1, 3, r2);    // Stays in SSO.
  test_array_replace_data_non_aliased<T>(small_span, 2, 4, r10);   // Grows SSO -> dynamic.
  test_array_replace_data_non_aliased<T>(small_span, 0, 6, empty); // Clears the array.

  // Aliased replacements - the replacement points into the array's own data.
  test_array_replace_data_aliased<T>(full, 10, 14, 0, 4);          // Source precedes the range (same size).
  test_array_replace_data_aliased<T>(full, 4, 8, 20, 4);           // Source follows the range (same size).
  test_array_replace_data_aliased<T>(full, 4, 12, 8, 8);           // Source overlaps the range (same size).
  test_array_replace_data_aliased<T>(full, 4, 20, 24, 4);          // Shrink; source is in the shifted tail.
  test_array_replace_data_aliased<T>(full, 15, 16, 0, 32);         // Grow a lot from the whole array (realloc path).

  // Aliased in-place grow - reserve enough capacity so the growth happens in place, forcing the aliasing memmove.
  test_array_replace_data_aliased<T>(full, 4, 6, 0, 10, 64);       // Source overlaps range + leading part.
  test_array_replace_data_aliased<T>(full, 4, 6, 20, 10, 64);      // Source is in the tail that gets shifted right.
  test_array_replace_data_aliased<T>(full, 4, 6, 8, 10, 64);       // Source spans unmoved and shifted data.

  // Copy-on-write: replacing a range in a shared array must not affect the other reference.
  {
    BLArray<T> a;
    EXPECT_OK(a.assign_data(full.data(), full.size()));

    BLArray<T> b(a);
    EXPECT_EQ(a.data(), b.data());

    EXPECT_OK(a.replace_data(BLRange{4, 8}, r2));

    BLArray<T> expected = array_replace_data_reference<T>(full, 4, 8, r2);
    EXPECT_TRUE(a.equals(expected));
    EXPECT_NE(a.data(), b.data());
    EXPECT_TRUE(b.equals(full));
  }

  // Direct C-API check.
  {
    BLArrayCore c;
    EXPECT_OK(bl_array_init(&c, BLArray<T>::Info::kType));

    const T items[] = { T(1), T(2), T(3), T(4), T(5) };
    EXPECT_OK(bl_array_append_data(&c, items, BL_ARRAY_SIZE(items)));

    const T repl[] = { T(80), T(81) };
    EXPECT_OK(bl_array_replace_data(&c, 1, 3, repl, BL_ARRAY_SIZE(repl)));

    const T after[] = { T(1), T(80), T(81), T(4), T(5) };
    EXPECT_EQ(bl_array_get_size(&c), BL_ARRAY_SIZE(after));
    for (size_t i = 0; i < BL_ARRAY_SIZE(after); i++)
      EXPECT_EQ(static_cast<const T*>(bl_array_get_data(&c))[i], after[i]);

    EXPECT_OK(bl_array_destroy(&c));
  }
}

TEST_CASE(core_array_replace_data_string) {
  BLArray<BLString> a;
  EXPECT_OK(a.append(BLString("a")));
  EXPECT_OK(a.append(BLString("b")));
  EXPECT_OK(a.append(BLString("c")));
  EXPECT_OK(a.append(BLString("d")));
  EXPECT_OK(a.append(BLString("e")));

  // Reserve so the following (non-aliased) replace happens in-place, exercising the object release+retain+move path.
  EXPECT_OK(a.reserve(16));

  // Replace [1, 3) (b, c) by three new strings - the array grows in-place and the tail (d, e) is shifted right.
  BLString repl[] = { BLString("X"), BLString("Y"), BLString("Z") };
  EXPECT_OK(a.replace_data(BLRange{1, 3}, repl, BL_ARRAY_SIZE(repl)));

  const char* expected_1[] = { "a", "X", "Y", "Z", "d", "e" };
  EXPECT_EQ(a.size(), BL_ARRAY_SIZE(expected_1));
  for (size_t i = 0; i < BL_ARRAY_SIZE(expected_1); i++)
    EXPECT_TRUE(a[i].equals(expected_1[i]));

  // Aliased in-place replace: replace [0, 2) by the whole array within the reserved capacity.
  EXPECT_OK(a.replace_data(BLRange{0, 2}, a.span()));

  const char* expected_2[] = { "a", "X", "Y", "Z", "d", "e", "Y", "Z", "d", "e" };
  EXPECT_EQ(a.size(), BL_ARRAY_SIZE(expected_2));
  for (size_t i = 0; i < BL_ARRAY_SIZE(expected_2); i++)
    EXPECT_TRUE(a[i].equals(expected_2[i]));

  BLString s("TESTING NON-SSO STRING REPRESENTATION");
  const BLString t("ANOTHER NON-SSO STRING REPRESENTATION");
  for (bool shared : {false, true}) {
    EXPECT_OK(a.assign_v(s, s, s));
    EXPECT_OK(a.reserve(16u));
    BLArray<BLString> copy;

    if (shared) {
      copy = a;
    }

    EXPECT_OK(a.replace_data(BLRange{1u, 2u}, &t, 1u));

    if (shared) {
      copy = a;
    }

    EXPECT_OK(a.replace_data(BLRange{1u, 2u}, a.data(), 2u));

    {
      const BLString expected[] = {s, s, t, s};
      const BLString expected_copy[] = {s, t, s};
      verify_elements(a, expected, BL_ARRAY_SIZE(expected));

      if (shared) {
        verify_elements(copy, expected_copy, BL_ARRAY_SIZE(expected_copy));
      }
    }

    EXPECT_OK(copy.reset());
    EXPECT_OK(a.reset());
    EXPECT_TRUE(ObjectLayer::is_object_mutable(&s));
    EXPECT_TRUE(ObjectLayer::is_object_mutable(&t));
  }

  const BLString replacement("replacement");
  for (size_t index = 0; index < 3u; index++) {
    EXPECT_OK(a.assign_v(s, s, s));

    BLArray<BLString> copy(a);
    EXPECT_OK(a.replace(index, replacement));
    EXPECT_EQ(a.size(), 3u);

    for (size_t i = 0; i < 3u; i++) {
      EXPECT_TRUE(a[i].equals(i == index ? replacement : s));
      EXPECT_TRUE(copy[i].equals(s));

    }
    EXPECT_OK(copy.reset());
    EXPECT_FALSE(ObjectLayer::is_object_mutable(&s));

    EXPECT_OK(a.reset());
    EXPECT_TRUE(ObjectLayer::is_object_mutable(&s));
  }
}

TEST_CASE_T(core_array_truncate, array_test_types) {
  constexpr size_t kArraySize = 1000;
  T expected_elements[kArraySize];

  for (size_t i = 0; i < kArraySize; i++) {
    expected_elements[i] = T(i & 0x7F);
  }

  for (bool shared : {false, true}) {
    for (bool truncate_instead_of_resize : {false, true}) {
      BLArray<T> a;
      BLArray<T> copy;

      for (size_t i = 1u; i < kArraySize; i++) {
        if (shared) {
          copy = a;
        }

        EXPECT_OK(a.resize(i, expected_elements[i - 1u]));
        verify_elements(a, expected_elements, i);
        verify_sso_storage(a);
        if (shared) {
          verify_elements(copy, expected_elements, i - 1u);
        }
      }

      for (size_t i = kArraySize; i != 0; i--) {
        if (shared) {
          copy = a;
        }

        if (truncate_instead_of_resize) {
          EXPECT_OK(a.truncate(i - 1u));
        }
        else{
          EXPECT_OK(a.resize(i - 1u, T(0)));
        }
        verify_elements(a, expected_elements, i - 1u);
        verify_sso_storage(a);
      }
    }
  }
}

TEST_CASE(core_array_truncate_string) {
  const BLString expected[] = {
    BLString("First string that doesn't fit in SSO"),
    BLString("Second string that doesn't fit in SSO")
  };
  {
    BLArray<BLString> a;
    EXPECT_OK(a.reserve(64u));
    EXPECT_OK(a.assign_data(expected, BL_ARRAY_SIZE(expected)));
    EXPECT_OK(a.truncate(1u));
    verify_elements(a, expected, 1u);

    EXPECT_OK(a.resize(0u, BLString()));
    verify_elements(a, expected, 0u);

    const BLString expected_fill[] = {expected[0], expected[0]};
    EXPECT_OK(a.resize(BL_ARRAY_SIZE(expected_fill), expected[0]));
    verify_elements(a, expected_fill, BL_ARRAY_SIZE(expected_fill));
    EXPECT_OK(a.shrink());

    const BLString expected_grown[] = {expected[0], expected[0], expected[1], expected[1]};
    BLArray<BLString> copy(a);
    EXPECT_OK(a.resize(4u, expected[1]));
    verify_elements(a, expected_grown, 4u);
    verify_elements(copy, expected_fill, 2u);

    EXPECT_LT(copy.capacity(), 4u);
    EXPECT_OK(copy.resize(4u, expected[1]));
    verify_elements(copy, expected_grown, 4u);

    copy = a;
    EXPECT_OK(a.truncate(1u));
    verify_elements(a, expected, 1u);
    verify_elements(copy, expected_grown, 4u);

    EXPECT_OK(a.assign(copy));
    EXPECT_OK(a.remove(BLRange{1u, 3u}));
    verify_elements(a, expected, 2u);

    EXPECT_OK(a.remove(1u));
    verify_elements(a, expected, 1u);

    EXPECT_OK(a.clear());
    verify_elements(a, expected, 0u);

    EXPECT_OK(a.assign(copy));
    EXPECT_OK(a.clear());
    EXPECT_TRUE(a._d.is_dynamic());
    verify_elements(a, expected, 0u);
    verify_elements(copy, expected_grown, 4u);
  }
  for (const BLString& s : expected)
    EXPECT_TRUE(ObjectLayer::is_object_mutable(&s));
}

TEST_CASE_T(core_array_accessors, array_test_types) {
  BLArray<T> a;
  EXPECT_TRUE(a._d.is_sso());

  EXPECT_OK(a.assign_v(T(100), T(123), T(127)));
  EXPECT_TRUE(a._d.is_sso());
  EXPECT_EQ(a.first(), T(100));
  EXPECT_EQ(a.last(), T(127));
  EXPECT_EQ(a.begin(), a.data());
  EXPECT_EQ(a.end(), a.data() + 3u);

  EXPECT_OK(a.assign_v(T(1), T(13), T(16), T(17), T(19), T(24), T(32), T(55), T(77), T(99), T(100), T(103), T(123), T(124), T(126)));
  EXPECT_EQ(a.first(), T(1));
  EXPECT_EQ(a.last(), T(126));
  EXPECT_EQ(a.begin(), a.data());
  EXPECT_EQ(a.end(), a.data() + 15u);

  // Overloaded operators `==`, `!=`, and `operator bool`.
  BLArray<T> b(a);
  EXPECT_EQ(a, b);
  EXPECT_TRUE(static_cast<bool>(a));
  EXPECT_TRUE(static_cast<bool>(b));

  BLArray<T> c;
  EXPECT_NE(a, c);
  EXPECT_FALSE(static_cast<bool>(c));
}

TEST_CASE_T(core_array_swap, array_test_types) {
  BLArray<T> a;
  BLArray<T> b;

  EXPECT_OK(a.assign_v(T(100), T(123), T(127)));
  {
    BLArray<T> t(a);
    a.swap(b);
    EXPECT_EQ(a.size(), 0u);
    EXPECT_EQ(b.size(), 3u);
    EXPECT_NE(a, t);
    EXPECT_EQ(b, t);
  }

  EXPECT_OK(a.assign_v(T(1), T(13), T(16), T(17), T(19), T(24), T(32), T(55), T(77), T(99), T(100), T(103), T(123), T(124), T(126)));
  EXPECT_OK(b.reset());
  {
    BLArray<T> t(a);
    a.swap(b);
    EXPECT_EQ(a.size(), 0u);
    EXPECT_EQ(b.size(), 15u);
    EXPECT_NE(a, t);
    EXPECT_EQ(b, t);
  }
}

TEST_CASE_T(core_array_search, array_test_types) {
  BLArray<T> a;

  // Empty array can be searched, but there cannot be match.
  EXPECT_EQ(a.index_of(T(0)), SIZE_MAX);
  EXPECT_EQ(a.index_of(T(0), 0), SIZE_MAX);
  EXPECT_EQ(a.index_of(T(0), 64), SIZE_MAX);
  EXPECT_EQ(a.last_index_of(T(0)), SIZE_MAX);
  EXPECT_EQ(a.last_index_of(T(0), 0), SIZE_MAX);
  EXPECT_EQ(a.last_index_of(T(0), 64), SIZE_MAX);

  // Array having duplicate values:
  //
  //   Index: {#0 #1 #2 #3 #4 #5}
  //   Value: { 5  7  5  9  7  5}
  //
  // Value 5 is at indexes {0, 2, 5}, value 7 at {1, 4}, and value 9 at {3}.
  EXPECT_OK(a.assign_v(T(5), T(7), T(5), T(9), T(7), T(5)));

  // Basic `index_of()` functionality.
  EXPECT_EQ(a.index_of(T(5)), 0u);
  EXPECT_EQ(a.index_of(T(7)), 1u);
  EXPECT_EQ(a.index_of(T(9)), 3u);
  EXPECT_EQ(a.index_of(T(3)), SIZE_MAX);
  EXPECT_EQ(a.index_of(T(5), 0), 0u);
  EXPECT_EQ(a.index_of(T(5), 1), 2u);
  EXPECT_EQ(a.index_of(T(5), 3), 5u);
  EXPECT_EQ(a.index_of(T(5), 5), 5u);
  EXPECT_EQ(a.index_of(T(5), 6), SIZE_MAX);
  EXPECT_EQ(a.index_of(T(5), 64), SIZE_MAX);
  EXPECT_EQ(a.index_of(T(7), 2), 4u);

  // Basic `last_index_of()` functionality.
  EXPECT_EQ(a.last_index_of(T(5)), 5u);
  EXPECT_EQ(a.last_index_of(T(7)), 4u);
  EXPECT_EQ(a.last_index_of(T(9)), 3u);
  EXPECT_EQ(a.last_index_of(T(3)), SIZE_MAX);
  EXPECT_EQ(a.last_index_of(T(5), 5), 5u);
  EXPECT_EQ(a.last_index_of(T(5), 4), 2u);
  EXPECT_EQ(a.last_index_of(T(5), 1), 0u);
  EXPECT_EQ(a.last_index_of(T(5), 0), 0u);
  EXPECT_EQ(a.last_index_of(T(5), 64), 5u);
  EXPECT_EQ(a.last_index_of(T(7), 3), 1u);
  EXPECT_EQ(a.last_index_of(T(7), 0), SIZE_MAX);
  EXPECT_EQ(a.last_index_of(T(9), 2), SIZE_MAX);
  EXPECT_EQ(a.last_index_of(T(9), 64), 3u);

  // Larger array of distinct values to exercise the dynamically allocated representation
  // for all types (the values above fit into SSO storage when the item size is small).
  EXPECT_OK(a.assign_data(array_test_values<T>::data, array_test_values<T>::kSize));
  EXPECT_EQ(a.index_of(T(42)), 0u);
  EXPECT_EQ(a.index_of(T(20)), 20u);
  EXPECT_EQ(a.index_of(T(31)), 31u);
  EXPECT_EQ(a.index_of(T(99)), SIZE_MAX);
  EXPECT_EQ(a.index_of(T(42), 1), SIZE_MAX);
  EXPECT_EQ(a.index_of(T(31), 1000), SIZE_MAX);

  EXPECT_EQ(a.last_index_of(T(42)), 0u);
  EXPECT_EQ(a.last_index_of(T(20)), 20u);
  EXPECT_EQ(a.last_index_of(T(31)), 31u);
  EXPECT_EQ(a.last_index_of(T(31), 1000), 31u);
  EXPECT_EQ(a.last_index_of(T(31), 30), SIZE_MAX);
  EXPECT_EQ(a.last_index_of(T(99)), SIZE_MAX);
}

TEST_CASE_T(core_array_clear, array_test_types) {
  BLArray<T> a;

  EXPECT_OK(a.assign_v(T(0)));
  EXPECT_EQ(a.size(), 1u);
  EXPECT_TRUE(a._d.is_sso());

  EXPECT_OK(a.assign_data(array_test_values<T>::data, array_test_values<T>::kSize));
  EXPECT_EQ(a.size(), array_test_values<T>::kSize);
  EXPECT_TRUE(a._d.is_dynamic());

  EXPECT_OK(a.clear());
  EXPECT_EQ(a.size(), 0u);
  EXPECT_TRUE(a._d.is_dynamic());

  EXPECT_OK(a.assign_data(array_test_values<T>::data, array_test_values<T>::kSize));
  EXPECT_EQ(a.size(), array_test_values<T>::kSize);
  EXPECT_TRUE(a._d.is_dynamic());

  // Clearing an array when it's implicitly shared makes it SSO.
  BLArray<T> b(a);

  EXPECT_OK(a.clear());
  EXPECT_EQ(a.size(), 0u);
  EXPECT_TRUE(a._d.is_sso());
}

TEST_CASE_T(core_array_external, array_test_types) {
  BLArray<T> a;
  T external_data[10] = {T(10), T(11), T(12), T(13), T(14), T(15), T(16), T(17), T(18), T(19)};

  struct DestroyData {
    BLObjectCore* object;
    void* external_data;
    unsigned counter;
  };

  DestroyData destroy_data {};
  auto destroy_func = [](BLObjectCore* object, void* external_data, void* user_data) noexcept {
    DestroyData* dd = static_cast<DestroyData*>(user_data);
    dd->object = object;
    dd->external_data = external_data;
    dd->counter++;
  };

  // Const external data.
  EXPECT_OK(a.assign_external_data_const(external_data, 10, 10, destroy_func, &destroy_data));
  EXPECT_EQ(a.data(), external_data);
  EXPECT_EQ(a.size(), 10u);
  EXPECT_EQ(a.capacity(), 10u);
  EXPECT_EQ(destroy_data.object, nullptr);
  EXPECT_EQ(destroy_data.external_data, nullptr);
  EXPECT_EQ(destroy_data.counter, 0u);

  EXPECT_EQ(a[0], T(10));
  EXPECT_EQ(a[1], T(11));
  EXPECT_EQ(a[2], T(12));
  EXPECT_EQ(a[3], T(13));
  EXPECT_EQ(a[4], T(14));
  EXPECT_EQ(a[5], T(15));
  EXPECT_EQ(a[6], T(16));
  EXPECT_EQ(a[7], T(17));
  EXPECT_EQ(a[8], T(18));
  EXPECT_EQ(a[9], T(19));

  EXPECT_OK(a.replace(0, T(123)));
  EXPECT_EQ(destroy_data.object, &a);
  EXPECT_EQ(destroy_data.external_data, external_data);
  EXPECT_EQ(destroy_data.counter, 1u);
  destroy_data.object = nullptr;
  destroy_data.external_data = nullptr;

  EXPECT_NE(a.data(), external_data);
  EXPECT_EQ(a.size(), 10u);
  EXPECT_GE(a.capacity(), 10u);
  EXPECT_EQ(a[0], T(123));
  EXPECT_EQ(external_data[0], T(10));

  // Mutable external data.
  EXPECT_OK(a.assign_external_data_mut(external_data, 0, 10, destroy_func, &destroy_data));
  EXPECT_EQ(a.data(), external_data);
  EXPECT_EQ(a.size(), 0u);
  EXPECT_EQ(a.capacity(), 10u);

  EXPECT_OK(a.reset());
  EXPECT_EQ(destroy_data.object, &a);
  EXPECT_EQ(destroy_data.external_data, external_data);
  EXPECT_EQ(destroy_data.counter, 2u);
  destroy_data.object = nullptr;
  destroy_data.external_data = nullptr;

  EXPECT_OK(a.assign_external_data_mut(external_data, 0, 10, destroy_func, &destroy_data));
  EXPECT_EQ(a.data(), external_data);
  EXPECT_EQ(a.size(), 0u);
  EXPECT_EQ(a.capacity(), 10u);
  EXPECT_EQ(destroy_data.counter, 2u);

  EXPECT_OK(a.append(T(42)));
  EXPECT_EQ(external_data[0], T(42));
  EXPECT_EQ(destroy_data.counter, 2u);

  EXPECT_OK(a.append(T(1), T(2), T(3), T(4), T(5), T(6), T(7), T(8), T(9)));
  EXPECT_EQ(external_data[0], T(42));
  EXPECT_EQ(external_data[1], T(1));
  EXPECT_EQ(external_data[2], T(2));
  EXPECT_EQ(external_data[3], T(3));
  EXPECT_EQ(external_data[4], T(4));
  EXPECT_EQ(external_data[5], T(5));
  EXPECT_EQ(external_data[6], T(6));
  EXPECT_EQ(external_data[7], T(7));
  EXPECT_EQ(external_data[8], T(8));
  EXPECT_EQ(external_data[9], T(9));
  EXPECT_EQ(destroy_data.counter, 2u);

  // Appending more items the external array can hold must reallocate it - such action must call destroy_func().
  EXPECT_OK(a.append(T(100)));
  EXPECT_EQ(destroy_data.object, &a);
  EXPECT_EQ(destroy_data.external_data, external_data);
  EXPECT_EQ(destroy_data.counter, 3u);
  destroy_data.object = nullptr;
  destroy_data.external_data = nullptr;

  EXPECT_NE(a.data(), external_data);
  EXPECT_EQ(a.size(), 11u);
  EXPECT_GE(a.capacity(), 11u);
  EXPECT_EQ(a[0], T(42));
  EXPECT_EQ(a[1], T(1));
  EXPECT_EQ(a[2], T(2));
  EXPECT_EQ(a[3], T(3));
  EXPECT_EQ(a[4], T(4));
  EXPECT_EQ(a[5], T(5));
  EXPECT_EQ(a[6], T(6));
  EXPECT_EQ(a[7], T(7));
  EXPECT_EQ(a[8], T(8));
  EXPECT_EQ(a[9], T(9));
  EXPECT_EQ(a[10], T(100));

  // Wrong external data - destroy_func is never called when the function fails.
  EXPECT_EQ(a.assign_external_data_const(external_data, 0, 0, destroy_func, &destroy_data), BL_ERROR_INVALID_VALUE);
  EXPECT_EQ(a.assign_external_data_const(external_data, 10, 0, destroy_func, &destroy_data), BL_ERROR_INVALID_VALUE);
  EXPECT_EQ(a.assign_external_data_const(external_data, 10, 9, destroy_func, &destroy_data), BL_ERROR_INVALID_VALUE);
  EXPECT_EQ(destroy_data.counter, 3u);

  EXPECT_EQ(a.assign_external_data_mut(external_data, 0, 0, destroy_func, &destroy_data), BL_ERROR_INVALID_VALUE);
  EXPECT_EQ(a.assign_external_data_mut(external_data, 10, 0, destroy_func, &destroy_data), BL_ERROR_INVALID_VALUE);
  EXPECT_EQ(a.assign_external_data_mut(external_data, 10, 9, destroy_func, &destroy_data), BL_ERROR_INVALID_VALUE);
  EXPECT_EQ(destroy_data.counter, 3u);

  // Shared external storage is released only when its final owner goes away.
  for (bool read_only : {false, true}) {
    for (bool detach : {false, true}) {
      unsigned counter = destroy_data.counter;
      EXPECT_OK(a.assign_external_data(external_data, 10u, 10u,
        read_only ? BL_DATA_ACCESS_MODE_RO : BL_DATA_ACCESS_MODE_RW, destroy_func, &destroy_data));
      {
        BLArray<T> copy(a);
        if (detach) {
          EXPECT_OK(a.replace(0u, T(123)));
          EXPECT_NE(a.data(), external_data);
          EXPECT_EQ(external_data[0], T(42));
        }
        EXPECT_OK(a.reset());
        EXPECT_EQ(destroy_data.counter, counter);
        EXPECT_EQ(copy.data(), external_data);
        verify_elements(copy, array_test_values<T>::data, 10u);
        EXPECT_OK(copy.reset());
        EXPECT_EQ(destroy_data.object, &copy);
        EXPECT_EQ(destroy_data.external_data, external_data);
        EXPECT_EQ(destroy_data.counter, counter + 1u);
      }
      EXPECT_EQ(destroy_data.counter, counter + 1u);
    }
  }
}

TEST_CASE_T(core_array_make_mutable, array_test_types) {
  T* ptr;

  // SSO arrays are always mutable - make_mutable() returns the SSO data in-place (no reallocation).
  {
    BLArray<T> a;
    EXPECT_OK(a.append(T(0), T(1), T(2)));
    EXPECT_TRUE(a._d.is_sso());

    const T* data_before = a.data();
    EXPECT_OK(a.make_mutable(&ptr));
    EXPECT_EQ(ptr, data_before);
    EXPECT_EQ(a.data(), data_before);

    ptr[1] = T(42);
    EXPECT_EQ(a[1], T(42));
  }

  // A uniquely owned dynamic array is already mutable - make_mutable() must not reallocate.
  {
    BLArray<T> a;
    EXPECT_OK(a.assign_data(array_test_values<T>::data, array_test_values<T>::kSize));
    EXPECT_TRUE(a._d.is_dynamic());

    const T* data_before = a.data();
    EXPECT_OK(a.make_mutable(&ptr));
    EXPECT_EQ(ptr, data_before);
    EXPECT_EQ(a.data(), data_before);

    ptr[0] = T(123);
    EXPECT_EQ(a[0], T(123));
  }

  // A dynamic array shared via a weak copy is immutable - make_mutable() must
  // perform a copy-on-write and leave the other instance untouched.
  {
    BLArray<T> a;
    EXPECT_OK(a.assign_data(array_test_values<T>::data, array_test_values<T>::kSize));

    BLArray<T> b(a);
    const T* shared = a.data();
    EXPECT_EQ(b.data(), shared);

    EXPECT_OK(a.make_mutable(&ptr));
    EXPECT_EQ(a.data(), ptr);
    EXPECT_NE(ptr, shared);        // `a` got its own copy.
    EXPECT_EQ(b.data(), shared);   // `b` still points to the original data...
    EXPECT_TRUE(a.equals(b));      // ...which is still identical in content.

    // Mutating `a` through the returned pointer must not affect `b`.
    ptr[0] = T(123);
    EXPECT_EQ(a[0], T(123));
    EXPECT_EQ(b[0], T(42));
    EXPECT_FALSE(a.equals(b));
  }

  // Read-only external data is immutable - make_mutable() must copy the data
  // into a mutable buffer and leave the external buffer untouched. Exercise both SSO and dynamic results.
  for (size_t size : {size_t(1u), array_test_values<T>::kSize}) {
    T external[array_test_values<T>::kSize];
    memcpy(external, array_test_values<T>::data, sizeof(external));

    BLArray<T> a;
    EXPECT_OK(a.assign_external_data_const(external, size, size));
    EXPECT_EQ(a.data(), external);

    EXPECT_OK(a.make_mutable(&ptr));
    EXPECT_EQ(a.data(), ptr);
    EXPECT_NE(ptr, external);
    verify_elements(a, external, size);

    ptr[0] = T(123);
    EXPECT_EQ(a[0], T(123));
    EXPECT_EQ(external[0], T(42));  // The external buffer must remain unchanged.
  }
}

TEST_CASE_T(core_array_c_api, array_test_types) {
  // Misuse of C-API - calling `bl_array_init()` with invalid object type.
  BLArrayCore invalid_array;
  EXPECT_EQ(bl_array_init(&invalid_array, BL_OBJECT_TYPE_PATTERN), BL_ERROR_INVALID_VALUE);

  BLArrayCore a;
  BLArray<T> b;

  EXPECT_OK(bl_array_init(&a, BLArray<T>::Info::kType));
  EXPECT_EQ(bl_array_get_size(&a), b.size());
  EXPECT_EQ(bl_array_get_capacity(&a), b.capacity());

  const T items[] = { T(1), T(2), T(3), T(4), T(5) };
  EXPECT_OK(bl_array_append_data(&a, items, BL_ARRAY_SIZE(items)));
  EXPECT_EQ(bl_array_get_size(&a), 5u);

  for (size_t i = 0; i < BL_ARRAY_SIZE(items); i++) {
    EXPECT_EQ(static_cast<const T*>(bl_array_get_data(&a))[i], items[i]);
  }

  EXPECT_OK(bl_array_insert_data(&a, 1, items, BL_ARRAY_SIZE(items)));
  const T items_after_insertion[] = { T(1), T(1), T(2), T(3), T(4), T(5), T(2), T(3), T(4), T(5) };

  for (size_t i = 0; i < BL_ARRAY_SIZE(items_after_insertion); i++) {
    EXPECT_EQ(static_cast<const T*>(bl_array_get_data(&a))[i], items_after_insertion[i]);
  }

  EXPECT_OK(bl_array_destroy(&a));
}

TEST_CASE_T(core_array_reserve_shrink, array_test_types) {
  BLArray<T> a;

  static const T data_0[] = {T(0), T(1), T(2)};
  static const T data_1[] = {T(3), T(4), T(5)};
  static const T data_01[] = {T(0), T(1), T(2), T(3), T(4), T(5)};

  EXPECT_OK(a.append_data(data_0, BL_ARRAY_SIZE(data_0)));
  EXPECT_TRUE(a.equals(BLSpan<const T>{data_0, BL_ARRAY_SIZE(data_0)}));
  EXPECT_TRUE(a._d.is_sso());

  EXPECT_OK(a.shrink());
  EXPECT_TRUE(a.equals(BLSpan<const T>{data_0, BL_ARRAY_SIZE(data_0)}));
  EXPECT_TRUE(a._d.is_sso());

  EXPECT_OK(a.reserve(1024));
  EXPECT_GE(a.capacity(), 1024u);
  EXPECT_TRUE(a.equals(BLSpan<const T>{data_0, BL_ARRAY_SIZE(data_0)}));

  EXPECT_OK(a.append_data(data_1, BL_ARRAY_SIZE(data_1)));
  EXPECT_EQ(a.size(), BL_ARRAY_SIZE(data_0) + BL_ARRAY_SIZE(data_1));
  EXPECT_GE(a.capacity(), 1024u);
  EXPECT_TRUE(a.equals(BLSpan<const T>{data_01, BL_ARRAY_SIZE(data_01)}));

  EXPECT_OK(a.shrink());
  EXPECT_EQ(a.size(), BL_ARRAY_SIZE(data_0) + BL_ARRAY_SIZE(data_1));
  EXPECT_TRUE(a.equals(BLSpan<const T>{data_01, BL_ARRAY_SIZE(data_01)}));

  BLArray<T> b(a);

  EXPECT_OK(a.truncate(BL_ARRAY_SIZE(data_0)));
  EXPECT_TRUE(a.equals(BLSpan<const T>{data_0, BL_ARRAY_SIZE(data_0)}));
  EXPECT_TRUE(b.equals(BLSpan<const T>{data_01, BL_ARRAY_SIZE(data_01)}));

  EXPECT_OK(a.shrink());
  EXPECT_TRUE(a.equals(BLSpan<const T>{data_0, BL_ARRAY_SIZE(data_0)}));
  EXPECT_TRUE(a._d.is_sso());

  EXPECT_OK(a.reserve_for_append(0u));
  EXPECT_TRUE(a.equals(BLSpan<const T>{data_0, BL_ARRAY_SIZE(data_0)}));
  EXPECT_TRUE(a._d.is_sso());

  EXPECT_OK(a.reserve_for_append(1000u));
  EXPECT_TRUE(a.equals(BLSpan<const T>{data_0, BL_ARRAY_SIZE(data_0)}));
  EXPECT_TRUE(a._d.is_dynamic());
}

TEST_CASE_T(core_array_alloc_overflow, array_test_types) {
  BLArray<T> a;

  for (size_t size : {SIZE_MAX, SIZE_MAX/sizeof(T)}) {
    EXPECT_EQ(a.assign_data(nullptr, size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(a.append_data(nullptr, size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(a.prepend_data(nullptr, size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(a.insert_data(0, nullptr, size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(a.insert_data(size, nullptr, size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(a.insert_data(SIZE_MAX, nullptr, size), BL_ERROR_OUT_OF_MEMORY);
  }
}

TEST_CASE_T(core_array_resize_overflow, array_test_types) {
  for (size_t size : {SIZE_MAX, SIZE_MAX/sizeof(T)}) {
    BLArray<T> a;
    EXPECT_EQ(a.resize(size, T(0)), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(a.size(), 0u);

    EXPECT_OK(a.assign_data(array_test_values<T>::data, array_test_values<T>::kSize));
    EXPECT_EQ(a.resize(size, T(0)), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(a.size(), array_test_values<T>::kSize);
  }
}

TEST_CASE_T(core_array_reserve_overflow, array_test_types) {
  for (size_t size : {SIZE_MAX, SIZE_MAX/sizeof(T)}) {
    BLArray<T> a;
    size_t initial_capacity = a.capacity();

    EXPECT_EQ(a.reserve(size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(a.capacity(), initial_capacity);

    EXPECT_EQ(a.reserve_for_append(size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(a.capacity(), initial_capacity);

    EXPECT_OK(a.assign_data(array_test_values<T>::data, array_test_values<T>::kSize));
    size_t capacity_after_assignment = a.capacity();

    EXPECT_EQ(a.reserve(size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(a.capacity(), capacity_after_assignment);

    EXPECT_EQ(a.reserve_for_append(size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(a.capacity(), capacity_after_assignment);
  }
}

} // {bl::ArrayTests}

#endif // BL_TEST
