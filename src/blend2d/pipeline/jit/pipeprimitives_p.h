// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_PIPEPRIMITIVES_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_PIPEPRIMITIVES_P_H_INCLUDED

#include "../../api-internal_p.h"
#include "../../compop_p.h"
#include "../../format_p.h"
#include "../../image_p.h"
#include "../../rgba_p.h"
#include "../../pipeline/pipedefs_p.h"
#include "../../pipeline/jit/jitbase_p.h"
#include "../../tables/tables_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

//! \namespace bl::Pipeline::JIT
//! Everything related to JIT pipeline generator and runtime.

namespace bl {
namespace Pipeline {
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
class FillAnalyticPart;

//! Pipeline generator loop-type, used by fillers & compositors.
enum class CMaskLoopType : uint8_t {
  //! Not in a cmask loop mode.
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
  kRGBA32 = 2,
  kRGBA64 = 3
};

enum class PixelFlags : uint32_t {
  kNone = 0,

  //! Scalar alpha or stencil value in `Pixel::sa` (single pixel quantities only).
  kSA = 0x00000001u,
  //! Packed alpha or stencil components stored in `Pixel::pa`.
  kPA = 0x00000002u,
  //! Packed inverted alpha or stencil components stored in `Pixel::pi`.
  kPI = 0x00000004u,
  //! Unpacked alpha or stencil components stored in `Pixel::ua`.
  kUA = 0x00000008u,
  //! Unpacked and inverted alpha or stencil components stored in `Pixel::ui`
  kUI = 0x00000010u,

  //! Packed ARGB32 components stored in `Pixel::pc`.
  kPC = 0x00000020u,
  //! Unpacked ARGB32 components stored in `Pixel::uc`.
  kUC = 0x00000040u,

  //! Last fetch in this scanline, thus at most `N-1` pixels would be used.
  kLastPartial = 0x40000000u,
  //! Fetch read-only, registers won't be modified.
  kImmutable = 0x80000000u,

  kPA_PI_UA_UI = kPA | kPI | kUA | kUI,
  kPC_UC = kPC | kUC
};
BL_DEFINE_ENUM_FLAGS(PixelFlags)

//! Pixel coverage format that is consumed by the compositor.
enum class PixelCoverageFormat : uint8_t {
  //! Uninitialized format (invalid when passed to API that expects an initialized one).
  kNone = 0,
  //! Pixel coverage must be packed.
  kPacked,
  //! Pixel coverage must be unpacked.
  kUnpacked
};

//! Pixel coverage flags used by \ref PixelCoverage.
enum class PixelCoverageFlags : uint8_t {
  //! No coverage flags set.
  kNone = 0,
  //! The coverage is repeated (c-mask fills).
  kRepeated = 0x01,
  //! The coverage is immutable (cannot be altered by the compositor).
  kImmutable = 0x02,

  //! A combination of `kRepeated` and `kImmutable`.
  kRepeatedImmutable = kRepeated | kImmutable
};
BL_DEFINE_ENUM_FLAGS(PixelCoverageFlags)

//! Specifies whether to advance pointers.
enum class AdvanceMode : uint32_t {
  kNoAdvance,
  kAdvance,
  kIgnored
};

//! Specifies gather options.
enum class GatherMode : uint32_t {
  kFetchAll = 0,
  kNeverFull = 1
};

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
  Gp sa;
  //! Packed alpha components.
  VecArray pa;
  //! Packed inverted alpha components.
  VecArray pi;
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

  BL_INLINE bool isA8() const noexcept { return _type == PixelType::kA8; }
  BL_INLINE bool isRGBA32() const noexcept { return _type == PixelType::kRGBA32; }
  BL_INLINE bool isRGBA64() const noexcept { return _type == PixelType::kRGBA64; }

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
  Gp sa;
  //! Scalar pre-processed component, shown as "X" in equations.
  Gp sx;
  //! Scalar pre-processed component, shown as "Y" in equations.
  Gp sy;

  //! Packed pre-processed components, shown as "X" in equations.
  Vec px;
  //! Packed pre-processed components, shown as "Y" in equations.
  Vec py;
  //! Unpacked pre-processed components, shown as "X" in equations.
  Vec ux;
  //! Unpacked pre-processed components, shown as "Y" in equations.
  Vec uy;

  //! Mask vector.
  Vec vm;
  //! Inverted mask vector.
  Vec vn;

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
  Gp sm;
  //! Inverted mask scalar.
  Gp sn;

  //! Mask vector.
  Vec vm;
  //! Inverted mask vector.
  Vec vn;

  BL_INLINE void reset() noexcept {
    JitUtils::resetVarStruct<PipeCMask>(this);
  }
};

enum class PredicateFlags : uint32_t {
  //! No flags specified.
  kNone = 0x00000000u,

  //! Predicate is never full - contains at most `size() - 1` elements to read/write.
  //!
  //! This is a hint to the implementation that can be also used as an assertion.
  kNeverFull = 0x00000001u
};
BL_DEFINE_ENUM_FLAGS(PredicateFlags)

//! Provides an abstraction regarding predicated loads and stores.
//!
//! Predicated composition may improve performance of span tails if the number of pixels to process is greater
//! than 1 and the processing pipeline can efficiently process more than 4 pixels. In that case it's better to
//! always use predicated loads and stores even if it would have to be emitted as branches.
//!
//! Predicates can also be used without masking, however, in that case branches may be emitted instead of
//! predicated (or masked) loads and stores. This is selected automatically depending on the CPU micro-architecture
//! and features.
struct PixelPredicate {
  //! Maximum number of elements that can be loaded / stored.
  //!
  //! This is typically power of 2 minus one - for example 8 pixel wide pipeline would use predicated loads and
  //! stores for 0-7 pixels.
  uint32_t _size {};
  //! Predicate flags.
  PredicateFlags _flags {};

  //! Number of pixels to load/store (starting at #0).
  //!
  //! For example if count is 3, pixels at [0, 1, 2] will be fetched / stored.
  Gp _count;

#if defined(BL_JIT_ARCH_X86)
  static constexpr uint32_t kMaterializedMaskCapacity = 2u;

  //! Contains predicates for load/store instructions that were materialized.
  struct MaterializedMask {
    //! The number of elements to access from the end.
    //!
    //! Non-zero offsets are used in cases in which there is multiple registers that are written by using predicates.
    //! In that case the access to the first register can be branched, and only the access to the last register can
    //! actually use predicate, at least this is how it's been designed.
    uint8_t lastN {};

    //! Element size in case this is a vector predicate (always zero when it's a {k} predicate).
    uint8_t elementSize {};

    uint8_t reserved[2] {};

    //! Mask register - either an AVX-512 mask (k register) or an xmm/ymm/zmm vector.
    Reg mask {};
  };

  uint32_t _materializedCount {};
  MaterializedMask _materializedMasks[kMaterializedMaskCapacity];
#endif // BL_JIT_ARCH_X86

  static constexpr uint32_t kMaterializedEndPtrCapacity = 2u;

  //! Contains two last clamped pointers of `ref`.
  struct MaterializedEndPtr {
    //! Reference pointer (this is the register used to calculate `end1` and `end2`)
    Gp ref;
    //! `unsigned_min(ref + 1 * N, ref + (count - 1) * N)`.
    Gp adjusted1;
    //! `unsigned_min(ref + 2 * N, ref + (count - 1) * N)`.
    Gp adjusted2;
  };

  uint32_t _materializedEndPtrCount {};
  MaterializedEndPtr _materializedEndPtrData[kMaterializedEndPtrCapacity];

  BL_INLINE_NODEBUG PixelPredicate() noexcept = default;
  BL_INLINE explicit PixelPredicate(uint32_t size, PredicateFlags flags, const Gp& count) noexcept { init(size, flags, count); }

  BL_INLINE void init(uint32_t size, PredicateFlags flags, const Gp& count) noexcept {
    _size = size;
    _flags = flags;
    _count = count;
  }

  BL_INLINE_NODEBUG bool empty() const noexcept { return _size == 0; }
  BL_INLINE_NODEBUG uint32_t size() const noexcept { return _size; }

  BL_INLINE_NODEBUG PredicateFlags flags() const noexcept { return _flags; }
  BL_INLINE_NODEBUG bool isNeverFull() const noexcept { return blTestFlag(_flags, PredicateFlags::kNeverFull); }

  BL_INLINE_NODEBUG const Gp& count() const noexcept { return _count; }

  BL_INLINE_NODEBUG GatherMode gatherMode() const noexcept {
    return isNeverFull() ? GatherMode::kNeverFull : GatherMode::kFetchAll;
  }

  BL_INLINE const MaterializedEndPtr* findMaterializedEndPtr(const Gp& ref) const noexcept {
    for (uint32_t i = 0; i < _materializedEndPtrCount; i++)
      if (_materializedEndPtrData[i].ref.id() == ref.id())
        return &_materializedEndPtrData[i];
    return nullptr;
  }

  BL_INLINE void addMaterializedEndPtr(const Gp& ref, const Gp& adjusted1, const Gp& adjusted2) noexcept {
    if (_materializedEndPtrCount >= kMaterializedEndPtrCapacity)
      return;

    uint32_t i = _materializedEndPtrCount;
    _materializedEndPtrData[i].ref = ref;
    _materializedEndPtrData[i].adjusted1 = adjusted1;
    _materializedEndPtrData[i].adjusted2 = adjusted2;
    _materializedEndPtrCount++;
  }
};

} // {JIT}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_PIPEPRIMITIVES_P_H_INCLUDED
