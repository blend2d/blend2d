// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_THREADING_FUTEX_P_H
#define BLEND2D_THREADING_FUTEX_P_H

#include "../api-internal_p.h"
#include "../runtime_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

#ifndef BL_BUILD_NO_FUTEX

#if defined(__linux__)

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace BLFutex {

static BL_INLINE int call(volatile uint32_t* addr, int op, int x) noexcept { return syscall(SYS_futex, (void*)addr, op, x, nullptr, nullptr, 0); }

static BL_INLINE int wait(volatile uint32_t* addr, uint32_t value) noexcept { return call(addr, FUTEX_WAIT_PRIVATE, int(value)); }
static BL_INLINE int wakeOne(volatile uint32_t* addr) noexcept { return call(addr, FUTEX_WAKE_PRIVATE, 1); }
static BL_INLINE int wakeAll(volatile uint32_t* addr) noexcept { return call(addr, FUTEX_WAKE_PRIVATE, std::numeric_limits<int>::max()); }

} // {BLFutex}

#elif defined(__OpenBSD__)

#include <sys/futex.h>

namespace BLFutex {

static BL_INLINE int call(volatile uint32_t* addr, int op, int x) noexcept { return futex((void*)addr, op, x, nullptr, nullptr); }

static BL_INLINE int wait(volatile uint32_t* addr, uint32_t value) noexcept { return call(addr, FUTEX_WAIT, int(value)); }
static BL_INLINE int wakeOne(volatile uint32_t* addr) noexcept { return call(addr, FUTEX_WAKE, 1); }
static BL_INLINE int wakeAll(volatile uint32_t* addr) noexcept { return call(addr, FUTEX_WAKE, std::numeric_limits<int>::max()); }

} // {BLFutex}

#elif defined(_WIN32)

namespace BLFutex {

struct FutexWinAPI {
  typedef int (BL_STDCALL *WaitOnAddressFunc)(void* Address, void* CompareAddress, size_t AddressSize, uint32_t dwMilliseconds);
  typedef void (BL_STDCALL *WakeByAddressSingleFunc)(void* Address);
  typedef void (BL_STDCALL *WakeByAddressAllFunc)(void* Address);

  WaitOnAddressFunc WaitOnAddress;
  WakeByAddressSingleFunc WakeByAddressSingle;
  WakeByAddressAllFunc WakeByAddressAll;
};

BL_HIDDEN extern FutexWinAPI futexWinAPI;

static BL_INLINE int wait(volatile uint32_t* addr, uint32_t x) noexcept { futexWinAPI.WaitOnAddress((void*)addr, &x, sizeof(x), INFINITE); return 0; }
static BL_INLINE int wakeOne(volatile uint32_t* addr) noexcept { futexWinAPI.WakeByAddressSingle((void*)addr); return 0; }
static BL_INLINE int wakeAll(volatile uint32_t* addr) noexcept { futexWinAPI.WakeByAddressAll((void*)addr); return 0; }

} // {BLFutex}

#else
#define BL_BUILD_NO_FUTEX
#endif

#endif

#ifndef BL_BUILD_NO_FUTEX
#define BL_FUTEX_ENABLED (blRuntimeContext.featuresInfo.futexEnabled)
#else
#define BL_FUTEX_ENABLED (0)

namespace BLFutex {

static BL_INLINE int wait(volatile uint32_t*, uint32_t) noexcept { return -1; }
static BL_INLINE int wakeOne(volatile uint32_t*) noexcept { return -1; }
static BL_INLINE int wakeAll(volatile uint32_t*) noexcept { return -1; }

} // {BLFutex}
#endif

//! \}
//! \endcond

#endif // BLEND2D_THREADING_FUTEX_P_H
