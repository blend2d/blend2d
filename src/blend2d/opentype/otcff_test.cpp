// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_test_p.h"
#if defined(BL_TEST)

#include "../font_p.h"
#include "../geometry_p.h"
#include "../matrix_p.h"
#include "../path_p.h"
#include "../trace_p.h"
#include "../opentype/otcff_p.h"
#include "../opentype/otface_p.h"
#include "../support/memops_p.h"
#include "../support/intops_p.h"
#include "../support/lookuptable_p.h"
#include "../support/ptrops_p.h"
#include "../support/scopedbuffer_p.h"
#include "../support/traits_p.h"

namespace bl {
namespace OpenType {
namespace CFFImpl {

// bl::OpenType::CFFImpl - Tests
// =============================

static void testReadFloat() noexcept {
  struct TestEntry {
    char data[16];
    uint32_t size;
    uint32_t pass;
    double value;
  };

  const double kTolerance = 1e-9;

  static const TestEntry entries[] = {
    #define PASS_ENTRY(DATA, VAL) { DATA, sizeof(DATA) - 1, 1, VAL }
    #define FAIL_ENTRY(DATA)      { DATA, sizeof(DATA) - 1, 0, 0.0 }

    PASS_ENTRY("\xE2\xA2\x5F"            ,-2.25       ),
    PASS_ENTRY("\x0A\x14\x05\x41\xC3\xFF", 0.140541e-3),
    PASS_ENTRY("\x0F"                    , 0          ),
    PASS_ENTRY("\x00\x0F"                , 0          ),
    PASS_ENTRY("\x00\x0A\x1F"            , 0.1        ),
    PASS_ENTRY("\x1F"                    , 1          ),
    PASS_ENTRY("\x10\x00\x0F"            , 10000      ),
    PASS_ENTRY("\x12\x34\x5F"            , 12345      ),
    PASS_ENTRY("\x12\x34\x5A\xFF"        , 12345      ),
    PASS_ENTRY("\x12\x34\x5A\x00\xFF"    , 12345      ),
    PASS_ENTRY("\x12\x34\x5A\x67\x89\xFF", 12345.6789 ),
    PASS_ENTRY("\xA1\x23\x45\x67\x89\xFF", .123456789 ),

    FAIL_ENTRY(""),
    FAIL_ENTRY("\xA2"),
    FAIL_ENTRY("\x0A\x14"),
    FAIL_ENTRY("\x0A\x14\x05"),
    FAIL_ENTRY("\x0A\x14\x05\x51"),
    FAIL_ENTRY("\x00\x0A\x1A\xFF"),
    FAIL_ENTRY("\x0A\x14\x05\x51\xC3")

    #undef PASS_ENTRY
    #undef FAIL_ENTRY
  };

  for (size_t i = 0; i < BL_ARRAY_SIZE(entries); i++) {
    const TestEntry& entry = entries[i];
    double valueOut = 0.0;
    size_t valueSizeInBytes = 0;

    BLResult result = readFloat(
      reinterpret_cast<const uint8_t*>(entry.data),
      reinterpret_cast<const uint8_t*>(entry.data) + entry.size,
      valueOut,
      valueSizeInBytes);

    if (entry.pass) {
      double a = valueOut;
      double b = entry.value;

      EXPECT_SUCCESS(result)
        .message("Entry %zu should have passed {Error=%08X}", i, unsigned(result));

      EXPECT_LE(blAbs(a - b), kTolerance)
        .message("Entry %zu returned value '%g' which doesn't match the expected value '%g'", i, a, b);
    }
    else {
      EXPECT_NE(result, BL_SUCCESS)
        .message("Entry %zu should have failed", i);
    }
  }
}

static void testDictIterator() noexcept {
  // This example dump was taken from "The Compact Font Format Specification" Appendix D.
  static const uint8_t dump[] = {
    0xF8, 0x1B, 0x00, 0xF8, 0x1C, 0x02, 0xF8, 0x1D, 0x03, 0xF8,
    0x19, 0x04, 0x1C, 0x6F, 0x00, 0x0D, 0xFB, 0x3C, 0xFB, 0x6E,
    0xFA, 0x7C, 0xFA, 0x16, 0x05, 0xE9, 0x11, 0xB8, 0xF1, 0x12
  };

  struct TestEntry {
    uint32_t op;
    uint32_t count;
    double values[4];
  };

  static const TestEntry testEntries[] = {
    { CFFTable::kDictOpTopVersion    , 1, { 391                   } },
    { CFFTable::kDictOpTopFullName   , 1, { 392                   } },
    { CFFTable::kDictOpTopFamilyName , 1, { 393                   } },
    { CFFTable::kDictOpTopWeight     , 1, { 389                   } },
    { CFFTable::kDictOpTopUniqueId   , 1, { 28416                 } },
    { CFFTable::kDictOpTopFontBBox   , 4, { -168, -218, 1000, 898 } },
    { CFFTable::kDictOpTopCharStrings, 1, { 94                    } },
    { CFFTable::kDictOpTopPrivate    , 2, { 45, 102               } }
  };

  uint32_t index = 0;
  DictIterator iter(dump, BL_ARRAY_SIZE(dump));

  while (iter.hasNext()) {
    EXPECT_LT(index, BL_ARRAY_SIZE(testEntries))
      .message("DictIterator found more entries than the data contains");

    DictEntry entry;
    EXPECT_EQ(iter.next(entry), BL_SUCCESS)
      .message("DictIterator failed to read entry #%u", index);

    EXPECT_EQ(entry.count, testEntries[index].count)
      .message("DictIterator failed to read entry #%u properly {entry.count == 0}", index);

    for (uint32_t j = 0; j < entry.count; j++) {
      EXPECT_EQ(entry.values[j], testEntries[index].values[j])
        .message("DictIterator failed to read entry #%u properly {entry.values[%u] (%f) != %f)", index, j, entry.values[j], testEntries[index].values[j]);
    }
    index++;
  }

  EXPECT_EQ(index, BL_ARRAY_SIZE(testEntries))
    .message("DictIterator must iterate over all entries, only %u of %u iterated", index, unsigned(BL_ARRAY_SIZE(testEntries)));
}

UNIT(opentype_cff, BL_TEST_GROUP_TEXT_OPENTYPE) {
  INFO("bl::OpenType::CFFImpl::readFloat()");
  testReadFloat();

  INFO("bl::OpenType::CFFImpl::DictIterator");
  testDictIterator();
}

} // {CFFImpl}
} // {OpenType}
} // {bl}

#endif // BL_TEST
