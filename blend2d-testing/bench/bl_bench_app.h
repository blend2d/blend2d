// This file is part of Blend2D project <https://blend2d.com>
//
// See LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BL_BENCH_APP_H
#define BL_BENCH_APP_H

#include <blend2d/blend2d.h>
#include <blend2d-testing/commons/cmdline.h>
#include <blend2d-testing/commons/jsonbuilder.h>
#include <blend2d-testing/bench/bl_bench_backend.h>

#include <array>
#include <unordered_map>

namespace blbench {

struct BenchApp {
  CmdLine _cmd_line;

  // Configuration.
  uint32_t _width = 512;
  uint32_t _height = 600;
  uint32_t _comp_op = 0xFFFFFFFF;
  uint32_t _size_count = kBenchShapeSizeCount;
  uint32_t _quantity = 0;
  uint32_t _repeat = 1;
  uint32_t _backends = 0xFFFFFFFF;

  bool _save_images = false;
  bool _save_overview = false;
  bool _isolated = false;
  bool _deep_bench = false;

  // Assets.
  using SpriteData = std::array<BLImage, 4>;

  SpriteData _sprite_data;
  mutable std::unordered_map<uint32_t, SpriteData> _scaled_sprites;

  BenchApp(int argc, char** argv);
  ~BenchApp();

  void print_app_info() const;
  void print_options() const;
  void print_backends() const;

  bool parse_command_line();
  bool init();
  void info();

  bool read_image(BLImage&, const char* name, const void* data, size_t size) noexcept;

  BLImage get_scaled_sprite(uint32_t id, uint32_t size) const;

  bool is_backend_enabled(BackendKind backend_kind) const;
  bool is_style_enabled(StyleKind style) const;

  void serialize_system_info(JSONBuilder& json) const;
  void serialize_params(JSONBuilder& json, const BenchParams& params) const;
  void serialize_options(JSONBuilder& json, const BenchParams& params) const;

  int run();
  int run_backend_tests(Backend& backend, BenchParams& params, JSONBuilder& json);
  uint64_t run_single_test(Backend& backend, BenchParams& params);
};

} // {blbench}

#endif // BL_BENCH_APP_H
