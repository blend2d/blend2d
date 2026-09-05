#include <blend2d/blend2d.h>

#include <blend2d-testing/commons/cmdline.h>
#include <blend2d-testing/commons/performance_timer.h>
#include <blend2d-testing/resources/abeezee_regular_ttf.h>

static size_t benchmark_outline_decoder(const BLFont& font, uint32_t repeat_count, bool same_path) noexcept {
  uint32_t glyph_count = font.face().glyph_count();
  size_t result = 0;

  if (same_path) {
    BLPath path;
    for (uint32_t test_id = 0; test_id < repeat_count; test_id++) {
      for (uint32_t glyph_id = 0; glyph_id < glyph_count; glyph_id++) {
        path.clear();
        font.get_glyph_outlines(glyph_id, path);
        result += path.size();
      }
    }
  }
  else {
    for (uint32_t test_id = 0; test_id < repeat_count; test_id++) {
      for (uint32_t glyph_id = 0; glyph_id < glyph_count; glyph_id++) {
        BLPath path;
        font.get_glyph_outlines(glyph_id, path);
        result += path.size();
      }
    }
  }
  return result;
}

template<typename T>
static void consume(T dummy) {
  [[maybe_unused]] volatile T stack = dummy;
}

int main(int argc, char* argv[]) {
  CmdLine cmd_line(argc, argv);

  uint32_t repeat = 100000;
  if (cmd_line.has_arg("--repeat")) {
    repeat = cmd_line.value_as_uint("--repeat", repeat);
  }

  BLFontData font_data;
  BLResult result = font_data.create_from_data(resource_abeezee_regular_ttf, sizeof(resource_abeezee_regular_ttf));

  if (result != BL_SUCCESS) {
    printf("Failed to create font-data.\n");
    return 1;
  }

  BLFontFace font_face;
  result = font_face.create_from_data(font_data, 0u);

  if (result != BL_SUCCESS) {
    printf("Failed to create font-face.\n");
    return 1;
  }

  BLFont font;
  result = font.create_from_face(font_face, 100.0f);

  if (result != BL_SUCCESS) {
    printf("Failed to create font.\n");
    return 1;
  }

  PerformanceTimer timer;
  timer.start();
  consume(benchmark_outline_decoder(font, repeat, true));
  timer.stop();

  printf("BLFont::get_glyph_outlines() [%u glyphs, repeat=%u]: %0.3f [ms]\n", font_face.glyph_count(), repeat, timer.duration());
  return 0;
}
