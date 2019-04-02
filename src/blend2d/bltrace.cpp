// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./bltrace_p.h"

// ============================================================================
// [BLOpenType::BLDebugTrace]
// ============================================================================

void BLDebugTrace::log(uint32_t severity, uint32_t indentation, const char* fmt, ...) noexcept {
  const char* prefix = "";
  if (indentation < 0xFFFFFFFFu) {
    switch (severity) {
      case 1: prefix = "[WARN] "; break;
      case 2: prefix = "[FAIL] "; break;
    }
    blRuntimeMessageFmt("%*s%s", indentation * 2, "", prefix);
  }

  va_list ap;
  va_start(ap, fmt);
  blRuntimeMessageVFmt(fmt, ap);
  va_end(ap);
}
