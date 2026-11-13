// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/core/bit_array.h>
#include <blend2d/core/context.h>
#include <blend2d/core/glyph_buffer_p.h>
#include <blend2d/core/image.h>
#include <blend2d/core/var.h>
#include <blend2d/opentype/ot_face_p.h>
#include <blend2d/opentype/ot_layout_context_p.h>
#include <blend2d/opentype/ot_layout_p.h>

#include <blend2d-testing/resources/abeezee_regular_ttf.h>

#include <initializer_list>

namespace bl::GlyphBufferTests {

// GlyphBuffer - Test Utilities
// ============================

static BLFont make_font() noexcept {
  BLFontData data;
  BLFontFace face;
  BLFont font;

  EXPECT_OK(data.create_from_data(resource_abeezee_regular_ttf, sizeof(resource_abeezee_regular_ttf)));
  EXPECT_OK(face.create_from_data(data, 0));
  EXPECT_OK(font.create_from_face(face, 20.0f));
  return font;
}

static void expect_same_data(const BLGlyphBuffer& a, const BLGlyphBuffer& b) noexcept {
  EXPECT_EQ(a.size(), b.size());
  EXPECT_EQ(a.flags(), b.flags());
  EXPECT_EQ(a.glyph_run().placement_type, b.glyph_run().placement_type);
  EXPECT_EQ(memcmp(a.content(), b.content(), a.size() * sizeof(uint32_t)), 0);
  EXPECT_EQ(memcmp(a.info_data(), b.info_data(), a.size() * sizeof(BLGlyphInfo)), 0);
  EXPECT_EQ(a.placement_data() != nullptr, b.placement_data() != nullptr);
  if (a.placement_data()) {
    EXPECT_EQ(memcmp(a.placement_data(), b.placement_data(), a.size() * sizeof(BLGlyphPlacement)), 0);
  }
}

static void expect_glyph_run(const BLGlyphBuffer& gb) noexcept {
  const BLGlyphRun& run = gb.glyph_run();
  EXPECT_EQ(run.glyph_data, gb.content());
  EXPECT_EQ(run.placement_data, gb.placement_data());
  EXPECT_EQ(run.size, gb.size());
  EXPECT_EQ(run.flags, gb.flags());
  EXPECT_EQ(run.reserved, 0u);
  EXPECT_EQ(run.glyph_advance, int8_t(sizeof(uint32_t)));
  EXPECT_EQ(run.placement_advance, int8_t(sizeof(BLGlyphPlacement)));
  EXPECT_EQ(bl_glyph_buffer_get_glyph_run(&gb), &run);
}

// GlyphBuffer - Tests
// ===================

TEST_CASE(core_glyph_buffer_lifetime) {
  static_assert(sizeof(BLGlyphBufferCore) == sizeof(BLObjectCore));
  static_assert(sizeof(BLGlyphBuffer) == sizeof(BLObjectCore));

  BLGlyphBuffer a;
  EXPECT_TRUE(a.is_empty());
  EXPECT_TRUE(a._d.is_glyph_buffer());
  EXPECT_FALSE(a._d.is_ref_counted());
  EXPECT_EQ(a.flags(), 0u);

  EXPECT_OK(a.set_utf8_text("ABC"));
  const uint32_t* original = a.content();
  BLGlyphBuffer b(a);
  EXPECT_EQ(b.content(), original);

  EXPECT_OK(b.assign(b));
  EXPECT_OK(b.assign(static_cast<BLGlyphBuffer&&>(b)));
  EXPECT_EQ(b.content(), original);
  EXPECT_EQ(b.size(), 3u);

  BLGlyphBuffer c(static_cast<BLGlyphBuffer&&>(b));
  EXPECT_TRUE(b.is_empty());
  EXPECT_EQ(c.content(), original);
  b = c;
  EXPECT_OK(c.reset());
  EXPECT_EQ(b.content(), original);

  BLGlyphBufferCore raw;
  EXPECT_OK(bl_glyph_buffer_init_weak(&raw, &a));
  EXPECT_OK(a.reset());
  EXPECT_OK(b.reset());
  EXPECT_EQ(bl_glyph_buffer_get_size(&raw), 3u);
  EXPECT_EQ(bl_glyph_buffer_get_content(&raw)[2], uint32_t('C'));
  EXPECT_OK(bl_object_reset(&raw));
  EXPECT_TRUE(raw._d.is_glyph_buffer());
  EXPECT_EQ(bl_glyph_buffer_get_size(&raw), 0u);
  EXPECT_OK(bl_object_destroy(&raw));
}

TEST_CASE(core_glyph_buffer_object_ownership) {
  BLVar value;
  EXPECT_OK(bl_var_init_type(&value, BL_OBJECT_TYPE_GLYPH_BUFFER));
  EXPECT_TRUE(value.is_glyph_buffer());
  EXPECT_TRUE(value.as<BLGlyphBuffer>().is_empty());

  BLGlyphBuffer source;
  EXPECT_OK(source.set_utf8_text("owned"));
  EXPECT_OK(value.assign(source));

  BLArray<BLGlyphBuffer> buffers;
  EXPECT_OK(buffers.append(source));
  EXPECT_OK(source.reset());
  EXPECT_EQ(value.as<BLGlyphBuffer>().size(), 5u);
  EXPECT_EQ(buffers[0].content()[0], uint32_t('o'));

  BLArray<BLGlyphBuffer> copies(buffers);
  EXPECT_OK(buffers.reset());
  EXPECT_OK(value.reset());
  EXPECT_EQ(copies[0].content()[4], uint32_t('d'));
  EXPECT_OK(copies.reset());
}

TEST_CASE(core_glyph_buffer_copy_on_write) {
  BLGlyphBuffer source;
  EXPECT_OK(source.set_utf8_text("ABCD"));

  BLGlyphBuffer copy(source);
  EXPECT_OK(copy.clear());
  EXPECT_TRUE(copy.is_empty());
  EXPECT_EQ(source.size(), 4u);

  copy = source;
  EXPECT_OK(copy.set_utf8_text("XY"));
  EXPECT_EQ(copy.size(), 2u);
  EXPECT_EQ(source.content()[0], uint32_t('A'));

  const uint32_t glyphs[] = { 100u, 200u, 300u };
  copy = source;
  EXPECT_OK(copy.set_glyphs(glyphs, BL_ARRAY_SIZE(glyphs)));
  EXPECT_EQ(copy.content()[0], 100u);
  EXPECT_EQ(copy.content()[1], 200u);
  EXPECT_EQ(copy.content()[2], 300u);
  EXPECT_TRUE(source.has_text());

  struct GlyphItem { uint16_t glyph_id; uint16_t other; };
  const GlyphItem items[] = { { 7, 99 }, { 8, 99 }, { 9, 99 } };
  copy = source;
  EXPECT_OK(copy.set_glyphs_from_struct(items, BL_ARRAY_SIZE(items), sizeof(uint16_t), sizeof(GlyphItem)));
  EXPECT_EQ(copy.content()[1], 8u);
  EXPECT_EQ(source.content()[1], uint32_t('B'));

  copy = source;
  uint32_t* content;
  BLGlyphInfo* info;
  BLGlyphPlacement* placement;
  EXPECT_OK(copy.make_mutable(&content, &info, &placement));
  EXPECT_NE(content, source.content());
  EXPECT_NE(info, source.info_data());
  EXPECT_EQ(placement, nullptr);
  content[0] = uint32_t('Z');
  info[0].cluster = 99;
  EXPECT_EQ(source.content()[0], uint32_t('A'));
  EXPECT_EQ(source.info_data()[0].cluster, 0u);
}

TEST_CASE(core_glyph_buffer_initial_allocation) {
  constexpr size_t count = BL_GLYPH_BUFFER_INITIAL_CAPACITY + 65;
  char text8[count + 1];
  uint16_t text16[count + 1];
  uint32_t text32[count + 1];
  for (size_t i = 0; i <= count; i++) {
    uint32_t value = i < count ? uint32_t('A') + uint32_t(i % 26u) : 0u;
    text8[i] = char(value);
    text16[i] = uint16_t(value);
    text32[i] = value;
  }

  for (uint32_t setter = 0; setter < 10; setter++) {
    BLGlyphBuffer gb;
    size_t length = setter < 4 ? count : SIZE_MAX;
    switch (setter) {
      case 0:
      case 4:
        EXPECT_OK(gb.set_latin1_text(text8, length));
        break;
      case 1:
      case 5:
        EXPECT_OK(gb.set_utf8_text(text8, length));
        break;
      case 2:
      case 6:
        EXPECT_OK(gb.set_utf16_text(text16, length));
        break;
      case 3:
      case 7:
        EXPECT_OK(gb.set_utf32_text(text32, length));
        break;
      case 8:
        EXPECT_OK(gb.set_glyphs(text32, count));
        break;
      case 9:
        EXPECT_OK(gb.set_glyphs_from_struct(text16, count, sizeof(uint16_t), sizeof(uint16_t)));
        break;
    }

    GlyphBufferPrivateImpl* impl = bl_glyph_buffer_get_impl(&gb);
    EXPECT_EQ(impl->buffer[0], reinterpret_cast<uint8_t*>(impl + 1));
    EXPECT_GE(impl->capacity[0], count);
    EXPECT_EQ(impl->buffer[1], nullptr);
    EXPECT_EQ(impl->capacity[1], 0u);
    EXPECT_EQ(gb.size(), count);
    EXPECT_EQ(gb.content()[count - 1], text32[count - 1]);
    EXPECT_EQ(gb.info_data()[count - 1].cluster, uint32_t(count - 1));
    expect_glyph_run(gb);
  }

  BLGlyphBuffer source;
  EXPECT_OK(source.set_utf8_text("ABC"));
  for (uint32_t shared : { 0u, 1u }) {
    BLGlyphBuffer gb;
    if (shared) {
      gb = source;
    }
    GlyphBufferPrivateImpl* impl = bl_glyph_buffer_get_impl(&gb);
    size_t impossible_size = SIZE_MAX / BL_GLYPH_BUFFER_ANY_ITEM_SIZE + 1;
    EXPECT_EQ(gb.set_utf8_text("x", impossible_size), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(bl_glyph_buffer_get_impl(&gb), impl);
    EXPECT_EQ(gb.size(), shared ? 3u : 0u);
    if (shared) {
      expect_same_data(gb, source);
    }
  }
}

TEST_CASE(core_glyph_buffer_primary_buffer_growth) {
  const uint32_t glyphs[] = { 11, 22, 33 };
  const BLGlyphInfo infos[] = { { 7, 101 }, { 8, 102 }, { 9, 103 } };

  for (size_t buffer_id : { size_t(0), size_t(1) }) {
    BLGlyphBuffer gb;
    EXPECT_OK(gb.set_glyphs(glyphs, BL_ARRAY_SIZE(glyphs)));
    GlyphBufferPrivateImpl* impl = bl_glyph_buffer_get_impl(&gb);
    memcpy(impl->info_data, infos, sizeof(infos));

    if (buffer_id == 1) {
      EXPECT_OK(impl->ensure_buffer(1, 0, gb.size()));
      uint32_t* glyph_data;
      BLGlyphInfo* info_data;
      impl->get_glyph_data_ptrs(1, &glyph_data, &info_data);
      bl_copy_glyph_data(glyph_data, info_data, impl->content, impl->info_data, gb.size());
      impl->flip();
    }

    uint8_t* primary = reinterpret_cast<uint8_t*>(impl + 1);
    EXPECT_EQ(impl->buffer[buffer_id], primary);
    uint8_t* other_buffer = impl->buffer[buffer_id ^ 1u];
    size_t other_capacity = impl->capacity[buffer_id ^ 1u];
    size_t old_capacity = impl->capacity[buffer_id];
    EXPECT_OK(impl->ensure_buffer(buffer_id, gb.size(), old_capacity + 1));
    EXPECT_EQ(bl_glyph_buffer_get_impl(&gb), impl);
    EXPECT_NE(impl->buffer[buffer_id], primary);
    EXPECT_GE(impl->capacity[buffer_id], old_capacity + 1);
    EXPECT_EQ(impl->buffer[buffer_id ^ 1u], other_buffer);
    EXPECT_EQ(impl->capacity[buffer_id ^ 1u], other_capacity);

    uint8_t* grown_buffer = impl->buffer[buffer_id];
    size_t grown_capacity = impl->capacity[buffer_id];
    EXPECT_EQ(impl->ensure_buffer(buffer_id, gb.size(), SIZE_MAX), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(bl_glyph_buffer_get_impl(&gb), impl);
    EXPECT_EQ(impl->buffer[buffer_id], grown_buffer);
    EXPECT_EQ(impl->capacity[buffer_id], grown_capacity);

    if (buffer_id == 1) {
      impl->flip();
    }
    EXPECT_EQ(memcmp(gb.content(), glyphs, sizeof(glyphs)), 0);
    EXPECT_EQ(memcmp(gb.info_data(), infos, sizeof(infos)), 0);
    expect_glyph_run(gb);
    EXPECT_OK(gb.reset());
  }
}

TEST_CASE(core_glyph_buffer_flipped_copy_on_write) {
  const uint32_t glyphs[] = { 11, 22, 33 };

  for (uint32_t positioned : { 0u, 1u }) {
    BLGlyphBuffer source;
    EXPECT_OK(source.set_glyphs(glyphs, BL_ARRAY_SIZE(glyphs)));
    GlyphBufferPrivateImpl* impl = bl_glyph_buffer_get_impl(&source);
    EXPECT_OK(impl->ensure_buffer(1, 0, source.size()));
    uint32_t* glyph_data;
    BLGlyphInfo* info_data;
    impl->get_glyph_data_ptrs(1, &glyph_data, &info_data);
    bl_copy_glyph_data(glyph_data, info_data, impl->content, impl->info_data, source.size());
    impl->flip();
    EXPECT_EQ(impl->buffer[1], reinterpret_cast<uint8_t*>(impl + 1));

    if (positioned) {
      EXPECT_OK(impl->ensure_placement());
      EXPECT_EQ(reinterpret_cast<uint8_t*>(impl->placement_data), impl->buffer[1]);
      EXPECT_EQ(bl_glyph_buffer_get_impl(&source), impl);
      for (size_t i = 0; i < source.size(); i++) {
        impl->placement_data[i] = BLGlyphPlacement { BLPointI(int(i), -int(i)), BLPointI(int(i) + 10, 0) };
      }
      impl->glyph_run.placement_type = BL_GLYPH_PLACEMENT_TYPE_ADVANCE_OFFSET;
      impl->flags |= BL_GLYPH_BUFFER_GLYPH_ADVANCES;
    }

    BLGlyphBuffer copy(source);
    uint32_t* mutable_content;
    BLGlyphInfo* mutable_info;
    BLGlyphPlacement* mutable_placement;
    EXPECT_OK(copy.make_mutable(&mutable_content, &mutable_info, &mutable_placement));
    GlyphBufferPrivateImpl* copy_impl = bl_glyph_buffer_get_impl(&copy);
    EXPECT_NE(copy_impl, impl);
    EXPECT_EQ(copy_impl->buffer[0], reinterpret_cast<uint8_t*>(copy_impl + 1));
    expect_same_data(source, copy);
    expect_glyph_run(source);
    expect_glyph_run(copy);

    mutable_content[0] = 99;
    mutable_info[0].cluster = 42;
    EXPECT_EQ(source.content()[0], glyphs[0]);
    EXPECT_EQ(source.info_data()[0].cluster, 0u);
    if (positioned) {
      mutable_placement[0].advance.x = 100;
      EXPECT_EQ(source.placement_data()[0].advance.x, 10);
    }
    EXPECT_OK(source.reset());
    EXPECT_EQ(copy.content()[0], 99u);
    EXPECT_EQ(copy.info_data()[0].cluster, 42u);
    if (positioned) {
      EXPECT_EQ(copy.placement_data()[0].advance.x, 100);
    }
    EXPECT_OK(copy.reset());
  }
}

TEST_CASE(core_glyph_buffer_unicode) {
  BLGlyphBuffer gb;
  const uint8_t utf8[] = { uint8_t('A'), 0xF0u, 0x9Fu, 0x98u, 0x80u, 0xFFu, uint8_t('B') };
  EXPECT_OK(gb.set_utf8_text(utf8, BL_ARRAY_SIZE(utf8)));
  EXPECT_EQ(gb.size(), 4u);
  EXPECT_TRUE(gb.has_invalid_chars());
  EXPECT_EQ(gb.content()[1], 0x1F600u);
  EXPECT_EQ(gb.content()[2], 0xFFFDu);
  EXPECT_EQ(gb.info_data()[2].cluster, 5u);

  BLGlyphBuffer invalid_copy(gb);
  const uint16_t utf16[] = { 'A', 0x20AC, 0xD83D, 0xDE00 };
  EXPECT_OK(gb.set_utf16_text(utf16, BL_ARRAY_SIZE(utf16)));
  EXPECT_EQ(gb.size(), 3u);
  EXPECT_FALSE(gb.has_invalid_chars());
  EXPECT_EQ(gb.content()[1], 0x20ACu);
  EXPECT_EQ(gb.content()[2], 0x1F600u);
  EXPECT_EQ(gb.info_data()[2].cluster, 2u);
  EXPECT_TRUE(invalid_copy.has_invalid_chars());
  EXPECT_EQ(invalid_copy.content()[2], 0xFFFDu);

  const uint32_t utf32[] = { 'A', 0x1F600, 0x110000, 'B' };
  EXPECT_OK(gb.set_utf32_text(utf32, BL_ARRAY_SIZE(utf32)));
  EXPECT_EQ(gb.size(), 4u);
  EXPECT_TRUE(gb.has_invalid_chars());
  EXPECT_EQ(gb.content()[2], 0xFFFDu);
  EXPECT_EQ(gb.info_data()[3].cluster, 3u);
  expect_glyph_run(gb);
}

TEST_CASE(core_glyph_buffer_shaping_copy_on_write) {
  BLFont font = make_font();
  constexpr const char text[] = "AVATAR office affine";

  BLGlyphBuffer text_buffer;
  EXPECT_OK(text_buffer.set_utf8_text(text));
  BLGlyphBuffer mapped(text_buffer);
  EXPECT_OK(font.map_text_to_glyphs(mapped));
  EXPECT_TRUE(text_buffer.has_text());
  EXPECT_EQ(text_buffer.content()[0], uint32_t('A'));
  EXPECT_FALSE(mapped.has_text());
  EXPECT_NE(mapped.content(), text_buffer.content());

  BLGlyphBuffer positioned(mapped);
  EXPECT_OK(font.position_glyphs(positioned));
  EXPECT_EQ(mapped.placement_data(), nullptr);
  EXPECT_NE(positioned.placement_data(), nullptr);
  expect_glyph_run(positioned);

  BLGlyphBuffer expected;
  EXPECT_OK(expected.set_utf8_text(text));
  EXPECT_OK(font.shape(expected));
  BLGlyphBuffer shaped(text_buffer);
  EXPECT_OK(font.shape(shaped));
  expect_same_data(shaped, expected);
  expect_glyph_run(shaped);

  BLGlyphBuffer mutable_copy(shaped);
  uint32_t* content;
  BLGlyphInfo* info;
  BLGlyphPlacement* placement;
  EXPECT_OK(mutable_copy.make_mutable(&content, &info, &placement));
  expect_same_data(mutable_copy, expected);
  EXPECT_NE(placement, shaped.placement_data());
  placement[0].advance.x++;
  content[0]++;
  info[0].cluster++;
  expect_same_data(shaped, expected);
  expect_glyph_run(mutable_copy);

  const OpenType::OTFaceImpl* face = ObjectLayer::get_impl<OpenType::OTFaceImpl>(&font.face());
  BLBitArray gsub_plan;
  BLBitArray gpos_plan;
  EXPECT_OK(OpenType::LayoutImpl::calculate_gsub_plan(face, font.feature_settings(), &gsub_plan));
  EXPECT_OK(OpenType::LayoutImpl::calculate_gpos_plan(face, font.feature_settings(), &gpos_plan));

  // ABeeZee has GPOS but no GSUB or legacy kern table. The unsupported entry points still must isolate writes.
  BLResult gsub_result = face->layout.gsub().lookup_count ? BL_SUCCESS : BL_ERROR_FONT_NOT_INITIALIZED;
  BLResult kern_result = bl_test_flag(face->ot_flags, OpenType::OTFaceFlags::kLegacyKernAvailable)
                      ? BL_SUCCESS : BL_ERROR_FONT_NOT_INITIALIZED;
  BLGlyphBuffer substituted(mapped);
  EXPECT_EQ(font.apply_gsub(substituted, gsub_plan), gsub_result);
  EXPECT_EQ(mapped.placement_data(), nullptr);
  EXPECT_NE(substituted.content(), mapped.content());

  BLGlyphBuffer resubstituted(shaped);
  EXPECT_EQ(font.apply_gsub(resubstituted, gsub_plan), gsub_result);
  EXPECT_EQ(resubstituted.placement_data(), nullptr);
  EXPECT_EQ(resubstituted.glyph_run().placement_type, BL_GLYPH_PLACEMENT_TYPE_NONE);
  expect_same_data(shaped, expected);
  EXPECT_OK(font.position_glyphs(resubstituted));
  expect_glyph_run(resubstituted);

  BLGlyphBuffer kerned(shaped);
  EXPECT_EQ(font.apply_kerning(kerned), kern_result);
  EXPECT_NE(kerned.placement_data(), shaped.placement_data());
  expect_same_data(shaped, expected);

  BLGlyphBuffer adjusted(shaped);
  EXPECT_OK(font.apply_gpos(adjusted, gpos_plan));
  EXPECT_NE(adjusted.placement_data(), shaped.placement_data());
  expect_same_data(shaped, expected);
  expect_glyph_run(adjusted);

  for (uint32_t shared : {0u, 1u}) {
    for (uint32_t setter = 0; setter < 3; setter++) {
      BLGlyphBuffer reused(shaped);
      if (!shared) {
        EXPECT_OK(reused.make_mutable());
      }
      if (setter == 0) {
        EXPECT_OK(reused.set_utf8_text("new"));
      }
      else if (setter == 1) {
        const uint32_t glyphs[] = { 1, 2, 3 };
        EXPECT_OK(reused.set_glyphs(glyphs, BL_ARRAY_SIZE(glyphs)));
      }
      else {
        EXPECT_OK(reused.set_utf8_text("", 0));
      }
      EXPECT_EQ(reused.placement_data(), nullptr);
      EXPECT_EQ(reused.glyph_run().placement_type, BL_GLYPH_PLACEMENT_TYPE_NONE);
      EXPECT_FALSE(reused.has_flag(BL_GLYPH_BUFFER_GLYPH_ADVANCES));
      expect_same_data(shaped, expected);
      expect_glyph_run(reused);
    }
  }
}

TEST_CASE(core_glyph_buffer_substitution_buffer) {
  BLGlyphBuffer gb;
  const uint32_t initial[] = { 1, 2, 3 };
  EXPECT_OK(gb.set_glyphs(initial, BL_ARRAY_SIZE(initial)));

  // Exercise the buffer replacement used by expanding GSUB lookups, beyond the initial capacity.
  OpenType::GSubContextPrimary ctx;
  ctx.init(bl_glyph_buffer_get_impl(&gb));
  for (size_t count : {size_t(600), size_t(900)}) {
    EXPECT_OK(ctx.prepare_output_buffer(count));
    for (size_t i = 0; i < count; i++) {
      ctx.glyph_data()[i] = uint32_t(i % 3u) + 1u;
      ctx.info_data()[i] = BLGlyphInfo { uint32_t(i), 0 };
    }
    ctx.done();
    EXPECT_EQ(gb.size(), count);
    EXPECT_EQ(gb.content()[count - 1], uint32_t((count - 1) % 3u) + 1u);
    EXPECT_EQ(gb.info_data()[count - 1].cluster, uint32_t(count - 1));
    EXPECT_EQ(gb.content(), ctx.glyph_data());
    expect_glyph_run(gb);
  }

  BLGlyphBuffer copy(gb);
  EXPECT_OK(copy.make_mutable());
  expect_same_data(copy, gb);
  expect_glyph_run(copy);
}

TEST_CASE(core_glyph_buffer_rendering) {
  BLFont font = make_font();
  char text[320];
  for (size_t i = 0; i < sizeof(text); i++) {
    text[i] = "AV office "[i % 10u];
  }

  // Exercise embedded text and the large-text job that transfers a glyph buffer to the renderer.
  for (size_t size : { size_t(20), sizeof(text) }) {
    BLGlyphBuffer gb;
    EXPECT_OK(gb.set_utf8_text(text, size));
    EXPECT_OK(font.shape(gb));
    expect_glyph_run(gb);

    BLImage reference;
    EXPECT_OK(reference.create(480, 64, BL_FORMAT_PRGB32));
    BLContext ctx;
    EXPECT_OK(ctx.begin(reference));
    EXPECT_OK(ctx.clear_all());
    EXPECT_OK(ctx.fill_glyph_run(BLPoint(3, 40), font, gb.glyph_run(), BLRgba32(0xFFFFFFFFu)));
    EXPECT_OK(ctx.end());

    for (uint32_t thread_count = 0; thread_count <= 2; thread_count++) {
      for (uint32_t render_text = 0; render_text <= 1; render_text++) {
        BLImage image;
        EXPECT_OK(image.create(480, 64, BL_FORMAT_PRGB32));
        BLContextCreateInfo options {};
        options.thread_count = thread_count;
        EXPECT_OK(ctx.begin(image, options));
        EXPECT_OK(ctx.clear_all());
        if (render_text) {
          EXPECT_OK(ctx.fill_utf8_text(BLPoint(3, 40), font, text, size, BLRgba32(0xFFFFFFFFu)));
        }
        else {
          BLGlyphBuffer copy(gb);
          EXPECT_OK(ctx.fill_glyph_run(BLPoint(3, 40), font, copy.glyph_run(), BLRgba32(0xFFFFFFFFu)));
          EXPECT_OK(copy.reset());
        }
        EXPECT_OK(ctx.end());
        EXPECT_EQ(image, reference);
      }
    }
  }
}

} // {bl::GlyphBufferTests}

#endif // BL_TEST
