cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

cmake_policy(PUSH)
cmake_policy(SET CMP0063 NEW) # Honor visibility properties.

include(CheckCXXCompilerFlag)

# Don't create a project if it was already created by another CMakeLists.txt.
# This allows one library to embed another library without a project collision.
if (NOT CMAKE_PROJECT_NAME OR "${CMAKE_PROJECT_NAME}" STREQUAL "blend2d")
  project(blend2d CXX)
endif()

# =============================================================================
# [Blend2D - Deprecated]
# =============================================================================

if (DEFINED BLEND2D_BUILD_EMBED)
  message(DEPRECATION "BLEND2D_BUILD_EMBED is deprecated, use BLEND2D_EMBED")
  set(BLEND2D_EMBED "${BLEND2D_BUILD_EMBED}")
endif()

if (DEFINED BLEND2D_BUILD_STATIC)
  message(DEPRECATION "BLEND2D_BUILD_STATIC is deprecated, use BLEND2D_STATIC")
  set(BLEND2D_STATIC "${BLEND2D_BUILD_STATIC}")
endif()

# =============================================================================
# [Blend2D - Configuration]
# =============================================================================

if (NOT DEFINED BLEND2D_DIR)
  set(BLEND2D_DIR "${CMAKE_CURRENT_LIST_DIR}")
endif()

if (NOT DEFINED BLEND2D_EMBED)
  set(BLEND2D_EMBED FALSE)
endif()

if (NOT DEFINED BLEND2D_STATIC)
  set(BLEND2D_STATIC ${BLEND2D_EMBED})
endif()

if (NOT DEFINED BLEND2D_TEST)
  set(BLEND2D_TEST FALSE)
endif()

set(BLEND2D_DIR    "${BLEND2D_DIR}"    CACHE PATH "Location of 'blend2d'")
set(BLEND2D_TEST   "${BLEND2D_TEST}"   CACHE BOOL "Build 'blend2d' test applications")
set(BLEND2D_EMBED  "${BLEND2D_EMBED}"  CACHE BOOL "Embed 'blend2d' library (no targets)")
set(BLEND2D_STATIC "${BLEND2D_STATIC}" CACHE BOOL "Build 'blend2d' statically")

# Experimental build options.
# set(BLEND2D_BUILD_NO_LOGGING 1)
# set(BLEND2D_BUILD_NO_JIT 1)

if (NOT DEFINED BLEND2D_NO_STDCXX)
  if (NOT BLEND2D_EMBED AND NOT BLEND2D_STATIC)
    set(BLEND2D_NO_STDCXX 1)
  else()
    set(BLEND2D_NO_STDCXX 0)
  endif()
endif()

# =============================================================================
# [Blend2D - Project]
# =============================================================================

set(BLEND2D_INCLUDE_DIRS "${BLEND2D_DIR}/src")   # Include directory is the same as source dir.
set(BLEND2D_DEPS "")                             # Blend2D dependencies (libraries) for the linker.
set(BLEND2D_LIBS "")                             # Dependencies of libs/apps that want to use Blend2D.
set(BLEND2D_CFLAGS "")                           # Public compiler flags.
set(BLEND2D_PRIVATE_LFLAGS "")                   # Private linker flags.
set(BLEND2D_PRIVATE_CFLAGS "")                   # Private compiler flags independent of build type.
set(BLEND2D_PRIVATE_CFLAGS_DBG "")               # Private compiler flags used by debug builds.
set(BLEND2D_PRIVATE_CFLAGS_REL "")               # Private compiler flags used by release builds.
set(BLEND2D_NO_STDCXX_LFLAGS "")                 # Private linker flags to disable linking to a standard C++ library.
set(BLEND2D_NO_STDCXX_CFLAGS "")                 # Private compiler flags to disable linking to a standard C++ library.

# =============================================================================
# [Blend2D - Utilities]
# =============================================================================

# Detects C++ flags and appends all detected ones to `out`.
function(blend2d_detect_cflags out)
  set(out_array ${${out}})
  foreach(flag ${ARGN})
    string(REGEX REPLACE "[+]" "x" flag_signature "${flag}")
    string(REGEX REPLACE "[-=:;/.\]" "_" flag_signature "${flag_signature}")
    check_cxx_compiler_flag(${flag} "__CxxFlag_${flag_signature}")
    if (${__CxxFlag_${flag_signature}})
      list(APPEND out_array "${flag}")
    endif()
  endforeach()
  set(${out} "${out_array}" PARENT_SCOPE)
endfunction()

function(blend2d_add_target target target_type src deps cflags cflags_dbg cflags_rel)
  if ("${target_type}" STREQUAL "EXECUTABLE")
    add_executable(${target} ${src})
  else()
    add_library(${target} ${target_type} ${src})
  endif()

  target_link_libraries(${target} ${deps})
  target_compile_options(${target} PRIVATE ${cflags} $<$<CONFIG:Debug>:${cflags_dbg}> $<$<NOT:$<CONFIG:Debug>>:${cflags_rel}>)
  set_property(TARGET ${target} PROPERTY CXX_EXTENSIONS NO)
  set_property(TARGET ${target} PROPERTY CXX_VISIBILITY_PRESET hidden)
  target_compile_features(${target} PUBLIC cxx_std_11)

  # target_link_options was added in cmake 3.13, which doesn't work for us.
  # set_property(TARGET ${target} APPEND PROPERTY LINK_OPTIONS ${BLEND2D_PRIVATE_LFLAGS})
  foreach(link_flag ${BLEND2D_PRIVATE_LFLAGS})
    set_property(TARGET ${target} APPEND_STRING PROPERTY LINK_FLAGS " ${link_flag}")
  endforeach()
endfunction()

# =============================================================================
# [Blend2D - Compiler]
# =============================================================================

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC" OR "x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC")
  list(APPEND BLEND2D_PRIVATE_CFLAGS
    -MP                      # [+] Multi-Process Compilation.
    -GR-                     # [-] Runtime type information.
    -GF                      # [+] Eliminate duplicate strings.
    -Zc:inline               # [+] Remove unreferenced COMDAT.
    -Zc:strictStrings        # [+] Strict const qualification of string literals.
    -Zc:threadSafeInit-      # [-] Thread-safe statics.
    -volatile:iso            # [+] Volatile loads and stores have standard semantics.
    -W4)                     # [+] Warning level 4.

  list(APPEND BLEND2D_PRIVATE_CFLAGS_DBG
    -GS)                     # [+] Buffer security-check.

  list(APPEND BLEND2D_PRIVATE_CFLAGS_REL
    -GS-                     # [-] Buffer security-check.
    -O2                      # [+] Favor speed over size.
    -Oi)                     # [+] Generate Intrinsic Functions.

  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    list(APPEND BLEND2D_PRIVATE_CFLAGS -fno-exceptions -fno-rtti -fno-math-errno)
  endif()
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "^(GNU|Clang|AppleClang)$")
  list(APPEND BLEND2D_PRIVATE_CFLAGS -Wall -Wextra)
  list(APPEND BLEND2D_PRIVATE_CFLAGS -fno-exceptions -fno-rtti -fno-math-errno -fno-threadsafe-statics)
  list(APPEND BLEND2D_PRIVATE_CFLAGS_REL -O2)

  blend2d_detect_cflags(BLEND2D_PRIVATE_CFLAGS_REL
    -ftree-vectorize         # We want this optimization if not enabled by default.
    -fmerge-all-constants)   # Not enabled by default, but we are fine with it.

  # GCC 4.X support requires -fabi-version=0 (or 6+). Please note that this
  # is internal and only required by `blsimd_p.h` as SIMD registers are used
  # as types in template specializations, which is not handled by older ABI.
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)
    list(APPEND BLEND2D_PRIVATE_CFLAGS "-fabi-version=0")
  endif()

  # Support for GCC|Clang undefined behavior sanitizers.
  if (BLEND2D_SANITIZE)
    blend2d_detect_cflags(BLEND2D_SANITIZE_FLAGS "-fsanitize=undefined")
    if (BLEND2D_SANITIZE_FLAGS)
      message("-- Enabling UB sanitizer via '${BLEND2D_SANITIZE_FLAGS}'")
      # TODO: I don't undertand why this is necessary, it's not nice.
      list(APPEND BLEND2D_PRIVATE_LFLAGS ${BLEND2D_SANITIZE_FLAGS})
      list(APPEND BLEND2D_PRIVATE_CFLAGS ${BLEND2D_SANITIZE_FLAGS})
    endif()
  endif()

  # Building Blend2D without C++ library support requires these to be setup.
  if (BLEND2D_NO_STDCXX)
    message("-- Enabling build without linking to the C++ standard library")
    # This fails when a compiler emits a symbol which requires libgcc:
    #   list(APPEND BLEND2D_NO_STDCXX_CFLAGS -nodefaultlibs -DBL_NO_STDCXX)
    #   list(APPEND BLEND2D_NO_STDCXX_LFLAGS -nodefaultlibs)
    # This has similar effect as we don't really use anything from libstdc++:
    list(APPEND BLEND2D_NO_STDCXX_CFLAGS -DBL_NO_STDCXX)
    list(APPEND BLEND2D_NO_STDCXX_LFLAGS -static-libstdc++)
  endif()
endif()

if (BLEND2D_EMBED)
  set(BLEND2D_TARGET_TYPE "EMBED")
elseif (BLEND2D_STATIC)
  set(BLEND2D_TARGET_TYPE "STATIC")
else()
  set(BLEND2D_TARGET_TYPE "SHARED")
endif()

if (BLEND2D_EMBED OR BLEND2D_STATIC)
  list(APPEND BLEND2D_CFLAGS         "-DBL_STATIC")
  list(APPEND BLEND2D_PRIVATE_CFLAGS "-DBL_STATIC")
endif()

if (BLEND2D_BUILD_NO_JIT)
  list(APPEND BLEND2D_PRIVATE_CFLAGS "-DBL_BUILD_NO_JIT")
endif()

# =============================================================================
# [Blend2D - Deprecated]
# =============================================================================

# DEPRECATED: Will be removed int the future.
set(BLEND2D_INCLUDE_DIR "${BLEND2D_INCLUDE_DIRS}")

# =============================================================================
# [Blend2D - Enable SIMD]
# =============================================================================

# TODO: Detect ARM when the support is added.

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC" OR "x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC")
  # AVX/AVX2 doesn't need custom defs as MSVC|Intel does define __AVX[2]__
  # similary to other compilers. In addition, we only detect the support
  # for AVX/AVX2 as if these are available all previous instruction sets
  # are also available. If such check fails it means that we are either
  # not compiling for X86/X64 or the compiler is very old, which cannot
  # be used anyway.
  blend2d_detect_cflags(BLEND2D_CFLAGS_AVX "-arch:AVX")
  blend2d_detect_cflags(BLEND2D_CFLAGS_AVX2 "-arch:AVX2")

  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    # Intel deprecated -arch:SSE, so it's implicit. In contrast to MSVC, Intel
    # also provides -arch:SSE3+ options and uses the same definitions as GCC
    # and Clang, so no magic needed here.
    if (BLEND2D_CFLAGS_AVX)
      list(APPEND BLEND2D_CFLAGS_SSE2 "-arch:SSE2")
      list(APPEND BLEND2D_CFLAGS_SSE3 "-arch:SSE3")
      list(APPEND BLEND2D_CFLAGS_SSSE3 "-arch:SSSE3")
      list(APPEND BLEND2D_CFLAGS_SSE4_1 "-arch:SSE4.1")
      list(APPEND BLEND2D_CFLAGS_SSE4_2 "-arch:SSE4.2")
    endif()
  else()
    if (BLEND2D_CFLAGS_AVX2)
      # 64-bit MSVC compiler doesn't like -arch:SSE[2] as it's implicit.
      if (NOT CMAKE_CL_64)
        list(APPEND BLEND2D_CFLAGS_SSE2 "-arch:SSE2")
        list(APPEND BLEND2D_CFLAGS_SSE3 "-arch:SSE2")
        list(APPEND BLEND2D_CFLAGS_SSSE3 "-arch:SSE2")
        list(APPEND BLEND2D_CFLAGS_SSE4_1 "-arch:SSE2")
        list(APPEND BLEND2D_CFLAGS_SSE4_2 "-arch:SSE2")
      endif()
      if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        # MSVC can generate SSE3 intrinsics even in SSE2 mode and has no switch
        # that would explicitly enable SSE3 code generation. However, clang-cl
        # cannot do this and requires additional flags to enable SSE3+ intrinsics.
        list(APPEND BLEND2D_CFLAGS_SSE3 "-msse3")
        list(APPEND BLEND2D_CFLAGS_SSSE3 "-mssse3")
        list(APPEND BLEND2D_CFLAGS_SSE4_1 "-msse4.1")
        list(APPEND BLEND2D_CFLAGS_SSE4_2 "-msse4.2")
      else()
        # MSVC doesn't provide any preprocessor definitions for SSE3 and higher,
        # thus we have to define them ourselves to match what other compilers do.
        list(APPEND BLEND2D_CFLAGS_SSE3 "-D__SSE3__")
        list(APPEND BLEND2D_CFLAGS_SSSE3 "-D__SSSE3__")
        list(APPEND BLEND2D_CFLAGS_SSE4_1 "-D__SSE4_1__")
        list(APPEND BLEND2D_CFLAGS_SSE4_2 "-D__SSE4_2__")
      endif()
    endif()
  endif()
else()
  # Assume all other compilers are compatible with GCC|Clang.
  blend2d_detect_cflags(BLEND2D_CFLAGS_AVX "-mavx")
  blend2d_detect_cflags(BLEND2D_CFLAGS_AVX2 "-mavx2")
  if (BLEND2D_CFLAGS_AVX)
    list(APPEND BLEND2D_CFLAGS_SSE2 "-msse2")
    list(APPEND BLEND2D_CFLAGS_SSE3 "-msse3")
    list(APPEND BLEND2D_CFLAGS_SSSE3 "-mssse3")
    list(APPEND BLEND2D_CFLAGS_SSE4_1 "-msse4.1")
    list(APPEND BLEND2D_CFLAGS_SSE4_2 "-msse4.2")
  endif()
endif()

# Use SSE2 by default on X86/X64 as this is our baseline.
if (BLEND2D_CFLAGS_SSE2)
  list(APPEND BLEND2D_PRIVATE_CFLAGS ${BLEND2D_CFLAGS_SSE2})
endif()

# Do not make this more complicated than it is. We assume that compiler can
# handle either all (SSE2, SSE3, ... AVX) or nothing. We require C++11 so
# this should exclude all old compilers where this assumption would not hold.
if (BLEND2D_CFLAGS_AVX2)
  list(APPEND BLEND2D_PRIVATE_CFLAGS "-DBL_BUILD_OPT_AVX2")
elseif (BLEND2D_CFLAGS_AVX)
  list(APPEND BLEND2D_PRIVATE_CFLAGS "-DBL_BUILD_OPT_AVX")
endif()

# =============================================================================
# [Blend2D - Dependencies]
# =============================================================================

if (NOT WIN32)
  list(APPEND BLEND2D_DEPS c m pthread)
endif()

if ("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
  list(APPEND BLEND2D_DEPS rt)
endif()

# Find asmjit dependency if building with JIT support.
if (NOT BLEND2D_BUILD_NO_JIT)
  if (NOT DEFINED ASMJIT_DIR)
    foreach(dir "${BLEND2D_DIR}/3rdparty/asmjit"
                "${CMAKE_CURRENT_LIST_DIR}/../asmjit")
      if (EXISTS ${dir}/CMakeLists.txt)
        set(ASMJIT_DIR "${dir}" CACHE PATH "Location of 'asmjit'")
        break()
      endif()
    endforeach()
    if (NOT DEFINED ASMJIT_DIR)
      message(FATAL "Unable to find asmjit, please visit <https://blend2d.com/doc/build-instructions.html>")
    endif()
  endif()

  if (NOT DEFINED ASMJIT_EMBED)
    set(ASMJIT_EMBED TRUE CACHE BOOL "")
  endif()

  include("${ASMJIT_DIR}/CMakeLists.txt")
  list(APPEND BLEND2D_DEPS ${ASMJIT_LIBS})
  list(APPEND BLEND2D_PRIVATE_CFLAGS ${ASMJIT_CFLAGS})
  list(APPEND BLEND2D_PRIVATE_CFLAGS -DASMJIT_NO_STDCXX)

  # A possibility to reduce the resulting binary size by disabling asmjit logging.
  if (BLEND2D_BUILD_NO_LOGGING)
    message("   Disabling AsmJit logging functionality to make Blend2D smaller")
    list(APPEND BLEND2D_PRIVATE_CFLAGS -DASMJIT_NO_LOGGING -DASMJIT_NO_TEXT)
  endif()
endif()

# =============================================================================
# [Blend2D - Finalize Build Options]
# =============================================================================

list(REMOVE_DUPLICATES BLEND2D_DEPS)
list(REMOVE_DUPLICATES BLEND2D_PRIVATE_CFLAGS)

set(BLEND2D_LIBS ${BLEND2D_DEPS})
if (NOT BLEND2D_EMBED)
  list(INSERT BLEND2D_LIBS 0 blend2d)
endif()

# =============================================================================
# [Blend2D - Source Files]
# =============================================================================

set(BLEND2D_SRC_LIST
  blend2d.h
  blend2d-debug.h
  blend2d-impl.h
  blend2d/api.h
  blend2d/api-build_p.h
  blend2d/api-impl.h
  blend2d/api-internal_p.h
  blend2d/api-nocxx.cpp
  blend2d/array.cpp
  blend2d/array.h
  blend2d/array_p.h
  blend2d/arrayops.cpp
  blend2d/arrayops_p.h
  blend2d/bitarray.cpp
  blend2d/bitarray.h
  blend2d/bitarray_p.h
  blend2d/bitops.cpp
  blend2d/bitops_p.h
  blend2d/compop.cpp
  blend2d/compop_p.h
  blend2d/context.cpp
  blend2d/context.h
  blend2d/context_p.h
  blend2d/filesystem.cpp
  blend2d/filesystem.h
  blend2d/filesystem_p.h
  blend2d/font.cpp
  blend2d/font.h
  blend2d/font_p.h
  blend2d/fontdefs.h
  blend2d/fontmanager.cpp
  blend2d/fontmanager.h
  blend2d/fontmanager_p.h
  blend2d/format.cpp
  blend2d/format.h
  blend2d/format_p.h
  blend2d/geometry.cpp
  blend2d/geometry.h
  blend2d/geometry_p.h
  blend2d/glyphbuffer.cpp
  blend2d/glyphbuffer.h
  blend2d/glyphbuffer_p.h
  blend2d/gradient.cpp
  blend2d/gradient_avx2.cpp
  blend2d/gradient_sse2.cpp
  blend2d/gradient.h
  blend2d/gradient_p.h
  blend2d/image.cpp
  blend2d/image.h
  blend2d/image_p.h
  blend2d/imagecodec.cpp
  blend2d/imagecodec.h
  blend2d/imagescale.cpp
  blend2d/imagescale_p.h
  blend2d/math.cpp
  blend2d/math_p.h
  blend2d/matrix.cpp
  blend2d/matrix_avx.cpp
  blend2d/matrix_sse2.cpp
  blend2d/matrix.h
  blend2d/matrix_p.h
  blend2d/path.cpp
  blend2d/path.h
  blend2d/path_p.h
  blend2d/pathstroke.cpp
  blend2d/pathstroke_p.h
  blend2d/pattern.cpp
  blend2d/pattern.h
  blend2d/pattern_p.h
  blend2d/pipedefs.cpp
  blend2d/pipedefs_p.h
  blend2d/piperuntime.cpp
  blend2d/piperuntime_p.h
  blend2d/pixelconverter.cpp
  blend2d/pixelconverter_avx2.cpp
  blend2d/pixelconverter_sse2.cpp
  blend2d/pixelconverter_ssse3.cpp
  blend2d/pixelconverter.h
  blend2d/pixelconverter_p.h
  blend2d/pixelops.cpp
  blend2d/pixelops_p.h
  blend2d/random.cpp
  blend2d/random.h
  blend2d/random_p.h
  blend2d/region.cpp
  blend2d/region.h
  blend2d/region_p.h
  blend2d/rgba.cpp
  blend2d/rgba.h
  blend2d/runtime.cpp
  blend2d/runtime.h
  blend2d/runtime_p.h
  blend2d/scopedallocator.cpp
  blend2d/scopedallocator_p.h
  blend2d/simd_p.h
  blend2d/simd_x86_p.h
  blend2d/string.cpp
  blend2d/string.h
  blend2d/string_p.h
  blend2d/support.cpp
  blend2d/support_p.h
  blend2d/tables.cpp
  blend2d/tables_p.h
  blend2d/trace.cpp
  blend2d/trace_p.h
  blend2d/unicode.cpp
  blend2d/unicode_p.h
  blend2d/variant.cpp
  blend2d/variant.h
  blend2d/variant_p.h
  blend2d/zeroallocator.cpp
  blend2d/zeroallocator_p.h
  blend2d/zoneallocator.cpp
  blend2d/zoneallocator_p.h
  blend2d/zonehash.cpp
  blend2d/zonehash_p.h
  blend2d/zonelist.cpp
  blend2d/zonelist_p.h
  blend2d/zonetree.cpp
  blend2d/zonetree_p.h

  blend2d/codec/bmpcodec.cpp
  blend2d/codec/bmpcodec_p.h
  blend2d/codec/deflate.cpp
  blend2d/codec/deflate_p.h
  blend2d/codec/jpegcodec.cpp
  blend2d/codec/jpegcodec_p.h
  blend2d/codec/jpeghuffman.cpp
  blend2d/codec/jpeghuffman_p.h
  blend2d/codec/jpegops.cpp
  blend2d/codec/jpegops_sse2.cpp
  blend2d/codec/jpegops_p.h
  blend2d/codec/pngcodec.cpp
  blend2d/codec/pngcodec_p.h
  blend2d/codec/pngops.cpp
  blend2d/codec/pngops_sse2.cpp
  blend2d/codec/pngops_p.h

  blend2d/fixedpipe/fixedpiperuntime_p.h
  blend2d/fixedpipe/fixedpiperuntime.cpp

  blend2d/opentype/otcff.cpp
  blend2d/opentype/otcff_p.h
  blend2d/opentype/otcmap.cpp
  blend2d/opentype/otcmap_p.h
  blend2d/opentype/otcore.cpp
  blend2d/opentype/otcore_p.h
  blend2d/opentype/otdefs_p.h
  blend2d/opentype/otface.cpp
  blend2d/opentype/otface_p.h
  blend2d/opentype/otglyf.cpp
  blend2d/opentype/otglyf_p.h
  blend2d/opentype/otkern.cpp
  blend2d/opentype/otkern_p.h
  blend2d/opentype/otlayout.cpp
  blend2d/opentype/otlayout_p.h
  blend2d/opentype/otmetrics.cpp
  blend2d/opentype/otmetrics_p.h
  blend2d/opentype/otname.cpp
  blend2d/opentype/otname_p.h
  blend2d/opentype/otplatform_p.h

  blend2d/pipegen/compoppart.cpp
  blend2d/pipegen/compoppart_p.h
  blend2d/pipegen/fetchgradientpart.cpp
  blend2d/pipegen/fetchgradientpart_p.h
  blend2d/pipegen/fetchpart.cpp
  blend2d/pipegen/fetchpart_p.h
  blend2d/pipegen/fetchpatternpart.cpp
  blend2d/pipegen/fetchpatternpart_p.h
  blend2d/pipegen/fetchpixelptrpart.cpp
  blend2d/pipegen/fetchpixelptrpart_p.h
  blend2d/pipegen/fetchsolidpart.cpp
  blend2d/pipegen/fetchsolidpart_p.h
  blend2d/pipegen/fetchutils.cpp
  blend2d/pipegen/fetchutils_p.h
  blend2d/pipegen/fillpart.cpp
  blend2d/pipegen/fillpart_p.h
  blend2d/pipegen/pipecompiler.cpp
  blend2d/pipegen/pipecompiler_p.h
  blend2d/pipegen/pipedebug_p.h
  blend2d/pipegen/pipegencore.cpp
  blend2d/pipegen/pipegencore_p.h
  blend2d/pipegen/pipegenruntime_p.h
  blend2d/pipegen/pipegenruntime.cpp
  blend2d/pipegen/pipepart.cpp
  blend2d/pipegen/pipepart_p.h
  blend2d/pipegen/piperegusage_p.h

  blend2d/raster/analyticrasterizer_p.h
  blend2d/raster/edgebuilder_p.h
  blend2d/raster/rastercommand_p.h
  blend2d/raster/rastercontext.cpp
  blend2d/raster/rastercontext_p.h
  blend2d/raster/rasterdefs_p.h
  blend2d/raster/rasterfiller.cpp
  blend2d/raster/rasterfiller_p.h
  blend2d/raster/rasterworkercontext.cpp
  blend2d/raster/rasterworkercontext_p.h
  blend2d/raster/rasterworkermanager.cpp
  blend2d/raster/rasterworkermanager_p.h

  blend2d/threading/atomic_p.h
  blend2d/threading/conditionvariable_p.h
  blend2d/threading/mutex_p.h
  blend2d/threading/thread.cpp
  blend2d/threading/thread_p.h
  blend2d/threading/threadpool.cpp
  blend2d/threading/threadpool_p.h
)

#if (MSVC)
#  list(APPEND BLEND2D_SRC_LIST blend2d.natvis)
#endif()

set(BLEND2D_SRC "")
foreach(src_file ${BLEND2D_SRC_LIST})
  set(src_file "${BLEND2D_DIR}/src/${src_file}")
  list(APPEND BLEND2D_SRC ${src_file})

  string(REGEX MATCH "_(sse[2|3]?|ssse3|sse4_[1|2]|avx|avx[2]?)\\.(c|cc|cxx|cpp|m|mm)$" FEATURE ${src_file})
  if (FEATURE)
    # HACK 1: Cmake uses global variables everywhere, `CMAKE_MATCH_1` is the first capture...
    string(TOUPPER "${CMAKE_MATCH_1}" FEATURE)
    # HACK 2: Interestingly COMPILE_OPTIONS is not available for SOURCE files before CMake 3.11.
    # set_property(SOURCE "${src_file}" APPEND PROPERTY COMPILE_OPTIONS ${BLEND2D_CFLAGS_${FEATURE}})
    foreach(src_cflag ${BLEND2D_CFLAGS_${FEATURE}})
      set_property(SOURCE "${src_file}" APPEND_STRING PROPERTY COMPILE_FLAGS " ${src_cflag}")
    endforeach()
  endif()
endforeach()
source_group(TREE "${BLEND2D_DIR}" FILES ${BLEND2D_SRC})

# =============================================================================
# [Blend2D - Summary]
# =============================================================================

message("** Blend2D Summary **")
message("   BLEND2D_DIR=${BLEND2D_DIR}")
message("   BLEND2D_TEST=${BLEND2D_TEST}")
message("   BLEND2D_TARGET_TYPE=${BLEND2D_TARGET_TYPE}")
message("   BLEND2D_DEPS=${BLEND2D_DEPS}")
message("   BLEND2D_LIBS=${BLEND2D_LIBS}")
message("   BLEND2D_CFLAGS=${BLEND2D_CFLAGS}")
message("   BLEND2D_PRIVATE_CFLAGS=${BLEND2D_PRIVATE_CFLAGS}")
message("   BLEND2D_PRIVATE_CFLAGS_DBG=${BLEND2D_PRIVATE_CFLAGS_DBG}")
message("   BLEND2D_PRIVATE_CFLAGS_REL=${BLEND2D_PRIVATE_CFLAGS_REL}")

# =============================================================================
# [Blend2D - Targets]
# =============================================================================

if (NOT BLEND2D_EMBED)
  # Add 'blend2d' library target.
  blend2d_add_target(blend2d
    "${BLEND2D_TARGET_TYPE}"
    "${BLEND2D_SRC};${ASMJIT_SRC}"
    "${BLEND2D_DEPS}"
    "${BLEND2D_PRIVATE_CFLAGS}"
    "${BLEND2D_PRIVATE_CFLAGS_DBG}"
    "${BLEND2D_PRIVATE_CFLAGS_REL}")

  # target_link_options was added in cmake 3.13, which doesn't work for us.
  # target_link_options(blend2d PRIVATE ${BLEND2D_NO_STDCXX_LFLAGS})
  foreach(link_flag ${BLEND2D_NO_STDCXX_LFLAGS})
    set_property(TARGET blend2d APPEND_STRING PROPERTY LINK_FLAGS " ${link_flag}")
  endforeach()

  target_compile_options(blend2d INTERFACE ${BLEND2D_CFLAGS})
  target_compile_options(blend2d PRIVATE ${BLEND2D_NO_STDCXX_CFLAGS})

  target_include_directories(blend2d BEFORE PRIVATE ${ASMJIT_INCLUDE_DIRS})
  target_include_directories(blend2d BEFORE INTERFACE ${BLEND2D_INCLUDE_DIRS})

  # Add Blend2D::Blend2D target (alias to blend2d).
  add_library(Blend2D::Blend2D ALIAS blend2d)

  # Install 'blend2d' target (shared or static).
  install(TARGETS blend2d RUNTIME DESTINATION "bin"
                          LIBRARY DESTINATION "lib${LIB_SUFFIX}"
                          ARCHIVE DESTINATION "lib${LIB_SUFFIX}")

  # Install 'blend2d' header files (private headers are filtered out).
  foreach(_src_file ${BLEND2D_SRC_LIST})
    if ("${_src_file}" MATCHES "\\.h$" AND NOT "${_src_file}" MATCHES "_p\\.h$")
      get_filename_component(_src_dir ${_src_file} PATH)
      install(FILES "${BLEND2D_DIR}/src/${_src_file}" DESTINATION "include/${_src_dir}")
    endif()
  endforeach()

  # Add 'blend2d' tests.
  if (BLEND2D_TEST)
    # Unit tests are so much different compared to other tests, they need the whole
    # source-code to be compiled with the unit test executable with `UNIT()` tests
    # enabled.
    set (BLEND2D_TEST_SRC test/bl_test_unit.cpp test/broken.cpp test/broken.h)
    blend2d_add_target(bl_test_unit
      EXECUTABLE
      "${BLEND2D_SRC};${ASMJIT_SRC};${BLEND2D_TEST_SRC}"
      "${BLEND2D_DEPS}"
      "${BLEND2D_PRIVATE_CFLAGS}"
      "${BLEND2D_PRIVATE_CFLAGS_DBG}"
      "${BLEND2D_PRIVATE_CFLAGS_REL}")
    target_compile_definitions(bl_test_unit PRIVATE BL_TEST BL_STATIC)
    target_include_directories(bl_test_unit BEFORE PRIVATE ${ASMJIT_INCLUDE_DIRS})
    target_include_directories(bl_test_unit BEFORE PRIVATE ${BLEND2D_INCLUDE_DIRS})

    # Other tests link Blend2D either statically or dynamically according to the
    # user settings.
    blend2d_add_target(bl_test_fuzzer EXECUTABLE test/bl_test_fuzzer.cpp Blend2D::Blend2D "" "" "")
  endif()
endif()

cmake_policy(POP)
