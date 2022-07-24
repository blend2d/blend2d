cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

cmake_policy(PUSH)

if(POLICY CMP0063)
  cmake_policy(SET CMP0063 NEW) # Honor visibility properties.
endif()

if(POLICY CMP0092)
  cmake_policy(SET CMP0092 NEW) # Don't add -W3 warning level by default.
endif()

# Don't create a project if it was already created by another CMakeLists.txt.
# This allows one library to embed another library without a project collision.
if (NOT CMAKE_PROJECT_NAME OR "${CMAKE_PROJECT_NAME}" STREQUAL "blend2d")
  project(blend2d C CXX)
endif()

include(CheckCXXCompilerFlag)
include(GNUInstallDirs)

# Blend2D - Configuration
# =======================

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

if (NOT DEFINED BLEND2D_NO_NATVIS)
  set(BLEND2D_NO_NATVIS FALSE)
endif()

set(BLEND2D_DIR      "${BLEND2D_DIR}"      CACHE PATH "Location of 'blend2d'")
set(BLEND2D_TEST     "${BLEND2D_TEST}"     CACHE BOOL "Build 'blend2d' tests and samples")
set(BLEND2D_EMBED    "${BLEND2D_EMBED}"    CACHE BOOL "Embed 'blend2d' library (no targets)")
set(BLEND2D_STATIC   "${BLEND2D_STATIC}"   CACHE BOOL "Build 'blend2d' statically")
set(BLEND2D_SANITIZE "${BLEND2D_SANITIZE}" CACHE STRING "List of sanitizers to use")

# Experimental build options.
# set(BLEND2D_NO_JIT 1)
# set(BLEND2D_NO_JIT_LOGGING 1)
# set(BLEND2D_NO_TLS 1)
# set(BLEND2D_NO_FUTEX 1)
# set(BLEND2D_NO_INTRINSICS 1)
# set(BLEND2D_NO_STDCXX 1)

if (NOT DEFINED BLEND2D_NO_STDCXX)
  if (NOT BLEND2D_EMBED AND NOT BLEND2D_STATIC)
    set(BLEND2D_NO_STDCXX 1)
  else()
    set(BLEND2D_NO_STDCXX 0)
  endif()
endif()

# Blend2D - Project
# =================

set(BLEND2D_INCLUDE_DIRS "${BLEND2D_DIR}/src")   # Include directory is the same as source dir.
set(BLEND2D_DEPS "")                             # Blend2D dependencies (libraries) for the linker.
set(BLEND2D_LIBS "")                             # Dependencies of libs/apps that want to use Blend2D.
set(BLEND2D_CFLAGS "")                           # Public compiler flags.
set(BLEND2D_PRIVATE_LFLAGS "")                   # Private linker flags.
set(BLEND2D_PRIVATE_CFLAGS "")                   # Private compiler flags independent of build type.
set(BLEND2D_PRIVATE_CFLAGS_DBG "")               # Private compiler flags used by debug builds.
set(BLEND2D_PRIVATE_CFLAGS_REL "")               # Private compiler flags used by release builds.
set(BLEND2D_SANITIZE_CFLAGS "")                  # Compiler flags required by currently enabled sanitizers.
set(BLEND2D_SANITIZE_LFLAGS "")                  # Linker flags required by currently enabled sanitizers.
set(BLEND2D_NO_STDCXX_CFLAGS "")                 # Private compiler flags to disable linking to a standard C++ library.
set(BLEND2D_NO_STDCXX_LFLAGS "")                 # Private linker flags to disable linking to a standard C++ library.

# Blend2D - Utilities
# ===================

# Detects C++ flags and appends all detected ones to `out`.
function(blend2d_detect_cflags out)
  set(_out_array ${${out}})
  foreach(_flag ${ARGN})
    string(REGEX REPLACE "[+]" "x" _flag_signature "${_flag}")
    string(REGEX REPLACE "[-=:;/.\]" "_" _flag_signature "${_flag_signature}")

    if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "^(GNU|Clang|AppleClang)$")
      check_cxx_compiler_flag("${_flag} -Werror" "__CxxFlag_${_flag_signature}")
    else()
      check_cxx_compiler_flag(${_flag} "__CxxFlag_${_flag_signature}")
    endif()

    if (${__CxxFlag_${_flag_signature}})
      list(APPEND _out_array "${_flag}")
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
  set(single_val "")
  set(multi_val SOURCES LIBRARIES CFLAGS CFLAGS_DBG CFLAGS_REL)
  cmake_parse_arguments("X" "" "${single_val}" "${multi_val}" ${ARGN})

  if ("${target_type}" MATCHES "^(EXECUTABLE|TEST)$")
    add_executable(${target} ${X_SOURCES})
    set_target_properties(${sample} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(TargetDir).")
  else()
    add_library(${target} ${target_type} ${X_SOURCES})
    set_target_properties(${target} PROPERTIES DEFINE_SYMBOL "")
  endif()

  target_link_libraries(${target} PRIVATE ${X_LIBRARIES})

  # target_link_options was added in cmake v3.13, don't use it for now...
  foreach(link_flag ${BLEND2D_SANITIZE_LFLAGS})
    set_property(TARGET ${target} APPEND_STRING PROPERTY LINK_FLAGS " ${link_flag}")
  endforeach()

  if (${CMAKE_VERSION} VERSION_LESS "3.8.0")
    set_property(TARGET ${target} PROPERTY CXX_STANDARD 11)
  else()
    target_compile_features(${target} PUBLIC cxx_std_11)
  endif()
  set_property(TARGET ${target} PROPERTY CXX_EXTENSIONS ON)
  set_property(TARGET ${target} PROPERTY CXX_VISIBILITY_PRESET hidden)
  target_compile_options(${target} PRIVATE ${X_CFLAGS} ${BLEND2D_SANITIZE_CFLAGS} $<IF:$<CONFIG:Debug>,${X_CFLAGS_DBG},${X_CFLAGS_REL}>)

  if ("${target_type}" STREQUAL "TEST")
    add_test(NAME ${target} COMMAND ${target})
  endif()
endfunction()


# Blend2D - Compiler Support
# ==========================

if ("x${CMAKE_CXX_COMPILER_ID}" STREQUAL "xMSVC" OR "x${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "xMSVC")
  list(APPEND BLEND2D_PRIVATE_CFLAGS
    -MP                      # [+] Multi-Process Compilation.
    -GR-                     # [-] Runtime type information.
    -GF                      # [+] Eliminate duplicate strings.
    -Zc:__cplusplus          # [+] Conforming __cplusplus definition.
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
    list(APPEND BLEND2D_PRIVATE_CFLAGS -clang:-fno-rtti -clang:-fno-math-errno -clang:-fno-trapping-math)
  endif()
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "^(GNU|Clang|AppleClang)$")
  list(APPEND BLEND2D_PRIVATE_CFLAGS -Wall -Wextra)
  list(APPEND BLEND2D_PRIVATE_CFLAGS -fno-exceptions -fno-rtti -fno-math-errno)
  list(APPEND BLEND2D_PRIVATE_CFLAGS_REL -O2)


  if (NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "iOS")
    # Disable these flags in case somebody uses -ffast-math globally.
    list(APPEND BLEND2D_PRIVATE_CFLAGS -fno-trapping-math -fno-finite-math-only)
    # Blend2D code should never throw, so we don't need strict exception handling enforcement.
    blend2d_detect_cflags(BLEND2D_PRIVATE_CFLAGS_REL -fno-enforce-eh-specs)
  endif()

  # W don't rely on threadsafe statics and we don't want to depend on a compiler runtime.
  blend2d_detect_cflags(BLEND2D_PRIVATE_CFLAGS -fno-threadsafe-statics)

  # Semantic interposition brings no benefits to our code, used by Clang by default.
  blend2d_detect_cflags(BLEND2D_PRIVATE_CFLAGS -fno-semantic-interposition)

  # We are fine with merging all matching constants into a single slot although it's against the rules.
  blend2d_detect_cflags(BLEND2D_PRIVATE_CFLAGS_REL -fmerge-all-constants)

  # We want this optimization if not enabled by default to autovectorize some of the C++ code.
  blend2d_detect_cflags(BLEND2D_PRIVATE_CFLAGS_REL -ftree-vectorize)

  # GCC 4.X support requires -fabi-version=0 (or 6+). Please note that this
  # is internal and only required by `blsimd_p.h` as SIMD registers are used
  # as types in template specializations, which is not handled by older ABI.
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)
    list(APPEND BLEND2D_PRIVATE_CFLAGS "-fabi-version=0")
  endif()

  # Building Blend2D without C++ library support requires these to be setup.
  if (BLEND2D_NO_STDCXX)
    message("-- Enabling build without linking to the C++ standard library")
    # This fails when a compiler emits a symbol which requires libgcc:
    #   list(APPEND BLEND2D_NO_STDCXX_CFLAGS -nodefaultlibs -DBL_BUILD_NO_STDCXX)
    #   list(APPEND BLEND2D_NO_STDCXX_LFLAGS -nodefaultlibs)
    # This has similar effect as we don't really use anything from libstdc++:
    list(APPEND BLEND2D_NO_STDCXX_CFLAGS -DBL_BUILD_NO_STDCXX)
    list(APPEND BLEND2D_NO_STDCXX_LFLAGS -static-libstdc++)
  endif()
endif()

# Support for sanitizers.
if (BLEND2D_SANITIZE)
  blend2d_detect_sanitizers(BLEND2D_SANITIZE_CFLAGS ${BLEND2D_SANITIZE})
  if (BLEND2D_SANITIZE_CFLAGS)
    message("-- Enabling sanitizers: '${BLEND2D_SANITIZE_CFLAGS}'")

    # Linker must receive the same flags as the compiler when it comes to sanitizers.
    set(BLEND2D_SANITIZE_LFLAGS ${BLEND2D_SANITIZE_CFLAGS})

    # Don't omit frame pointer if sanitizers are enabled.
    if ("x${CMAKE_CXX_COMPILER_ID}" STREQUAL "xMSVC" OR "x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC")
      list(APPEND BLEND2D_SANITIZE_CFLAGS -Oy-)
    else()
      list(APPEND BLEND2D_SANITIZE_CFLAGS -fno-omit-frame-pointer -g)
    endif()

    list(APPEND BLEND2D_PRIVATE_CFLAGS ${BLEND2D_SANITIZE_CFLAGS})
    list(APPEND BLEND2D_PRIVATE_LFLAGS ${BLEND2D_SANITIZE_LFLAGS})
  endif()
endif()

# Target type.
if (BLEND2D_EMBED)
  set(BLEND2D_TARGET_TYPE "EMBED")
elseif (BLEND2D_STATIC)
  set(BLEND2D_TARGET_TYPE "STATIC")
else()
  set(BLEND2D_TARGET_TYPE "SHARED")
endif()

if (BLEND2D_EMBED OR BLEND2D_STATIC)
  list(APPEND BLEND2D_CFLAGS "-DBL_STATIC")
  list(APPEND BLEND2D_PRIVATE_CFLAGS "-DBL_STATIC")
endif()

if (BLEND2D_NO_FUTEX)
  message("-- Disabling futex support (BL_BUILD_NO_FUTEX)")
  list(APPEND BLEND2D_PRIVATE_CFLAGS "-DBL_BUILD_NO_FUTEX")
endif()

if (BLEND2D_NO_JIT)
  message("-- Disabling JIT compiler support (BL_BUILD_NO_JIT)")
  list(APPEND BLEND2D_PRIVATE_CFLAGS "-DBL_BUILD_NO_JIT")
endif()

if (BLEND2D_NO_TLS)
  message("-- Disabling thread local storage support (BL_BUILD_NO_TLS)")
  list(APPEND BLEND2D_PRIVATE_CFLAGS "-DBL_BUILD_NO_TLS")
endif()

if (BLEND2D_NO_INTRINSICS)
  message("-- Disabling compiler intrinsics in C++ code (BL_BUILD_NO_INTRINSICS)")
  list(APPEND BLEND2D_PRIVATE_CFLAGS "-DBL_BUILD_NO_INTRINSICS")
endif()

# Blend2D - Linker Support
# ========================

if (WIN32)
  if(CMAKE_LINKER MATCHES "link\\.exe" OR CMAKE_LINKER MATCHES "lld-link\\.exe")
    set(BLEND2D_LINKER_SUPPORTS_NATVIS TRUE)
  endif()
endif()

# Blend2D - Enable SIMD
# =====================

# TODO: Detect ARM when the support is added.

if ("x${CMAKE_CXX_COMPILER_ID}" STREQUAL "xMSVC" OR "x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC")
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

# Blend2D - Dependencies
# ======================

if (NOT WIN32)
  if (HAIKU)
    list(APPEND BLEND2D_DEPS root m pthread)
  else()
    list(APPEND BLEND2D_DEPS c m pthread)
  endif()
  if ("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
    list(APPEND BLEND2D_DEPS rt)
  endif()
endif()

# Find asmjit dependency if building with JIT support.
if (NOT BLEND2D_NO_JIT)
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
  if (BLEND2D_NO_JIT_LOGGING)
    message("-- Disabling AsmJit logging functionality (PipeGen)")
    list(APPEND BLEND2D_PRIVATE_CFLAGS -DASMJIT_NO_LOGGING -DASMJIT_NO_TEXT)
  endif()
endif()

# Blend2D - Finalize Build Options
# ================================

list(REMOVE_DUPLICATES BLEND2D_DEPS)
list(REMOVE_DUPLICATES BLEND2D_PRIVATE_CFLAGS)

set(BLEND2D_LIBS ${BLEND2D_DEPS})
if (NOT BLEND2D_EMBED)
  list(INSERT BLEND2D_LIBS 0 blend2d)
endif()

# Blend2D - Source Files
# ======================

set(BLEND2D_SRC_LIST
  blend2d.h
  blend2d-debug.h
  blend2d-impl.h
  blend2d/api.h
  blend2d/api-build_p.h
  blend2d/api-globals.cpp
  blend2d/api-impl.h
  blend2d/api-internal_p.h
  blend2d/api-nocxx.cpp
  blend2d/array.cpp
  blend2d/array.h
  blend2d/array_p.h
  blend2d/bitset.cpp
  blend2d/bitset.h
  blend2d/bitset_p.h
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
  blend2d/fontdata.cpp
  blend2d/fontdata.h
  blend2d/fontdata_p.h
  blend2d/fontdefs.h
  blend2d/fontface.cpp
  blend2d/fontface.h
  blend2d/fontface_p.h
  blend2d/fontfeaturesettings.cpp
  blend2d/fontfeaturesettings.h
  blend2d/fontfeaturesettings_p.h
  blend2d/fontmanager.cpp
  blend2d/fontmanager.h
  blend2d/fontmanager_p.h
  blend2d/fonttags.cpp
  blend2d/fonttags_p.h
  blend2d/fontvariationsettings.cpp
  blend2d/fontvariationsettings.h
  blend2d/fontvariationsettings_p.h
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
  blend2d/gradient.h
  blend2d/gradient_p.h
  blend2d/image.cpp
  blend2d/image.h
  blend2d/image_p.h
  blend2d/imagecodec.cpp
  blend2d/imagecodec.h
  blend2d/imagedecoder.cpp
  blend2d/imagedecoder.h
  blend2d/imageencoder.cpp
  blend2d/imageencoder.h
  blend2d/imagescale.cpp
  blend2d/imagescale_p.h
  blend2d/math.cpp
  blend2d/math_p.h
  blend2d/matrix.cpp
  blend2d/matrix_avx.cpp
  blend2d/matrix_sse2.cpp
  blend2d/matrix.h
  blend2d/matrix_p.h
  blend2d/object.cpp
  blend2d/object.h
  blend2d/object_p.h
  blend2d/path.cpp
  blend2d/path.h
  blend2d/path_p.h
  blend2d/pathstroke.cpp
  blend2d/pathstroke_p.h
  blend2d/pattern.cpp
  blend2d/pattern.h
  blend2d/pattern_p.h
  blend2d/pixelconverter.cpp
  blend2d/pixelconverter_avx2.cpp
  blend2d/pixelconverter_sse2.cpp
  blend2d/pixelconverter_ssse3.cpp
  blend2d/pixelconverter.h
  blend2d/pixelconverter_p.h
  blend2d/random.cpp
  blend2d/random.h
  blend2d/random_p.h
  blend2d/rgba.cpp
  blend2d/rgba.h
  blend2d/runtime.cpp
  blend2d/runtime.h
  blend2d/runtime_p.h
  blend2d/simd_p.h
  blend2d/simd_x86_p.h
  blend2d/string.cpp
  blend2d/string.h
  blend2d/string_p.h
  blend2d/tables.cpp
  blend2d/tables_p.h
  blend2d/trace.cpp
  blend2d/trace_p.h
  blend2d/unicode.cpp
  blend2d/unicode_p.h
  blend2d/var.cpp
  blend2d/var.h
  blend2d/var_p.h
  blend2d/zeroallocator.cpp
  blend2d/zeroallocator_p.h

  blend2d/codec/bmpcodec.cpp
  blend2d/codec/bmpcodec_p.h
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

  blend2d/compression/checksum.cpp
  blend2d/compression/checksum_p.h
  blend2d/compression/deflatedecoder.cpp
  blend2d/compression/deflatedecoder_p.h
  blend2d/compression/deflatedefs_p.h
  blend2d/compression/deflateencoder.cpp
  blend2d/compression/deflateencoder_p.h
  blend2d/compression/matchfinder_p.h

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
  blend2d/opentype/otglyf_avx2.cpp
  blend2d/opentype/otglyf_sse4_2.cpp
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

  blend2d/pipeline/pipedefs.cpp
  blend2d/pipeline/pipedefs_p.h
  blend2d/pipeline/piperuntime.cpp
  blend2d/pipeline/piperuntime_p.h

  blend2d/pipeline/jit/compoppart.cpp
  blend2d/pipeline/jit/compoppart_p.h
  blend2d/pipeline/jit/fetchgradientpart.cpp
  blend2d/pipeline/jit/fetchgradientpart_p.h
  blend2d/pipeline/jit/fetchpart.cpp
  blend2d/pipeline/jit/fetchpart_p.h
  blend2d/pipeline/jit/fetchpatternpart.cpp
  blend2d/pipeline/jit/fetchpatternpart_p.h
  blend2d/pipeline/jit/fetchpixelptrpart.cpp
  blend2d/pipeline/jit/fetchpixelptrpart_p.h
  blend2d/pipeline/jit/fetchsolidpart.cpp
  blend2d/pipeline/jit/fetchsolidpart_p.h
  blend2d/pipeline/jit/fetchutils.cpp
  blend2d/pipeline/jit/fetchutils_p.h
  blend2d/pipeline/jit/fillpart.cpp
  blend2d/pipeline/jit/fillpart_p.h
  blend2d/pipeline/jit/jitbase_p.h
  blend2d/pipeline/jit/pipecompiler.cpp
  blend2d/pipeline/jit/pipecompiler_p.h
  blend2d/pipeline/jit/pipedebug_p.h
  blend2d/pipeline/jit/pipegencore.cpp
  blend2d/pipeline/jit/pipegencore_p.h
  blend2d/pipeline/jit/pipegenruntime_p.h
  blend2d/pipeline/jit/pipegenruntime.cpp
  blend2d/pipeline/jit/pipepart.cpp
  blend2d/pipeline/jit/pipepart_p.h

  blend2d/pipeline/reference/compopgeneric_p.h
  blend2d/pipeline/reference/fetchgeneric_p.h
  blend2d/pipeline/reference/fillgeneric_p.h
  blend2d/pipeline/reference/fixedpiperuntime_p.h
  blend2d/pipeline/reference/fixedpiperuntime.cpp
  blend2d/pipeline/reference/pixelbufferptr_p.h
  blend2d/pipeline/reference/pixelgeneric_p.h

  blend2d/pixelops/interpolation.cpp
  blend2d/pixelops/interpolation_avx2.cpp
  blend2d/pixelops/interpolation_sse2.cpp
  blend2d/pixelops/funcs.cpp
  blend2d/pixelops/funcs_p.h
  blend2d/pixelops/scalar.cpp
  blend2d/pixelops/scalar_p.h

  blend2d/raster/analyticrasterizer.cpp
  blend2d/raster/analyticrasterizer_p.h
  blend2d/raster/debugging_p.h
  blend2d/raster/edgebuilder_p.h
  blend2d/raster/edgestorage_p.h
  blend2d/raster/rastercontext.cpp
  blend2d/raster/rastercontext_p.h
  blend2d/raster/rastercontextops.cpp
  blend2d/raster/rastercontextops_p.h
  blend2d/raster/rasterdefs_p.h
  blend2d/raster/renderbatch_p.h
  blend2d/raster/rendercommand_p.h
  blend2d/raster/rendercommandprocasync_p.h
  blend2d/raster/rendercommandprocsync_p.h
  blend2d/raster/rendercommandserializer_p.h
  blend2d/raster/renderfetchdata.cpp
  blend2d/raster/renderfetchdata_p.h
  blend2d/raster/renderjob_p.h
  blend2d/raster/renderjobproc_p.h
  blend2d/raster/renderqueue_p.h
  blend2d/raster/rendertargetinfo.cpp
  blend2d/raster/rendertargetinfo_p.h
  blend2d/raster/statedata_p.h
  blend2d/raster/styledata_p.h
  blend2d/raster/workdata.cpp
  blend2d/raster/workdata_p.h
  blend2d/raster/workermanager.cpp
  blend2d/raster/workermanager_p.h
  blend2d/raster/workerproc.cpp
  blend2d/raster/workerproc_p.h
  blend2d/raster/workersynchronization.cpp
  blend2d/raster/workersynchronization_p.h

  blend2d/support/algorithm.cpp
  blend2d/support/algorithm_p.h
  blend2d/support/arenaallocator.cpp
  blend2d/support/arenaallocator_p.h
  blend2d/support/arenahashmap.cpp
  blend2d/support/arenahashmap_p.h
  blend2d/support/arenalist.cpp
  blend2d/support/arenalist_p.h
  blend2d/support/arenatree.cpp
  blend2d/support/arenatree_p.h
  blend2d/support/bitops.cpp
  blend2d/support/bitops_p.h
  blend2d/support/fixedbitarray_p.h
  blend2d/support/hashops_p.h
  blend2d/support/intops.cpp
  blend2d/support/intops_p.h
  blend2d/support/lookuptable_p.h
  blend2d/support/memops.cpp
  blend2d/support/memops_p.h
  blend2d/support/scopedallocator.cpp
  blend2d/support/scopedallocator_p.h
  blend2d/support/scopedbuffer_p.h
  blend2d/support/stringops_p.h
  blend2d/support/wrap_p.h

  blend2d/threading/atomic_p.h
  blend2d/threading/conditionvariable_p.h
  blend2d/threading/futex.cpp
  blend2d/threading/futex_p.h
  blend2d/threading/mutex_p.h
  blend2d/threading/thread.cpp
  blend2d/threading/thread_p.h
  blend2d/threading/threadingutils_p.h
  blend2d/threading/threadpool.cpp
  blend2d/threading/threadpool_p.h
  blend2d/threading/uniqueidgenerator.cpp
  blend2d/threading/uniqueidgenerator_p.h
)

if (MSVC AND NOT BLEND2D_NO_NATVIS)
  list(APPEND BLEND2D_SRC_LIST blend2d.natvis)
endif()

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

  if ("${src_file}" MATCHES "\\.natvis")
    if (BLEND2D_LINKER_SUPPORTS_NATVIS)
      list(APPEND BLEND2D_PRIVATE_LFLAGS "-natvis:${src_file}")
    endif()
  endif()
endforeach()

source_group(TREE "${BLEND2D_DIR}" FILES ${BLEND2D_SRC})

set(BLEND2D_ALL_SRC_FILES ${BLEND2D_SRC})
if (ASMJIT_EMBED)
  list(APPEND BLEND2D_ALL_SRC_FILES ${ASMJIT_SRC})
endif()

# Blend2D - Summary
# =================

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

# Blend2D - Targets
# =================

if (NOT BLEND2D_EMBED)
  # Add 'blend2d' library target.
  blend2d_add_target(blend2d    ${BLEND2D_TARGET_TYPE}
                     SOURCES    ${BLEND2D_ALL_SRC_FILES}
                     LIBRARIES  ${BLEND2D_DEPS}
                     CFLAGS     ${BLEND2D_PRIVATE_CFLAGS}
                     CFLAGS_DBG ${BLEND2D_PRIVATE_CFLAGS_DBG}
                     CFLAGS_REL ${BLEND2D_PRIVATE_CFLAGS_REL})

  target_compile_options(blend2d PRIVATE ${BLEND2D_NO_STDCXX_CFLAGS})
  target_compile_options(blend2d INTERFACE ${BLEND2D_CFLAGS})

  target_include_directories(blend2d BEFORE PRIVATE ${ASMJIT_INCLUDE_DIRS})
  target_include_directories(blend2d BEFORE INTERFACE
                             $<BUILD_INTERFACE:${BLEND2D_INCLUDE_DIRS}>
                             $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

  # target_link_options was added in cmake 3.13, which doesn't work for us.
  # target_link_options(blend2d PRIVATE ${BLEND2D_NO_STDCXX_LFLAGS})
  foreach(link_flag ${BLEND2D_NO_STDCXX_LFLAGS})
    set_property(TARGET blend2d APPEND_STRING PROPERTY LINK_FLAGS " ${link_flag}")
  endforeach()

  # Add blend2d::blend2d alias.
  add_library(blend2d::blend2d ALIAS blend2d)
  # TODO: [CMAKE] Deprecated alias - we use projectname::libraryname convention now.
  add_library(Blend2D::Blend2D ALIAS blend2d)

  # Add Blend2D install instructions (library and public headers).
  if (NOT BLEND2D_NO_INSTALL)
    install(TARGETS blend2d
            EXPORT blend2d-config
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(EXPORT blend2d-config
            NAMESPACE blend2d::
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/blend2d")

    foreach(_src_file ${BLEND2D_SRC_LIST})
      if ("${_src_file}" MATCHES "\\.h$" AND NOT "${_src_file}" MATCHES "_p\\.h$")
        get_filename_component(_src_dir ${_src_file} PATH)
        install(FILES "${BLEND2D_DIR}/src/${_src_file}" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${_src_dir}")
      endif()
    endforeach()
  endif()

  # Add 'blend2d' tests.
  if (BLEND2D_TEST)
    enable_testing()

    # Blend2D Tests
    # -------------

    # Special target that always uses embedded Blend2D.
    blend2d_add_target(bl_test_unit TEST
                       SOURCES    ${BLEND2D_ALL_SRC_FILES}
                                  test/bl_test_unit.cpp
                                  test/broken.cpp
                                  test/broken.h
                       LIBRARIES  ${BLEND2D_DEPS}
                       CFLAGS     ${BLEND2D_PRIVATE_CFLAGS}
                                  -DBL_TEST
                                  -DBL_STATIC
                       CFLAGS_DBG ${BLEND2D_PRIVATE_CFLAGS_DBG}
                       CFLAGS_REL ${BLEND2D_PRIVATE_CFLAGS_REL})
    target_include_directories(bl_test_unit BEFORE PRIVATE ${ASMJIT_INCLUDE_DIRS})
    target_include_directories(bl_test_unit BEFORE PRIVATE ${BLEND2D_INCLUDE_DIRS})

    blend2d_add_target(bl_test_fuzzer TEST
                       SOURCES test/bl_test_fuzzer.cpp
                       LIBRARIES blend2d::blend2d
                       CFLAGS "${BLEND2D_SANITIZE_CFLAGS}")

    blend2d_add_target(bl_test_verify_mt TEST
                       SOURCES test/bl_test_verify_mt.cpp
                       LIBRARIES blend2d::blend2d
                       CFLAGS "${BLEND2D_SANITIZE_CFLAGS}")

    # Blend2D C & C++ Samples
    # -----------------------

    set(BLEND2D_SAMPLES_CXX
      "bl_sample_1"
      "bl_sample_2"
      "bl_sample_3"
      "bl_sample_4"
      "bl_sample_5"
      "bl_sample_6"
      "bl_sample_7"
      "bl_sample_8"
    )

    set(BLEND2D_SAMPLES_CAPI
      "bl_sample_capi"
    )

    set(BLEND2D_SAMPLES_RESOURCES
      "ABeeZee-Regular.ttf"
      "Leaves.jpeg"
    )

    foreach(sample ${BLEND2D_SAMPLES_CXX})
      blend2d_add_target(${sample} EXECUTABLE
                         SOURCES test/${sample}.cpp
                         LIBRARIES blend2d::blend2d
                         CFLAGS "${BLEND2D_SANITIZE_CFLAGS}")
    endforeach()

    # NOTE: It's possible these samples would link to C++ standard library as well.
    # Since this is just examples we won't do anything here that could break some
    # setups.
    foreach(sample ${BLEND2D_SAMPLES_CAPI})
      blend2d_add_target(${sample} EXECUTABLE
                         SOURCES test/${sample}.c
                         LIBRARIES blend2d::blend2d
                         CFLAGS "${BLEND2D_SANITIZE_CFLAGS}")

      # Sanitizers require C++ linker (C++ sanitizer needs both C++ standard library and C++ sanitizer library support).
      if (BLEND2D_SANITIZE_CFLAGS)
        set_target_properties(${sample} PROPERTIES LINKER_LANGUAGE CXX)
      endif()

    endforeach()

    # TODO: Not sure this is the best way of copying resources.
    foreach(resource ${BLEND2D_SAMPLES_RESOURCES})
      if (NOT CMAKE_BUILD_TYPE)
        add_custom_command(TARGET blend2d POST_BUILD
          COMMAND "${CMAKE_COMMAND}" -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/test/resources/${resource}"
            "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/${resource}"
          COMMENT "Resources")
      else()
        add_custom_command(TARGET blend2d POST_BUILD
          COMMAND "${CMAKE_COMMAND}" -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/test/resources/${resource}"
            "${CMAKE_CURRENT_BINARY_DIR}/${resource}"
          COMMENT "Resources")
      endif()
    endforeach()

  endif()
endif()

cmake_policy(POP)
