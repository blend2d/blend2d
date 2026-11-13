#include <blend2d/blend2d.h>

#include <blend2d-testing/commons/zero_bench.h>
#include <blend2d-testing/resources/abeezee_regular_ttf.h>

#include <algorithm>
#include <initializer_list>
#include <vector>

namespace bl::Benchmarks {

static bool walk_directory(BLString path, auto cb) noexcept {
  constexpr size_t kBulkRemove = 128;

  size_t path_base_size = path.size();
  size_t pending_index = 0u;
  BLArray<BLString> pending_entries;
  BLString entry_prefix;

  for (;;) {
    BLDirectoryIterator dir_iterator;
    BLResult result = dir_iterator.open(path.data());

    if (result != BL_SUCCESS) {
      printf("Failed to open directory %s: %s\n", path.data(), bl_result_as_string(result));
      return false;
    }

    BLStringView entry_name;
    BLFileInfo entry_info;

    while (dir_iterator.next(&entry_name, &entry_info) == BL_SUCCESS) {
      BLString entry_path(entry_prefix);
      if (!entry_path.is_empty()) {
        entry_path.append('/');
      }
      entry_path.append(entry_name);

      BLString full_path(path);
      full_path.append('/');
      full_path.append(entry_name);

      if (entry_info.is_regular()) {
        cb(full_path, entry_path);
      }
      else if (entry_info.is_directory()) {
        pending_entries.append(entry_path);
      }
    }

    if (pending_index >= pending_entries.size()) {
      break;
    }

    entry_prefix = pending_entries[pending_index];
    path.truncate(path_base_size);
    path.append('/');
    path.append(entry_prefix);

    if (++pending_index == kBulkRemove) {
      pending_index = 0;
      pending_entries.remove(0, kBulkRemove);
    }
  }
  return true;
};

static void stringify_image_info(ZeroBench::String& sb, const BLImageInfo& info) noexcept {
  sb.append_format("%s/%s", info.format, info.compression);
}

static void stringify_pixel_format(ZeroBench::String& sb, BLFormat format) noexcept {
  switch (format) {
    case BL_FORMAT_PRGB32:
      sb.append("PRGB32");
      break;

    case BL_FORMAT_XRGB32:
      sb.append("XRGB32");
      break;

    case BL_FORMAT_A8:
      sb.append("A8");
      break;

    default:
      sb.append("?");
      break;
  }
}

#define FAIL_ON_ERROR(...) do {                    \
  BLResult _result = __VA_ARGS__;                  \
  if (_result != BL_SUCCESS) {                     \
    ctx.make_failed(bl_result_as_string(_result)); \
    return;                                        \
  }                                                \
} while(0)

class ImageDecoderBenchmark : public ZeroBench::Benchmark {
public:
  BLString _image_path;
  BLImageCodec _image_codec;
  BLArray<uint8_t> _image_data;

  ImageDecoderBenchmark(BLStringView name, BLStringView path) : ZeroBench::Benchmark() {
    set_label(0u, name.data(), name.size());
    _image_path.assign(path);
  }

  void prepare(ZeroBench::Context& ctx) override {
    BLImage img;
    BLImageInfo img_info{};
    BLImageDecoder decoder;

    FAIL_ON_ERROR(BLFileSystem::read_file(_image_path.data(), _image_data));
    FAIL_ON_ERROR(_image_codec.find_by_data(_image_data));
    FAIL_ON_ERROR(_image_codec.create_decoder(&decoder));

    FAIL_ON_ERROR(decoder.read_info(img_info, _image_data));
    FAIL_ON_ERROR(decoder.read_frame(img, _image_data));

    ZeroBench::String sb;
    stringify_image_info(sb, img_info);
    set_label(1, sb);
  }

  void run(ZeroBench::Context& ctx) override {
    BLImage img;
    BLImageDecoder decoder;

    FAIL_ON_ERROR(_image_codec.create_decoder(&decoder));
    FAIL_ON_ERROR(decoder.read_frame(img, _image_data));

    ctx.end_now();
    ctx.set_value(0, static_cast<unsigned>(img.width()) * static_cast<unsigned>(img.height()));
    ctx.set_value(1, _image_data.size());
  }

  void teardown() override {
    _image_data.reset();
    _image_codec.reset();
  }
};

class ImageEncoderBenchmark : public ZeroBench::Benchmark {
public:
  BLImageCodec _image_codec;
  BLString _image_path;
  BLImage _image;

  ImageEncoderBenchmark(BLStringView name, BLStringView path, const BLImageCodec& codec) : ZeroBench::Benchmark() {
    set_label(0u, name.data(), name.size());
    _image_path.assign(path);
    _image_codec.assign(codec);
  }

  void prepare(ZeroBench::Context& ctx) override {
    BLImageInfo img_info{};
    FAIL_ON_ERROR(_image.read_from_file_with_info(_image_path.data(), &img_info));

    ZeroBench::String sb;
    stringify_image_info(sb, img_info);
    set_label(1, sb);

    sb.clear();
    stringify_pixel_format(sb, _image.format());
    set_label(2, sb);
  }

  void run(ZeroBench::Context& ctx) override {
    BLImageEncoder encoder;

    bool is_png = _image_codec.name().equals("PNG");

    FAIL_ON_ERROR(_image_codec.create_encoder(&encoder));
    ctx.set_value(0, static_cast<unsigned>(_image.width()) * static_cast<unsigned>(_image.height()));

    if (is_png) {
      for (uint32_t level = 1; level <= 12; level++) {
        encoder.restart();

        FAIL_ON_ERROR(encoder.set_property("compression", level));

        ZeroBench::Timer timer;
        BLArray<uint8_t> buffer;

        timer.start();
        BLResult result = encoder.write_frame(buffer, _image);
        uint64_t t = timer.elapsed();

        FAIL_ON_ERROR(result);

        ctx.set_value(level * 2u + 0, buffer.size());
        ctx.set_value(level * 2u + 1, t);
      }
    }
    else {
      ZeroBench::Timer timer;
      BLArray<uint8_t> buffer;

      timer.start();
      BLResult result = encoder.write_frame(buffer, _image);
      uint64_t t = timer.elapsed();

      if (result != BL_SUCCESS) {
        ctx.make_failed(bl_result_as_string(result));
        return;
      }

      ctx.set_value(1, buffer.size());
      ctx.set_value(2, t);
    }
  }

  void teardown() override {
    _image.reset();
    _image_codec.reset();
  }
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

  const char* encode_param = runner.cmd_line().get_arg("--encode", nullptr);
  BLImageCodec codec;

  if (encode_param) {
    BLResult result = codec.find_by_name(encode_param);
    if (result != BL_SUCCESS) {
      printf("Invalid --encode argument: Codec '%s' not available\n", encode_param);
      return 1;
    }
  }

  std::vector<ZeroBench::BenchmarkRef> benchmarks;

  auto make_bench = [&](BLStringView name, BLStringView path) {
    if (encode_param) {
      return ZeroBench::BenchmarkRef::make<ImageEncoderBenchmark>(name, path, codec);
    }
    else {
      return ZeroBench::BenchmarkRef::make<ImageDecoderBenchmark>(name, path);
    }
  };

  runner.set_label(0, "Image");
  if (runner.cmd_line().has_arg("--file-format")) {
    runner.set_label(1, "File Format");
  }
  runner.set_label(2, "Format");

  if (encode_param) {
    runner.set_metric(0, "Pixels", {
      .color = ZeroBench::kDefaultMetricColor,
      .value_kind = ZeroBench::ValueKind::kPixels,
      .value_index = 0,
      .aggregate = ZeroBench::Aggregate::kStatic,
      .scale = ZeroBench::Scale::kDefault
    });

    if (codec.name().equals("PNG")) {
      for (uint32_t level : {1u, 2u, 3u, 4u, 6u, 8u, 10u, 12u}) {
        runner.add_metric(ZeroBench::String::format("Comp. L%u", level), {
          .color = ZeroBench::Color::kBrightCyan,
          .value_kind = ZeroBench::ValueKind::kBytes,
          .value_index = static_cast<uint8_t>(level * 2u + 0u),
          .aggregate = ZeroBench::Aggregate::kStatic,
          .scale = ZeroBench::Scale::kDefault
        });

        runner.add_metric(ZeroBench::String::format("T.Med L%u", level), {
          .color = ZeroBench::kOtherDurationColor,
          .value_kind = ZeroBench::ValueKind::kDuration,
          .value_index = static_cast<uint8_t>(level * 2u + 1u),
          .aggregate = ZeroBench::Aggregate::kP50,
          .scale = ZeroBench::Scale::kDefault
        });
      }
    }
    else {
      runner.add_metric(codec.name().data(), {
        .color = ZeroBench::Color::kBrightYellow,
        .value_kind = ZeroBench::ValueKind::kBytes,
        .value_index = 1,
        .aggregate = ZeroBench::Aggregate::kStatic,
        .scale = ZeroBench::Scale::kDefault
      });

      runner.add_metric(ZeroBench::String::format("%s Enc", codec.name().data()), {
        .color = ZeroBench::kOtherDurationColor,
        .value_kind = ZeroBench::ValueKind::kDuration,
        .value_index = 2,
        .aggregate = ZeroBench::Aggregate::kP50,
        .scale = ZeroBench::Scale::kDefault
      });
    }
  }
  else {
    runner.add_metric("Pixels", {
      .color = ZeroBench::kDefaultMetricColor,
      .value_kind = ZeroBench::ValueKind::kPixels,
      .value_index = 0,
      .aggregate = ZeroBench::Aggregate::kStatic,
      .scale = ZeroBench::Scale::kDefault
    });
    runner.add_metric("Compressed", {
      .color = ZeroBench::Color::kBrightCyan,
      .value_kind = ZeroBench::ValueKind::kBytes,
      .value_index = 1,
      .aggregate = ZeroBench::Aggregate::kStatic,
      .scale = ZeroBench::Scale::kDefault
    });
    runner.add_time_metrics();
  }

  if (path) {
    BLFileInfo entry_info;
    BLResult result = BLFileSystem::get_file_info(path, &entry_info);

    if (result != BL_SUCCESS) {
      printf("Failed to access the provided path %s: %s\n", path, bl_result_as_string(result));
      return 1;
    }

    if (entry_info.is_regular()) {
      BLStringView path_sv{path, strlen(path)};
      benchmarks.push_back(make_bench(path_sv, path_sv));
    }
    else if (entry_info.is_directory()) {
      walk_directory(BLString(path), [&](const BLString& full_path, const BLString& rel_path) {
        if (has_suffix(rel_path.span(), BLStringView{".bmp", 4}) ||
            has_suffix(rel_path.span(), BLStringView{".png", 4}) ||
            has_suffix(rel_path.span(), BLStringView{".jpg", 4}) ||
            has_suffix(rel_path.span(), BLStringView{".jpeg", 5}) ||
            has_suffix(rel_path.span(), BLStringView{".qoi", 4})
        ) {
          benchmarks.push_back(make_bench(rel_path.span(), full_path.span()));
        }
      });
    }
    else {
      printf("The provided path '%s' is an unsupported object\n", path);
      return 1;
    }
  }

  if (benchmarks.size() == 0) {
    printf("No files to benchmark: Use --path <path> to specify a directory where to look for images\n");
    return 1;
  }

  std::sort(benchmarks.begin(), benchmarks.end(),
    [](const ZeroBench::BenchmarkRef& a, const ZeroBench::BenchmarkRef& b) -> bool { return a->name().less(b->name()); }
  );

  return runner.run(benchmarks.data(), benchmarks.size());
}
