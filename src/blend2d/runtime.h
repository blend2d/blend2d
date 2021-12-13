// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RUNTIME_H_INCLUDED
#define BLEND2D_RUNTIME_H_INCLUDED

#include "api.h"

//! \addtogroup blend2d_api_runtime
//! \{

//! \name Runtime - Constants
//! \{

//! Blend2D runtime limits.
//!
//! \note These constanst are used across Blend2D, but they are not designed to be ABI stable. New versions of Blend2D
//! can increase certain limits without notice. Use runtime to query the limits dynamically, see `BLRuntimeBuildInfo`.
BL_DEFINE_ENUM(BLRuntimeLimits) {
  //! Maximum width and height of an image.
  BL_RUNTIME_MAX_IMAGE_SIZE = 65535,
  //! Maximum number of threads for asynchronous operations (including rendering).
  BL_RUNTIME_MAX_THREAD_COUNT = 32
};

//! Type of runtime information that can be queried through \ref blRuntimeQueryInfo().
BL_DEFINE_ENUM(BLRuntimeInfoType) {
  //! Blend2D build information.
  BL_RUNTIME_INFO_TYPE_BUILD = 0,
  //! System information (includes CPU architecture, features, core count, etc...).
  BL_RUNTIME_INFO_TYPE_SYSTEM = 1,
  //! Resources information (includes Blend2D memory consumption)
  BL_RUNTIME_INFO_TYPE_RESOURCE = 2,

  //! Count of runtime information types.
  BL_RUNTIME_INFO_TYPE_MAX_VALUE = 2

  BL_FORCE_ENUM_UINT32(BL_RUNTIME_INFO_TYPE)
};

//! Blend2D runtime build type.
BL_DEFINE_ENUM(BLRuntimeBuildType) {
  //! Describes a Blend2D debug build.
  BL_RUNTIME_BUILD_TYPE_DEBUG = 0,
  //! Describes a Blend2D release build.
  BL_RUNTIME_BUILD_TYPE_RELEASE = 1

  BL_FORCE_ENUM_UINT32(BL_RUNTIME_BUILD_TYPE)
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

  BL_FORCE_ENUM_UINT32(BL_RUNTIME_CPU_ARCH)
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

  BL_FORCE_ENUM_UINT32(BL_RUNTIME_CPU_FEATURE)
};

//! Runtime cleanup flags that can be used through `BLRuntime::cleanup()`.
BL_DEFINE_ENUM(BLRuntimeCleanupFlags) {
  //! No flags.
  BL_RUNTIME_CLEANUP_NO_FLAGS = 0u,
  //! Cleanup object memory pool.
  BL_RUNTIME_CLEANUP_OBJECT_POOL = 0x00000001u,
  //! Cleanup zeroed memory pool.
  BL_RUNTIME_CLEANUP_ZEROED_POOL = 0x00000002u,
  //! Cleanup thread pool (would join unused threads).
  BL_RUNTIME_CLEANUP_THREAD_POOL = 0x00000010u,

  //! Cleanup everything.
  BL_RUNTIME_CLEANUP_EVERYTHING = 0xFFFFFFFFu

  BL_FORCE_ENUM_UINT32(BL_RUNTIME_CLEANUP_FLAG)
};

//! \}

//! \name Runtime - Structs
//! \{

//! Blend2D build information.
struct BLRuntimeBuildInfo {
  //! Major version number.
  uint32_t majorVersion;
  //! Minor version number.
  uint32_t minorVersion;
  //! Patch version number.
  uint32_t patchVersion;

  //! Blend2D build type, see `BLRuntimeBuildType`.
  uint32_t buildType;

  //! Baseline CPU features, see `BLRuntimeCpuFeatures`.
  //!
  //! These features describe CPU features that were detected at compile-time. Baseline features are used to compile
  //! all source files so they represent the minimum feature-set the target CPU must support to run Blend2D.
  //!
  //! Official Blend2D builds set baseline at SSE2 on X86 target and NEON on ARM target. Custom builds can set use
  //! a different baseline, which can be read through `BLRuntimeBuildInfo`.
  uint32_t baselineCpuFeatures;

  //! Supported CPU features, see `BLRuntimeCpuFeatures`.
  //!
  //! These features do not represent the features that the host CPU must support, instead, they represent all features
  //! that Blend2D can take advantage of in C++ code that uses instruction intrinsics. For example if AVX2 is part of
  //! `supportedCpuFeatures` it means that Blend2D can take advantage of it if there is a separate code-path.
  uint32_t supportedCpuFeatures;

  //! Maximum size of an image (both width and height).
  uint32_t maxImageSize;

  //! Maximum number of threads for asynchronous operations, including rendering.
  uint32_t maxThreadCount;

  //! Reserved, must be zero.
  uint32_t reserved[2];

  //! Identification of the C++ compiler used to build Blend2D.
  char compilerInfo[32];

#ifdef __cplusplus
  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
#endif
};

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

#ifdef __cplusplus
  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
#endif
};

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

  //! Reserved for future use.
  size_t reserved[7];

#ifdef __cplusplus
  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
#endif
};

//! \}

//! \name Runtime - C API
//! \{

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blRuntimeInit() BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blRuntimeShutdown() BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blRuntimeCleanup(BLRuntimeCleanupFlags cleanupFlags) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blRuntimeQueryInfo(BLRuntimeInfoType infoType, void* infoOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blRuntimeMessageOut(const char* msg) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blRuntimeMessageFmt(const char* fmt, ...) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blRuntimeMessageVFmt(const char* fmt, va_list ap) BL_NOEXCEPT_C;

#ifdef _WIN32
BL_API BLResult BL_CDECL blResultFromWinError(uint32_t e) BL_NOEXCEPT_C;
#else
BL_API BLResult BL_CDECL blResultFromPosixError(int e) BL_NOEXCEPT_C;
#endif

BL_END_C_DECLS

//! \}

//! \name Runtime - C++ API
//! \{
#ifdef __cplusplus

//! Blend2D runtime initializer.
//!
//! Calls \ref blRuntimeInit() on entry and \ref blRuntimeShutdown() on exit.
//!
//! When using Blend2D as shared library the initialization and shutdown of the library is guaranteed by the loader,
//! however, when Blend2D is compiled as a static library and user uses static Blend2D instances it's possible that
//! the instance is created before Blend2D is initialized, which would be undefined behavior and would lead most
//! likely to a crash. BLRuntimeInitializer can be used in such compilation unit to ensure that the initialization
//! is called first. The initializer can be used more than once as Blend2D uses a counter so it would only initialize
//! and shutdown the library once.
//!
//! \note The default initializer of the library uses GCC/Clang extension `__attribute__((init_priority(102))` if
//! supported by the compiler. The priority is the second lowest number that is available to user code. If you are
//! using such attribute yourself and want something initialized before Blend2D you should consider using
//! `__attribute__((init_priority(101)))` while compiled by GCC/Clang.
class BLRuntimeInitializer {
public:
  // Disable copy and assignment - only used to statically initialize Blend2D.
  BL_INLINE BLRuntimeInitializer(const BLRuntimeInitializer&) = delete;
  BL_INLINE BLRuntimeInitializer& operator=(const BLRuntimeInitializer&) = delete;

  BL_INLINE BLRuntimeInitializer() noexcept { blRuntimeInit(); }
  BL_INLINE ~BLRuntimeInitializer() noexcept { blRuntimeShutdown(); }
};

//! Interface to access Blend2D runtime (wraps C API).
namespace BLRuntime {

static BL_INLINE BLResult cleanup(BLRuntimeCleanupFlags cleanupFlags) noexcept {
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

//! \}

#endif // BLEND2D_RUNTIME_H_INCLUDED
