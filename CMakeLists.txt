cmake_minimum_required(VERSION 3.1)
include(CheckCXXCompilerFlag)

# Don't create a project if it was already created by another CMakeLists.txt.
# This allows one library to embed another library without a project collision.
if(NOT CMAKE_PROJECT_NAME OR "${CMAKE_PROJECT_NAME}" STREQUAL "blend2d")
  project(blend2d CXX)
endif()

if (NOT DEFINED BLEND2D_BUILD_EMBED)
  set(BLEND2D_BUILD_EMBED FALSE)
endif()

if (NOT DEFINED BLEND2D_BUILD_STATIC)
  set(BLEND2D_BUILD_STATIC ${BLEND2D_BUILD_EMBED})
endif()

if (NOT DEFINED BLEND2D_BUILD_TEST)
  set(BLEND2D_BUILD_TEST FALSE)
endif()

# =============================================================================
# [Blend2D - Configuration]
# =============================================================================

set(BLEND2D_DIR          "${CMAKE_CURRENT_LIST_DIR}"  CACHE PATH "Location of 'blend2d'")
set(BLEND2D_BUILD_EMBED  "${BLEND2D_BUILD_EMBED}"     CACHE BOOL "Embed 'blend2d' library (no targets)")
set(BLEND2D_BUILD_STATIC "${BLEND2D_BUILD_STATIC}"    CACHE BOOL "Build 'blend2d' library as static")
set(BLEND2D_BUILD_TEST   "${BLEND2D_BUILD_TEST}"      CACHE BOOL "Build 'blend2d' test applications")

# Experimental, will be used in the future by default.
# set(BLEND2D_BUILD_NO_CXX_LIB 1)
# set(BLEND2D_BUILD_NO_LOGGING 1)

# Find asmjit dependency.
if(NOT DEFINED ASMJIT_DIR)
  set(ASMJIT_LOOKUP_DIRS
    "${BLEND2D_DIR}/3rdparty/asmjit"
    "${CMAKE_CURRENT_LIST_DIR}/../asmjit"
  )
  foreach(dir ${ASMJIT_LOOKUP_DIRS})
    if(EXISTS ${dir}/CMakeLists.txt)
      set(ASMJIT_DIR "${dir}" CACHE PATH "Location of 'asmjit'")
      break()
    endif()
  endforeach()
  if(NOT DEFINED ASMJIT_DIR)
    message(FATAL "Unable to find asmjit, cannot continue...")
    message(FATAL "Please visit https://blend2d.com/download.html")
  endif()
endif()

# =============================================================================
# [Blend2D - Utilities]
# =============================================================================

set(BLEND2D_MAKE_DEF "-D") # Define a preprocessor macro: "${BLEND2D_MAKE_DEF}VAR=1"
set(BLEND2D_MAKE_INC "-I") # Define an include directory: "${BLEND2D_MAKE_INC}PATH"

if(("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel" AND WIN32))
  set(BLEND2D_MAKE_DEF "/D")
  set(BLEND2D_MAKE_INC "/I")
endif()

# Detects C++ flags and appends all detected ones to `out`.
function(blend2d_detect_cflags out)
  set(out_array ${${out}})

  list(GET ARGN 0 FIRST)
  if("${FIRST}" STREQUAL "FIRST")
    list(REMOVE_AT ARGN 0)
  endif()

  foreach(flag ${ARGN})
    string(REGEX REPLACE "[+]" "x" flag_signature "${flag}")
    string(REGEX REPLACE "[-=:;/.\]" "_" flag_signature "${flag_signature}")
    check_cxx_compiler_flag(${flag} "__CxxFlag_${flag_signature}")
    if(${__CxxFlag_${flag_signature}})
      list(APPEND out_array "${flag}")
      if("${FIRST}" STREQUAL "FIRST")
        break()
      endif()
    endif()
  endforeach()

  set(${out} "${out_array}" PARENT_SCOPE)
endfunction()

# Appends source files to `out` by prepending `src_dir` to each of them and by
# adding architecture specific flags to files that require them (sse|avx|neon).
function(blend2d_add_source out src_dir)
  set(src_path "${BLEND2D_SOURCE_DIR}/${src_dir}")
  set(src_array)

  foreach(file ${ARGN})
    set(src_file "${src_path}/${file}")
    list(APPEND src_array ${src_file})

    string(REGEX MATCH "_(sse[2|3]?|ssse3|sse4_[1|2]|avx|avx[2]?)\\.(c|cc|cxx|cpp|m|mm)$" FEATURE ${file})
    if(FEATURE)
      # HACK 1: Cmake uses global variables everywhere, `CMAKE_MATCH_1` is the first capture...
      string(TOUPPER "${CMAKE_MATCH_1}" FEATURE)
      # HACK 2: Setting `COMPILE_FLAGS` property cannot be used when your input is LIST.
      foreach(src_cflag ${BLEND2D_CFLAGS_${FEATURE}})
        set_property(SOURCE "${src_file}" APPEND_STRING PROPERTY COMPILE_FLAGS " ${src_cflag}")
      endforeach()
    endif()
  endforeach()
  source_group(${src_dir} FILES ${src_array})

  set(out_tmp ${${out}})
  list(APPEND out_tmp ${src_array})
  set("${out}" "${out_tmp}" PARENT_SCOPE)
endfunction()

function(blend2d_add_target target target_type src deps cflags cflags_dbg cflags_rel)
  if("${target_type}" STREQUAL "EXECUTABLE")
    add_executable(${target} ${src})
  else()
    add_library(${target} ${target_type} ${src})
  endif()

  target_link_libraries(${target} ${deps})
  foreach(link_flag ${BLEND2D_LFLAGS} ${BLEND2D_PRIVATE_LFLAGS})
    set_property(TARGET ${target} APPEND_STRING PROPERTY LINK_FLAGS " ${link_flag}")
  endforeach()

  if(CMAKE_BUILD_TYPE)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
      target_compile_options(${target} PRIVATE ${cflags} ${cflags_dbg})
    else()
      target_compile_options(${target} PRIVATE ${cflags} ${cflags_rel})
    endif()
  else()
    target_compile_options(${target} PRIVATE ${cflags} $<$<CONFIG:Debug>:${cflags_dbg}> $<$<NOT:$<CONFIG:Debug>>:${cflags_rel}>)
  endif()

  if(NOT BLEND2D_BUILD_STATIC)
    install(TARGETS ${target} RUNTIME DESTINATION "bin"
                              LIBRARY DESTINATION "lib${LIB_SUFFIX}"
                              ARCHIVE DESTINATION "lib${LIB_SUFFIX}")
  endif()
endfunction()

# =============================================================================
# [Blend2D - Project]
# =============================================================================

set(BLEND2D_SOURCE_DIR "${BLEND2D_DIR}/src")     # Deduce source directory
set(BLEND2D_INCLUDE_DIR "${BLEND2D_SOURCE_DIR}") # Include directory is the same as source dir.
set(BLEND2D_DEPS "")                             # Blend2D dependencies (libraries) for the linker.
set(BLEND2D_LIBS "")                             # Dependencies of libs/apps that want to use Blend2D.
set(BLEND2D_CFLAGS "")                           # Public compiler flags.
set(BLEND2D_LFLAGS "")                           # Public linker flags.
set(BLEND2D_CXX_STD "")                          # Flags that turn on C++[11|14|17] standard.
set(BLEND2D_PRIVATE_LFLAGS "")                   # Private linker flags.
set(BLEND2D_PRIVATE_CFLAGS "")                   # Private compiler flags independent of build type.
set(BLEND2D_PRIVATE_CFLAGS_DBG "")               # Private compiler flags used by debug builds.
set(BLEND2D_PRIVATE_CFLAGS_REL "")               # Private compiler flags used by release builds.

if(BLEND2D_BUILD_EMBED)
  set(BLEND2D_BUILD_STATIC TRUE)                 # EMBED implies STATIC.
else()
  if(BLEND2D_BUILD_STATIC)
    set(BLEND2D_TARGET_TYPE "STATIC")
  else()
    set(BLEND2D_TARGET_TYPE "SHARED")
  endif()
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  blend2d_detect_cflags(BLEND2D_CXX_STD FIRST "/std:c++latest" "/std:c++14")

  set(BLEND2D_PRIVATE_LFLAGS "/OPT:REF /OPT:ICF")

  list(APPEND BLEND2D_PRIVATE_CFLAGS
    ${BLEND2D_CXX_STD}
    /MP   # [+] Multi-process compilation
    /GF   # [+] Eliminate duplicate strings.
    /GR-) # [-] Runtime type information.

  list(APPEND BLEND2D_PRIVATE_CFLAGS_DBG
    /GS)  # [+] Buffer security-check.

  list(APPEND BLEND2D_PRIVATE_CFLAGS_REL
    /GS-  # [-] Buffer security-check.
    /Ox)  # [+] Global optimizations.
endif()

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "^(GNU|Clang|AppleClang)$")
  blend2d_detect_cflags(BLEND2D_CXX_STD FIRST "-std=c++17" "-std=c++14" "-std=c++11")

  list(APPEND BLEND2D_PRIVATE_CFLAGS
    ${BLEND2D_CXX_STD}
    "-fvisibility=hidden"
    "-fno-exceptions"
    "-fno-rtti"
    "-fno-math-errno"
    "-fno-threadsafe-statics")

  list(APPEND BLEND2D_PRIVATE_CFLAGS_REL
    "-O2")

  blend2d_detect_cflags(BLEND2D_PRIVATE_CFLAGS_REL
    "-fmerge-all-constants" # Not enabled by default, but we are fine with it.
    "-ftree-vectorize")     # We want this optimization if not enabled by default.

  # Support for GCC|Clang undefined behavior sanitizers.
  if(BLEND2D_BUILD_SANITIZE)
    blend2d_detect_cflags(BLEND2D_SANITIZE_FLAGS "-fsanitize=undefined")
    if (BLEND2D_SANITIZE_FLAGS)
      message("-- Enabling UB sanitizer via '${BLEND2D_SANITIZE_FLAGS}'")
      # TODO: I don't undertand why this is necessary, it's not nice.
      list(APPEND BLEND2D_CFLAGS ${BLEND2D_SANITIZE_FLAGS})
      list(APPEND BLEND2D_LFLAGS ${BLEND2D_SANITIZE_FLAGS})

      list(APPEND BLEND2D_PRIVATE_CFLAGS ${BLEND2D_SANITIZE_FLAGS})
      list(APPEND BLEND2D_PRIVATE_LFLAGS ${BLEND2D_SANITIZE_FLAGS})
    endif()
  endif()

  # Building Blend2D without C++ library support requires these to be setup.
  if(BLEND2D_BUILD_NO_CXX_LIB)
    message("-- Enabling build without linking to C++ standard library")
    list(APPEND BLEND2D_PRIVATE_LFLAGS -nodefaultlibs)
    list(APPEND BLEND2D_PRIVATE_CFLAGS -DBL_BUILD_NO_CXX_LIB -nodefaultlibs)
  endif()
endif()

if(BLEND2D_BUILD_EMBED)
  list(APPEND BLEND2D_CFLAGS         "${BLEND2D_MAKE_DEF}BL_BUILD_EMBED")
  list(APPEND BLEND2D_PRIVATE_CFLAGS "${BLEND2D_MAKE_DEF}BL_BUILD_EMBED")
endif()

if(BLEND2D_BUILD_STATIC)
  list(APPEND BLEND2D_CFLAGS         "${BLEND2D_MAKE_DEF}BL_BUILD_STATIC")
  list(APPEND BLEND2D_PRIVATE_CFLAGS "${BLEND2D_MAKE_DEF}BL_BUILD_STATIC")
endif()

# =============================================================================
# [Blend2D - Enable SIMD]
# =============================================================================

# TODO: Detect ARM when the support is added.

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  # AVX/AVX2 doesn't need custom defs as MSVC does define __AVX[2]__
  # similary to other compilers. In addition, we only detect the support
  # for AVX/AVX2 as if these are available all previous instruction sets
  # are also available. If such check fails it means that we are either
  # not compiling for X86/X64 or the compiler is very old, which cannot
  # be used anyway.
  blend2d_detect_cflags(BLEND2D_CFLAGS_AVX "/arch:AVX")
  blend2d_detect_cflags(BLEND2D_CFLAGS_AVX2 "/arch:AVX2")

  if(BLEND2D_CFLAGS_AVX2)
    # 64-bit MSVC compiler doesn't like /arch:SSE[2] as it's implicit.
    if(NOT CMAKE_CL_64)
      list(APPEND BLEND2D_CFLAGS_SSE2 "/arch:SSE2")
      list(APPEND BLEND2D_CFLAGS_SSE3 "/arch:SSE2")
      list(APPEND BLEND2D_CFLAGS_SSSE3 "/arch:SSE2")
      list(APPEND BLEND2D_CFLAGS_SSE4_1 "/arch:SSE2")
      list(APPEND BLEND2D_CFLAGS_SSE4_2 "/arch:SSE2")
    endif()
    # MSVC doesn't provide any preprocessor definitions for SSE3 and higher, thus
    # we have to define these ourselves to match these defined by Intel|Clang|GCC.
    list(APPEND BLEND2D_CFLAGS_SSE2 "/D__SSE2__")
    list(APPEND BLEND2D_CFLAGS_SSE3 "/D__SSE3__")
    list(APPEND BLEND2D_CFLAGS_SSSE3 "/D__SSSE3__")
    list(APPEND BLEND2D_CFLAGS_SSE4_1 "/D__SSE4_1__")
    list(APPEND BLEND2D_CFLAGS_SSE4_2 "/D__SSE4_2__")
  endif()
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel" AND WIN32)
  # Intel deprecated /arch:SSE, so it's implicit. In contrast to MSVC, Intel
  # also provides /arch:SSE3+ options and uses the same definitions as GCC
  # and Clang, so no magic needed here.
  blend2d_detect_cflags(BLEND2D_CFLAGS_AVX "/arch:AVX")
  blend2d_detect_cflags(BLEND2D_CFLAGS_AVX2 "/arch:AVX2")
  if(BLEND2D_CFLAGS_AVX)
    list(APPEND BLEND2D_CFLAGS_SSE2 "/arch:SSE2")
    list(APPEND BLEND2D_CFLAGS_SSE3 "/arch:SSE3")
    list(APPEND BLEND2D_CFLAGS_SSSE3 "/arch:SSSE3")
    list(APPEND BLEND2D_CFLAGS_SSE4_1 "/arch:SSE4.1")
    list(APPEND BLEND2D_CFLAGS_SSE4_2 "/arch:SSE4.2")
  endif()
else()
  # Assume all other compilers are compatible with GCC|Clang.
  blend2d_detect_cflags(BLEND2D_CFLAGS_AVX "-mavx")
  blend2d_detect_cflags(BLEND2D_CFLAGS_AVX2 "-mavx2")
  if(BLEND2D_CFLAGS_AVX)
    list(APPEND BLEND2D_CFLAGS_SSE2 "-msse2")
    list(APPEND BLEND2D_CFLAGS_SSE3 "-msse3")
    list(APPEND BLEND2D_CFLAGS_SSSE3 "-mssse3")
    list(APPEND BLEND2D_CFLAGS_SSE4_1 "-msse4.1")
    list(APPEND BLEND2D_CFLAGS_SSE4_2 "-msse4.2")
  endif()
endif()

# Do not make this more complicated than it is. We assume that compiler can
# handle either all (SSE2, SSE3, ... AVX) or nothing. We require C++11 so
# this should exclude all old compilers where this assumption would not hold.
if(BLEND2D_CFLAGS_AVX)
  # Use SSE2 by default on X86.
  list(APPEND BLEND2D_PRIVATE_CFLAGS ${BLEND2D_CFLAGS_SSE2})

  list(APPEND BLEND2D_PRIVATE_CFLAGS "${BLEND2D_MAKE_DEF}BL_BUILD_OPT_SSE2")
  list(APPEND BLEND2D_PRIVATE_CFLAGS "${BLEND2D_MAKE_DEF}BL_BUILD_OPT_SSE3")
  list(APPEND BLEND2D_PRIVATE_CFLAGS "${BLEND2D_MAKE_DEF}BL_BUILD_OPT_SSSE3")
  list(APPEND BLEND2D_PRIVATE_CFLAGS "${BLEND2D_MAKE_DEF}BL_BUILD_OPT_SSE4_1")
  list(APPEND BLEND2D_PRIVATE_CFLAGS "${BLEND2D_MAKE_DEF}BL_BUILD_OPT_SSE4_2")
  list(APPEND BLEND2D_PRIVATE_CFLAGS "${BLEND2D_MAKE_DEF}BL_BUILD_OPT_AVX")
endif()
if(BLEND2D_CFLAGS_AVX2)
  list(APPEND BLEND2D_PRIVATE_CFLAGS "${BLEND2D_MAKE_DEF}BL_BUILD_OPT_AVX2")
endif()

# =============================================================================
# [Blend2D - Dependencies and OS Settings]
# =============================================================================

if(WIN32)
  list(APPEND BLEND2D_DEPS user32 uuid shell32)
  # Use Unicode by default on Windows target.
  list(APPEND BLEND2D_PRIVATE_CFLAGS "${BLEND2D_MAKE_DEF}_UNICODE")
else()
  list(APPEND BLEND2D_DEPS c m pthread)
endif()

# =============================================================================
# [Blend2D - AsmJit Embedding]
# =============================================================================

if(NOT DEFINED ASMJIT_BUILD_EMBED)
  set(ASMJIT_BUILD_EMBED TRUE CACHE BOOL "")
endif()

include("${ASMJIT_DIR}/CMakeLists.txt")
list(APPEND BLEND2D_DEPS ${ASMJIT_LIBS})
list(APPEND BLEND2D_PRIVATE_CFLAGS ${ASMJIT_CFLAGS})

# A possibility to reduce the resulting binary size by disabling asmjit logging.
if(BLEND2D_BUILD_NO_LOGGING)
  message("   Disabling AsmJit logging functionality to make Blend2D smaller")
  list(APPEND BLEND2D_PRIVATE_CFLAGS -DASMJIT_DISABLE_LOGGING -DASMJIT_DISABLE_TEXT)
endif()

# =============================================================================
# [Blend2D - Finalize Build Options]
# =============================================================================

list(REMOVE_DUPLICATES BLEND2D_DEPS)
list(REMOVE_DUPLICATES BLEND2D_PRIVATE_CFLAGS)

set(BLEND2D_LIBS ${BLEND2D_DEPS})
if(NOT BLEND2D_BUILD_EMBED)
  list(INSERT BLEND2D_LIBS 0 blend2d)
endif()

# =============================================================================
# [Blend2D - Source Files]
# =============================================================================

set(BLEND2D_SRC "")

blend2d_add_source(BLEND2D_SRC .
  blend2d.h
  blend2d-impl.h
)

blend2d_add_source(BLEND2D_SRC blend2d
  blapi.h
  blapi-build_p.h
  blapi-impl.h
  blapi-internal_p.h
  blapi-nocxx.cpp
  blarray.cpp
  blarray.h
  blarray_p.h
  blbitarray.cpp
  blbitarray.h
  blbitarray_p.h
  blcompop.cpp
  blcompop_p.h
  blcontext.cpp
  blcontext.h
  blcontext_p.h
  blfilesystem.cpp
  blfilesystem.h
  blfilesystem_p.h
  blfont.cpp
  blfont.h
  blfont_p.h
  blfontdefs.h
  blformat.cpp
  blformat.h
  blformat_p.h
  blgeometry.cpp
  blgeometry.h
  blgeometry_p.h
  blglyphbuffer.cpp
  blglyphbuffer.h
  blglyphbuffer_p.h
  blgradient.cpp
  blgradient_avx2.cpp
  blgradient_sse2.cpp
  blgradient.h
  blgradient_p.h
  blimage.cpp
  blimage.h
  blimage_p.h
  blimagescale.cpp
  blimagescale_p.h
  blmath.cpp
  blmath_p.h
  blmatrix.cpp
  blmatrix_avx.cpp
  blmatrix_sse2.cpp
  blmatrix.h
  blmatrix_p.h
  blpath.cpp
  blpath.h
  blpath_p.h
  blpathstroke.cpp
  blpathstroke_p.h
  blpattern.cpp
  blpattern.h
  blpattern_p.h
  blpipe.cpp
  blpipe_p.h
  blpixelconverter.cpp
  blpixelconverter_avx2.cpp
  blpixelconverter_sse2.cpp
  blpixelconverter_ssse3.cpp
  blpixelconverter.h
  blpixelconverter_p.h
  blpixelops.cpp
  blpixelops_p.h
  blrandom.cpp
  blrandom.h
  blrandom_p.h
  blregion.cpp
  blregion.h
  blregion_p.h
  blrgba.cpp
  blrgba.h
  blruntime.cpp
  blruntime.h
  blruntime_p.h
  blsimd_p.h
  blsimd_x86_p.h
  blstring.cpp
  blstring.h
  blstring_p.h
  blsupport.cpp
  blsupport_p.h
  bltables.cpp
  bltables_p.h
  blthreading.cpp
  blthreading_p.h
  bltrace.cpp
  bltrace_p.h
  blunicode.cpp
  blunicode_p.h
  blvariant.cpp
  blvariant.h
  blvariant_p.h
  blzeroallocator.cpp
  blzeroallocator_p.h
  blzoneallocator.cpp
  blzoneallocator_p.h
  blzonelist.cpp
  blzonelist_p.h
  blzonetree.cpp
  blzonetree_p.h
)

blend2d_add_source(BLEND2D_SRC blend2d/codec
  blbmpcodec.cpp
  blbmpcodec_p.h
  bldeflate.cpp
  bldeflate_p.h
  bljpegcodec.cpp
  bljpegcodec_p.h
  bljpeghuffman.cpp
  bljpeghuffman_p.h
  bljpegops.cpp
  bljpegops_sse2.cpp
  bljpegops_p.h
  blpngcodec.cpp
  blpngcodec_p.h
  blpngops.cpp
  blpngops_sse2.cpp
  blpngops_p.h
)

blend2d_add_source(BLEND2D_SRC blend2d/opentype
  blotcff.cpp
  blotcff_p.h
  blotcmap.cpp
  blotcmap_p.h
  blotcore.cpp
  blotcore_p.h
  blotdefs_p.h
  blotface.cpp
  blotface_p.h
  blotglyf.cpp
  blotglyf_p.h
  blotkern.cpp
  blotkern_p.h
  blotlayout.cpp
  blotlayout_p.h
  blotmetrics.cpp
  blotmetrics_p.h
  blotname.cpp
  blotname_p.h
  blotplatform_p.h
)

blend2d_add_source(BLEND2D_SRC blend2d/pipegen
  blcompoppart.cpp
  blcompoppart_p.h
  blfetchgradientpart.cpp
  blfetchgradientpart_p.h
  blfetchpart.cpp
  blfetchpart_p.h
  blfetchpatternpart.cpp
  blfetchpatternpart_p.h
  blfetchpixelptrpart.cpp
  blfetchpixelptrpart_p.h
  blfetchsolidpart.cpp
  blfetchsolidpart_p.h
  blfetchutils.cpp
  blfetchutils_p.h
  blfillpart.cpp
  blfillpart_p.h
  blpipecompiler.cpp
  blpipecompiler_p.h
  blpipegencore.cpp
  blpipegencore_p.h
  blpipedebug_p.h
  blpipepart.cpp
  blpipepart_p.h
  blpiperegusage_p.h
  blpiperuntime_p.h
  blpiperuntime.cpp
)

blend2d_add_source(BLEND2D_SRC blend2d/raster
  blanalyticrasterizer_p.h
  bledgebuilder_p.h
  blrastercontext.cpp
  blrastercontext_p.h
  blrasterfiller.cpp
  blrasterfiller_p.h
  blrasterdefs_p.h
  blrasterworker.cpp
  blrasterworker_p.h
)
#if(MSVC)
#  blend2d_add_source(BLEND2D_SRC blend2d blend2d.natvis)
#endif()

# =============================================================================
# [Blend2D - Info]
# =============================================================================

message("-- [blend2d]")

if(BLEND2D_BUILD_EMBED)
  message("   BuildMode=Embed")
elseif(BLEND2D_BUILD_STATIC)
  message("   BuildMode=Static")
else()
  message("   BuildMode=Shared")
endif()

if(BLEND2D_BUILD_TEST)
  message("   BuildTest=On")
else()
  message("   BuildTest=Off")
endif()

message("   BLEND2D_DIR=${BLEND2D_DIR}")
message("   BLEND2D_SOURCE_DIR=${BLEND2D_SOURCE_DIR}")
message("   BLEND2D_INCLUDE_DIR=${BLEND2D_INCLUDE_DIR}")
message("   BLEND2D_DEPS=${BLEND2D_DEPS}")
message("   BLEND2D_LIBS=${BLEND2D_LIBS}")
message("   BLEND2D_CFLAGS=${BLEND2D_CFLAGS}")
message("   BLEND2D_PRIVATE_CFLAGS=")

foreach(flag ${BLEND2D_PRIVATE_CFLAGS})
  message("     ${flag}")
endforeach()

foreach(flag ${BLEND2D_PRIVATE_CFLAGS_DBG})
  message("     ${flag} [DEBUG]")
endforeach()

foreach(flag ${BLEND2D_PRIVATE_CFLAGS_REL})
  message("     ${flag} [RELEASE]")
endforeach()

# =============================================================================
# [Blend2D - Targets]
# =============================================================================

if(BLEND2D_TARGET_TYPE)
  blend2d_add_target(blend2d
    "${BLEND2D_TARGET_TYPE}"
    "${BLEND2D_SRC};${ASMJIT_SRC}"
    "${BLEND2D_DEPS}"
    "${BLEND2D_PRIVATE_CFLAGS}"
    "${BLEND2D_PRIVATE_CFLAGS_DBG}"
    "${BLEND2D_PRIVATE_CFLAGS_REL}")

  target_include_directories(blend2d BEFORE PRIVATE ${ASMJIT_INCLUDE_DIR})
  target_include_directories(blend2d PUBLIC ${BLEND2D_INCLUDE_DIR})

  foreach(i ${BLEND2D_SRC})
    get_filename_component(path ${i} PATH)
    get_filename_component(name ${i} NAME)
    string(REGEX REPLACE "^${BLEND2D_SOURCE_DIR}/" "" targetpath "${path}")
    if("${name}" MATCHES "\\.h$")
      if(NOT "${name}" MATCHES "_p\\.h$")
        install(FILES ${i} DESTINATION "include/${targetpath}")
      endif()
    endif()
  endforeach()
endif()

# The library is always embedded into the unit-test executable. This way it's
# much easier to test private functions compared to just linking to `blend2d`.
if(BLEND2D_BUILD_TEST)
  blend2d_add_source(BLEND2D_TEST_SRC ../test broken.cpp broken.h bl_test_unit.cpp)
  set(BLEND2D_TEST_CFLAGS ${BLEND2D_CXX_STD}
                          ${BLEND2D_PRIVATE_CFLAGS}
                          ${BLEND2D_MAKE_DEF}BL_BUILD_STATIC
                          ${BLEND2D_MAKE_DEF}BL_BUILD_TEST)

  blend2d_add_target(bl_test_unit
    EXECUTABLE
    "${BLEND2D_SRC};${ASMJIT_SRC};${BLEND2D_TEST_SRC}"
    "${BLEND2D_DEPS}"
    "${BLEND2D_TEST_CFLAGS}"
    "${BLEND2D_PRIVATE_CFLAGS_DBG}"
    "${BLEND2D_PRIVATE_CFLAGS_REL}")

  set_target_properties(bl_test_unit PROPERTIES LINK_FLAGS "${BLEND2D_LFLAGS}")
  target_include_directories(bl_test_unit BEFORE PRIVATE ${ASMJIT_INCLUDE_DIR})
  target_include_directories(bl_test_unit BEFORE PRIVATE ${BLEND2D_INCLUDE_DIR})
endif()
