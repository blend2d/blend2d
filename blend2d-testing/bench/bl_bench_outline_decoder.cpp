#include <blend2d/blend2d.h>

#include <blend2d-testing/commons/zero_bench.h>
#include <blend2d-testing/resources/abeezee_regular_ttf.h>

#include <algorithm>
#include <vector>

namespace bl::Benchmarks {

class GlyphOutlineBenchmark : public ZeroBench::Benchmark {
public:
  BLString _font_path;
  BLFontData _font_data;
  BLFont _font;
  bool _is_path_based {};
  bool _use_unique_paths {};
  uint8_t _reserved[12] {}; // Avoid MSVC warning about struct padding.

  GlyphOutlineBenchmark(BLStringView name, BLStringView font_path)
    : ZeroBench::Benchmark(),
      _font_path(font_path),
      _is_path_based(true) {
    set_label(0, name.data(), name.size());
  }

  GlyphOutlineBenchmark(BLStringView name, const BLFontData& font_data)
    : ZeroBench::Benchmark(),
      _font_data(font_data),
      _is_path_based(false) {
    set_label(0, name.data(), name.size());
  }

  void prepare(ZeroBench::Context& ctx) override {
    BLFontData font_data;

    if (_is_path_based) {
      BLResult result = font_data.create_from_file(_font_path.data());
      if (result != BL_SUCCESS) {
        BLString message;
        message.assign_format("Failed to load BLFontData: %s", bl_result_as_string(result));
        ctx.make_failed(message.data());
        return;
      }
    }
    else {
      font_data = _font_data;
    }

    BLFontFace font_face;
    BLResult result = font_face.create_from_data(font_data, 0u);

    if (result != BL_SUCCESS) {
      ctx.make_failed("Failed to create BLFontFace");
      return;
    }

    result = _font.create_from_face(font_face, 100.0f);
    if (result != BL_SUCCESS) {
      ctx.make_failed("Failed to create BLFont");
      return;
    }
  }

  void run(ZeroBench::Context& ctx) override {
    uint32_t glyph_count = _font.face().glyph_count();
    uint64_t all_paths_size = 0;

    if (_use_unique_paths) {
      for (uint32_t glyph_id = 0; glyph_id < glyph_count; glyph_id++) {
        BLPath path;
        _font.get_glyph_outlines(glyph_id, path);
        all_paths_size += path.byte_size();
      }
    }
    else {
      BLPath path;
      for (uint32_t glyph_id = 0; glyph_id < glyph_count; glyph_id++) {
        path.clear();
        _font.get_glyph_outlines(glyph_id, path);
        all_paths_size += path.byte_size();
      }
    }

    ctx.end_now();
    ctx.set_value(0, glyph_count);
    ctx.set_value(1, all_paths_size);
  }

  void teardown() override { _font.reset(); }
};

} // {bl::Benchmarks}

static bool has_suffix(BLStringView file_name, BLStringView ext) {
  if (file_name.size() <= ext.size()) {
    return false;
  }

  const char* a_data = file_name.data() + file_name.size() - ext.size();
  const char* b_data = ext.data() + ext.size() - ext.size();

  for (size_t i = 0; i < ext.size(); i++) {
    uint32_t a = static_cast<uint8_t>(a_data[i]);
    uint32_t b = static_cast<uint8_t>(b_data[i]);

    if (a >= uint32_t('a') && a <= uint32_t('z')) a -= uint32_t('a') - uint32_t('A');
    if (b >= uint32_t('a') && a <= uint32_t('z')) b -= uint32_t('a') - uint32_t('A');

    if (a != b) {
      return false;
    }
  }

  return true;
};

int main(int argc, char* argv[]) {
  using namespace bl::Benchmarks;

  ZeroBench::Runner runner(argc, argv);

  const char* path = runner.cmd_line().get_arg("--path", nullptr);
  if (argc == 2 && argv[1][0] != '-') {
    path = argv[1];
  }

  std::vector<ZeroBench::BenchmarkRef> benchmarks;

  if (path) {
    BLFileInfo entry_info;
    BLResult result = BLFileSystem::get_file_info(path, &entry_info);

    if (result != BL_SUCCESS) {
      printf("Failed to access the provided path %s: %s\n", path, bl_result_as_string(result));
      return 1;
    }

    if (entry_info.is_regular()) {
      BLStringView path_sv{path, strlen(path)};
      benchmarks.push_back(ZeroBench::BenchmarkRef::make<GlyphOutlineBenchmark>(path_sv, path_sv));
    }
    else if (entry_info.is_directory()) {
      BLDirectoryIterator dir_iterator;
      result = dir_iterator.open(path);

      if (result != BL_SUCCESS) {
        printf("Failed to open the provided directory %s: %s\n", path, bl_result_as_string(result));
        return 1;
      }

      BLStringView entry_name;
      while (dir_iterator.next(&entry_name, &entry_info) == BL_SUCCESS) {
        if (has_suffix(entry_name, BLStringView{".ttf", 4}) ||
            has_suffix(entry_name, BLStringView{".otf", 4}) ||
            has_suffix(entry_name, BLStringView{".ttc", 4}) ||
            has_suffix(entry_name, BLStringView{".otc", 4})
        ) {
          BLString file_path;
          file_path.assign_format("%s/%s", path, entry_name.data());
          benchmarks.push_back(ZeroBench::BenchmarkRef::make<GlyphOutlineBenchmark>(entry_name, file_path.span()));
        }
      }
    }
    else {
      printf("The provided path '%s' is an unsupported object\n", path);
      return 1;
    }
  }
  else {
    BLString entry_name("abeezee_regular.ttf (built-in)");
    BLFontData font_data;
    BLResult result = font_data.create_from_data(resource_abeezee_regular_ttf, sizeof(resource_abeezee_regular_ttf));

    if (result != BL_SUCCESS) {
      printf("Failed to create a built-int font-data (%s): %s\n", entry_name.data(), bl_result_as_string(result));
      return 1;
    }

    benchmarks.push_back(ZeroBench::BenchmarkRef::make<GlyphOutlineBenchmark>(entry_name.span(), font_data));
  }

  if (benchmarks.size() == 0) {
    printf("No files to benchmark\n");
    return 1;
  }

  std::sort(benchmarks.begin(), benchmarks.end(),
    [](const ZeroBench::BenchmarkRef& a, const ZeroBench::BenchmarkRef& b) -> bool { return a->name().less(b->name()); }
  );

  runner.set_label(0, "Outline Decoder Benchmark");
  runner.add_metric("Glyph Count", {
    .color = ZeroBench::kDefaultMetricColor,
    .value_kind = ZeroBench::ValueKind::kCount,
    .value_index = 0,
    .aggregate = ZeroBench::Aggregate::kStatic,
    .scale = ZeroBench::Scale::kDefault
  });
  runner.add_metric("Path Size", {
    .color = ZeroBench::kDefaultMetricColor,
    .value_kind = ZeroBench::ValueKind::kBytes,
    .value_index = 1,
    .aggregate = ZeroBench::Aggregate::kStatic,
    .scale = ZeroBench::Scale::kDefault
  });
  runner.add_metric("Iterations", {
    .color = ZeroBench::kDefaultMetricColor,
    .value_kind = ZeroBench::ValueKind::kCount,
    .value_index = ZeroBench::kIterCountValueIndex,
    .aggregate = ZeroBench::Aggregate::kStatic,
    .scale = ZeroBench::Scale::kDefault
  });
  runner.add_time_metrics();

  return runner.run(benchmarks.data(), benchmarks.size());
}
