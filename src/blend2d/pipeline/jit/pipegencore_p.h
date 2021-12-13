// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_PIPEGENCORE_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_PIPEGENCORE_P_H_INCLUDED

#include "../../api-internal_p.h"
#include "../../compop_p.h"
#include "../../format_p.h"
#include "../../image_p.h"
#include "../../rgba_p.h"
#include "../../tables_p.h"
#include "../../pipeline/pipedefs_p.h"
#include "../../pipeline/jit/jitbase_p.h"

// External dependencies of BLPipeline.
#if BL_TARGET_ARCH_X86
  #include <asmjit/x86.h>
#endif

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

//! \namespace BLPipeline::JIT
//! Everything related to JIT pipeline generator and runtime is within `BLPipeline::JIT` namespace.

namespace BLPipeline {
namespace JIT {

class PipeCompiler;
class PipePart;

class CompOpPart;

class FetchPart;
class FetchGradientPart;
class FetchPixelPtrPart;
class FetchSolidPart;
class FetchPatternPart;

class FillPart;
class FillBoxAPart;
class FillBoxUPart;
class FillAnalyticPart;

BL_DEFINE_STRONG_TYPE(Alignment, uint32_t)

//! Pipeline generator loop-type, used by fillers & compositors.
enum class CMaskLoopType : uint8_t {
  //! Not in a loop-mode.
  kNone = 0,
  //! CMask opaque loop (alpha is 1.0).
  kOpaque = 1,
  //! CMask masked loop (alpha is not 1.0).
  kVariant = 2
};

enum class PixelType : uint8_t {
  kNone = 0,
  kAlpha = 1,
  kRGBA = 2
};

enum class PixelFlags : uint32_t {
  kNone = 0,

  //! Last fetch in this scanline, thus at most `N-1` pixels would be used.
  kLastPartial = 0x40000000u,
  //! Fetch read-only, registers won't be modified.
  kImmutable   = 0x80000000u,

  //! Scalar alpha or stencil value in `sa` (single pixel quantities only).
  kSA = 0x00000001u,
  //! Packed alpha or stencil components stored in `pa`.
  kPA = 0x00000002u,
  //! Unpacked alpha or stencil components stored in `ua`.
  kUA = 0x00000004u,
  //! Unpacked+inverted alpha or stencil components stored in `uia`
  kUIA = 0x00000008u,

  //! Packed ARGB32 components stored in `pc`.
  kPC = 0x00000010u,
  //! Unpacked ARGB32 components stored in `uc`.
  kUC = 0x00000020u,

  // TODO: [PipeGen] REMOVE.
  //! Any of PC|UC|UA|UIA.
  kAny = kPC | kUC | kUA | kUIA
};
BL_DEFINE_ENUM_FLAGS(PixelFlags);

//! Represents either Alpha or RGBA pixel.
//!
//! Convention used to define and process pixel components:
//!
//!   - Prefixes:
//!     - "p"  - packed pixel(s) or component(s).
//!     - "u"  - unpacked pixel(s) or component(s).
//!
//!   - Components:
//!     - "c"  - Pixel components (ARGB).
//!     - "a"  - Pixel alpha values (A).
//!     - "ia" - Inverted pixel alpha values (IA).
//!     - "m"  - Mask (not part of the pixel itself, comes from a FillPart).
//!     - "im" - Mask (not part of the pixel itself, comes from a FillPart).
struct Pixel {
  PixelType _type;
  PixelFlags _flags;
  uint32_t _count;

  //! Scalar alpha component (single value only, no packing/unpacking here).
  x86::Gp sa;
  //! Packed alpha components.
  VecArray pa;
  //! Unpacked alpha components.
  VecArray ua;
  //! Unpacked and inverted alpha components.
  VecArray uia;
  //! Packed ARGB32 pixel(s), maximum 8, 16, or 32, depending on SIMD width.
  VecArray pc;
  //! Unpacked ARGB32 pixel(s), maximum 8, 16, or 32, depending on SIMD width.
  VecArray uc;

  BL_INLINE Pixel(PixelType type = PixelType::kNone) noexcept
    : _type(type),
      _flags(PixelFlags::kNone),
      _count(0) {}

  BL_INLINE void reset(PixelType type = PixelType::kNone) noexcept {
    _type = type;
    resetAllExceptType();
  }

  BL_INLINE void resetAllExceptType() noexcept {
    _flags = PixelFlags::kNone;
    _count = 0;
    sa.reset();
    pa.reset();
    ua.reset();
    uia.reset();
    pc.reset();
    uc.reset();
  }

  BL_INLINE PixelType type() const noexcept { return _type; }
  BL_INLINE PixelFlags flags() const noexcept { return _flags; }
  BL_INLINE uint32_t count() const noexcept { return _count; }

  BL_INLINE void setType(PixelType type) noexcept { _type = type; }
  BL_INLINE void setCount(uint32_t count) noexcept { _count = count; }

  BL_INLINE bool isAlpha() const noexcept { return _type == PixelType::kAlpha; }
  BL_INLINE bool isRGBA() const noexcept { return _type == PixelType::kRGBA; }

  //! Tests whether all members are immutable (solid fills).
  BL_INLINE bool isImmutable() const noexcept { return blTestFlag(_flags, PixelFlags::kImmutable); }
  //! Tests whether this pixel was a partial fetch (the last pixel could be missing).
  BL_INLINE bool isLastPartial() const noexcept { return blTestFlag(_flags, PixelFlags::kLastPartial); }

  BL_INLINE void makeImmutable() noexcept { _flags |= PixelFlags::kImmutable; }

  BL_INLINE void setImmutable(bool immutable) noexcept {
    _flags = (_flags & ~PixelFlags::kImmutable) | (immutable ? PixelFlags::kImmutable : PixelFlags::kNone);
  }
};

//! Optimized pixel representation used by solid fills.
//!
//! Used by both Alpha and RGBA pixel pipelines.
struct SolidPixel {
  BL_INLINE SolidPixel() noexcept { reset(); }

  //! Scalar alpha or stencil value (A8 pipeline).
  x86::Gp sa;
  //! Scalar pre-processed component, shown as "X" in equations.
  x86::Gp sx;
  //! Scalar pre-processed component, shown as "Y" in equations.
  x86::Gp sy;

  //! Packed pre-processed components, shown as "X" in equations.
  x86::Vec px;
  //! Packed pre-processed components, shown as "Y" in equations.
  x86::Vec py;
  //! Unpacked pre-processed components, shown as "X" in equations.
  x86::Vec ux;
  //! Unpacked pre-processed components, shown as "Y" in equations.
  x86::Vec uy;

  //! Mask vector.
  x86::Vec vm;
  //! Inverted mask vector.
  x86::Vec vn;

  BL_INLINE void reset() noexcept {
    sa.reset();
    sx.reset();
    sy.reset();

    px.reset();
    ux.reset();

    py.reset();
    uy.reset();

    vm.reset();
    vn.reset();
  }
};

//! A constant mask (CMASK) stored in either GP or XMM register.
struct PipeCMask {
  //! Mask scalar.
  x86::Gp sm;
  //! Inverted mask scalar.
  x86::Gp sn;

  //! Mask vector.
  x86::Vec vm;
  //! Inverted mask vector.
  x86::Vec vn;

  BL_INLINE void reset() noexcept {
    JitUtils::resetVarStruct<PipeCMask>(this);
  }
};

struct PixelPtrLoadStoreMask {
  VecArray m;

  BL_INLINE bool empty() const noexcept { return m.empty(); }
};

} // {JIT}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_PIPEGENCORE_P_H_INCLUDED
