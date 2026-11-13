// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/blend2d.h>

#include <stdio.h>
#include <string.h>

static constexpr uint32_t kCharRangeInTag = 95;
static constexpr uint32_t kUniqueTagCount = kCharRangeInTag * kCharRangeInTag * kCharRangeInTag * kCharRangeInTag;

static void print_string_allocation_strategy() {
  printf("BLString:\n");

  BLString s;
  size_t threshold = 10000000;
  size_t capacity = s.capacity();

  printf("  Initial capacity (SSO) = %zu\n", capacity);

  for (size_t i = 0; i < threshold; i++) {
    char c = char(size_t('a') + (i % size_t('z' - 'a')));
    s.append(c);
    if (capacity != s.capacity()) {
      printf("  Capacity increased from %zu to %zu\n", capacity, s.capacity());
      capacity = s.capacity();
    }
  }

  printf("\n");
}

static void print_coverage_set_strategy() {
  printf("BLCoverageSet:\n");

  BLCoverageSet cov;
  uint32_t threshold = 0x110000u;
  size_t capacity = cov.page_capacity();

  printf("  Initial capacity = %zu\n", capacity);

  for (uint32_t i = 0; i < threshold; i+=32) {
    cov.add(uint32_t(i));
    if (capacity != cov.page_capacity()) {
      printf("  Capacity increased from %zu to %zu\n", capacity, cov.page_capacity());
      capacity = cov.page_capacity();
    }
  }

  printf("\n");
}

static void print_path_allocation_strategy() {
  printf("BLPath:\n");

  BLPath p;
  size_t threshold = 1000000;
  size_t capacity = p.byte_capacity();

  printf("  Initial capacity = %zu\n", capacity);

  for (size_t i = 0; i < threshold; i++) {
    if (i == 0)
      p.move_to(0, 0);
    else
      p.line_to(double(i), double(i));

    if (capacity != p.byte_capacity()) {
      printf("  Capacity increased from %zu to %zu\n", capacity, p.byte_capacity());
      capacity = p.byte_capacity();
    }
  }

  printf("\n");
}

static void print_font_feature_settings_allocation_strategy() {
  printf("BLFontFeatureSettings:\n");

  BLFontFeatureSettings ffs;
  size_t capacity = ffs.capacity();

  printf("  Initial capacity (SSO) = %zu\n", capacity);

  constexpr uint32_t threshold = kUniqueTagCount / 100;
  for (uint32_t i = 0; i < threshold; i++) {
    BLTag tag = BL_MAKE_TAG(
      uint32_t(' ') + (i / (kCharRangeInTag * kCharRangeInTag * kCharRangeInTag)),
      uint32_t(' ') + (i / (kCharRangeInTag * kCharRangeInTag)) % kCharRangeInTag,
      uint32_t(' ') + (i / (kCharRangeInTag)) % kCharRangeInTag,
      uint32_t(' ') + (i % kCharRangeInTag));

    ffs.set_value(tag, i & 0xFFFFu);
    if (capacity != ffs.capacity()) {
      printf("  Capacity increased from %zu to %zu\n", capacity, ffs.capacity());
      capacity = ffs.capacity();
    }
  }

  printf("\n");
}

static void print_font_variation_settings_allocation_strategy() {
  printf("BLFontVariationSettings:\n");

  BLFontVariationSettings fvs;
  size_t capacity = fvs.capacity();

  printf("  Initial capacity (SSO) = %zu\n", capacity);

  constexpr uint32_t threshold = kUniqueTagCount / 100;
  for (uint32_t i = 0; i < threshold; i++) {
    BLTag tag = BL_MAKE_TAG(
      uint32_t(' ') + (i / (kCharRangeInTag * kCharRangeInTag * kCharRangeInTag)),
      uint32_t(' ') + (i / (kCharRangeInTag * kCharRangeInTag)) % kCharRangeInTag,
      uint32_t(' ') + (i / (kCharRangeInTag)) % kCharRangeInTag,
      uint32_t(' ') + (i % kCharRangeInTag));

    fvs.set_value(tag, float(i & 0xFFFFu));
    if (capacity != fvs.capacity()) {
      printf("  Capacity increased from %zu to %zu\n", capacity, fvs.capacity());
      capacity = fvs.capacity();
    }
  }

  printf("\n");
}

static void print_gradient_allocation_strategy() {
  printf("BLGradient:\n");

  BLGradient g;
  size_t threshold = 10000000;
  size_t capacity = g.capacity();

  printf("  Initial capacity = %zu\n", capacity);

  for (size_t i = 0; i < threshold; i++) {
    g.add_stop(double(i) / double(threshold), BLRgba32(0xFFFFFFFF));

    if (capacity != g.capacity()) {
      printf("  Capacity increased from %zu to %zu\n", capacity, g.capacity());
      capacity = g.capacity();
    }
  }

  printf("\n");
}

int main() {
  BLRuntimeBuildInfo build_info;
  BLRuntime::query_build_info(&build_info);

  printf(
    "Blend2D Allocation Strategy Tests v%u.%u.%u %s/%s [--help for options]\n\n",
    build_info.major_version,
    build_info.minor_version,
    build_info.patch_version,
    build_info.build_type == BL_RUNTIME_BUILD_TYPE_DEBUG ? "debug" : "release",
    build_info.compiler_info);
  fflush(stdout);

  BLRuntimeScope rt_scope;

  print_string_allocation_strategy();
  print_coverage_set_strategy();
  print_path_allocation_strategy();
  print_font_feature_settings_allocation_strategy();
  print_font_variation_settings_allocation_strategy();
  print_gradient_allocation_strategy();

  return 0;
}
