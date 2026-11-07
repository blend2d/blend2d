# Blend2D
# =======

# To consume blend2d as a dependency, use `blend2d::blend2d` alias.

cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

# Blend2D - Integration
# =====================

# This is a hack for CMakePresets.json, which cannot conditionally include this when building Blend2D demos.
# So instead, it passes BLEND2D_SETUP_VCPKG_TOOLCHAIN_FILE set to ON and the logic of setting up the toolchain
# is here.
if (BLEND2D_SETUP_VCPKG_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
  message(STATUS "[blend2d] Setting CMake toolchain file to $ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()

# Blend2D - Project & Configuration
# =================================

project(blend2d
  LANGUAGES C CXX
  DESCRIPTION "High-performance 2D vector graphics engine with JIT compiler"
  HOMEPAGE_URL "https://blend2d.com")

# BLEND2D_XXX are cmake variables whereas BL_XXX are build-time macros passed to compiler via flags.
set(BLEND2D_SANITIZE           "" CACHE STRING "Sanitizers to use (development)")
set(BLEND2D_SANITIZE_OPTS      "" CACHE STRING "Extra flags for sanitizers (development)")

option(BLEND2D_STATIC          "Build 'blend2d' as a static library instead" OFF)
option(BLEND2D_TEST            "Build 'blend2d' tests and samples (development)" OFF)
option(BLEND2D_DEMOS           "Build 'blend2d' interactive demos that use Qt library (development)" OFF)

option(BLEND2D_NO_NATVIS       "Disable 'blend2d' natvis support (development)" OFF)
option(BLEND2D_NO_JIT          "Disable 'blend2d' JIT pipelines generation support (not recommended)" OFF)
option(BLEND2D_NO_JIT_LOGGING  "Disable 'blend2d' JIT pipelines logging support (deployment)" OFF)
option(BLEND2D_NO_TLS          "Disable 'blend2d' use of TLS (development)" OFF)
option(BLEND2D_NO_FUTEX        "Disable 'blend2d' use of futexes (development)" OFF)
option(BLEND2D_NO_INSTALL      "Disable 'blend2d' install support for cmake (development/deployment)" OFF)
option(BLEND2D_NO_FILESYSTEM   "Disable 'blend2d' filesystem API (optimizing for size)" OFF)
option(BLEND2D_EXTERNAL_ASMJIT "Enable support for external asmjit library (no support by upstream)" OFF)

# Avoid linking to the C++ stdlib by default if blend2d is being built as a shared library with bundled asmjit.
if (NOT BLEND2D_STATIC AND NOT BLEND2D_EXTERNAL_ASMJIT AND NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  option(BLEND2D_NO_STDCXX     "Disable 'blend2d' linking to a standard C++ library (deployment)" ON)
else()
  option(BLEND2D_NO_STDCXX     "Disable 'blend2d' linking to a standard C++ library (deployment)" OFF)
endif()

# Blend2D - Build Definitions and Source Files
# ============================================

set(BLEND2D_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}")   # Blend2D include directory is the same as <root> directory.
set(BLEND2D_LIBS "")                                    # Blend2D library dependencies for the linker.
set(BLEND2D_CFLAGS "")                                  # Public compiler flags.
set(BLEND2D_WARNING_CFLAGS "")                          # Flags that enable compiler warnings (we add them to various targets).
set(BLEND2D_PRIVATE_CFLAGS "")                          # Private compiler flags independent of build type.
set(BLEND2D_PRIVATE_CFLAGS_DBG "")                      # Private compiler flags used by debug builds.
set(BLEND2D_PRIVATE_CFLAGS_REL "")                      # Private compiler flags used by release builds.
set(BLEND2D_PRIVATE_LFLAGS "")                          # Private linker flags.
set(BLEND2D_SANITIZE_CFLAGS "${BLEND2D_SANITIZE_OPTS}") # Compiler flags required by currently enabled sanitizers.
set(BLEND2D_SANITIZE_LFLAGS "")                         # Linker flags required by currently enabled sanitizers.
set(BLEND2D_NO_STDCXX_CFLAGS "")                        # Private compiler flags to disable linking to a standard C++ library.
set(BLEND2D_NO_STDCXX_LFLAGS "")                        # Private linker flags to disable linking to a standard C++ library.

if (BLEND2D_STATIC)
  set(BLEND2D_TARGET_TYPE "STATIC")
  list(APPEND BLEND2D_CFLAGS -DBL_STATIC)
else()
  set(BLEND2D_TARGET_TYPE "SHARED")
endif()
message(STATUS "[blend2d] == Configuring Blend2D ('BLEND2D_TARGET_TYPE=${BLEND2D_TARGET_TYPE}') ==")

set(BLEND2D_AXL_SRC
  blend2d/axl/algorithm_test.cpp
  blend2d/axl/algorithm_p.h
  blend2d/axl/arena_allocator.cpp
  blend2d/axl/arena_allocator_p.h
  blend2d/axl/arena_bit_set_test.cpp
  blend2d/axl/arena_bit_set_p.h
  blend2d/axl/arena_hash_map.cpp
  blend2d/axl/arena_hash_map_test.cpp
  blend2d/axl/arena_hash_map_p.h
  blend2d/axl/arena_list_test.cpp
  blend2d/axl/arena_list_p.h
  blend2d/axl/arena_pool_p.h
  blend2d/axl/arena_tree_test.cpp
  blend2d/axl/arena_tree_p.h
  blend2d/axl/bitops_test.cpp
  blend2d/axl/bitops_p.h
  blend2d/axl/bit_set_ops.h
  blend2d/axl/build_integration.h
  blend2d/axl/commons_test.cpp
  blend2d/axl/commons.h
  blend2d/axl/fixedarray_p.h
  blend2d/axl/fixedbit_array_p.h
  blend2d/axl/lookuptable_p.h
  blend2d/axl/math.cpp
  blend2d/axl/math_p.h
  blend2d/axl/math_test.cpp
  blend2d/axl/mem_alloc.h
  blend2d/axl/mem_ops.h
  blend2d/axl/mem_ops_test.cpp
  blend2d/axl/scopedallocator.cpp
  blend2d/axl/scopedallocator_p.h
  blend2d/axl/scopedbuffer_p.h
  blend2d/axl/string_utils_p.h
  blend2d/axl/string_utils_test.cpp
  blend2d/axl/wrap_p.h
  blend2d/axl/zeroallocator.cpp
  blend2d/axl/zeroallocator_test.cpp
  blend2d/axl/zeroallocator_p.h
)

set(BLEND2D_SRC_LIST
  blend2d/blend2d.h
  blend2d/blend2d-debug.h
  blend2d/blend2d-impl.h

  ${BLEND2D_AXL_SRC}

  blend2d/core/api.h
  blend2d/core/api_build_p.h
  blend2d/core/api_build_test_p.h
  blend2d/core/api_impl.h
  blend2d/core/api_internal_p.h
  blend2d/core/array.cpp
  blend2d/core/array_test.cpp
  blend2d/core/array.h
  blend2d/core/array_p.h
  blend2d/core/bit_array.cpp
  blend2d/core/bit_array_test.cpp
  blend2d/core/bit_array.h
  blend2d/core/bit_array_p.h
  blend2d/core/compop_p.h
  blend2d/core/compopinfo_p.h
  blend2d/core/compopinfo.cpp
  blend2d/core/context.cpp
  blend2d/core/context_test.cpp
  blend2d/core/context.h
  blend2d/core/context_p.h
  blend2d/core/file_system.cpp
  blend2d/core/file_system.h
  blend2d/core/file_system_p.h
  blend2d/core/file_system_test.cpp
  blend2d/core/font.cpp
  blend2d/core/font.h
  blend2d/core/font_p.h
  blend2d/core/font_test.cpp
  blend2d/core/font_data.cpp
  blend2d/core/font_data.h
  blend2d/core/font_data_p.h
  blend2d/core/font_defs.h
  blend2d/core/font_face.cpp
  blend2d/core/font_face.h
  blend2d/core/font_face_p.h
  blend2d/core/font_feature_settings.cpp
  blend2d/core/font_feature_settings_test.cpp
  blend2d/core/font_feature_settings.h
  blend2d/core/font_feature_settings_p.h
  blend2d/core/font_manager.cpp
  blend2d/core/font_manager.h
  blend2d/core/font_manager_p.h
  blend2d/core/font_tag_data_p.h
  blend2d/core/font_tag_data_ids.cpp
  blend2d/core/font_tag_data_ids_test.cpp
  blend2d/core/font_tag_data_ids_p.h
  blend2d/core/font_tag_data_info.cpp
  blend2d/core/font_tag_data_info_test.cpp
  blend2d/core/font_tag_data_info_p.h
  blend2d/core/font_tag_set.cpp
  blend2d/core/font_tag_set_p.h
  blend2d/core/font_variation_settings.cpp
  blend2d/core/font_variation_settings_test.cpp
  blend2d/core/font_variation_settings.h
  blend2d/core/font_variation_settings_p.h
  blend2d/core/format.cpp
  blend2d/core/format.h
  blend2d/core/format_p.h
  blend2d/core/geometry.h
  blend2d/core/glyph_buffer.cpp
  blend2d/core/glyph_buffer.h
  blend2d/core/glyph_buffer_p.h
  blend2d/core/glyph_run.h
  blend2d/core/gradient.cpp
  blend2d/core/gradient_test.cpp
  blend2d/core/gradient.h
  blend2d/core/gradient_p.h
  blend2d/core/image.cpp
  blend2d/core/image_test.cpp
  blend2d/core/image.h
  blend2d/core/image_p.h
  blend2d/core/image_codec.cpp
  blend2d/core/image_codec_test.cpp
  blend2d/core/image_codec.h
  blend2d/core/image_codec_p.h
  blend2d/core/image_decoder.cpp
  blend2d/core/image_decoder.h
  blend2d/core/image_decoder_p.h
  blend2d/core/image_encoder.cpp
  blend2d/core/image_encoder.h
  blend2d/core/image_encoder_p.h
  blend2d/core/image_scale.cpp
  blend2d/core/image_scale_p.h
  blend2d/core/matrix.cpp
  blend2d/core/matrix_avx.cpp
  blend2d/core/matrix_sse2.cpp
  blend2d/core/matrix_test.cpp
  blend2d/core/matrix.h
  blend2d/core/matrix_p.h
  blend2d/core/object.cpp
  blend2d/core/object.h
  blend2d/core/object_layer.h
  blend2d/core/object_layer_p.h
  blend2d/core/path.cpp
  blend2d/core/path_test.cpp
  blend2d/core/path.h
  blend2d/core/path_p.h
  blend2d/core/pattern.cpp
  blend2d/core/pattern_test.cpp
  blend2d/core/pattern.h
  blend2d/core/pattern_p.h
  blend2d/core/pixel_converter.cpp
  blend2d/core/pixel_converter_avx2.cpp
  blend2d/core/pixel_converter_sse2.cpp
  blend2d/core/pixel_converter_ssse3.cpp
  blend2d/core/pixel_converter_test.cpp
  blend2d/core/pixel_converter.h
  blend2d/core/pixel_converter_p.h
  blend2d/core/random.cpp
  blend2d/core/random_test.cpp
  blend2d/core/random.h
  blend2d/core/random_p.h
  blend2d/core/rgba_test.cpp
  blend2d/core/rgba.h
  blend2d/core/runtime.cpp
  blend2d/core/runtime.h
  blend2d/core/runtime_p.h
  blend2d/core/runtime_scope.cpp
  blend2d/core/runtime_scope.h
  blend2d/core/span.h
  blend2d/core/string.cpp
  blend2d/core/string_test.cpp
  blend2d/core/string.h
  blend2d/core/string_p.h
  blend2d/core/stroke_options.cpp
  blend2d/core/stroke_options_test.cpp
  blend2d/core/stroke_options.h
  blend2d/core/stroke_options_p.h
  blend2d/core/trace.cpp
  blend2d/core/trace_p.h
  blend2d/core/coverage_set.cpp
  blend2d/core/coverage_set_test.cpp
  blend2d/core/coverage_set.h
  blend2d/core/coverage_set_p.h
  blend2d/core/var.cpp
  blend2d/core/var_test.cpp
  blend2d/core/var.h
  blend2d/core/var_p.h

  blend2d/codec/bmp_codec.cpp
  blend2d/codec/bmp_codec_p.h
  blend2d/codec/codec_test.cpp
  blend2d/codec/jpeg_codec.cpp
  blend2d/codec/jpeg_codec_p.h
  blend2d/codec/jpeg_huffman.cpp
  blend2d/codec/jpeg_huffman_p.h
  blend2d/codec/jpeg_ops.cpp
  blend2d/codec/jpeg_ops_sse2.cpp
  blend2d/codec/jpeg_ops_p.h
  blend2d/codec/png_codec.cpp
  blend2d/codec/png_codec_p.h
  blend2d/codec/png_encoder_fast.cpp
  blend2d/codec/png_encoder_fast_avx512_v2.cpp
  blend2d/codec/png_encoder_fast_p.h
  blend2d/codec/png_encoder_fast_simd_p.h
  blend2d/codec/png_encoder_fast_ssse3.cpp
  blend2d/codec/png_encoder_fast_test.cpp
  blend2d/codec/png_ops.cpp
  blend2d/codec/png_ops_asimd.cpp
  blend2d/codec/png_ops_avx.cpp
  blend2d/codec/png_ops_sse2.cpp
  blend2d/codec/png_ops_p.h
  blend2d/codec/png_ops_simd_impl_p.h
  blend2d/codec/png_ops_simd_impl_test.cpp
  blend2d/codec/qoi_codec.cpp
  blend2d/codec/qoi_codec_p.h

  blend2d/compression/checksum.cpp
  blend2d/compression/checksum_adler32_simd_impl_p.h
  blend2d/compression/checksum_asimd.cpp
  blend2d/compression/checksum_asimd_crypto.cpp
  blend2d/compression/checksum_crc32_simd_impl_p.h
  blend2d/compression/checksum_sse2.cpp
  blend2d/compression/checksum_sse4_2.cpp
  blend2d/compression/checksum_test.cpp
  blend2d/compression/checksum_p.h
  blend2d/compression/deflate_decoder.cpp
  blend2d/compression/deflate_decoder_p.h
  blend2d/compression/deflate_decoder_fast.cpp
  blend2d/compression/deflate_decoder_fast_avx2.cpp
  blend2d/compression/deflate_decoder_fast_p.h
  blend2d/compression/deflate_decoder_fast_impl_p.h
  blend2d/compression/deflate_decoder_utils.cpp
  blend2d/compression/deflate_decoder_utils_p.h
  blend2d/compression/deflate_defs_p.h
  blend2d/compression/deflate_defs.cpp
  blend2d/compression/deflate_encoder.cpp
  blend2d/compression/deflate_encoder_p.h
  blend2d/compression/deflate_encoder_utils_p.h
  blend2d/compression/deflate_test.cpp
  blend2d/compression/match_finder_p.h

  blend2d/geometry/bezier_test.cpp
  blend2d/geometry/bezier_p.h
  blend2d/geometry/commons_p.h
  blend2d/geometry/dasher.cpp
  blend2d/geometry/dasher_p.h
  blend2d/geometry/dasher_test.cpp
  blend2d/geometry/intersect_test.cpp
  blend2d/geometry/intersect_p.h
  blend2d/geometry/size_table.cpp
  blend2d/geometry/size_table_p.h
  blend2d/geometry/stroke.cpp
  blend2d/geometry/stroke_test.cpp
  blend2d/geometry/stroke_p.h
  blend2d/geometry/tolerance_p.h

  blend2d/opentype/ot_cff.cpp
  blend2d/opentype/ot_cff_test.cpp
  blend2d/opentype/ot_cff_p.h
  blend2d/opentype/ot_cmap.cpp
  blend2d/opentype/ot_cmap_p.h
  blend2d/opentype/ot_core.cpp
  blend2d/opentype/ot_core_p.h
  blend2d/opentype/ot_defs_p.h
  blend2d/opentype/ot_face.cpp
  blend2d/opentype/ot_face_p.h
  blend2d/opentype/ot_glyf.cpp
  blend2d/opentype/ot_glyf_asimd.cpp
  blend2d/opentype/ot_glyf_avx2.cpp
  blend2d/opentype/ot_glyf_sse4_2.cpp
  blend2d/opentype/ot_glyf_p.h
  blend2d/opentype/ot_glyf_simd_data_p.h
  blend2d/opentype/ot_glyf_simd_data.cpp
  blend2d/opentype/ot_glyf_simd_impl_p.h
  blend2d/opentype/ot_glyf_simd_impl_test.cpp
  blend2d/opentype/ot_kern.cpp
  blend2d/opentype/ot_kern_p.h
  blend2d/opentype/ot_layout.cpp
  blend2d/opentype/ot_layout_p.h
  blend2d/opentype/ot_layout_context_p.h
  blend2d/opentype/ot_layout_tables_p.h
  blend2d/opentype/ot_metrics.cpp
  blend2d/opentype/ot_metrics_p.h
  blend2d/opentype/ot_name.cpp
  blend2d/opentype/ot_name_p.h
  blend2d/opentype/ot_platform_p.h

  blend2d/pipeline/pipe_defs.cpp
  blend2d/pipeline/pipe_defs_p.h
  blend2d/pipeline/pipe_runtime.cpp
  blend2d/pipeline/pipe_runtime_p.h

  blend2d/pipeline/jit/comp_op_part.cpp
  blend2d/pipeline/jit/comp_op_part_p.h
  blend2d/pipeline/jit/comp_op_utils_p.h
  blend2d/pipeline/jit/fetch_gradient_part.cpp
  blend2d/pipeline/jit/fetch_gradient_part_p.h
  blend2d/pipeline/jit/fetch_part.cpp
  blend2d/pipeline/jit/fetch_part_p.h
  blend2d/pipeline/jit/fetch_pattern_part.cpp
  blend2d/pipeline/jit/fetch_pattern_part_p.h
  blend2d/pipeline/jit/fetch_pixel_ptr_part.cpp
  blend2d/pipeline/jit/fetch_pixel_ptr_part_p.h
  blend2d/pipeline/jit/fetch_solid_part.cpp
  blend2d/pipeline/jit/fetch_solid_part_p.h
  blend2d/pipeline/jit/fetch_utils_bilinear_p.h
  blend2d/pipeline/jit/fetch_utils_coverage.cpp
  blend2d/pipeline/jit/fetch_utils_coverage_p.h
  blend2d/pipeline/jit/fetch_utils_inline_loops.cpp
  blend2d/pipeline/jit/fetch_utils_inline_loops_p.h
  blend2d/pipeline/jit/fetch_utils_pixel_access.cpp
  blend2d/pipeline/jit/fetch_utils_pixel_access_p.h
  blend2d/pipeline/jit/fetch_utils_pixel_gather.cpp
  blend2d/pipeline/jit/fetch_utils_pixel_gather_p.h
  blend2d/pipeline/jit/fill_part.cpp
  blend2d/pipeline/jit/fill_part_p.h
  blend2d/pipeline/jit/jit_base_p.h
  blend2d/pipeline/jit/jit_pipe_runtime_p.h
  blend2d/pipeline/jit/jit_pipe_runtime.cpp
  blend2d/pipeline/jit/pipe_compiler.cpp
  blend2d/pipeline/jit/pipe_compiler_p.h
  blend2d/pipeline/jit/pipe_composer.cpp
  blend2d/pipeline/jit/pipe_composer_p.h
  blend2d/pipeline/jit/pipe_debug_p.h
  blend2d/pipeline/jit/pipe_function_p.h
  blend2d/pipeline/jit/pipe_function.cpp
  blend2d/pipeline/jit/pipe_primitives.cpp
  blend2d/pipeline/jit/pipe_primitives_p.h
  blend2d/pipeline/jit/pipe_part.cpp
  blend2d/pipeline/jit/pipe_part_p.h

  blend2d/pipeline/reference/compopgeneric_p.h
  blend2d/pipeline/reference/fetchgeneric_p.h
  blend2d/pipeline/reference/fillgeneric_p.h
  blend2d/pipeline/reference/fixed_pipe_runtime_p.h
  blend2d/pipeline/reference/fixed_pipe_runtime.cpp
  blend2d/pipeline/reference/pixelbufferptr_p.h
  blend2d/pipeline/reference/pixelgeneric_p.h

  blend2d/pixelops/interpolation.cpp
  blend2d/pixelops/interpolation_avx2.cpp
  blend2d/pixelops/interpolation_sse2.cpp
  blend2d/pixelops/funcs.cpp
  blend2d/pixelops/funcs_p.h
  blend2d/pixelops/scalar_test.cpp
  blend2d/pixelops/scalar_p.h

  blend2d/raster/analytic_rasterizer_test.cpp
  blend2d/raster/analytic_rasterizer_p.h
  blend2d/raster/debugging_p.h
  blend2d/raster/edge_builder_p.h
  blend2d/raster/edge_storage_p.h
  blend2d/raster/raster_context.cpp
  blend2d/raster/raster_context_p.h
  blend2d/raster/raster_context_ops.cpp
  blend2d/raster/raster_context_ops_p.h
  blend2d/raster/raster_defs_p.h
  blend2d/raster/render_batch_p.h
  blend2d/raster/render_command_p.h
  blend2d/raster/render_command_proc_async_p.h
  blend2d/raster/render_command_proc_sync_p.h
  blend2d/raster/render_fetch_data.cpp
  blend2d/raster/render_fetch_data_p.h
  blend2d/raster/render_job_p.h
  blend2d/raster/render_job_proc_p.h
  blend2d/raster/render_queue_p.h
  blend2d/raster/render_target_info.cpp
  blend2d/raster/render_target_info_p.h
  blend2d/raster/state_data_p.h
  blend2d/raster/style_data_p.h
  blend2d/raster/work_data.cpp
  blend2d/raster/work_data_p.h
  blend2d/raster/worker_manager.cpp
  blend2d/raster/worker_manager_p.h
  blend2d/raster/worker_proc.cpp
  blend2d/raster/worker_proc_p.h
  blend2d/raster/worker_synchronization.cpp
  blend2d/raster/worker_synchronization_p.h

  blend2d/simd/simd_p.h
  blend2d/simd/simd_test.cpp
  blend2d/simd/simd_test_p.h
  blend2d/simd/simdbase_p.h
  blend2d/simd/simdarm_p.h
  blend2d/simd/simdarm_asimd_test.cpp
  blend2d/simd/simdx86_p.h
  blend2d/simd/simdx86_avx_test.cpp
  blend2d/simd/simdx86_avx2_test.cpp
  blend2d/simd/simdx86_avx512_test.cpp
  blend2d/simd/simdx86_sse2_test.cpp
  blend2d/simd/simdx86_sse4_1_test.cpp
  blend2d/simd/simdx86_sse4_2_test.cpp
  blend2d/simd/simdx86_ssse3_test.cpp

  blend2d/tables/tables.cpp
  blend2d/tables/tables_test.cpp
  blend2d/tables/tables_p.h

  blend2d/threading/atomic_p.h
  blend2d/threading/condition_variable_p.h
  blend2d/threading/futex.cpp
  blend2d/threading/futex_p.h
  blend2d/threading/mutex_p.h
  blend2d/threading/thread.cpp
  blend2d/threading/thread_p.h
  blend2d/threading/threading_utils_p.h
  blend2d/threading/thread_pool.cpp
  blend2d/threading/thread_pool_test.cpp
  blend2d/threading/thread_pool_p.h
  blend2d/threading/tsan_utils_p.h
  blend2d/threading/unique_id_generator.cpp
  blend2d/threading/unique_id_generator_p.h

  blend2d/tracing/instrument_p.h

  blend2d/unicode/unicode.cpp
  blend2d/unicode/unicode_test.cpp
  blend2d/unicode/unicode_p.h

  blend2d/utils/popcnt_test.cpp
  blend2d/utils/popcnt_p.h
)

set(BLEND2D_BENCH_CONTEXT_SRC
  blend2d-testing/bench/bl_bench_context.cpp
  blend2d-testing/bench/bl_bench_context.h
  blend2d-testing/bench/bl_bench_context_agg.cpp
  blend2d-testing/bench/bl_bench_context_blend2d.cpp
  blend2d-testing/bench/bl_bench_context_cairo.cpp
  blend2d-testing/bench/bl_bench_context_coregraphics.cpp
  blend2d-testing/bench/bl_bench_context_juce.cpp
  blend2d-testing/bench/bl_bench_context_qt.cpp
  blend2d-testing/bench/bl_bench_context_skia.cpp
  blend2d-testing/bench/images_data.h
  blend2d-testing/commons/zero_lib.cpp
  blend2d-testing/commons/zero_lib.h
  blend2d-testing/resources/shape_data.cpp
  blend2d-testing/resources/shape_data.h
)

# Blend2D - CMake Utilities
# =========================

include(CheckCXXCompilerFlag)
include(CheckCSourceCompiles)
include(CheckLinkerFlag)
include(GNUInstallDirs)

# Detects target architecture - since the code doesn't run it works also for cross-compilation.
function(blend2d_detect_target_arch out)
  set(${out} "unknown" PARENT_SCOPE)
  foreach(target_arch "X86_64" "AARCH64" "X86" "AARCH32")
    check_c_source_compiles("
      #if defined(_M_X64) || defined(__x86_64__)
        static int target_arch_is_X86_64() { return 1; }
      #elif defined(_M_IX86) || defined(__X86__) || defined(__i386__)
        static int target_arch_is_X86() { return 1; }
      #elif defined(_M_ARM64) || defined(__arm64__) || defined(__aarch64__)
        static int target_arch_is_AARCH64() { return 1; }
      #elif defined(_M_ARM) || defined(_M_ARMT) || defined(__arm__) || defined(__thumb__) || defined(__thumb2__)
        static int target_arch_is_AARCH32() { return 1; }
      #endif
      int main(void) { return target_arch_is_${target_arch}(); }" "${out}_${target_arch}")
    if (${${out}_${target_arch}})
      set(${out} "${target_arch}" PARENT_SCOPE)
      return()
    endif()
  endforeach()
endfunction()

# Detects C++ compile flags and appends all detected ones to `out`.
function(blend2d_detect_cflags out)
  set(_out_array ${${out}})
  string(REGEX REPLACE "[+]" "x" _flag_signature "${ARGN}")
  string(REGEX REPLACE "[-= :;/.\]" "_" _flag_signature "${_flag_signature}")

  if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "^(GNU|Clang|AppleClang)$")
    check_cxx_compiler_flag("${ARGN} -Werror" "__CxxFlag_${_flag_signature}")
  else()
    check_cxx_compiler_flag("${ARGN}" "__CxxFlag_${_flag_signature}")
  endif()

  if (${__CxxFlag_${_flag_signature}})
    list(APPEND _out_array ${ARGN})
  endif()
  set(${out} "${_out_array}" PARENT_SCOPE)
endfunction()

# Detects C++ link flags and appends all detected ones to `out`.
function(blend2d_detect_lflags out)
  set(_out_array ${${out}})

  foreach(_flag ${ARGN})
    string(REGEX REPLACE "[+]" "x" _flag_signature "${_flag}")
    string(REGEX REPLACE "[-= :;/.\]" "_" _flag_signature "${_flag_signature}")
    check_linker_flag(CXX "${_flag}" "__LinkFlag_${_flag_signature}")
    if (${__LinkFlag_${_flag_signature}})
      list(APPEND _out_array ${_flag})
    endif()
  endforeach()

  set(${out} "${_out_array}" PARENT_SCOPE)
endfunction()

# Support for various sanitizers provided by C/C++ compilers.
function(blend2d_detect_sanitizers out)
  set(_out_array ${${out}})
  set(_flags "")

  foreach(_arg ${ARGN})
    string(REPLACE "," ";" _arg "${_arg}")
    list(APPEND _flags ${_arg})
  endforeach()

  foreach(_flag ${_flags})
    if (NOT "${_flag}" MATCHES "^-fsanitize=")
      SET(_flag "-fsanitize=${_flag}")
    endif()

    # Sanitizers also require link flags, see CMAKE_REQUIRED_FLAGS.
    set(CMAKE_REQUIRED_FLAGS "${_flag}")
    blend2d_detect_cflags(_out_array ${_flag})
    unset(CMAKE_REQUIRED_FLAGS)
  endforeach()

  set(${out} "${_out_array}" PARENT_SCOPE)
endfunction()

function(blend2d_add_target target target_type)
  set(multi_value_args SOURCES INCLUDE_DIRS CFLAGS CFLAGS_DBG CFLAGS_REL LFLAGS LIBRARIES)
  cmake_parse_arguments("ARG" "" "" "${multi_value_args}" ${ARGN})

  if ("${target_type}" MATCHES "^(EXECUTABLE|EXECUTABLE_WIN32|TEST)$")
    if ("${target_type}" MATCHES "^(EXECUTABLE_WIN32)$")
      add_executable(${target} WIN32 ${ARG_SOURCES})
    else()
      add_executable(${target} ${ARG_SOURCES})
    endif()
    set_target_properties(${target} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(TargetDir).")
  else()
    add_library(${target} ${target_type} ${ARG_SOURCES})
  endif()

  set_target_properties(${target} PROPERTIES DEFINE_SYMBOL "" CXX_VISIBILITY_PRESET hidden)
  target_include_directories(${target} BEFORE PRIVATE ${ARG_INCLUDE_DIRS})
  target_compile_options(${target} PRIVATE ${ARG_CFLAGS} ${BLEND2D_SANITIZE_CFLAGS} $<IF:$<CONFIG:Debug>,${ARG_CFLAGS_DBG},${ARG_CFLAGS_REL}>)
  target_compile_features(${target} PUBLIC cxx_std_20)
  target_link_options(${target} PRIVATE ${ARG_LFLAGS} ${BLEND2D_SANITIZE_LFLAGS})
  target_link_libraries(${target} PRIVATE ${ARG_LIBRARIES})

  if ("${target_type}" STREQUAL "TEST")
    add_test(NAME ${target} COMMAND ${target})
  endif()
endfunction()

# Blend2D - Features
# ==================

blend2d_detect_target_arch(BLEND2D_TARGET_ARCH)

if (BLEND2D_NO_JIT)
  message(STATUS "[blend2d] Disabling JIT compiler support ('BLEND2D_TARGET_ARCH=${BLEND2D_TARGET_ARCH}', but 'BLEND2D_NO_JIT=${BLEND2D_NO_JIT}')")
  set(BLEND2D_BUILD_WITH_JIT FALSE)
elseif ("${BLEND2D_TARGET_ARCH}" MATCHES "^(X86|X86_64|AARCH64)$")
  message(STATUS "[blend2d] Enabling JIT compiler support ('BLEND2D_TARGET_ARCH=${BLEND2D_TARGET_ARCH}' is supported)")
  set(BLEND2D_BUILD_WITH_JIT TRUE)
else()
  message(STATUS "[blend2d] Disabling JIT compiler support ('BLEND2D_TARGET_ARCH=${BLEND2D_TARGET_ARCH}' isn't supported)")
  set(BLEND2D_BUILD_WITH_JIT FALSE)
endif()

if (BLEND2D_NO_TLS)
  message(STATUS "[blend2d] Disabling TLS support ('BLEND2D_NO_TLS=${BLEND2D_NO_TLS}')")
  list(APPEND BLEND2D_PRIVATE_CFLAGS -DBL_BUILD_NO_TLS)
endif()

if (BLEND2D_NO_FUTEX)
  message(STATUS "[blend2d] Disabling futex support ('BLEND2D_NO_FUTEX=${BLEND2D_NO_FUTEX}')")
  list(APPEND BLEND2D_PRIVATE_CFLAGS -DBL_BUILD_NO_FUTEX)
endif()

if (BLEND2D_NO_CODECS)
  message(STATUS "[blend2d] Disabling codecs support ('BLEND2D_NO_CODECS=${BLEND2D_NO_CODECS}')")
  list(APPEND BLEND2D_CFLAGS -DBL_BUILD_NO_CODECS)
endif()

if (BLEND2D_NO_FILESYSTEM)
  message(STATUS "[blend2d] Disabling filesystem support ('BLEND2D_NO_FILESYSTEM=${BLEND2D_NO_FILESYSTEM}')")
  list(APPEND BLEND2D_CFLAGS -DBL_BUILD_NO_FILESYSTEM)
endif()

# Blend2D - Compiler Support - Detection
# ======================================

if ("x${CMAKE_CXX_COMPILER_ID}" STREQUAL "xMSVC" OR "x${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "xMSVC")
  list(APPEND BLEND2D_WARNING_CFLAGS -W4)                                       # [+] Warning level 4.

  list(APPEND BLEND2D_PRIVATE_CFLAGS -MP)                                       # [+] Multi-Process Compilation.
  list(APPEND BLEND2D_PRIVATE_CFLAGS -GR-)                                      # [-] Runtime type information.
  list(APPEND BLEND2D_PRIVATE_CFLAGS -GF)                                       # [+] Eliminate duplicate strings.
  list(APPEND BLEND2D_PRIVATE_CFLAGS -Zc:__cplusplus)                           # [+] Conforming __cplusplus definition.
  list(APPEND BLEND2D_PRIVATE_CFLAGS -Zc:inline)                                # [+] Remove unreferenced COMDAT.
  list(APPEND BLEND2D_PRIVATE_CFLAGS -Zc:strictStrings)                         # [+] Strict const qualification of string literals.
  list(APPEND BLEND2D_PRIVATE_CFLAGS -Zc:threadSafeInit-)                       # [-] Thread-safe statics.

  list(APPEND BLEND2D_PRIVATE_CFLAGS_DBG -GS)                                   # [+] Buffer security-check.
  list(APPEND BLEND2D_PRIVATE_CFLAGS_REL -GS-)                                  # [-] Buffer security-check.
  list(APPEND BLEND2D_PRIVATE_CFLAGS_REL -O2)                                   # [+] Favor speed over size.
  list(APPEND BLEND2D_PRIVATE_CFLAGS_REL -Oi)                                   # [+] Generate Intrinsic Functions.

  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    list(APPEND BLEND2D_PRIVATE_CFLAGS -clang:-fno-rtti -clang:-fno-math-errno -clang:-fno-trapping-math)
  endif()
  blend2d_detect_cflags(BLEND2D_PRIVATE_CFLAGS -Zc:arm64-aliased-neon-types-)   # [-] ARM64 aliased neon types
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU|Clang")
  list(APPEND BLEND2D_WARNING_CFLAGS -Wall -Wextra -Wconversion)                # [+] Add baseline warnings that can be used safely even with system headers.
  blend2d_detect_cflags(BLEND2D_WARNING_CFLAGS -Wdouble-promotion)              # [+] Warn about double promotions.
  blend2d_detect_cflags(BLEND2D_WARNING_CFLAGS -Wduplicated-cond)               # [+] Warn about duplicate conditions.
  blend2d_detect_cflags(BLEND2D_WARNING_CFLAGS -Wduplicated-branches)           # [+] Warn about duplicate branches.
  blend2d_detect_cflags(BLEND2D_WARNING_CFLAGS -Wlogical-op)                    # [+] Warn about suspicious uses of logical operators in expressions.
  blend2d_detect_cflags(BLEND2D_WARNING_CFLAGS -Wlogical-not-parentheses)       # [+] Warn about logical not used on the left hand side operand of a comparison.
  blend2d_detect_cflags(BLEND2D_WARNING_CFLAGS -Wrestrict)
  blend2d_detect_cflags(BLEND2D_WARNING_CFLAGS -Wbidi-chars=any)                # [+] Warn about using BIDI characters in source code (we don't use them).
  blend2d_detect_cflags(BLEND2D_WARNING_CFLAGS -Wlifetime-safety)               # [+] Lifetime safety warnings
  blend2d_detect_cflags(BLEND2D_WARNING_CFLAGS -Wno-unused-template)            # [-] Don't warn about unused templates / template functions.

  list(APPEND BLEND2D_PRIVATE_CFLAGS -fno-math-errno)                           # [-] Disable math functions setting errno (performance reasons).
  list(APPEND BLEND2D_PRIVATE_CFLAGS -fno-threadsafe-statics)                   # [-] Don't add guards when initializing statics (we don't need it).
  list(APPEND BLEND2D_PRIVATE_CFLAGS -fno-exceptions -fno-rtti)                 # [-] Disable exceptions and RTTI (we never throw, we don't need RTTI).
  list(APPEND BLEND2D_PRIVATE_CFLAGS_REL -O2)                                   # [+] Compiling with -O2 in release mode is what we generally want.
  list(APPEND BLEND2D_PRIVATE_CFLAGS_REL -fmerge-all-constants)                 # [+] We don't need unique address per constant (merging improves library size).

  # -fno-semantic-interposition is not available on apple - the compiler issues a warning, which is not detected.
  if (NOT APPLE)
    blend2d_detect_cflags(BLEND2D_PRIVATE_CFLAGS -fno-semantic-interposition)
  endif()

  if (NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "iOS")
    list(APPEND BLEND2D_PRIVATE_CFLAGS -fno-trapping-math -fno-finite-math-only)# [-] Disable these flags in case somebody uses `-ffast-math` globally.
    blend2d_detect_cflags(BLEND2D_PRIVATE_CFLAGS_REL -fno-enforce-eh-specs)     # [-] We never throw, so we don't need strict exception handling enforcement.
  endif()

  blend2d_detect_cflags(BLEND2D_PRIVATE_CFLAGS -mllvm --disable-loop-idiom-all) # [-] Don't use memset/memcpy instead of our own loops (critical in some cases).
  blend2d_detect_cflags(BLEND2D_PRIVATE_CFLAGS_REL -ftree-vectorize)            # [+] Auto-vectorize by default in release builds (sometimes helps).

  # Building Blend2D without C++ library support requires these to be setup.
  if (BLEND2D_NO_STDCXX)
    message(STATUS "[blend2d] Enabling build without linking to the C++ standard library ('BLEND2D_NO_STDCXX=${BLEND2D_NO_STDCXX}')")
    list(APPEND BLEND2D_NO_STDCXX_CFLAGS)

    # TODO: Experiment - we rather not link than link statically, to avoid garbage in the shared library.
    blend2d_detect_lflags(BLEND2D_NO_STDCXX_LFLAGS -nostdlib++)
    #blend2d_detect_lflags(BLEND2D_NO_STDCXX_LFLAGS -static-libgcc -static-libstdc++)
  endif()
endif()

if (BLEND2D_TARGET_ARCH MATCHES "^(X86|X86_64)$")
  set(BLEND2D_PRIVATE_CFLAGS_SSE2 "")
  if ("x${CMAKE_CXX_COMPILER_ID}" STREQUAL "xMSVC" OR "x${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "xMSVC")
    if ("${BLEND2D_TARGET_ARCH}" STREQUAL "X86")
      set(BLEND2D_PRIVATE_CFLAGS_SSE2 -arch:SSE2) # 32-bit target requires -arch:SSE2.
    endif()

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
      # To enable certain optimizations for clang-cl additional flags have to be passed (MSVC incompatibility).
      set(BLEND2D_PRIVATE_CFLAGS_SSE3   ${BLEND2D_PRIVATE_CFLAGS_SSE2} -msse3)
      set(BLEND2D_PRIVATE_CFLAGS_SSSE3  ${BLEND2D_PRIVATE_CFLAGS_SSE2} -mssse3)
      set(BLEND2D_PRIVATE_CFLAGS_SSE4_1 ${BLEND2D_PRIVATE_CFLAGS_SSE2} -msse4.1)
      set(BLEND2D_PRIVATE_CFLAGS_SSE4_2 ${BLEND2D_PRIVATE_CFLAGS_SSE2} -msse4.2 -mpclmul -mpopcnt)
      set(BLEND2D_PRIVATE_CFLAGS_AVX    -arch:AVX    -mpclmul -mpopcnt -mlzcnt)
      set(BLEND2D_PRIVATE_CFLAGS_AVX2   -arch:AVX2   -mpclmul -mpopcnt -mlzcnt -mbmi -mbmi2 -mfma)
      set(BLEND2D_PRIVATE_CFLAGS_AVX512 -arch:AVX512 -mpclmul -mpopcnt -mlzcnt -mbmi -mbmi2 -mfma)
      set(BLEND2D_PRIVATE_CFLAGS_AVX512_V2 ${BLEND2D_PRIVATE_CFLAGS_AVX512} -mavx512bitalg -mavx512ifma -mavx512vbmi -mavx512vbmi2 -mavx512vnni -mgfni)
    else()
      # MSVC doesn't distinguish between some optimization levels - so add some definitions manually.
      set(BLEND2D_PRIVATE_CFLAGS_SSE3   ${BLEND2D_PRIVATE_CFLAGS_SSE2} -D__SSE3__)
      set(BLEND2D_PRIVATE_CFLAGS_SSSE3  ${BLEND2D_PRIVATE_CFLAGS_SSE2} -D__SSSE3__)
      set(BLEND2D_PRIVATE_CFLAGS_SSE4_1 ${BLEND2D_PRIVATE_CFLAGS_SSE2} -D__SSE4_1__)
      set(BLEND2D_PRIVATE_CFLAGS_SSE4_2 ${BLEND2D_PRIVATE_CFLAGS_SSE2} -D__SSE4_2__)
      set(BLEND2D_PRIVATE_CFLAGS_AVX    -arch:AVX)
      set(BLEND2D_PRIVATE_CFLAGS_AVX2   -arch:AVX2 -D__FMA__)
      set(BLEND2D_PRIVATE_CFLAGS_AVX512 -arch:AVX512)
      set(BLEND2D_PRIVATE_CFLAGS_AVX512_V2 ${BLEND2D_PRIVATE_CFLAGS_AVX512})
    endif()
  else()
    if ("${BLEND2D_TARGET_ARCH}" STREQUAL "X86")
      if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(BLEND2D_PRIVATE_CFLAGS_SSE2 -msse2 -mfpmath=sse)
      else()
        set(BLEND2D_PRIVATE_CFLAGS_SSE2 -msse2)
      endif()
    endif()
    set(BLEND2D_PRIVATE_CFLAGS_SSE3   -msse3)
    set(BLEND2D_PRIVATE_CFLAGS_SSSE3  -mssse3)
    set(BLEND2D_PRIVATE_CFLAGS_SSE4_1 -msse4.1)
    set(BLEND2D_PRIVATE_CFLAGS_SSE4_2 -msse4.2 -mpclmul -mpopcnt)
    set(BLEND2D_PRIVATE_CFLAGS_AVX    -mavx    -mpclmul -mpopcnt)
    set(BLEND2D_PRIVATE_CFLAGS_AVX2   -mavx2   -mpclmul -mpopcnt -mlzcnt -mbmi -mbmi2 -mfma)
    set(BLEND2D_PRIVATE_CFLAGS_AVX512 -mavx2   -mpclmul -mpopcnt -mlzcnt -mbmi -mbmi2 -mfma -mavx512f -mavx512bw -mavx512dq -mavx512cd -mavx512vl)
    set(BLEND2D_PRIVATE_CFLAGS_AVX512_V2 ${BLEND2D_PRIVATE_CFLAGS_AVX512} -mavx512bitalg -mavx512ifma -mavx512vbmi -mavx512vbmi2 -mavx512vnni -mgfni)
  endif()

  list(APPEND BLEND2D_PRIVATE_CFLAGS_AVX2      -DBL_TARGET_OPT_POPCNT -DBL_TARGET_OPT_BMI2 -DBL_TARGET_OPT_FMA)
  list(APPEND BLEND2D_PRIVATE_CFLAGS_AVX512    -DBL_TARGET_OPT_POPCNT -DBL_TARGET_OPT_BMI2 -DBL_TARGET_OPT_FMA)
  list(APPEND BLEND2D_PRIVATE_CFLAGS_AVX512_V2 -DBL_TARGET_OPT_POPCNT -DBL_TARGET_OPT_BMI2 -DBL_TARGET_OPT_FMA -DBL_TARGET_OPT_AVX512_V2)

  # SSE2 is our baseline if no other baseline is specified; AVX512_V2 is our maximum.
  list(APPEND BLEND2D_PRIVATE_CFLAGS ${BLEND2D_PRIVATE_CFLAGS_SSE2} -DBL_BUILD_OPT_AVX512_V2)
elseif ("${BLEND2D_TARGET_ARCH}" MATCHES "^(AARCH32|AARCH64)$")
  if ("x${CMAKE_CXX_COMPILER_ID}" STREQUAL "xMSVC" OR "x${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "xMSVC")
    # FIXME: Not sure whether it makes sense to detect anything in this case.
  else()
    blend2d_detect_cflags(BLEND2D_PRIVATE_CFLAGS_ASIMD -mfpu=neon-vfpv4)
    if ("${BLEND2D_TARGET_ARCH}" STREQUAL "AARCH64")
      blend2d_detect_cflags(BLEND2D_PRIVATE_CFLAGS_ASIMD_CRYPTO -march=armv8-a+aes+crc+crypto)
    endif()
  endif()

  if (BLEND2D_PRIVATE_CFLAGS_ASIMD)
    list(APPEND BLEND2D_PRIVATE_CFLAGS -DBL_BUILD_OPT_ASIMD)
    list(APPEND BLEND2D_PRIVATE_CFLAGS_ASIMD -DBL_TARGET_OPT_ASIMD)
  endif()

  if (BLEND2D_PRIVATE_CFLAGS_ASIMD_CRYPTO)
    list(APPEND BLEND2D_PRIVATE_CFLAGS -DBL_BUILD_OPT_ASIMD_CRYPTO)
    list(APPEND BLEND2D_PRIVATE_CFLAGS_ASIMD_CRYPTO -DBL_TARGET_OPT_ASIMD_CRYPTO)
  endif()
endif()

list(APPEND BLEND2D_PRIVATE_CFLAGS ${BLEND2D_WARNING_CFLAGS})

# Blend2D - Compiler Support - Sanitizers
# =======================================

if (BLEND2D_SANITIZE)
  blend2d_detect_sanitizers(BLEND2D_SANITIZE_CFLAGS ${BLEND2D_SANITIZE})
  if (BLEND2D_SANITIZE_CFLAGS)
    message(STATUS "[blend2d] Enabling sanitizers: '${BLEND2D_SANITIZE_CFLAGS}'")

    # Linker must receive the same flags as the compiler when it comes to sanitizers.
    set(BLEND2D_SANITIZE_LFLAGS ${BLEND2D_SANITIZE_CFLAGS})

    # Don't omit frame pointer if sanitizers are enabled.
    if ("x${CMAKE_CXX_COMPILER_ID}" STREQUAL "xMSVC" OR "x${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "xMSVC")
      list(APPEND BLEND2D_SANITIZE_CFLAGS -Oy-)
    else()
      list(APPEND BLEND2D_SANITIZE_CFLAGS -fno-omit-frame-pointer -g)
    endif()

    list(APPEND BLEND2D_PRIVATE_CFLAGS ${BLEND2D_SANITIZE_CFLAGS})
    list(APPEND BLEND2D_PRIVATE_LFLAGS ${BLEND2D_SANITIZE_LFLAGS})
  endif()
endif()

# Blend2D - Dependencies
# ======================

if ("${CMAKE_SYSTEM_NAME}" MATCHES "^Windows")
  # Dependency: Windows target needs no extra dependencies, however, we have to check for .natvis support.
  if(CMAKE_LINKER MATCHES "^(lld-link|link)\\.exe$")
    set(BLEND2D_LINKER_SUPPORTS_NATVIS TRUE)
  endif()
elseif ("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
  # Dependency: libc is the only required library on Android as it also provides libpthread.
  message(STATUS "[blend2d] Adding libc dependency (Android)")
  list(APPEND BLEND2D_LIBS c)
elseif ("${CMAKE_SYSTEM_NAME}" STREQUAL "Haiku")
  # Dependency: libroot is used by Haiku instead of libc, so link to libroot and libpthread.
  message(STATUS "[blend2d] Adding libroot, libm, and libpthread dependencies (Haiku)")
  list(APPEND BLEND2D_LIBS root m pthread)
else()
  # Dependency: libc is always required.
  message(STATUS "[blend2d] Adding libc dependency (Linux, BSD, or other UNIX/POSIX environment)")
  list(APPEND BLEND2D_LIBS c)

  # Dependency: pthread (required so Blend2D can use pthread_rwlock, pthread_create, ...).
  check_c_source_compiles("
    #include <pthread.h>

    static void* thread_entry_point(void* arg) { return arg; }
    int main() {
      pthread_rwlock_t m;
      pthread_rwlock_init(&m, (void*)0);
      pthread_rwlock_wrlock(&m);
      pthread_rwlock_unlock(&m);

      pthread_attr_t attr;
      pthread_attr_init(&attr);
      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
      pthread_attr_setstacksize(&attr, 1024*1024);

      pthread_t handle;
      return pthread_create(&handle, &attr, thread_entry_point, (void*)0);
    }
    " BLEND2D_LIBC_HAS_LIBPTHREAD)
  if (BLEND2D_LIBC_HAS_LIBPTHREAD)
    message(STATUS "[blend2d] Library 'libpthread' provided by 'libc' (not linking to libpthread)")
  else()
    message(STATUS "[blend2d] Library 'libpthread' not provided by 'libc' (linking to libpthread)")
    list(APPEND BLEND2D_LIBS pthread)
  endif()

  check_c_source_compiles("
    #include <math.h>
    int main(int argc, char* argv[]) {
      (void)argv; return (int)sin((double)argc * 0.1234);
    }
    " BLEND2D_LIBC_HAS_LIBM)
  if (BLEND2D_LIBC_HAS_LIBM)
    message(STATUS "[blend2d] Library 'libm' provided by 'libc' (not linking to libm)")
  else()
    message(STATUS "[blend2d] Library 'libm' not provided by 'libc' (linking to libm)")
    list(APPEND BLEND2D_LIBS m)
  endif()
endif()

# Find asmjit dependency if building with JIT.
if (BLEND2D_BUILD_WITH_JIT)
  if (BLEND2D_EXTERNAL_ASMJIT)
    message(STATUS "[blend2d] Trying to find external AsmJit ('BLEND2D_EXTERNAL_ASMJIT=${BLEND2D_EXTERNAL_ASMJIT}')\n")
    message(WARNING
      "Using external AsmJit library means you are opting out of using the\n"
      "bundled one, which is the preferred way especially if Blend2D is compiled\n"
      "as a shared library. In that case AsmJit is configured by Blend2D to only\n"
      "include the target architecture that is actually used (all foreign backends\n"
      "are disabled) and is built the same way as Blend2D (without exceptions/rtti).\n"
      "\n"
      "In general, using external AsmJit option was introduced for package managers\n"
      "that prefer each dependency to be built separately. In this case the burden\n"
      "of configuring the dependency is on you.\n"
      "\n"
      "Do you want a smaller binary? Use 'ASMJIT_NO_FOREIGN' option to disable foreign\n"
      "architectures.\n"
      "\n"
      "If you are concerned about security (not using anything bundled) you can\n"
      "just git checkout https://github.com/asmjit/asmjit into [root]/3rdparty/asmjit\n"
      "or set ASMJIT_DIR variable to point to a previously checked out AsmJit.\n")
    find_package(asmjit REQUIRED)
    list(APPEND BLEND2D_LIBS asmjit::asmjit)
  else()
    if (NOT DEFINED ASMJIT_DIR)
      foreach(dir "${CMAKE_CURRENT_LIST_DIR}/3rdparty/asmjit" "${CMAKE_CURRENT_LIST_DIR}/../asmjit")
        if (EXISTS ${dir}/CMakeLists.txt)
          set(ASMJIT_DIR "${dir}" CACHE PATH "Location of 'asmjit'")
          message(STATUS "[blend2d] AsmJit found ('BLEND2D_EXTERNAL_ASMJIT=OFF' => 'ASMJIT_DIR=${ASMJIT_DIR}')")
          break()
        endif()
      endforeach()
      if (NOT DEFINED ASMJIT_DIR)
        message(FATAL
          "[blend2d] Unable to find AsmJit, please see <https://blend2d.com/doc/build-instructions.html>\n"
          "\n"
          "Blend2D expects AsmJit to be in the following directories:\n"
          "\n"
          "  - ASMJIT_DIR               - CMake variable specifying AsmJit directory\n"
          "  - '<root>/3rdparty/asmjit' - asmjAsmJitit placed into 3rdparty subdirectory (dev & releases)\n"
          "  - '<root>/../asmjit'       - AsmJit placed outside of blend2d project (development)\n"
          "\n"
          "Alternatively, if you use C/C++ package managers and would like to use\n"
          "a packaged AsmJit instead, set 'BLEND2D_EXTERNAL_ASMJIT' cmake variable to ON.\n"
          "This way Blend2D would not try to auto-detect the location of AsmJit and would\n"
          "try to find it by using find_package() instead. Please note that using extenal\n"
          "AsmJit means that Blend2D would not be able to avoid linking to a C++ standard\n"
          "library, which it avoids by default when building a shared Blend2D library with\n"
          "bundled AsmJit."
        )
      endif()
    endif()

    if (NOT DEFINED ASMJIT_EMBED)
      set(ASMJIT_EMBED TRUE CACHE BOOL "")
    endif()

    add_subdirectory("${ASMJIT_DIR}" asmjit)
    list(APPEND BLEND2D_LIBS $<BUILD_INTERFACE:asmjit::asmjit>)
    list(APPEND BLEND2D_PRIVATE_CFLAGS -DASMJIT_NO_FOREIGN -DASMJIT_NO_STDCXX -DASMJIT_ABI_NAMESPACE=abi_bl)

    # A possibility to reduce the resulting binary size by disabling asmjit logging.
    if (BLEND2D_NO_JIT_LOGGING)
      message(STATUS "[blend2d] Disabling asmjit logging functionality (JIT)")
      list(APPEND BLEND2D_PRIVATE_CFLAGS -DASMJIT_NO_LOGGING -DASMJIT_NO_TEXT)
    endif()
  endif()
else()
  list(APPEND BLEND2D_PRIVATE_CFLAGS -DBL_BUILD_NO_JIT)
endif()

# Blend2D - Finalize Build Options
# ================================

list(REMOVE_DUPLICATES BLEND2D_LIBS)
list(REMOVE_DUPLICATES BLEND2D_PRIVATE_CFLAGS)

# Blend2D - Source Files Processing
# =================================

if (MSVC AND NOT BLEND2D_NO_NATVIS)
  list(APPEND BLEND2D_SRC_LIST blend2d/blend2d.natvis)
endif()

set(BLEND2D_SRC "")
set(BLEND2D_TEST_SRC "")
set(BLEND2D_OPT_EXTENSIONS "sse2|sse3|ssse3|sse4_1|sse4_2|avx|avx2|avx512|avx512_v2|asimd|asimd_crypto")

foreach(src_file ${BLEND2D_SRC_LIST})
  set(src_file "${CMAKE_CURRENT_LIST_DIR}/${src_file}")

  if ("${src_file}" MATCHES "_test\\.cpp$")
    list(APPEND BLEND2D_TEST_SRC ${src_file})
  else()
    list(APPEND BLEND2D_SRC ${src_file})
  endif()

  string(REGEX MATCH "_(${BLEND2D_OPT_EXTENSIONS})(_test)?\\.(c|cc|cxx|cpp|m|mm)$" FEATURE ${src_file})
  if (FEATURE)
    # HACK 1: Cmake uses global variables everywhere, `CMAKE_MATCH_1` is the first capture...
    string(TOUPPER "${CMAKE_MATCH_1}" FEATURE)
    set_property(SOURCE "${src_file}" APPEND PROPERTY COMPILE_OPTIONS ${BLEND2D_PRIVATE_CFLAGS_${FEATURE}})
  endif()

  if ("${src_file}" MATCHES "\\.natvis")
    if (BLEND2D_LINKER_SUPPORTS_NATVIS)
      list(APPEND BLEND2D_PRIVATE_LFLAGS "-natvis:${src_file}")
    endif()
  endif()
endforeach()

source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES ${BLEND2D_SRC})

# Blend2D - Targets - Core Library & 'blend2d::blend2d' Alias
# ===========================================================

message(STATUS "[blend2d] Adding 'blend2d::blend2d' target ('BLEND2D_TARGET_TYPE=${BLEND2D_TARGET_TYPE}')")
foreach(v "BLEND2D_LIBS"
          "BLEND2D_CFLAGS"
          "BLEND2D_WARNING_CFLAGS"
          "BLEND2D_PRIVATE_CFLAGS"
          "BLEND2D_PRIVATE_CFLAGS_DBG"
          "BLEND2D_PRIVATE_CFLAGS_REL"
          "BLEND2D_PRIVATE_CFLAGS_SSE3"
          "BLEND2D_PRIVATE_CFLAGS_SSSE3"
          "BLEND2D_PRIVATE_CFLAGS_SSE4_1"
          "BLEND2D_PRIVATE_CFLAGS_SSE4_2"
          "BLEND2D_PRIVATE_CFLAGS_AVX"
          "BLEND2D_PRIVATE_CFLAGS_AVX2"
          "BLEND2D_PRIVATE_CFLAGS_AVX512"
          "BLEND2D_PRIVATE_CFLAGS_AVX512_V2"
          "BLEND2D_PRIVATE_CFLAGS_ASIMD"
          "BLEND2D_PRIVATE_CFLAGS_ASIMD_CRYPTO"
          "BLEND2D_SANITIZE_CFLAGS"
          "BLEND2D_SANITIZE_LFLAGS")
  if (NOT "${${v}}" STREQUAL "")
    message("   ${v}=${${v}}")
  endif()
endforeach()

blend2d_add_target(blend2d ${BLEND2D_TARGET_TYPE}
  SOURCES      ${BLEND2D_SRC}
  LIBRARIES    ${BLEND2D_LIBS}
  CFLAGS       ${BLEND2D_PRIVATE_CFLAGS} ${BLEND2D_NO_STDCXX_CFLAGS}
  CFLAGS_DBG   ${BLEND2D_PRIVATE_CFLAGS_DBG}
  CFLAGS_REL   ${BLEND2D_PRIVATE_CFLAGS_REL}
  LFLAGS       ${BLEND2D_NO_STDCXX_LFLAGS})

target_compile_options(blend2d PUBLIC ${BLEND2D_CFLAGS})
target_include_directories(blend2d BEFORE PUBLIC $<BUILD_INTERFACE:${BLEND2D_INCLUDE_DIRS}>
                                                 $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
add_library(blend2d::blend2d ALIAS blend2d)

# Add Blend2D install instructions (library and public headers).
if (NOT BLEND2D_NO_INSTALL)
  message(STATUS "[blend2d] Enabling install support ('BLEND2D_NO_INSTALL=OFF')")

  include(CMakePackageConfigHelpers)

  install(TARGETS blend2d
          EXPORT blend2d-targets
          RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
          ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
          LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
          INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

  install(EXPORT blend2d-targets
          NAMESPACE blend2d::
          DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/blend2d")

  configure_package_config_file("${CMAKE_CURRENT_LIST_DIR}/blend2d-config.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/blend2d-config.cmake"
          INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/blend2d"
          NO_SET_AND_CHECK_MACRO
          NO_CHECK_REQUIRED_COMPONENTS_MACRO)

  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/blend2d-config.cmake"
          DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/blend2d")

  foreach(src_file ${BLEND2D_SRC_LIST})
    # Install public headers only, ignore _p.h (private headers) and axl library, which is used privately.
    if ("${src_file}" MATCHES "\\.h$" AND NOT "${src_file}" MATCHES "_p\\.h$" AND NOT "${src_file}" MATCHES "blend2d/axl")
      get_filename_component(_src_dir ${src_file} PATH)
      install(FILES "${CMAKE_CURRENT_LIST_DIR}/${src_file}" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${_src_dir}")
    endif()
  endforeach()
else()
  message(STATUS "[blend2d] Disabling install support ('BLEND2D_NO_INSTALL=${BLEND2D_NO_INSTALL}')")
endif()

# Blend2D - Targets - Tests
# =========================

if (BLEND2D_TEST)
  enable_testing()
  message(STATUS "[blend2d] Enabling Blend2D tests ('BLEND2D_TEST=${BLEND2D_TEST}')")

  # Blend2D Tests
  # -------------

  # bl_test_runner - optimize static builds (no need to build Blend2D twice).
  set(BLEND2D_TEST_RUNNER_SRC
    blend2d-testing/commons/zero_lib.cpp
    blend2d-testing/commons/zero_lib.h
    blend2d-testing/commons/zero_test.cpp
    blend2d-testing/commons/zero_test.h
    blend2d-testing/tests/bl_test_runner.cpp
  )

  if (BLEND2D_BUILD_WITH_JIT)
    get_target_property(ASMJIT_INCLUDE_DIRS asmjit::asmjit INTERFACE_INCLUDE_DIRECTORIES)
  endif()

  if (BLEND2D_STATIC)
    blend2d_add_target(bl_test_runner TEST
      SOURCES      ${BLEND2D_TEST_RUNNER_SRC} ${BLEND2D_TEST_SRC}
      LIBRARIES    blend2d::blend2d
      CFLAGS       ${BLEND2D_PRIVATE_CFLAGS} -DBL_TEST -DBL_STATIC
      CFLAGS_DBG   ${BLEND2D_PRIVATE_CFLAGS_DBG}
      CFLAGS_REL   ${BLEND2D_PRIVATE_CFLAGS_REL}
      INCLUDE_DIRS ${ASMJIT_INCLUDE_DIRS} ${BLEND2D_INCLUDE_DIRS})
  else()
    blend2d_add_target(bl_test_runner TEST
      SOURCES      ${BLEND2D_TEST_RUNNER_SRC} ${BLEND2D_TEST_SRC} ${BLEND2D_SRC}
      LIBRARIES    ${BLEND2D_LIBS}
      CFLAGS       ${BLEND2D_PRIVATE_CFLAGS} -DBL_TEST -DBL_STATIC
      CFLAGS_DBG   ${BLEND2D_PRIVATE_CFLAGS_DBG}
      CFLAGS_REL   ${BLEND2D_PRIVATE_CFLAGS_REL}
      INCLUDE_DIRS ${ASMJIT_INCLUDE_DIRS} ${BLEND2D_INCLUDE_DIRS})
  endif()

  # bl_test_context_xxx are all based on the same base-app.
  foreach(app bl_test_allocation_strategy
              bl_test_context_simple
              bl_test_context_mt
              bl_test_context_jit)
    blend2d_add_target(${app} TEST
      SOURCES    blend2d-testing/tests/${app}.cpp
                 blend2d-testing/tests/bl_test_context_baseapp.cpp
                 blend2d-testing/tests/bl_test_context_baseapp.h
                 blend2d-testing/tests/bl_test_context_utilities.h
      LIBRARIES  blend2d::blend2d
      CFLAGS     ${BLEND2D_PRIVATE_CFLAGS}
      CFLAGS_DBG ${BLEND2D_PRIVATE_CFLAGS_DBG}
      CFLAGS_REL ${BLEND2D_PRIVATE_CFLAGS_REL})
  endforeach()

  if (NOT BLEND2D_NO_CODECS)
    blend2d_add_target(bl_test_image_io EXECUTABLE
      SOURCES    blend2d-testing/tests/bl_test_image_io.cpp
      LIBRARIES  blend2d::blend2d
      CFLAGS     ${BLEND2D_PRIVATE_CFLAGS}
      CFLAGS_DBG ${BLEND2D_PRIVATE_CFLAGS_DBG}
      CFLAGS_REL ${BLEND2D_PRIVATE_CFLAGS_REL})
  endif()

  # Add a limited version of 'bl_bench_context' in case we are building tests, but not demos.
  if (NOT BLEND2D_DEMOS)
    blend2d_add_target(bl_bench_context EXECUTABLE
      SOURCES    ${BLEND2D_BENCH_CONTEXT_SRC}
      LIBRARIES  blend2d::blend2d
      CFLAGS     ${BLEND2D_PRIVATE_CFLAGS}
      CFLAGS_DBG ${BLEND2D_PRIVATE_CFLAGS_DBG}
      CFLAGS_REL ${BLEND2D_PRIVATE_CFLAGS_REL})
  endif()

  # Standalone benchmarks.
  foreach(app bl_bench_coverage_set bl_bench_image_codecs bl_bench_outline_decoder)
    blend2d_add_target(${app} EXECUTABLE
      SOURCES    blend2d-testing/commons/zero_lib.cpp
                 blend2d-testing/commons/zero_lib.h
                 blend2d-testing/commons/zero_bench.cpp
                 blend2d-testing/commons/zero_bench.h
                 blend2d-testing/bench/${app}.cpp
      LIBRARIES  blend2d::blend2d
      CFLAGS     ${BLEND2D_PRIVATE_CFLAGS}
      CFLAGS_DBG ${BLEND2D_PRIVATE_CFLAGS_DBG}
      CFLAGS_REL ${BLEND2D_PRIVATE_CFLAGS_REL})
  endforeach()

  # Blend2D C & C++ Samples
  # -----------------------

  # Sample applications require codecs to store the rendered images.
  if (NOT BLEND2D_NO_CODECS)
    foreach(app bl_sample_1 bl_sample_2 bl_sample_3 bl_sample_4 bl_sample_5 bl_sample_6 bl_sample_7 bl_sample_8 bl_sample_tmp)
      blend2d_add_target(${app} EXECUTABLE
        SOURCES   blend2d-testing/samples/${app}.cpp
        LIBRARIES blend2d::blend2d
        CFLAGS    ${BLEND2D_WARNING_CFLAGS})
    endforeach()

    foreach(app bl_sample_capi)
      blend2d_add_target(${app} EXECUTABLE
        SOURCES   blend2d-testing/samples/${app}.c
        LIBRARIES blend2d::blend2d
        CFLAGS    ${BLEND2D_WARNING_CFLAGS})

      # Sanitizers usually require a C++ linker and library support.
      if (BLEND2D_SANITIZE_CFLAGS)
        set_target_properties(${app} PROPERTIES LINKER_LANGUAGE CXX)
      endif()
    endforeach()
  endif()

  # Blend2D Generator
  # -----------------

  blend2d_add_target(bl_generator EXECUTABLE
    SOURCES      blend2d-testing/generator/bl_generator.cpp
                 blend2d-testing/generator/bl_generator.h
    INCLUDE_DIRS ${BLEND2D_INCLUDE_DIRS}
    CFLAGS       ${BLEND2D_WARNING_CFLAGS})
  if (NOT WIN32)
    target_link_libraries(bl_generator "pthread")
  endif()

  # TODO: Not sure this is the best way of copying resources.
  foreach(resource ABeeZee-Regular.ttf Leaves.jpeg)
    if (NOT CMAKE_BUILD_TYPE)
      add_custom_command(TARGET blend2d POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy
          "${CMAKE_CURRENT_SOURCE_DIR}/blend2d-testing/resources/${resource}"
          "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/${resource}"
        COMMENT "Resources")
    else()
      add_custom_command(TARGET blend2d POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy
          "${CMAKE_CURRENT_SOURCE_DIR}/blend2d-testing/resources/${resource}"
          "${CMAKE_CURRENT_BINARY_DIR}/${resource}"
        COMMENT "Resources")
    endif()
  endforeach()
else()
  message(STATUS "[blend2d] Disabling Blend2D tests ('BLEND2D_TEST=OFF')")
endif()

# Blend2D - Targets - Demos + Benchmarking
# ========================================

if (BLEND2D_DEMOS)
  include(FindPkgConfig)
  message(STATUS "[blend2d] Enabling Blend2D demos ('BLEND2D_DEMOS=${BLEND2D_DEMOS}')")

  set(BLEND2D_BENCH_CONTEXT_SRCS "")
  set(BLEND2D_BENCH_LIBS "")
  set(BLEND2D_BENCH_CFLAGS "")
  set(BLEND2D_BENCH_INCLUDE_DIRS "")
  set(BLEND2D_BENCH_LIBRARY_DIRS "")

  # Demos - Probe Dependencies
  find_package(Qt6 COMPONENTS Core Gui Widgets)
  if(Qt6Gui_FOUND)
    message(STATUS "[blend2d] Qt6 found - adding support for Qt6 (benchmarking + demo apps)")

    list(APPEND BLEND2D_BENCH_CFLAGS -DBL_BENCH_ENABLE_QT)
    list(APPEND BLEND2D_BENCH_LIBS Qt6::Gui Qt6::Widgets)
  else()
    message(STATUS "[blend2d] Qt6 not found - cannot build Qt6 benchmark backend and demo apps")
  endif()

  pkg_check_modules(CAIRO cairo)
  if(CAIRO_FOUND)
    message(STATUS "[blend2d] Cairo found - adding support for cairo (benchmarking)")

    list(APPEND BLEND2D_BENCH_CFLAGS -DBL_BENCH_ENABLE_CAIRO)
    list(APPEND BLEND2D_BENCH_LIBS ${CAIRO_LIBRARIES})
    list(APPEND BLEND2D_BENCH_LIBRARY_DIRS ${CAIRO_LIBRARY_DIRS})
    list(APPEND BLEND2D_BENCH_INCLUDE_DIRS ${CAIRO_INCLUDE_DIRS})
  else()
    message(STATUS "[blend2d] Cairo not found - cannot build cairo benchmark backend")
  endif()

  find_package(unofficial-skia CONFIG)
  if(TARGET unofficial::skia::skia)
    message(STATUS "[blend2d] Skia found - adding support for SKIA (benchmarking)")

    list(APPEND BLEND2D_BENCH_CFLAGS -DBL_BENCH_ENABLE_SKIA)
    list(APPEND BLEND2D_BENCH_LIBS unofficial::skia::skia)
  else()
    message(STATUS "[blend2d] Skia not found - cannot build Skia benchmark backend")
  endif()

  if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/3rdparty/agg/include/agg_curves.h")
    message(STATUS "[blend2d] AGG found (./3rdparty/agg) - adding support for AGG (benchmarking)")

    file(GLOB_RECURSE
      BLEND2D_BENCH_CONTEXT_SRCS
      LIST_DIRECTORIES FALSE
      RELATIVE "${CMAKE_CURRENT_LIST_DIR}"
      CONFIGURE_DEPENDS
      "3rdparty/agg/*.h" "3rdparty/agg/*.cpp")
    list(APPEND BLEND2D_BENCH_CFLAGS -DBL_BENCH_ENABLE_AGG)
    list(APPEND BLEND2D_BENCH_INCLUDE_DIRS 3rdparty/agg/include)
  else()
    message(STATUS "[blend2d] AGG not found (./3rdparty/agg) - cannot build AGG benchmark backend")
  endif()

  if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/3rdparty/JUCE/CMakeLists.txt")
    message(STATUS "[blend2d] JUCE found (./3rdparty/JUCE) - adding support for JUCE (benchmarking)")

    add_definitions(-DJUCE_USE_CURL=0 -DUSE_COREGRAPHICS_RENDERING=0)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/3rdparty/JUCE" juce)

    list(APPEND BLEND2D_BENCH_CFLAGS -DBL_BENCH_ENABLE_JUCE)
    list(APPEND BLEND2D_BENCH_LIBS juce::juce_core juce::juce_events juce::juce_graphics)
  else()
    message(STATUS "[blend2d] JUCE not found (./3rdparty/JUCE) - cannot build JUCE benchmark backend")
  endif()

  if (APPLE)
    message(STATUS "[blend2d] CoreGraphics (system) - adding support for CoreGraphics (benchmarking)")

    list(APPEND BLEND2D_BENCH_CFLAGS -DBL_BENCH_ENABLE_COREGRAPHICS)
    list(APPEND BLEND2D_BENCH_LIBS "-framework CoreGraphics")
  endif()

  if(Qt6Gui_FOUND)
    message(STATUS "[blend2d] Adding demos that use Qt framework")

    foreach(app bl_demo_app
                bl_demo_bounces
                bl_demo_bubbles
                bl_demo_circles
                bl_demo_gradients
                bl_demo_image_view
                bl_demo_particles
                bl_demo_paths
                bl_demo_pattern
                bl_demo_playground
                bl_demo_rects
                bl_demo_sprites
                bl_demo_text
                bl_demo_tiger)
      blend2d_add_target(${app} EXECUTABLE_WIN32
        SOURCES
          blend2d-testing/demos/${app}.cpp
          blend2d-testing/demos/bl_demo_utils.h
          blend2d-testing/demos/bl_qt_canvas.cpp
          blend2d-testing/demos/bl_qt_canvas.h
          blend2d-testing/demos/bl_qt_headers.h
        LIBRARIES
          blend2d::blend2d Qt6::Gui Qt6::Widgets)
      set_property(TARGET ${app} PROPERTY AUTOMOC TRUE)
    endforeach()
  endif()

  blend2d_add_target(bl_bench_context EXECUTABLE
    SOURCES      ${BLEND2D_BENCH_CONTEXT_SRC} ${BLEND2D_BENCH_CONTEXT_SRCS}
    LIBRARIES    blend2d::blend2d ${BLEND2D_BENCH_LIBS}
    CFLAGS       ${BLEND2D_BENCH_CFLAGS}
    INCLUDE_DIRS ${BLEND2D_BENCH_INCLUDE_DIRS})
  target_link_directories(bl_bench_context PRIVATE ${BLEND2D_BENCH_LIBRARY_DIRS})
else()
  message(STATUS "[blend2d] Disabling Blend2D demos ('BLEND2D_DEMOS=OFF')")
endif()

message(STATUS "[blend2d] == Configuring done ==")
