// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "../api-build_p.h"
#include "../runtime_p.h"
#include "../threading/futex_p.h"

#if !defined(BL_BUILD_NO_FUTEX)
  #ifdef _WIN32
    #include <windows.h>
  #endif
#endif

// bl::Futex - Runtime Registration
// ================================

#if !defined(BL_BUILD_NO_FUTEX) && defined(__linux__)

void blFuxexRtInit(BLRuntimeContext* rt) noexcept {
  // Initially in Linux 2.6.0, improved at 2.6.7, which is the minimum for FUTEX_WAIT_PRIVATE / FUTEX_WAKE_PRIVATE.
  rt->featuresInfo.futexEnabled = 1;
}

#elif !defined(BL_BUILD_NO_FUTEX) && defined(__OpenBSD__)

void blFuxexRtInit(BLRuntimeContext* rt) noexcept {
  // TODO: How to detect support on OpenBSD?
  rt->featuresInfo.futexEnabled = 0;
}

#elif !defined(BL_BUILD_NO_FUTEX) && defined(_WIN32)

namespace bl {
namespace Futex {
namespace Native {

FutexSyncAPI futexSyncAPI;

} // {Native}
} // {Futex}
} // {bl}

void blFuxexRtInit(BLRuntimeContext* rt) noexcept {
  HMODULE hModule = GetModuleHandleA("api-ms-win-core-synch-l1-2-0.dll");

  if (!hModule)
    return;

  using FutexSyncAPI = bl::Futex::Native::FutexSyncAPI;
  FutexSyncAPI& fn = bl::Futex::Native::futexSyncAPI;

  fn.WaitOnAddress       = (FutexSyncAPI::WaitOnAddressFunc      )GetProcAddress(hModule, "WaitOnAddress");
  fn.WakeByAddressSingle = (FutexSyncAPI::WakeByAddressSingleFunc)GetProcAddress(hModule, "WakeByAddressSingle");
  fn.WakeByAddressAll    = (FutexSyncAPI::WakeByAddressAllFunc   )GetProcAddress(hModule, "WakeByAddressAll");

  bool ok = fn.WaitOnAddress != nullptr && fn.WakeByAddressSingle != nullptr && fn.WakeByAddressAll != nullptr;
  rt->featuresInfo.futexEnabled = ok;
}

#else

// Futex not supported.
void blFuxexRtInit(BLRuntimeContext* rt) noexcept { blUnused(rt); }

#endif
