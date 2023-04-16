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

class GlobalAlpha;

//! Pipeline optimization flags used by \ref PipeCompiler.
enum class PipeOptFlags : uint32_t {
  //! No flags.
  kNone = 0x0u,

  //! CPU has instructions that can perform 8-bit masked loads and stores.
  kMaskOps8Bit = 0x00000001u,

  //! CPU has instructions that can perform 16-bit masked loads and stores.
  kMaskOps16Bit = 0x00000002u,

  //! CPU has instructions that can perform 32-bit masked loads and stores.
  kMaskOps32Bit = 0x00000004u,

  //! CPU has instructions that can perform 64-bit masked loads and stores.
  kMaskOps64Bit = 0x00000008u,

  //! CPU provides low-latency 32-bit multiplication (AMD CPUs).
  kFastVpmulld = 0x00000010u,

  //! CPU provides low-latency 64-bit multiplication (AMD CPUs).
  kFastVpmullq = 0x00000020u,

  //! CPU performs hardware gathers faster than a sequence of loads and packing.
  kFastGather = 0x00000040u,

  //! CPU has fast stores with mask.
  //!
  //! \note This is a hint to the compiler to emit a masked store instead of a sequence having branches.
  kFastStoreWithMask = 0x00000080u
};
BL_DEFINE_ENUM_FLAGS(PipeOptFlags)

//! Pipeline generator loop-type, used by fillers & compositors.
enum class CMaskLoopType : uint8_t {
  //! Not in a loop-mode.
  kNone = 0,
  //! CMask opaque loop (alpha is 1.0).
  kOpaque = 1,
  //! CMask masked loop (alpha is not 1.0).
  kVariant = 2
};

//! Type of the pixel.
//!
//! Not the same as format, PixelType could be a bit simplified.
enum class PixelType : uint8_t {
  kNone = 0,
  kA8 = 1,
  kRGBA32 = 2
};

enum class PixelFlags : uint32_t {
  kNone = 0,

  //! Scalar alpha or stencil value in `Pixel::sa` (single pixel quantities only).
  kSA = 0x00000001u,
  //! Packed alpha or stencil components stored in `Pixel::pa`.
  kPA = 0x00000002u,
  //! Unpacked alpha or stencil components stored in `Pixel::ua`.
  kUA = 0x00000004u,
  //! Unpacked and inverted alpha or stencil components stored in `Pixel::ui`
  kUI = 0x00000008u,

  //! Packed ARGB32 components stored in `Pixel::pc`.
  kPC = 0x00000010u,
  //! Unpacked ARGB32 components stored in `Pixel::uc`.
  kUC = 0x00000020u,

  //! Last fetch in this scanline, thus at most `N-1` pixels would be used.
  kLastPartial = 0x40000000u,
  //! Fetch read-only, registers won't be modified.
  kImmutable   = 0x80000000u
};
BL_DEFINE_ENUM_FLAGS(PixelFlags)

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
//!     - "i"  - Inverted pixel alpha values (IA).
//!     - "m"  - Mask (not part of the pixel itself, comes from a FillPart).
//!     - "im" - Mask (not part of the pixel itself, comes from a FillPart).
class Pixel {
public:
  PixelType _type;
  char _name[15];

  PixelFlags _flags;
  PixelCount _count;

  //! Scalar alpha component (single value only, no packing/unpacking here).
  x86::Gp sa;
  //! Packed alpha components.
  VecArray pa;
  //! Unpacked alpha components.
  VecArray ua;
  //! Unpacked and inverted alpha components.
  VecArray ui;
  //! Packed ARGB32 pixel(s), maximum 8, 16, or 32, depending on SIMD width.
  VecArray pc;
  //! Unpacked ARGB32 pixel(s), maximum 8, 16, or 32, depending on SIMD width.
  VecArray uc;

  BL_NOINLINE Pixel(PixelType type = PixelType::kNone) noexcept
    : _type(type),
      _name {},
      _flags(PixelFlags::kNone),
      _count(0) {}

  BL_NOINLINE Pixel(const char* name, PixelType type = PixelType::kNone) noexcept
    : _type(type),
      _name {},
      _flags(PixelFlags::kNone),
      _count(0) { setName(name); }

  BL_INLINE void reset(PixelType type = PixelType::kNone) noexcept {
    _type = type;
    memset(_name, 0, BL_ARRAY_SIZE(_name));
    resetAllExceptTypeAndName();
  }

  BL_NOINLINE void resetAllExceptTypeAndName() noexcept {
    _flags = PixelFlags::kNone;
    _count = 0;
    sa.reset();
    pa.reset();
    ua.reset();
    ui.reset();
    pc.reset();
    uc.reset();
  }

  BL_INLINE PixelType type() const noexcept { return _type; }
  BL_INLINE void setType(PixelType type) noexcept { _type = type; }

  BL_INLINE bool isRGBA32() const noexcept { return _type == PixelType::kRGBA32; }
  BL_INLINE bool isA8() const noexcept { return _type == PixelType::kA8; }

  BL_INLINE const char* name() const noexcept { return _name; }

  BL_NOINLINE void setName(const char* name) noexcept {
    size_t len = strnlen(name, BL_ARRAY_SIZE(_name) - 2);
    _name[0] = '\0';

    if (len) {
      memcpy(_name, name, len);
      _name[len + 0] = '.';
      _name[len + 1] = '\0';
    }
  }

  BL_INLINE PixelFlags flags() const noexcept { return _flags; }
  //! Tests whether all members are immutable (solid fills).
  BL_INLINE bool isImmutable() const noexcept { return blTestFlag(_flags, PixelFlags::kImmutable); }
  //! Tests whether this pixel was a partial fetch (the last pixel could be missing).
  BL_INLINE bool isLastPartial() const noexcept { return blTestFlag(_flags, PixelFlags::kLastPartial); }

  BL_INLINE void makeImmutable() noexcept { _flags |= PixelFlags::kImmutable; }

  BL_INLINE void setImmutable(bool immutable) noexcept {
    _flags = (_flags & ~PixelFlags::kImmutable) | (immutable ? PixelFlags::kImmutable : PixelFlags::kNone);
  }

  BL_INLINE PixelCount count() const noexcept { return _count; }
  BL_INLINE void setCount(PixelCount count) noexcept { _count = count; }
};

//! Optimized pixel representation used by solid fills.
//!
//! Used by both Alpha and RGBA pixel pipelines.
class SolidPixel {
public:
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

enum class PredicateFlags : uint32_t {
  //! No flags specified.
  kNone = 0x00000000u,

  //! Predicate is never empty - contains at least 1 element to read/write.
  //!
  //! This is a hint to the implementation that can be also used as an assertion.
  kNeverEmpty = 0x00000001u,

  //! Predicate is never full - contains at most `size() - 1` elements to read/write.
  //!
  //! This is a hint to the implementation that can be also used as an assertion.
  kNeverFull = 0x00000002u,

  kNeverEmptyOrFull = kNeverEmpty | kNeverFull
};
BL_DEFINE_ENUM_FLAGS(PredicateFlags);

//! Provides an abstraction regarding predicated loads and stores.
//!
//! Predicated composition may improve performance of span tails if the number of pixels to process is greater
//! than 1 and the processing pipeline can efficiently process more than 4 pixels. In that case it's better to
//! always use predicated loads and stores even if it would have to be emitted as branches.
//!
//! Predicates can also be used without masking, however, in that case branches may be emitted instead of
//! predicated (or masked) loads and stores. This is selected automatically depending on the CPU microarchitecture
//! and features.
struct PixelPredicate {
  //! Maximum number of pixels that can be loaded / stored.
  //!
  //! This is typically power of 2 minus one - for example 8 pixel wide pipeline would use predicated loads and
  //! stores for 0-7 pixels.
  uint32_t _size = 0;
  //! Predicate flags.
  PredicateFlags _flags;

  //! Number of pixels to load/store (starting at #0).
  //!
  //! For example if count is 3, pixels at [0, 1, 2] will be fetched / stored.
  x86::Gp count;
  //! AVX-512 predicate (mask) register.
  x86::KReg k;
  //! Vector of 32-bit masks.
  x86::Vec v32;
  //! Vector of 64-bit masks.
  x86::Vec v64;

  BL_INLINE PixelPredicate() noexcept = default;
  BL_INLINE explicit PixelPredicate(uint32_t size, PredicateFlags flags, const x86::Gp& i) noexcept { init(size, flags, i); }

  BL_INLINE bool empty() const noexcept { return _size == 0; }
  BL_INLINE uint32_t size() const noexcept { return _size; }

  BL_INLINE PredicateFlags flags() const noexcept { return _flags; }
  BL_INLINE bool isNeverEmpty() const noexcept { return blTestFlag(_flags, PredicateFlags::kNeverEmpty); }
  BL_INLINE bool isNeverFull() const noexcept { return blTestFlag(_flags, PredicateFlags::kNeverFull); }

  BL_INLINE void init(uint32_t size, PredicateFlags flags, const x86::Gp& i) noexcept {
    _size = size;
    _flags = flags;
    count = i;
  }
};

} // {JIT}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_PIPEGENCORE_P_H_INCLUDED
