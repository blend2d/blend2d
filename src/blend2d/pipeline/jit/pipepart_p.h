// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_PIPEPART_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_PIPEPART_P_H_INCLUDED

#include "../../pipeline/jit/pipeprimitives_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace bl {
namespace Pipeline {
namespace JIT {

//! Pipeline part type.
enum class PipePartType : uint8_t {
  //! Fill part.
  kFill = 0,
  //! Fetch part.
  kFetch = 1,
  //! Composite part, which uses two fetch parts and composites them.
  kComposite = 2
};

//! Pipeline part flags.
enum class PipePartFlags : uint32_t {
  //! No flags.
  kNone = 0,
  //! Part was already prepared.
  kPrepareDone = 0x00000001u,
  //! Part supports masked access (fetching / storing pixels predicated by a mask).
  kMaskedAccess = 0x00000002u,

  //! Fetch is always rectangular, thus the fetcher should optimize for this case.
  kRectFill = 0x00000010u,

  //! This part performs expensive operations.
  //!
  //!   - if it's a fetcher, the fetch is expensive.
  //!   - if it's a CompOp the composition is expensive.
  //!   - other parts don't use this flag.
  kExpensive = 0x00000020u,

  //! Fetching always performs multiple pixels at once, thus the fetcher always stays in vectorized N mode
  //! and not in scalar mode. This flag helps with avoiding enterN() and leaveN() when entering main loops.
  kAlwaysMultiple = 0x00000040u,

  //! Advancing in X direction is simple and can be called even with zero `x`.
  kAdvanceXIsSimple = 0x0001000u,
  //! Advancing in X direction needs the final X coordinate for calculations.
  kAdvanceXNeedsX = 0x00020000u,
  //! Advancing in X direction needs `delta` argument for calculations.
  kAdvanceXNeedsDiff = 0x00040000u,

  kFetchFlags = kAdvanceXIsSimple | kAdvanceXNeedsX | kAdvanceXNeedsDiff
};
BL_DEFINE_ENUM_FLAGS(PipePartFlags)

//! A base class used by all pipeline parts.
//!
//! This class has basically no functionality, it just defines members that all parts inherit and can use.
//!
//! \note Since many parts use virtual functions to make them extensible `PipePart` must contain at least one virtual
//! function too.
class PipePart {
public:
  BL_NONCOPYABLE(PipePart)
  BL_OVERRIDE_NEW_DELETE(PipePart)

  //! \name Members
  //! \{

  //! Pointer to `PipeCompiler`.
  PipeCompiler* pc = nullptr;
  //! Pointer to `asmjit::<arch>::Compiler`.
  AsmCompiler* cc = nullptr;

  //! Reference to a common constant pool.
  const CommonTable& ct;

  //! Part type.
  PipePartType _partType {};
  //! Count of children parts, cannot be greater than the capacity of `_children`.
  uint8_t _childCount = 0;
  //! Maximum SIMD width this part supports.
  VecWidth _maxVecWidthSupported = VecWidth::k128;

  //! Part flags.
  PipePartFlags _partFlags = PipePartFlags::kNone;

  //! Used to store children parts, can be introspected as well.
  PipePart* _children[2] {};

  //! A global initialization hook.
  //!
  //! This hook is acquired during initialization phase of the part. Please do
  //! not confuse this with loop initializers that contain another hook that
  //! is used during the loop only. Initialization hooks define an entry for
  //! the part where an additional  code can be injected at any time during
  //! pipeline construction.
  asmjit::BaseNode* _globalHook = nullptr;

  //! \}

  //! \name Construction & Destruction
  //! \{

  PipePart(PipeCompiler* pc, PipePartType partType) noexcept;

  //! \}

  //! \name Accessors
  //! \{

  template<typename T>
  BL_INLINE_NODEBUG T* as() noexcept { return static_cast<T*>(this); }
  template<typename T>
  BL_INLINE_NODEBUG const T* as() const noexcept { return static_cast<const T*>(this); }

  //! Tests whether the part is initialized
  BL_INLINE_NODEBUG bool isPartInitialized() const noexcept { return _globalHook != nullptr; }
  //! Returns the type of the part.
  BL_INLINE_NODEBUG PipePartType partType() const noexcept { return _partType; }
  //! Returns PipePart flags.
  BL_INLINE_NODEBUG PipePartFlags partFlags() const noexcept { return _partFlags; }
  //! Tests whether this part has the given `flag` set.
  BL_INLINE_NODEBUG bool hasPartFlag(PipePartFlags flag) const noexcept { return blTestFlag(_partFlags, flag); }
  //! Adds new `flags` to the current \ref PipePartFlags.
  BL_INLINE_NODEBUG void addPartFlags(PipePartFlags flags) noexcept { _partFlags |= flags; }
  //! Adds new `flags` to the current \ref PipePartFlags.
  BL_INLINE_NODEBUG void removePartFlags(PipePartFlags flags) noexcept { _partFlags &= ~flags; }

  //! Tests whether the fetch is currently initialized for a rectangular fill.
  BL_INLINE_NODEBUG bool isRectFill() const noexcept { return hasPartFlag(PipePartFlags::kRectFill); }
  //! Tests whether a compositor or fetcher perform expensive operations.
  BL_INLINE_NODEBUG bool isExpensive() const noexcept { return hasPartFlag(PipePartFlags::kExpensive); }

  //! Tests whether masked access is available.
  //!
  //! \note This is more a hint than a feature as masked access must be supported by all fetchers.
  BL_INLINE_NODEBUG bool hasMaskedAccess() const noexcept { return hasPartFlag(PipePartFlags::kMaskedAccess); }

  //! Returns the maximum supported SIMD width.
  BL_INLINE_NODEBUG VecWidth maxVecWidthSupported() const noexcept { return _maxVecWidthSupported; }

  //! Returns the number of children.
  BL_INLINE_NODEBUG uint32_t childCount() const noexcept { return _childCount; }
  //! Returns children parts as an array.
  BL_INLINE_NODEBUG PipePart** children() const noexcept { return (PipePart**)_children; }

  //! \}

  //! \name Prepare
  //! \{

  virtual void preparePart() noexcept;

  //! \}

  //! \name Children
  //! \{

  template<typename Function>
  void forEachPart(Function&& f) noexcept {
    uint32_t n = childCount();
    for (uint32_t i = 0; i < n; i++) {
      PipePart* child = children()[i];
      child->forEachPart(BLInternal::forward<Function>(f));
    }

    f(this);
  }

  template<typename Function>
  void forEachPartAndMark(PipePartFlags flag, Function&& f) noexcept {
    _partFlags |= flag;

    uint32_t n = childCount();
    for (uint32_t i = 0; i < n; i++) {
      PipePart* child = children()[i];
      if (uint32_t(child->partFlags() & flag) == 0)
        child->forEachPartAndMark(flag, BLInternal::forward<Function>(f));
    }

    f(this);
  }

  //! \}

  //! \name Hooks
  //! \{

  BL_INLINE void _initGlobalHook(asmjit::BaseNode* node) noexcept {
    // Can be initialized only once.
    BL_ASSERT(_globalHook == nullptr);
    _globalHook = node;
  }

  BL_INLINE void _finiGlobalHook() noexcept {
    // Initialized by `_initGlobalHook()`, cannot be null here.
    BL_ASSERT(_globalHook != nullptr);
    _globalHook = nullptr;
  }

  //! \}
};

} // {JIT}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_PIPEPART_P_H_INCLUDED
