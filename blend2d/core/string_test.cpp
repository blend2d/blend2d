// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/core/array_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/string_p.h>
#include <blend2d/core/runtime_p.h>

#include <initializer_list>

// String - Tests
// ==============

namespace bl::StringTests {

static void verify_string(const BLString& s) noexcept {
  size_t size = s.size();
  const char* data = s.data();

  EXPECT_EQ(data[size], 0)
    .message("BLString's data is not null terminated");

  if (s._d.is_sso()) {
    for (size_t i = size; i < BLString::Info::kSSOCapacity; i++) {
      EXPECT_EQ(data[i], 0)
        .message("BLString's SSO data is invalid - found non-null character at [%zu], after string size %zu", i, size);
    }
  }
}

TEST_CASE(core_string_constructors) {
  BLString empty0("");
  EXPECT_TRUE(empty0.is_empty());
  verify_string(empty0);

  BLString empty1("abc", 0);
  EXPECT_TRUE(empty1.is_empty());
  verify_string(empty1);

  BLString s0("abc");
  EXPECT_TRUE(s0.equals("abc"));
  verify_string(s0);

  BLString s1("abc", 3);
  EXPECT_TRUE(s1.equals("abc"));
  verify_string(s1);

  BLString s2(BLStringView("abc", 3));
  EXPECT_TRUE(s2.equals("abc"));
  verify_string(s2);

  BLString s3("abcdefghijklmnopqrstuvwxyz0123456789");
  EXPECT_TRUE(s3.equals("abcdefghijklmnopqrstuvwxyz0123456789"));
  verify_string(s3);

  BLString s4("abcdefghijklmnopqrstuvwxyz0123456789", 36);
  EXPECT_TRUE(s4.equals("abcdefghijklmnopqrstuvwxyz0123456789"));
  EXPECT_EQ(s3, s4);
  verify_string(s4);

  BLString s5(BLStringView("abcdefghijklmnopqrstuvwxyz0123456789", 36));
  EXPECT_TRUE(s5.equals("abcdefghijklmnopqrstuvwxyz0123456789"));
  EXPECT_EQ(s3, s5);
  verify_string(s5);

  BLString s6(axl::move(s5));
  EXPECT_EQ(s5.size(), 0u);
  EXPECT_TRUE(s6.equals("abcdefghijklmnopqrstuvwxyz0123456789"));
  verify_string(s6);

  BLStringView multi_spans_sso[3] = {
    BLStringView{"abc", 3},
    BLStringView{"ABCD", 4},
    BLStringView{"12345", 5}
  };
  BLString s7({multi_spans_sso, 3});
  EXPECT_TRUE(s7.equals("abcABCD12345"));
  verify_string(s7);

  BLStringView multi_spans_dyn[6] = {
    BLStringView{"abc", 3},
    BLStringView{"ABCD", 4},
    BLStringView{"12345", 5},
    BLStringView{"{--------}", 10},
    BLStringView{"[========]", 10},
    BLStringView{"<~~~~~~~~>", 10},
  };
  BLString s8({multi_spans_dyn, 6});
  EXPECT_TRUE(s8.equals("abcABCD12345{--------}[========]<~~~~~~~~>"));
  verify_string(s8);
}

TEST_CASE(core_string_assign) {
  BLString s;

  char tmp_buffer[BLString::Info::kSSOCapacity];
  for (size_t i = 0; i < BLString::Info::kSSOCapacity; i++) {
    tmp_buffer[i] = char(size_t('a') + i);
    EXPECT_OK(s.assign(tmp_buffer, i + 1));
    EXPECT_TRUE(s.equals(tmp_buffer, i + 1));
    verify_string(s);
  }

  EXPECT_OK(s.assign('b'));
  verify_string(s);
  EXPECT_EQ(s.size(), 1u);
  EXPECT_EQ(s[0], 'b');
  EXPECT_TRUE(s.equals("b"   ));
  EXPECT_TRUE(s.equals("b", 1));
  EXPECT_GT(s.compare("a"    ), 0);
  EXPECT_GT(s.compare("a" , 1), 0);
  EXPECT_GT(s.compare("a?"   ), 0);
  EXPECT_GT(s.compare("a?", 2), 0);
  EXPECT_EQ(s.compare("b"    ), 0);
  EXPECT_EQ(s.compare("b" , 1), 0);
  EXPECT_LT(s.compare("b?"   ), 0);
  EXPECT_LT(s.compare("b?", 2), 0);
  EXPECT_LT(s.compare("c"    ), 0);
  EXPECT_LT(s.compare("c" , 1), 0);
  EXPECT_LT(s.compare("c?"   ), 0);
  EXPECT_LT(s.compare("c?", 2), 0);

  EXPECT_OK(s.assign('b', 4));
  verify_string(s);
  EXPECT_EQ(s.size(), 4u);
  EXPECT_EQ(s[0], 'b');
  EXPECT_EQ(s[1], 'b');
  EXPECT_EQ(s[2], 'b');
  EXPECT_EQ(s[3], 'b');
  EXPECT_TRUE(s.equals("bbbb"   ));
  EXPECT_TRUE(s.equals("bbbb", 4));
  EXPECT_EQ(s.compare("bbbb"   ), 0);
  EXPECT_EQ(s.compare("bbbb", 4), 0);
  EXPECT_GT(s.compare("bbba"   ), 0);
  EXPECT_GT(s.compare("bbba", 4), 0);
  EXPECT_LT(s.compare("bbbc"   ), 0);
  EXPECT_LT(s.compare("bbbc", 4), 0);

  EXPECT_OK(s.assign("abc"));
  verify_string(s);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(s[0], 'a');
  EXPECT_EQ(s[1], 'b');
  EXPECT_EQ(s[2], 'c');
  EXPECT_TRUE(s.equals("abc"));
  EXPECT_TRUE(s.equals("abc", 3));

  EXPECT_OK(s.assign("TESTING NON-SSO STRING REPRESENTATION"));
  EXPECT_TRUE(s.equals("TESTING NON-SSO STRING REPRESENTATION"));
  EXPECT_TRUE(s._d.is_dynamic());
  EXPECT_OK(s.reset());
  EXPECT_TRUE(s._d.is_sso());
  verify_string(s);

  EXPECT_OK(s.assign("TESTING NON-SSO STRING REPRESENTATION !@#$%^&*()", 45));
  EXPECT_TRUE(s.equals("TESTING NON-SSO STRING REPRESENTATION !@#$%^&"));
  EXPECT_TRUE(s._d.is_dynamic());

  EXPECT_OK(s.assign("{}ABCDEF", 6));
  EXPECT_TRUE(s.equals("{}ABCD"));
  EXPECT_TRUE(s._d.is_dynamic());
  verify_string(s);

  // Making a copy of `s` would involve COW during assignment - assigning less than
  // sso-storage means that the string would be turned into SSO again as the size fits.
  BLString s2(s);
  EXPECT_OK(s.assign("{}ABCDEF{}GHIJ", 10));
  EXPECT_TRUE(s.equals("{}ABCDEF{}"));
  EXPECT_TRUE(s._d.is_sso());
  EXPECT_TRUE(s2.equals("{}ABCD"));
  verify_string(s);

  EXPECT_OK(s.assign(s2));
  EXPECT_TRUE(s.equals("{}ABCD"));
  verify_string(s);

  EXPECT_OK(s.assign("TESTING NON-SSO STRING REPRESENTATION !@#$%^&*()", 45));
  EXPECT_TRUE(s.equals("TESTING NON-SSO STRING REPRESENTATION !@#$%^&"));
  verify_string(s);

  EXPECT_OK(s.assign("{}ABCDEF{}GHIJ", 10));
  EXPECT_TRUE(s.equals("{}ABCDEF{}"));
  EXPECT_TRUE(s._d.is_dynamic());
  verify_string(s);
}

TEST_CASE(core_string_assign_aliased) {
  BLString s;
  EXPECT_OK(s.assign("abcdefghij"));
  EXPECT_OK(s.assign(BLSpan<const char>{s.data() + 2, 4}));
  verify_string(s);
  EXPECT_TRUE(s.equals("cdef"));

  EXPECT_OK(s.prepend('=', 4));
  EXPECT_OK(s.append('-', 333));
  verify_string(s);

  BLString s_copy(s);
  EXPECT_EQ(s_copy.data(), s.data());

  EXPECT_OK(s.assign(BLSpan<const char>{s.data() + 2, 20}));
  EXPECT_TRUE(s.equals("==cdef--------------"));
  EXPECT_NE(s_copy.data(), s.data());
  verify_string(s);

  EXPECT_OK(s_copy.assign(BLSpan<const char>{s_copy.data() + 2, 20}));
  EXPECT_TRUE(s_copy.equals("==cdef--------------"));
  verify_string(s_copy);

  EXPECT_OK(s.assign(s));
  EXPECT_TRUE(s.equals(s_copy));

  // Self-assign a dynamically allocated string that isn't mutable.
  EXPECT_OK(s.reset());
  EXPECT_OK(s.assign("ABCDEFGHIJKLMNOPQRSTUVEXYZ_0123456789"));

  s_copy = s;
  EXPECT_OK(s.assign(BLSpan<const char>{s.data(), 10u}));
  EXPECT_TRUE(s._d.is_sso());
  EXPECT_TRUE(s.equals("ABCDEFGHIJ"));

  EXPECT_FALSE(s.equals(s_copy));
  EXPECT_OK(s_copy.truncate(10u));
  EXPECT_TRUE(s.equals(s_copy));
}

TEST_CASE(core_string_append) {
  static constexpr char dashes[] = "--------------------------------------------------"
                                   "--------------------------------------------------";

  BLString s;

  for (uint32_t i = 0; i < BLString::Info::kSSOCapacity; i++) {
    char c = char('a' + i);
    EXPECT_OK(s.append(c));
    EXPECT_TRUE(s._d.is_sso());
    EXPECT_EQ(s.data()[i], c);
    verify_string(s);
  }

  EXPECT_OK(s.assign('-', 1));
  EXPECT_OK(s.append('-', 99));
  EXPECT_TRUE(s.equals(dashes, 100u));

  EXPECT_OK(s.reset());
  for (size_t i = 0; i < 100u; i++) {
    EXPECT_OK(s.append('-'));
  }
  EXPECT_TRUE(s.equals(dashes, 100u));

  EXPECT_OK(s.reset());
  for (size_t i = 0; i < 10u; i++) {
    EXPECT_OK(s.append("----------"));
  }
  EXPECT_TRUE(s.equals(dashes, 100u));

  EXPECT_OK(s.reset());
  EXPECT_OK(s.append("abcdefghijklmnopqrst"));
  EXPECT_TRUE(s.equals("abcdefghijklmnopqrst", 20));
  EXPECT_OK(s.append("ABCDEFGHIJ"));
  EXPECT_TRUE(s.equals("abcdefghijklmnopqrstABCDEFGHIJ", 30));

  BLString u(s);
  EXPECT_OK(s.append('1'));
  EXPECT_TRUE(s.equals("abcdefghijklmnopqrstABCDEFGHIJ1", 31));
  EXPECT_OK(u.append("1xx"));
  EXPECT_TRUE(u.equals("abcdefghijklmnopqrstABCDEFGHIJ1xx", 33));
}

TEST_CASE(core_string_append_aliased) {
  BLString s;
  EXPECT_OK(s.assign("abcdefghij"));
  EXPECT_OK(s.append(BLSpan<const char>{s.data() + 2, 4}));
  verify_string(s);
  EXPECT_TRUE(s.equals("abcdefghijcdef"));

  EXPECT_OK(s.prepend('=', 4));
  EXPECT_OK(s.append('-', 30));
  verify_string(s);

  BLString s_copy(s);
  EXPECT_EQ(s_copy.data(), s.data());

  EXPECT_OK(s.append(BLSpan<const char>{s.data() + 2, 20}));
  EXPECT_TRUE(s.equals("====abcdefghijcdef------------------------------==abcdefghijcdef----"));
  EXPECT_NE(s_copy.data(), s.data());
  verify_string(s);

  EXPECT_OK(s_copy.append(BLSpan<const char>{s_copy.data() + 2, 20}));
  EXPECT_TRUE(s_copy.equals("====abcdefghijcdef------------------------------==abcdefghijcdef----"));
  verify_string(s_copy);

  EXPECT_OK(s.append(s));
  EXPECT_TRUE(s.equals("====abcdefghijcdef------------------------------==abcdefghijcdef----"
                       "====abcdefghijcdef------------------------------==abcdefghijcdef----"));
}

TEST_CASE(core_string_prepend) {
  BLString s;

  EXPECT_OK(s.prepend("world"));
  EXPECT_TRUE(s.equals("world"));
  verify_string(s);

  EXPECT_OK(s.prepend("hello "));
  EXPECT_TRUE(s.equals("hello world"));
  verify_string(s);

  EXPECT_OK(s.prepend('!'));
  EXPECT_TRUE(s.equals("!hello world"));
  EXPECT_OK(s.prepend('>', 3));
  EXPECT_TRUE(s.equals(">>>!hello world"));
  verify_string(s);

  EXPECT_OK(s.prepend("[tag]ignored", 5));
  EXPECT_TRUE(s.equals("[tag]>>>!hello world"));
  verify_string(s);

  BLString other;
  EXPECT_OK(other.assign("PFX:"));
  EXPECT_OK(s.prepend(other));
  EXPECT_TRUE(s.equals("PFX:[tag]>>>!hello world"));
  verify_string(s);

  EXPECT_OK(s.prepend(other.span()));
  EXPECT_TRUE(s.equals("PFX:PFX:[tag]>>>!hello world"));
  verify_string(s);

  BLString t;
  EXPECT_OK(t.assign("tail"));
  EXPECT_TRUE(t._d.is_sso());
  EXPECT_OK(t.prepend('p', BLString::Info::kSSOCapacity));
  EXPECT_EQ(t.size(), BLString::Info::kSSOCapacity + 4u);
  EXPECT_TRUE(t._d.is_dynamic());
  EXPECT_EQ(t[t.size() - 1u], 'l');
  EXPECT_EQ(t[0], 'p');
  verify_string(t);
}

TEST_CASE(core_string_prepend_aliased) {
  BLString s;
  EXPECT_OK(s.assign("abcdefghij"));
  EXPECT_OK(s.prepend(BLSpan<const char>{s.data() + 2, 4}));
  verify_string(s);
  EXPECT_TRUE(s.equals("cdefabcdefghij"));

  EXPECT_OK(s.prepend('=', 4));
  EXPECT_OK(s.append('-', 30));
  verify_string(s);

  BLString s_copy(s);
  EXPECT_EQ(s_copy.data(), s.data());

  EXPECT_OK(s.prepend(BLSpan<const char>{s.data() + 2, 20}));
  EXPECT_TRUE(s.equals("==cdefabcdefghij----====cdefabcdefghij------------------------------"));
  EXPECT_NE(s_copy.data(), s.data());
  verify_string(s);

  EXPECT_OK(s_copy.prepend(BLSpan<const char>{s_copy.data() + 2, 20}));
  EXPECT_TRUE(s_copy.equals("==cdefabcdefghij----====cdefabcdefghij------------------------------"));
  verify_string(s_copy);

  EXPECT_OK(s.prepend(s));
  EXPECT_TRUE(s.equals("==cdefabcdefghij----====cdefabcdefghij------------------------------"
                       "==cdefabcdefghij----====cdefabcdefghij------------------------------"));
}

TEST_CASE(core_string_insert) {
  BLString s;

  EXPECT_OK(s.assign("0123456789"));
  EXPECT_OK(s.insert(5, '-', 3));
  EXPECT_TRUE(s.equals("01234---56789"));
  verify_string(s);

  EXPECT_OK(s.insert(s.size(), ":END"));
  EXPECT_TRUE(s.equals("01234---56789:END"));
  verify_string(s);

  EXPECT_OK(s.insert(0, "START:", 6));
  EXPECT_TRUE(s.equals("START:01234---56789:END"));
  verify_string(s);

  EXPECT_OK(s.insert(10, ""));
  EXPECT_TRUE(s.equals("START:01234---56789:END"));
  verify_string(s);

  EXPECT_OK(s.insert(10, "<ignored>", 0));
  EXPECT_TRUE(s.equals("START:01234---56789:END"));
  verify_string(s);

  BLString s2(s);
  EXPECT_OK(s2.insert(12, 'x', 20));
  EXPECT_TRUE(s2.equals("START:01234-xxxxxxxxxxxxxxxxxxxx--56789:END"));
  verify_string(s2);

  BLString t;
  EXPECT_OK(t.assign("<M>"));
  EXPECT_OK(s.insert(6, t));
  EXPECT_TRUE(s.equals("START:<M>01234---56789:END"));
  verify_string(s);

  EXPECT_OK(s.insert(10, "<Long String to Make it Dynamic>"));
  EXPECT_TRUE(s.equals("START:<M>0<Long String to Make it Dynamic>1234---56789:END"));
  verify_string(s);

  BLString s3(s);
  EXPECT_OK(s.insert(10, "[COW]"));
  EXPECT_TRUE(s.equals("START:<M>0[COW]<Long String to Make it Dynamic>1234---56789:END"));
  EXPECT_TRUE(s3.equals("START:<M>0<Long String to Make it Dynamic>1234---56789:END"));
  verify_string(s);
}

TEST_CASE(core_string_insert_at_or_beyond_end) {
  static constexpr char inserted[] = {'X', '\0', 'Y'};
  static constexpr char expected[] = "0123456789X\0Y";
  static constexpr size_t indexes[] = {10u, 11u, SIZE_MAX};

  for (uint32_t mode = 0; mode < 3; mode++) {
    for (size_t index : indexes) {
      BLString s("0123456789");
      if (mode != 0) {
        EXPECT_OK(s.reserve(128));
        EXPECT_TRUE(s._d.is_dynamic());
      }

      BLString peer;
      if (mode == 2) {
        EXPECT_OK(peer.assign(s));
        EXPECT_EQ(peer.data(), s.data());
      }

      const char* original_data = s.data();
      EXPECT_OK(s.insert(index, inserted, sizeof(inserted)));
      EXPECT_EQ(s.size(), sizeof(expected) - 1u);
      EXPECT_TRUE(s.equals(expected, sizeof(expected) - 1u));
      verify_string(s);

      if (mode == 1) {
        EXPECT_EQ(s.data(), original_data);
      }
      if (mode == 2) {
        EXPECT_NE(s.data(), peer.data());
        EXPECT_TRUE(peer.equals("0123456789"));
        verify_string(peer);
      }
    }
  }

  BLString empty;
  EXPECT_OK(empty.insert(SIZE_MAX, "abc"));
  EXPECT_TRUE(empty.equals("abc"));
  verify_string(empty);
}

TEST_CASE(core_string_insert_zero_count) {
  static constexpr size_t indexes[] = {0u, 5u, 10u, SIZE_MAX};

  for (uint32_t mode = 0; mode < 3; mode++) {
    for (size_t index : indexes) {
      for (uint32_t use_char = 0; use_char < 2; use_char++) {
        BLString s("0123456789");
        if (mode != 0) {
          EXPECT_OK(s.reserve(128));
        }

        BLString peer;
        if (mode == 2) {
          EXPECT_OK(peer.assign(s));
        }

        if (use_char) {
          EXPECT_OK(s.insert(index, '?', 0));
        }
        else {
          EXPECT_OK(s.insert(index, "ignored", 0));
        }

        EXPECT_TRUE(s.equals("0123456789"));
        verify_string(s);

        if (mode == 2) {
          EXPECT_TRUE(peer.equals("0123456789"));
          verify_string(peer);
        }
      }
    }
  }

  BLString empty;
  EXPECT_OK(empty.insert(0, '?', 0));
  EXPECT_OK(empty.insert(SIZE_MAX, "ignored", 0));
  EXPECT_TRUE(empty.is_empty());
  verify_string(empty);
}

TEST_CASE(core_string_insert_aliased_positions) {
  static const struct {
    size_t source_index;
    const char* expected;
  } cases[] = {
    {0u, "0123401256789"},
    {5u, "0123456756789"},
    {7u, "0123478956789"}
  };

  for (const auto& test : cases) {
    BLString s("0123456789");
    EXPECT_OK(s.reserve(128));
    const char* original_data = s.data();

    EXPECT_OK(s.insert(5, BLStringView{s.data() + test.source_index, 3}));
    EXPECT_EQ(s.data(), original_data);
    EXPECT_TRUE(s.equals(test.expected));
    verify_string(s);
  }
}

TEST_CASE(core_string_insert_aliased) {
  BLString s;
  EXPECT_OK(s.assign("(a[bcde]fghij)"));
  EXPECT_OK(s.insert(9, BLSpan<const char>{s.data() + 2, 6}));
  verify_string(s);
  EXPECT_TRUE(s.equals("(a[bcde]f[bcde]ghij)"));

  EXPECT_OK(s.prepend('=', 12));
  EXPECT_OK(s.append('-', 12));
  verify_string(s);

  BLString s_copy(s);
  EXPECT_EQ(s_copy.data(), s.data());

  EXPECT_OK(s.insert(30, BLSpan<const char>{s.data() + 11, 22}));
  EXPECT_TRUE(s.equals("============(a[bcde]f[bcde]ghi=(a[bcde]f[bcde]ghij)-j)------------"));
  EXPECT_NE(s_copy.data(), s.data());
  verify_string(s);

  EXPECT_OK(s_copy.insert(30, BLSpan<const char>{s_copy.data() + 11, 22}));
  EXPECT_TRUE(s_copy.equals("============(a[bcde]f[bcde]ghi=(a[bcde]f[bcde]ghij)-j)------------"));
  verify_string(s_copy);

  EXPECT_OK(s.insert(10, s));
  EXPECT_TRUE(s.equals("=========="
                       "============(a[bcde]f[bcde]ghi=(a[bcde]f[bcde]ghij)-j)------------"
                       "==(a[bcde]f[bcde]ghi=(a[bcde]f[bcde]ghij)-j)------------"));
}

TEST_CASE(core_string_remove) {
  BLString s;

  EXPECT_OK(s.assign("0123456789"));
  EXPECT_OK(s.remove(BLRange{0, 0})); // empty range is nop.
  EXPECT_TRUE(s.equals("0123456789"));
  EXPECT_OK(s.remove(BLRange{5, 5})); // range with zero size is nop.
  EXPECT_TRUE(s.equals("0123456789"));
  EXPECT_OK(s.remove(BLRange{5, 2})); // inverted range is nop.
  EXPECT_TRUE(s.equals("0123456789"));
  verify_string(s);

  EXPECT_OK(s.remove(0));
  EXPECT_TRUE(s.equals("123456789"));
  verify_string(s);

  EXPECT_OK(s.remove(0));
  EXPECT_TRUE(s.equals("23456789"));
  verify_string(s);

  EXPECT_OK(s.remove(5));
  EXPECT_TRUE(s.equals("2345689"));
  verify_string(s);

  EXPECT_OK(s.remove(s.size() - 1u));
  EXPECT_TRUE(s.equals("234568"));
  verify_string(s);

  EXPECT_OK(s.append("abcdefghijklmnopqrstuvwxyz"));
  EXPECT_TRUE(s.equals("234568abcdefghijklmnopqrstuvwxyz"));
  verify_string(s);

  EXPECT_OK(s.remove(0));
  EXPECT_TRUE(s.equals("34568abcdefghijklmnopqrstuvwxyz"));
  verify_string(s);

  EXPECT_OK(s.remove(10));
  EXPECT_TRUE(s.equals("34568abcdeghijklmnopqrstuvwxyz"));
  verify_string(s);

  BLString s2(s);
  BLString s3(s);
  EXPECT_OK(s3.remove(10));
  EXPECT_TRUE(s.equals("34568abcdeghijklmnopqrstuvwxyz"));
  EXPECT_TRUE(s2.equals("34568abcdeghijklmnopqrstuvwxyz"));
  EXPECT_TRUE(s3.equals("34568abcdehijklmnopqrstuvwxyz"));
  verify_string(s);

  EXPECT_OK(s.remove(BLRange{5, 20}));
  EXPECT_TRUE(s.equals("34568qrstuvwxyz"));
  verify_string(s);

  EXPECT_OK(s2.remove(BLRange{5, 20}));
  EXPECT_TRUE(s2.equals("34568qrstuvwxyz"));
  EXPECT_TRUE(s3.equals("34568abcdehijklmnopqrstuvwxyz"));
  verify_string(s);

  // Invalid index
  EXPECT_OK(s2.remove(1000));
  EXPECT_TRUE(s2.equals("34568qrstuvwxyz"));

  EXPECT_OK(s3.remove(SIZE_MAX));
  EXPECT_TRUE(s3.equals("34568abcdehijklmnopqrstuvwxyz"));

  // Invalid/empty ranges.
  EXPECT_OK(s2.remove(BLRange{SIZE_MAX, 1}));
  EXPECT_TRUE(s2.equals("34568qrstuvwxyz"));

  EXPECT_OK(s3.remove(BLRange{0, 0}));
  EXPECT_TRUE(s3.equals("34568abcdehijklmnopqrstuvwxyz"));

  // Oversized ranges.
  EXPECT_OK(s2.remove(BLRange{1, SIZE_MAX}));
  EXPECT_TRUE(s2.equals("3"));

  EXPECT_OK(s3.remove(BLRange{10, SIZE_MAX}));
  EXPECT_TRUE(s3.equals("34568abcde"));
}

TEST_CASE(core_string_clear) {
  BLString s;

  EXPECT_OK(s.assign("abcdefghijklmnopqrstuvwxyz"));
  EXPECT_TRUE(s.equals("abcdefghijklmnopqrstuvwxyz"));
  EXPECT_TRUE(s._d.is_sso());
  verify_string(s);

  EXPECT_OK(s.clear());
  EXPECT_TRUE(s.is_empty());
  EXPECT_TRUE(s._d.is_sso());
  EXPECT_EQ(s.size(), 0u);
  verify_string(s);

  EXPECT_OK(s.assign("abcdefghijklmnopqrstuvwxyz"));
  EXPECT_OK(s.append("-0123456789"));
  EXPECT_TRUE(s.equals("abcdefghijklmnopqrstuvwxyz-0123456789"));
  verify_string(s);

  BLString s2(s);
  EXPECT_OK(s2.clear());
  EXPECT_TRUE(s2.is_empty());
  EXPECT_TRUE(s2._d.is_sso());
  verify_string(s2);

  EXPECT_OK(s.clear());
  EXPECT_TRUE(s.is_empty());
  EXPECT_TRUE(s._d.is_dynamic());
  EXPECT_EQ(s.size(), 0u);
  verify_string(s);

  EXPECT_OK(s.reset());
  EXPECT_TRUE(s._d.is_sso());

  EXPECT_OK(s.assign("TESTING NON-SSO STRING REPRESENTATION"));
  EXPECT_TRUE(s._d.is_dynamic());
  size_t dynamic_capacity = s.capacity();

  EXPECT_OK(s.clear());
  EXPECT_EQ(s.size(), 0u);
  EXPECT_TRUE(s.is_empty());
  EXPECT_TRUE(s._d.is_dynamic());
  EXPECT_EQ(s.capacity(), dynamic_capacity);
  verify_string(s);
}

TEST_CASE(core_string_combined_ops) {
  BLString s;

  EXPECT_OK(s.assign("abc"));
  verify_string(s);
  EXPECT_OK(s.append("xyz"));
  verify_string(s);
  EXPECT_TRUE(s.equals("abcxyz"));

  EXPECT_OK(s.insert(2, s.span()));
  verify_string(s);
  EXPECT_TRUE(s.equals("ababcxyzcxyz"));

  EXPECT_OK(s.remove(BLRange{1, 11}));
  verify_string(s);
  EXPECT_TRUE(s.equals("az"));

  EXPECT_OK(s.insert(1, s.span()));
  verify_string(s);
  EXPECT_TRUE(s.equals("aazz"));

  EXPECT_OK(s.insert(1, "xxx"));
  verify_string(s);
  EXPECT_TRUE(s.equals("axxxazz"));

  EXPECT_OK(s.remove(BLRange{4, 6}));
  verify_string(s);
  EXPECT_TRUE(s.equals("axxxz"));

  BLString x(s);
  EXPECT_OK(s.insert(3, "INSERTED"));
  verify_string(s);
  EXPECT_TRUE(s.equals("axxINSERTEDxz"));

  x = s;
  verify_string(x);
  EXPECT_OK(s.remove(BLRange{1, 11}));
  verify_string(s);
  EXPECT_TRUE(s.equals("axz"));

  EXPECT_OK(s.insert(3, "APPENDED"));
  verify_string(s);
  EXPECT_TRUE(s.equals("axzAPPENDED"));
}

TEST_CASE(core_string_uninitialized_ops) {
  BLString s;
  char* p = nullptr;

  EXPECT_OK(s.assign_uninitialized(5, &p));
  EXPECT_NOT_NULL(p);
  EXPECT_EQ(s.size(), 5u);
  memcpy(p, "AaBbC", 5);
  EXPECT_TRUE(s.equals("AaBbC"));
  verify_string(s);

  EXPECT_OK(s.append_uninitialized(3, &p));
  EXPECT_EQ(s.size(), 8u);
  memcpy(p, "cDd", 3);
  EXPECT_TRUE(s.equals("AaBbCcDd"));
  verify_string(s);

  EXPECT_OK(s.insert_uninitialized(5, 10, &p));
  EXPECT_EQ(s.size(), 18u);
  memcpy(p, "0123456789", 10);
  EXPECT_TRUE(s.equals("AaBbC0123456789cDd"));
  verify_string(s);

  EXPECT_OK(s.prepend_uninitialized(13, &p));
  EXPECT_EQ(s.size(), 31u);
  memcpy(p, "=!@#$%^&*(){}", 13);
  EXPECT_TRUE(s.equals("=!@#$%^&*(){}AaBbC0123456789cDd"));
  verify_string(s);

  EXPECT_OK(s.assign_uninitialized(BLString::Info::kSSOCapacity + 16u, &p));
  EXPECT_TRUE(s._d.is_dynamic());
  axl::fill_bytes(p, BLString::Info::kSSOCapacity + 16u, 'E');
  EXPECT_EQ(s.size(), BLString::Info::kSSOCapacity + 16u);
  EXPECT_EQ(s[0], 'E');
  EXPECT_EQ(s[s.size() - 1u], 'E');
  verify_string(s);

  EXPECT_OK(s.insert_uninitialized(4, BLString::Info::kSSOCapacity * 4u, &p));
  axl::fill_bytes(p, BLString::Info::kSSOCapacity * 4u, 'F');
  EXPECT_EQ(s.size(), BLString::Info::kSSOCapacity * 5u + 16u);
  EXPECT_EQ(s[3], 'E');
  EXPECT_EQ(s[4], 'F');
  EXPECT_EQ(s[4u + BLString::Info::kSSOCapacity * 4u - 1u], 'F');
  EXPECT_EQ(s[4u + BLString::Info::kSSOCapacity * 4u], 'E');
  verify_string(s);
}

TEST_CASE(core_string_insert_uninitialized) {
  static constexpr char original[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  static constexpr char inserted[] = {'X', '\0', 'Y'};

  // Exercise SSO storage, mutable dynamic storage, and shared dynamic storage.
  for (uint32_t mode = 0; mode < 3; mode++) {
    size_t original_size = mode == 0 ? 10u : sizeof(original) - 1u;
    size_t indexes[] = {0u, 5u, original_size, original_size + 1u, SIZE_MAX};

    for (size_t index : indexes) {
      for (size_t count : {size_t(0), sizeof(inserted)}) {
        BLString s;
        EXPECT_OK(s.assign(original, original_size));

        if (mode != 0) {
          EXPECT_OK(s.reserve(128u));
          EXPECT_TRUE(s._d.is_dynamic());
        }
        else {
          EXPECT_TRUE(s._d.is_sso());
        }

        BLString peer;
        if (mode == 2) {
          EXPECT_OK(peer.assign(s));
          EXPECT_EQ(peer.data(), s.data());
        }

        size_t insertion_index = axl::min(index, original_size);
        size_t result_size = original_size + count;
        char expected[sizeof(original) + sizeof(inserted)];
        memcpy(expected, original, insertion_index);
        memcpy(expected + insertion_index, inserted, count);
        memcpy(expected + insertion_index + count, original + insertion_index, original_size - insertion_index);
        expected[result_size] = '\0';

        const char* original_data = s.data();
        char* p = nullptr;
        EXPECT_OK(s.insert_uninitialized(index, count, &p));
        EXPECT_NOT_NULL(p);
        EXPECT_EQ(p, s.data() + insertion_index);
        EXPECT_EQ(s.size(), result_size);
        memcpy(p, inserted, count);
        EXPECT_TRUE(s.equals(expected, result_size))
          .message("mode=%u index=%zu count=%zu", mode, index, count);
        verify_string(s);

        if (mode == 1) {
          EXPECT_EQ(s.data(), original_data);
        }

        if (mode == 2) {
          EXPECT_TRUE(peer.equals(original, original_size));
          if (count != 0) {
            EXPECT_NE(s.data(), peer.data());
          }
          verify_string(peer);
        }
      }
    }
  }
}

TEST_CASE(core_string_format) {
  BLString s;

  EXPECT_OK(s.assign_format("%s", ""));
  verify_string(s);
  EXPECT_TRUE(s._d.is_sso());
  EXPECT_TRUE(s.is_empty());

  EXPECT_OK(s.assign_format("%d", 1000));
  verify_string(s);
  EXPECT_TRUE(s._d.is_sso());
  EXPECT_TRUE(s.equals("1000"));

  EXPECT_OK(s.append_format(" %d", 9876));
  verify_string(s);
  EXPECT_TRUE(s._d.is_sso());
  EXPECT_TRUE(s.equals("1000 9876"));

  EXPECT_OK(s.assign_format("TESTING NON-SSO STRING FORMAT %d %d %d", 1000, 2000, 3000));
  verify_string(s);
  EXPECT_TRUE(s.equals("TESTING NON-SSO STRING FORMAT 1000 2000 3000"));

  EXPECT_OK(s.append_format(" %d", 9876));
  verify_string(s);
  EXPECT_TRUE(s.equals("TESTING NON-SSO STRING FORMAT 1000 2000 3000 9876"));

  EXPECT_OK(s.append_format("-%s", "0123456789ABCDEFGHIJ0123456789ABCDEFGHIJ"));
  verify_string(s);
  EXPECT_TRUE(s.equals("TESTING NON-SSO STRING FORMAT 1000 2000 3000 9876-0123456789ABCDEFGHIJ0123456789ABCDEFGHIJ"));

  BLString s2(s);
  EXPECT_OK(s2.append_format("-%d", 1234));
  verify_string(s2);
  EXPECT_TRUE(s2.equals("TESTING NON-SSO STRING FORMAT 1000 2000 3000 9876-0123456789ABCDEFGHIJ0123456789ABCDEFGHIJ-1234"));
  EXPECT_TRUE(s.equals("TESTING NON-SSO STRING FORMAT 1000 2000 3000 9876-0123456789ABCDEFGHIJ0123456789ABCDEFGHIJ"));

  // `s` is dynamic and mutable - assigning a formatted string must keep it dynamic.
  EXPECT_OK(s.assign_format("%s=%d", "value", 42));
  verify_string(s);
  EXPECT_TRUE(s.equals("value=42"));
  EXPECT_TRUE(s._d.is_dynamic());

  // `s3` would be shared with `s` - assigning to `s` should make it SSO, if the assignment is small.
  BLString s3(s);
  EXPECT_OK(s.assign_format("%s=%d", "value", 12984));
  EXPECT_TRUE(s._d.is_sso());
  EXPECT_TRUE(s.equals("value=12984"));
  EXPECT_TRUE(s3.equals("value=42"));

  BLString large;
  EXPECT_OK(large.append('-', 3333));
  EXPECT_OK(s.assign_format("x%sy", large.data()));

  EXPECT_OK(large.prepend('x'));
  EXPECT_OK(large.append('y'));
  EXPECT_EQ(s, large);

  EXPECT_OK(large.reset());
  EXPECT_OK(large.assign('-', 3333));
  EXPECT_OK(s.reset());
  EXPECT_OK(s.append('x'));
  EXPECT_OK(s.append_format("%sy", large.data()));

  EXPECT_OK(large.prepend('x'));
  EXPECT_OK(large.append('y'));
  EXPECT_EQ(s, large);
}

TEST_CASE(core_string_sso_to_dynamic) {
  BLString s;
  BLString t;

  EXPECT_OK(s.assign('a', BLString::Info::kSSOCapacity));
  EXPECT_EQ(s.size(), BLString::Info::kSSOCapacity);
  EXPECT_TRUE(s._d.is_sso());
  verify_string(s);

  EXPECT_OK(t.assign('b', BLString::Info::kSSOCapacity + 1u));
  EXPECT_EQ(t.size(), BLString::Info::kSSOCapacity + 1u);
  EXPECT_TRUE(t._d.is_dynamic());
  verify_string(t);

  EXPECT_OK(s.append('c'));
  EXPECT_EQ(s.size(), BLString::Info::kSSOCapacity + 1u);
  EXPECT_TRUE(s._d.is_dynamic());
  verify_string(s);
}

TEST_CASE(core_string_copy_on_write) {
  BLString a;
  EXPECT_OK(a.assign("short"));
  EXPECT_TRUE(a._d.is_sso());

  BLString b(a);
  EXPECT_OK(a.append("X"));
  EXPECT_TRUE(a.equals("shortX"));
  EXPECT_TRUE(b.equals("short"));
  verify_string(a);
  verify_string(b);

  BLString c;
  EXPECT_OK(c.assign("TESTING NON-SSO STRING REPRESENTATION !!!"));
  EXPECT_TRUE(c._d.is_dynamic());

  BLString d(c);
  EXPECT_EQ(c.data(), d.data());
  EXPECT_OK(c.append("?"));
  EXPECT_NE(c.data(), d.data());
  EXPECT_TRUE(d.equals("TESTING NON-SSO STRING REPRESENTATION !!!"));
  EXPECT_TRUE(c.equals("TESTING NON-SSO STRING REPRESENTATION !!!?"));
  verify_string(c);
  verify_string(d);

  BLString e(c);
  EXPECT_EQ(c.data(), e.data());

  char* mutable_data = nullptr;
  EXPECT_OK(c.make_mutable(&mutable_data));
  EXPECT_EQ(mutable_data, c.data());
  EXPECT_NE(c.data(), e.data());
  EXPECT_TRUE(c.equals(e));
  verify_string(c);
  verify_string(e);

  BLString f;
  EXPECT_OK(f.assign("sso"));
  EXPECT_TRUE(f._d.is_sso());
  char* sso_data = nullptr;
  EXPECT_OK(f.make_mutable(&sso_data));
  EXPECT_EQ(sso_data, f.data());
  EXPECT_TRUE(f.equals("sso"));
}

TEST_CASE(core_string_resize_truncate) {
  BLString s;

  EXPECT_OK(s.resize(5, 'x'));
  EXPECT_TRUE(s.equals("xxxxx"));
  EXPECT_TRUE(s._d.is_sso());
  verify_string(s);

  EXPECT_OK(s.resize(8, 'y'));
  EXPECT_TRUE(s.equals("xxxxxyyy"));
  EXPECT_TRUE(s._d.is_sso());
  verify_string(s);

  EXPECT_OK(s.resize(3, 'z'));
  EXPECT_TRUE(s.equals("xxx"));
  EXPECT_TRUE(s._d.is_sso());
  verify_string(s);

  EXPECT_OK(s.resize(40, 'q'));
  EXPECT_TRUE(s.equals("xxxqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"));
  EXPECT_TRUE(s._d.is_dynamic());
  verify_string(s);

  BLString s_copy(s);
  EXPECT_OK(s.resize(30, 'X'));
  EXPECT_TRUE(s.equals("xxxqqqqqqqqqqqqqqqqqqqqqqqqqqq"));
  EXPECT_TRUE(s._d.is_sso());
  verify_string(s);

  EXPECT_TRUE(s_copy.equals("xxxqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"));
  verify_string(s_copy);

  EXPECT_OK(s.truncate(SIZE_MAX));
  EXPECT_TRUE(s.equals("xxxqqqqqqqqqqqqqqqqqqqqqqqqqqq"));
  verify_string(s);

  EXPECT_OK(s.append("==")); // Makes s dynamic again
  EXPECT_FALSE(s._d.is_sso());
  s_copy = s;

  EXPECT_OK(s.truncate(5));
  EXPECT_TRUE(s.equals("xxxqq"));
  EXPECT_TRUE(s._d.is_sso()); // `s` was shared, so it uses SSO now.
  verify_string(s);

  EXPECT_TRUE(s_copy.equals("xxxqqqqqqqqqqqqqqqqqqqqqqqqqqq=="));
  verify_string(s_copy);

  EXPECT_OK(s.truncate(100));
  EXPECT_TRUE(s.equals("xxxqq"));
  verify_string(s);

  EXPECT_OK(s.truncate(0));
  EXPECT_EQ(s.size(), 0u);
  EXPECT_TRUE(s.is_empty());
  verify_string(s);
}

TEST_CASE(core_string_move_and_swap) {
  BLString a;
  EXPECT_OK(a.assign("TESTING NON-SSO STRING REPRESENTATION"));
  EXPECT_TRUE(a._d.is_dynamic());
  const char* a_data = a.data();

  // Move construction leaves the source empty but valid.
  BLString b(axl::move(a));
  EXPECT_TRUE(b.equals("TESTING NON-SSO STRING REPRESENTATION"));
  EXPECT_EQ(b.data(), a_data);
  EXPECT_TRUE(a.is_empty());
  verify_string(a);
  verify_string(b);

  // Move assignment keeps move constructor semantics - it leaves the source empty but valid.
  BLString c;
  EXPECT_OK(c.assign("temporary"));
  c = axl::move(b);
  EXPECT_TRUE(c.equals("TESTING NON-SSO STRING REPRESENTATION"));
  EXPECT_TRUE(b.is_empty());
  verify_string(b);
  verify_string(c);

  // swap() between a dynamic and an SSO string exchanges their contents.
  BLString d;
  EXPECT_OK(d.assign("sso"));
  EXPECT_TRUE(d._d.is_sso());

  c.swap(d);
  EXPECT_TRUE(c.equals("sso"));
  EXPECT_TRUE(c._d.is_sso());
  EXPECT_TRUE(d.equals("TESTING NON-SSO STRING REPRESENTATION"));
  EXPECT_TRUE(d._d.is_dynamic());
  verify_string(c);
  verify_string(d);
}

TEST_CASE(core_string_compare) {
  BLString sso1("ABCDEF00");
  BLString sso2("ABCDEF11");

  BLString dyn1("ABCDEFGHIJKLMNOPQRSTUVWXYZ012345678_0");
  BLString dyn2("ABCDEFGHIJKLMNOPQRSTUVWXYZ012345678_1");

  EXPECT_EQ(sso1.compare(sso1), 0);
  EXPECT_EQ(sso2.compare(sso2), 0);
  EXPECT_LT(sso1.compare(sso2), 0);
  EXPECT_GT(sso2.compare(sso1), 0);

  EXPECT_EQ(dyn1.compare(dyn1), 0);
  EXPECT_EQ(dyn2.compare(dyn2), 0);
  EXPECT_LT(dyn1.compare(dyn2), 0);
  EXPECT_GT(dyn2.compare(dyn1), 0);

  EXPECT_GT(dyn1.compare(sso1), 0);
  EXPECT_GT(dyn2.compare(sso1), 0);
  EXPECT_GT(dyn1.compare(sso2), 0);
  EXPECT_GT(dyn2.compare(sso2), 0);

  EXPECT_LT(sso1.compare(dyn1), 0);
  EXPECT_LT(sso2.compare(dyn1), 0);
  EXPECT_LT(sso1.compare(dyn2), 0);
  EXPECT_LT(sso2.compare(dyn2), 0);
}

TEST_CASE(core_string_compare_lengths) {
  BLString empty;
  BLString prefix("ab");
  BLString text("abc");

  EXPECT_EQ(empty.compare(empty), 0);
  EXPECT_LT(empty.compare(text), 0);
  EXPECT_GT(text.compare(empty), 0);

  EXPECT_EQ(empty.compare("", 0), 0);
  EXPECT_LT(empty.compare(text.span()), 0);
  EXPECT_GT(text.compare("", 0), 0);
  EXPECT_LT(prefix.compare(text.data(), text.size()), 0);
  EXPECT_GT(text.compare(prefix.span()), 0);

  // Call the C-string entry point directly so literal-length optimizations cannot bypass it.
  EXPECT_EQ(bl_string_compare_cstr(&empty, ""), 0);
  EXPECT_LT(bl_string_compare_cstr(&empty, text.data()), 0);
  EXPECT_GT(bl_string_compare_cstr(&text, ""), 0);
  EXPECT_LT(bl_string_compare_cstr(&prefix, text.data()), 0);
  EXPECT_GT(bl_string_compare_cstr(&text, prefix.data()), 0);

  BLString dynamic(text);
  EXPECT_OK(dynamic.reserve(128));
  EXPECT_TRUE(dynamic._d.is_dynamic());
  EXPECT_EQ(dynamic.compare(text), 0);
  EXPECT_EQ(text.compare(dynamic), 0);
  EXPECT_GT(dynamic.compare(prefix), 0);
  EXPECT_LT(prefix.compare(dynamic), 0);
}

TEST_CASE(core_string_compare_embedded_nulls) {
  const char data_a[] = {'a', '\0', 'b'};
  const char data_b[] = {'a', '\0', 'c'};
  BLString a(data_a, sizeof(data_a));
  BLString b(data_b, sizeof(data_b));

  EXPECT_LT(a.compare(b), 0);
  EXPECT_GT(b.compare(a), 0);
  EXPECT_LT(a.compare(b.span()), 0);
  EXPECT_GT(b.compare(a.span()), 0);
  EXPECT_EQ(a.compare(data_a, sizeof(data_a)), 0);
  EXPECT_GT(a.compare("a", 1), 0);

  // Embedded nulls belong to BLString's content, while the C-string ends at its first null.
  EXPECT_GT(bl_string_compare_cstr(&a, "a"), 0);
  EXPECT_LT(bl_string_compare_cstr(&a, "ab"), 0);
}

TEST_CASE(core_string_compare_unsigned_bytes) {
  BLString a("\x7F");
  BLString b("\x80");
  BLString c("\xFF");

  EXPECT_LT(a.compare(b), 0);
  EXPECT_LT(b.compare(c), 0);
  EXPECT_GT(c.compare(a), 0);
  EXPECT_LT(a.compare(b.span()), 0);
  EXPECT_GT(b.compare(a.span()), 0);
  EXPECT_LT(b.compare(c.data(), c.size()), 0);
  EXPECT_GT(c.compare(b.data(), b.size()), 0);

  EXPECT_LT(bl_string_compare_cstr(&a, b.data()), 0);
  EXPECT_GT(bl_string_compare_cstr(&c, b.data()), 0);
  EXPECT_EQ(bl_string_compare_cstr(&b, b.data()), 0);
}

TEST_CASE(core_string_compare_operators) {
  BLString a;
  BLString b;
  EXPECT_OK(a.assign("abc"));
  EXPECT_OK(b.assign("abc"));

  EXPECT_TRUE(a.equals(b));
  EXPECT_EQ(a.compare(b), 0);

  EXPECT_TRUE(a.equals(b.span()));
  EXPECT_EQ(a.compare(b.span()), 0);

  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
  EXPECT_TRUE(a <= b);
  EXPECT_TRUE(a >= b);

  BLString c;
  EXPECT_OK(c.assign("abd"));
  EXPECT_TRUE(a < c);
  EXPECT_TRUE(a <= c);
  EXPECT_TRUE(c > a);
  EXPECT_TRUE(c >= a);
  EXPECT_TRUE(a != c);

  BLString prefix;
  EXPECT_OK(prefix.assign("ab"));
  EXPECT_TRUE(prefix < a);
  EXPECT_TRUE(a > prefix);
  EXPECT_LT(prefix.compare(a), 0);
  EXPECT_GT(a.compare(prefix), 0);

  EXPECT_TRUE(a == "abc");
  EXPECT_TRUE(a != "abd");
  EXPECT_TRUE(a < "abd");
  EXPECT_TRUE(a > "abb");
  EXPECT_TRUE(a >= "abc");
  EXPECT_TRUE(a <= "abc");

  EXPECT_TRUE(a == b.span());
  EXPECT_TRUE(a != c.span());
  EXPECT_TRUE(a < c.span());
  EXPECT_TRUE(c > a.span());
}

TEST_CASE(core_string_search) {
  BLString s;

  EXPECT_OK(s.assign("abcdefghijklmnop-ponmlkjihgfedcba"));
  EXPECT_EQ(s.index_of('a'), 0u);
  EXPECT_EQ(s.index_of('a', 1), 32u);
  EXPECT_EQ(s.index_of('b'), 1u);
  EXPECT_EQ(s.index_of('b', 1), 1u);
  EXPECT_EQ(s.index_of('b', 2), 31u);
  EXPECT_EQ(s.last_index_of('b'), 31u);
  EXPECT_EQ(s.last_index_of('b', 30), 1u);
  EXPECT_EQ(s.index_of('z'), SIZE_MAX);
  EXPECT_EQ(s.index_of('z', SIZE_MAX), SIZE_MAX);
  EXPECT_EQ(s.last_index_of('z'), SIZE_MAX);
  EXPECT_EQ(s.last_index_of('z', 0), SIZE_MAX);
  EXPECT_EQ(s.last_index_of('z', SIZE_MAX), SIZE_MAX);
}

TEST_CASE(core_string_embedded_nulls) {
  // A string is binary safe - embedded null characters are regular content.
  static const char data0[] = {'a', '\0', 'b', 'c'};
  static const char data1[] = {'a', '\0', 'b', 'd'};

  BLString s;
  EXPECT_OK(s.assign(data0, 4));
  EXPECT_EQ(s.size(), 4u);
  EXPECT_EQ(s[1], '\0');
  EXPECT_TRUE(s.equals(data0, 4));
  verify_string(s);

  EXPECT_FALSE(s.equals("a"));       // The string has 4 chars, cannot be equal to a string with one char.
  EXPECT_FALSE(s.equals(data1, 4));  // String is equal with data1, which contains embedded null characters.

  EXPECT_EQ(s.compare(data0, 4), 0);
  EXPECT_LT(s.compare(data1, 4), 0); // 'c' < 'd' at index 3

  EXPECT_EQ(s.index_of('\0'), 1u);
  EXPECT_EQ(s.last_index_of('\0'), 1u);

  for (size_t i = 0; i < BLString::Info::kSSOCapacity; i++) {
    EXPECT_OK(s.append(data0, 4));
  }

  EXPECT_EQ(s.size(), 4u + BLString::Info::kSSOCapacity * 4u);
  EXPECT_TRUE(s._d.is_dynamic());
  EXPECT_EQ(s[0], 'a');
  EXPECT_EQ(s[1], '\0');
  EXPECT_EQ(s[5], '\0');
  verify_string(s);

  char* mutable_data = nullptr;
  EXPECT_OK(s.make_mutable(&mutable_data));
  EXPECT_EQ(mutable_data[1], '\0');
  EXPECT_EQ(mutable_data[2], 'b');
  verify_string(s);
}

TEST_CASE(core_string_reserve_shrink) {
  BLString s;

  EXPECT_OK(s.append("abcdefgh"));
  EXPECT_TRUE(s.equals("abcdefgh"));
  EXPECT_TRUE(s._d.is_sso());
  EXPECT_OK(s.shrink());
  EXPECT_TRUE(s.equals("abcdefgh"));
  EXPECT_TRUE(s._d.is_sso());
  verify_string(s);

  EXPECT_OK(s.clear());
  verify_string(s);

  EXPECT_OK(s.reserve(1024));
  EXPECT_EQ(s.size(), 0u);
  EXPECT_GE(s.capacity(), 1024u);
  verify_string(s);

  EXPECT_OK(s.append("abcdefghijklmnop"));
  EXPECT_EQ(s.size(), 16u);
  EXPECT_GE(s.capacity(), 1024u);
  EXPECT_TRUE(s.equals("abcdefghijklmnop"));
  verify_string(s);

  EXPECT_OK(s.shrink());
  EXPECT_EQ(s.size(), 16u);
  EXPECT_LT(s.capacity(), 1024u);
  EXPECT_TRUE(s.equals("abcdefghijklmnop"));
  verify_string(s);
}

TEST_CASE(core_string_shrink_dynamic) {
  static constexpr char data[] = "abcdefghijklmnopqrstuvwxyz\0ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  static constexpr size_t data_size = sizeof(data) - 1u;

  for (uint32_t shared = 0; shared < 2; shared++) {
    BLString s(data, data_size);
    EXPECT_OK(s.reserve(1024));
    EXPECT_TRUE(s._d.is_dynamic());
    size_t original_capacity = s.capacity();
    const char* original_data = s.data();

    BLString peer;
    if (shared) {
      EXPECT_OK(peer.assign(s));
      EXPECT_EQ(peer.data(), original_data);
    }

    EXPECT_OK(s.shrink());
    EXPECT_TRUE(s._d.is_dynamic());
    EXPECT_EQ(s.size(), data_size);
    EXPECT_GE(s.capacity(), data_size);
    EXPECT_LT(s.capacity(), original_capacity);
    EXPECT_TRUE(s.equals(data, data_size));
    verify_string(s);

    if (shared) {
      EXPECT_NE(s.data(), peer.data());
      EXPECT_EQ(peer.data(), original_data);
      EXPECT_EQ(peer.capacity(), original_capacity);
      EXPECT_TRUE(peer.equals(data, data_size));
      verify_string(peer);
    }
  }
}

TEST_CASE(core_string_shrink_fitting_dynamic) {
  static constexpr char data[] = "abcdefghijklmnopqrstuvwxyz0123456789";

  for (uint32_t shared = 0; shared < 2; shared++) {
    BLString s(data);
    EXPECT_TRUE(s._d.is_dynamic());
    size_t original_capacity = s.capacity();
    const char* original_data = s.data();

    BLString peer;
    if (shared) {
      EXPECT_OK(peer.assign(s));
    }

    // A fitting allocation can have unused bytes due to alignment, but shrinking it is a no-op.
    EXPECT_OK(s.shrink());
    EXPECT_TRUE(s._d.is_dynamic());
    EXPECT_EQ(s.data(), original_data);
    EXPECT_EQ(s.capacity(), original_capacity);
    EXPECT_TRUE(s.equals(data));
    verify_string(s);

    if (shared) {
      EXPECT_EQ(s.data(), peer.data());
      EXPECT_EQ(peer.capacity(), original_capacity);
      EXPECT_TRUE(peer.equals(data));
      verify_string(peer);
    }
  }
}

TEST_CASE(core_string_shrink_shared_to_sso) {
  char expected[BLString::Info::kSSOCapacity];
  memset(expected, 'x', sizeof(expected));
  const size_t sizes[] = {0u, BLString::Info::kSSOCapacity - 1u, BLString::Info::kSSOCapacity};

  for (size_t size : sizes) {
    BLString s;
    EXPECT_OK(s.reserve(1024));
    EXPECT_OK(s.assign('x', size));
    EXPECT_TRUE(s._d.is_dynamic());

    BLString peer(s);
    size_t original_capacity = peer.capacity();
    const char* original_data = peer.data();
    EXPECT_EQ(s.data(), peer.data());

    EXPECT_OK(s.shrink());
    EXPECT_TRUE(s._d.is_sso());
    EXPECT_EQ(s.size(), size);
    EXPECT_EQ(s.capacity(), BLString::Info::kSSOCapacity);
    EXPECT_TRUE(s.equals(expected, size));
    verify_string(s);

    EXPECT_TRUE(peer._d.is_dynamic());
    EXPECT_EQ(peer.data(), original_data);
    EXPECT_EQ(peer.capacity(), original_capacity);
    EXPECT_TRUE(peer.equals(expected, size));
    verify_string(peer);
  }
}

TEST_CASE(core_string_c_api_vs_cxx_api) {
  // C-API getters are inlined in C++ API, so these have to be tested explicitly.
  for (uint32_t is_dynamic : {0u, 1u}) {
    BLString s;

    EXPECT_EQ(bl_string_get_size(&s), 0u);
    EXPECT_EQ(bl_string_get_capacity(&s), BLString::Info::kSSOCapacity);
    EXPECT_EQ(bl_string_get_data(&s), s.data());

    if (!is_dynamic) {
      EXPECT_OK(s.append("abc"));
    }
    else {
      EXPECT_OK(s.append("<([{-=ABCDEFGHIJKLMNOP_QRSTUVWXYZ=-}])>"));
    }

    BLStringCore s_default;
    BLStringCore s_weak;
    BLStringCore s_moved;

    EXPECT_OK(bl_string_init(&s_default));
    EXPECT_FALSE(s.equals(s_default));

    EXPECT_OK(bl_string_init_weak(&s_weak, &s));
    EXPECT_TRUE(s.equals(s_weak));

    EXPECT_OK(bl_string_init_move(&s_moved, &s));
    EXPECT_FALSE(s.equals(s_moved));

    // Make sure `s` was reset to construction-state after move operation.
    BLString empty;
    EXPECT_EQ(memcmp(&empty, &s, sizeof(BLStringCore)), 0);
    EXPECT_TRUE(empty.equals(s_default));

    EXPECT_TRUE(bl_string_equals(&s_weak, &s_moved));

    EXPECT_OK(bl_string_destroy(&s_weak));
    EXPECT_OK(bl_string_destroy(&s_moved));
    EXPECT_OK(bl_string_destroy(&s_default));
  }
}

TEST_CASE(core_string_alloc_overflow) {
  constexpr size_t invalid_sizes[] = {
    SIZE_MAX,
    SIZE_MAX - 1u,
    SIZE_MAX - ObjectLayer::kMinImplAlignment,
    StringDetail::kMaxSize + 1u
  };


  BLString s1;
  BLString s2;
  EXPECT_OK(s2.append("this is a string that would not use SSO storage"));

  for (size_t size : invalid_sizes) {
    BLStringCore s_core;
    EXPECT_EQ(bl_string_init_with_data(&s_core, nullptr, size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_OK(bl_string_destroy(&s_core));

    BLString s_ctor(nullptr, size);
    EXPECT_TRUE(s_ctor.is_empty());

    EXPECT_EQ(s1.assign(nullptr, size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(s2.assign(nullptr, size), BL_ERROR_OUT_OF_MEMORY);

    EXPECT_EQ(s1.assign('-', size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(s2.assign('-', size), BL_ERROR_OUT_OF_MEMORY);

    EXPECT_EQ(s1.append(nullptr, size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(s2.append(nullptr, size), BL_ERROR_OUT_OF_MEMORY);

    EXPECT_EQ(s1.append('-', size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(s2.append('-', size), BL_ERROR_OUT_OF_MEMORY);

    EXPECT_EQ(s1.prepend(nullptr, size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(s2.prepend(nullptr, size), BL_ERROR_OUT_OF_MEMORY);

    EXPECT_EQ(s1.prepend('-', size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(s2.prepend('-', size), BL_ERROR_OUT_OF_MEMORY);

    EXPECT_EQ(s1.insert(13, nullptr, size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(s2.insert(13, nullptr, size), BL_ERROR_OUT_OF_MEMORY);

    EXPECT_EQ(s1.insert(13, '-', size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(s2.insert(13, '-', size), BL_ERROR_OUT_OF_MEMORY);

    char* data_out = nullptr;
    EXPECT_EQ(s1.assign_uninitialized(size, &data_out), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(s2.assign_uninitialized(size, &data_out), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(s1.append_uninitialized(size, &data_out), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(s2.append_uninitialized(size, &data_out), BL_ERROR_OUT_OF_MEMORY);

    EXPECT_TRUE(s1.is_empty());
    EXPECT_TRUE(s2.equals("this is a string that would not use SSO storage"));
    verify_string(s1);
    verify_string(s2);
  }

  for (size_t size : invalid_sizes) {
    BLString s;

    EXPECT_EQ(s.resize(size, 'a'), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(s.size(), 0u);

    EXPECT_EQ(s.reserve(size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(s.size(), 0u);

    EXPECT_OK(s.append("this is a string that would not use SSO storage"));
    size_t dyn_size = s.size();

    EXPECT_EQ(s.resize(size, 'a'), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(s.size(), dyn_size);

    EXPECT_EQ(s.reserve(size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(s.size(), dyn_size);
  }
}

} // {bl::StringTests}

#endif // BL_TEST
