// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "../api-build_p.h"
#include "../runtime_p.h"
#include "../threading/futex_p.h"

#ifdef _WIN32
  #include <windows.h>
#endif

// Futex - Runtime Registration
// ============================

#if defined(__linux__)

// Initially in Linux 2.6.0, improved at 2.6.7, which is the minimum for FUTEX_WAIT_PRIVATE / FUTEX_WAKE_PRIVATE.
void blFuxexRtInit(BLRuntimeContext* rt) noexcept {
  rt->featuresInfo.futexEnabled = 1;
}

#elif defined(__OpenBSD__)

// TODO: How to detect support on OpenBSD?
void blFuxexRtInit(BLRuntimeContext* rt) noexcept {
  rt->featuresInfo.futexEnabled = 0;
}

#elif defined(_WIN32)

namespace BLFutex { FutexWinAPI futexWinAPI; }

void blFuxexRtInit(BLRuntimeContext* rt) noexcept {
  HMODULE hModule = GetModuleHandleA("api-ms-win-core-synch-l1-2-0.dll");

  if (!hModule)
    return;

  BLFutex::futexWinAPI.WaitOnAddress       = (BLFutex::FutexWinAPI::WaitOnAddressFunc      )GetProcAddress(hModule, "WaitOnAddress");
  BLFutex::futexWinAPI.WakeByAddressSingle = (BLFutex::FutexWinAPI::WakeByAddressSingleFunc)GetProcAddress(hModule, "WakeByAddressSingle");
  BLFutex::futexWinAPI.WakeByAddressAll    = (BLFutex::FutexWinAPI::WakeByAddressAllFunc   )GetProcAddress(hModule, "WakeByAddressAll");

  rt->featuresInfo.futexEnabled = BLFutex::futexWinAPI.WaitOnAddress       != nullptr &&
                                  BLFutex::futexWinAPI.WakeByAddressSingle != nullptr &&
                                  BLFutex::futexWinAPI.WakeByAddressAll    != nullptr ;
}

#else

// Futex not supported.
void blFuxexRtInit(BLRuntimeContext* rt) noexcept { blUnused(rt); }

#endif
