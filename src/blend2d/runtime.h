// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef BLEND2D_RUNTIME_H_INCLUDED
#define BLEND2D_RUNTIME_H_INCLUDED

#include "./api.h"

//! \addtogroup blend2d_api_runtime
//! \{

// ============================================================================
// [Constants]
// ============================================================================

//! Blend2D runtime limits.
//!
//! \note These constanst are used across Blend2D, but they are not designed to
//! be ABI stable. New versions of Blend2D can increase certain limits without
//! notice. Use runtime to query the limits dynamically, see `BLRuntimeBuildInfo`.
BL_DEFINE_ENUM(BLRuntimeLimits) {
  //! Maximum width and height of an image.
  BL_RUNTIME_MAX_IMAGE_SIZE = 65535,
  //! Maximum number of threads for asynchronous operations (including rendering).
  BL_RUNTIME_MAX_THREAD_COUNT = 32
};

//! Type of runtime information that can be queried through `blRuntimeQueryInfo()`.
BL_DEFINE_ENUM(BLRuntimeInfoType) {
  //! Blend2D build information.
  BL_RUNTIME_INFO_TYPE_BUILD = 0,
  //! System information (includes CPU architecture, features, core count, etc...).
  BL_RUNTIME_INFO_TYPE_SYSTEM = 1,
  //! Resources information (includes Blend2D memory consumption, file handles
  //! used, etc...)
  BL_RUNTIME_INFO_TYPE_RESOURCE = 2,

  //! Count of runtime information types.
  BL_RUNTIME_INFO_TYPE_COUNT = 3
};

//! Blend2D runtime build type.
BL_DEFINE_ENUM(BLRuntimeBuildType) {
  //! Describes a Blend2D debug build.
  BL_RUNTIME_BUILD_TYPE_DEBUG = 0,
  //! Describes a Blend2D release build.
  BL_RUNTIME_BUILD_TYPE_RELEASE = 1
};

//! CPU architecture that can be queried by `BLRuntime::querySystemInfo()`.
BL_DEFINE_ENUM(BLRuntimeCpuArch) {
  //! Unknown architecture.
  BL_RUNTIME_CPU_ARCH_UNKNOWN = 0,
  //! 32-bit or 64-bit X86 architecture.
  BL_RUNTIME_CPU_ARCH_X86 = 1,
  //! 32-bit or 64-bit ARM architecture.
  BL_RUNTIME_CPU_ARCH_ARM = 2,
  //! 32-bit or 64-bit MIPS architecture.
  BL_RUNTIME_CPU_ARCH_MIPS = 3
};

//! CPU features Blend2D supports.
BL_DEFINE_ENUM(BLRuntimeCpuFeatures) {
  BL_RUNTIME_CPU_FEATURE_X86_SSE2 = 0x00000001u,
  BL_RUNTIME_CPU_FEATURE_X86_SSE3 = 0x00000002u,
  BL_RUNTIME_CPU_FEATURE_X86_SSSE3 = 0x00000004u,
  BL_RUNTIME_CPU_FEATURE_X86_SSE4_1 = 0x00000008u,
  BL_RUNTIME_CPU_FEATURE_X86_SSE4_2 = 0x00000010u,
  BL_RUNTIME_CPU_FEATURE_X86_AVX = 0x00000020u,
  BL_RUNTIME_CPU_FEATURE_X86_AVX2 = 0x00000040u
};

//! Runtime cleanup flags that can be used through `BLRuntime::cleanup()`.
BL_DEFINE_ENUM(BLRuntimeCleanupFlags) {
  //! Cleanup object memory pool.
  BL_RUNTIME_CLEANUP_OBJECT_POOL = 0x00000001u,
  //! Cleanup zeroed memory pool.
  BL_RUNTIME_CLEANUP_ZEROED_POOL = 0x00000002u,
  //! Cleanup thread pool (would join unused threads).
  BL_RUNTIME_CLEANUP_THREAD_POOL = 0x00000010u,

  //! Cleanup everything.
  BL_RUNTIME_CLEANUP_EVERYTHING = 0xFFFFFFFFu
};

// ============================================================================
// [BLRuntime - BuildInfo]
// ============================================================================

//! Blend2D build information.
struct BLRuntimeBuildInfo {
  union {
    //! Blend2D version stored as `((MAJOR << 16) | (MINOR << 8) | PATCH)`.
    uint32_t version;

    //! Decomposed Blend2D version so it's easier to access without bit shifting.
    struct {
    #if BL_BYTE_ORDER == 1234 // LITTLE ENDIAN
      uint8_t patchVersion;
      uint8_t minorVersion;
      uint16_t majorVersion;
    #else
      uint16_t majorVersion;
      uint8_t minorVersion;
      uint8_t patchVersion;
    #endif
    };
  };

  //! Blend2D build type, see `BLRuntimeBuildType`.
  uint32_t buildType;

  //! Baseline CPU features, see `BLRuntimeCpuFeatures`.
  //!
  //! These features describe CPU features that were detected at compile-time.
  //! Baseline features are used to compile all source files so they represent
  //! the minimum feature-set the target CPU must support to run Blend2D.
  //!
  //! Official Blend2D builds set baseline at SSE2 on X86 target and NEON on
  //! ARM target. Custom builds can set use different baseline, which can be
  //! read through `BLRuntimeBuildInfo`.
  uint32_t baselineCpuFeatures;

  //! Supported CPU features, see `BLRuntimeCpuFeatures`.
  //!
  //! These features do not represent the features that the host CPU must support,
  //! instead, they represent all features that Blend2D can take advantage of in
  //! C++ code that uses instruction intrinsics. For example if AVX2 is part of
  //! `supportedCpuFeatures` it means that Blend2D can take advantage of it if
  //! there is a separate code-path.
  uint32_t supportedCpuFeatures;

  //! Maximum size of an image (both width and height).
  uint32_t maxImageSize;

  //! Maximum number of threads for asynchronous operations, including rendering.
  uint32_t maxThreadCount;

  //! Reserved, must be zero.
  uint32_t reserved[2];

  //! Identification of the C++ compiler used to build Blend2D.
  char compilerInfo[32];

  // --------------------------------------------------------------------------
  #ifdef __cplusplus
  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLRuntime - SystemInfo]
// ============================================================================

//! System information queried by the runtime.
struct BLRuntimeSystemInfo {
  //! Host CPU architecture, see `BLRuntimeCpuArch`.
  uint32_t cpuArch;
  //! Host CPU features, see `BLRuntimeCpuFeatures`.
  uint32_t cpuFeatures;
  //! Number of cores of the host CPU/CPUs.
  uint32_t coreCount;
  //! Number of threads of the host CPU/CPUs.
  uint32_t threadCount;
  //! Minimum stack size of a worker thread used by Blend2D.
  uint32_t threadStackSize;
  //! Removed field.
  uint32_t removed;
  //! Allocation granularity of virtual memory (includes thread's stack).
  uint32_t allocationGranularity;
  //! Reserved for future use.
  uint32_t reserved[5];

  // --------------------------------------------------------------------------
  #ifdef __cplusplus
  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLRuntime - ResourceInfo]
// ============================================================================

//! Provides information about resources allocated by Blend2D.
struct BLRuntimeResourceInfo {
  //! Virtual memory used at this time.
  size_t vmUsed;
  //! Virtual memory reserved (allocated internally).
  size_t vmReserved;
  //! Overhead required to manage virtual memory allocations.
  size_t vmOverhead;
  //! Number of blocks of virtual memory allocated.
  size_t vmBlockCount;

  //! Zeroed memory used at this time.
  size_t zmUsed;
  //! Zeroed memory reserved (allocated internally).
  size_t zmReserved;
  //! Overhead required to manage zeroed memory allocations.
  size_t zmOverhead;
  //! Number of blocks of zeroed memory allocated.
  size_t zmBlockCount;

  //! Count of dynamic pipelines created and cached.
  size_t dynamicPipelineCount;

  //! Number of active file handles used by Blend2D.
  //!
  //! \note File handles are counted by `BLFile` - when a file is opened a
  //! global counter is incremented and when it's closed it's decremented.
  //! This means that this number represents the actual use of `BLFile` and
  //! doesn't consider the origin of the use (it's either Blend2D or user).
  size_t fileHandleCount;

  //! Number of active file mappings used by Blend2D.
  //!
  //! \note Blend2D maps file content to `BLArray<uint8_t>` container, so this
  //! number represents the actual number of `BLArray<uint8_t>` instances that
  //! contain a mapped file.
  size_t fileMappingCount;

  //! Reserved for future use.
  size_t reserved[5];

  // --------------------------------------------------------------------------
  #ifdef __cplusplus
  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLRuntime - C++ API]
// ============================================================================

#ifdef __cplusplus
//! Interface to access Blend2D runtime (wraps C-API).
namespace BLRuntime {

static BL_INLINE BLResult cleanup(uint32_t cleanupFlags) noexcept {
  return blRuntimeCleanup(cleanupFlags);
}

static BL_INLINE BLResult queryBuildInfo(BLRuntimeBuildInfo* out) noexcept {
  return blRuntimeQueryInfo(BL_RUNTIME_INFO_TYPE_BUILD, out);
}

static BL_INLINE BLResult querySystemInfo(BLRuntimeSystemInfo* out) noexcept {
  return blRuntimeQueryInfo(BL_RUNTIME_INFO_TYPE_SYSTEM, out);
}

static BL_INLINE BLResult queryResourceInfo(BLRuntimeResourceInfo* out) noexcept {
  return blRuntimeQueryInfo(BL_RUNTIME_INFO_TYPE_RESOURCE, out);
}

static BL_INLINE BLResult message(const char* msg) noexcept {
  return blRuntimeMessageOut(msg);
}

template<typename... Args>
static BL_INLINE BLResult message(const char* fmt, Args&&... args) noexcept {
  return blRuntimeMessageFmt(fmt, std::forward<Args>(args)...);
}

} // {BLRuntime}
#endif

//! \}

#endif // BLEND2D_RUNTIME_H_INCLUDED
