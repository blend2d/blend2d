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

#ifndef BLEND2D_TRACE_P_H_INCLUDED
#define BLEND2D_TRACE_P_H_INCLUDED

#include "./api-internal_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLDummyTrace]
// ============================================================================

//! Dummy trace - no tracing, no runtime overhead.
class BLDummyTrace {
public:
  BL_INLINE bool enabled() const noexcept { return false; };
  BL_INLINE void indent() noexcept {}
  BL_INLINE void deindent() noexcept {}

  template<typename... Args>
  BL_INLINE void out(Args&&...) noexcept {}

  template<typename... Args>
  BL_INLINE void info(Args&&...) noexcept {}

  template<typename... Args>
  BL_INLINE bool warn(Args&&...) noexcept { return false; }

  template<typename... Args>
  BL_INLINE bool fail(Args&&...) noexcept { return false; }
};

// ============================================================================
// [BLDebugTrace]
// ============================================================================

//! Debug trace - active / enabled trace that can be useful during debugging.
class BLDebugTrace {
public:
  BL_INLINE BLDebugTrace() noexcept
    : indentation(0) {}
  BL_INLINE BLDebugTrace(const BLDebugTrace& other) noexcept
    : indentation(other.indentation) {}

  BL_INLINE bool enabled() const noexcept { return true; };
  BL_INLINE void indent() noexcept { indentation++; }
  BL_INLINE void deindent() noexcept { indentation--; }

  template<typename... Args>
  BL_INLINE void out(Args&&... args) noexcept { log(0, 0xFFFFFFFFu, std::forward<Args>(args)...); }

  template<typename... Args>
  BL_INLINE void info(Args&&... args) noexcept { log(0, indentation, std::forward<Args>(args)...); }

  template<typename... Args>
  BL_INLINE bool warn(Args&&... args) noexcept { log(1, indentation, std::forward<Args>(args)...); return false; }

  template<typename... Args>
  BL_INLINE bool fail(Args&&... args) noexcept { log(2, indentation, std::forward<Args>(args)...); return false; }

  BL_HIDDEN static void log(uint32_t severity, uint32_t indentation, const char* fmt, ...) noexcept;

  int indentation;
};

//! \}
//! \endcond

#endif // BLEND2D_TRACE_P_H_INCLUDED
