// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_PIPEGEN_PIPEPART_P_H
#define BLEND2D_PIPEGEN_PIPEPART_P_H

#include "../pipegen/pipegencore_p.h"
#include "../pipegen/piperegusage_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_pipegen
//! \{

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::PipePart]
// ============================================================================

//! A base interface of a pipeline part.
//!
//! This class has basically no functionality, it just defines members that all
//! parts inherit and can use. Also, since many parts use virtual functions to
//! make them extensible `Part` must contain at least one virtual function
//! too.
class PipePart {
public:
  BL_OVERRIDE_NEW_DELETE(PipePart)

  //! PipePart type.
  enum Type {
    kTypeComposite        = 0,           //!< Composite two `FetchPart` parts.
    kTypeFetch            = 1,           //!< Fetch part.
    kTypeFill             = 2            //!< Fill part.
  };

  //! PipePart flags.
  enum Flags : uint32_t {
    kFlagPrepareDone      = 0x00000001u, //!< Prepare was already called().
    kFlagPreInitDone      = 0x00000002u, //!< Part was already pre-initialized.
    kFlagPostInitDone     = 0x00000004u  //!< Part was already post-initialized.
  };

  //! Reference to `PipeCompiler`.
  PipeCompiler* pc;
  //! Reference to `asmjit::x86::Compiler`.
  x86::Compiler* cc;

  //! Part type.
  uint8_t _partType;
  //! Count of children parts, cannot be greater than the capacity of `_children`.
  uint8_t _childrenCount;

  //! Maximum SIMD width this part supports.
  uint8_t _maxSimdWidthSupported;
  //! Informs to conserve a particular group of registers.
  uint8_t _hasLowRegs[kNumVirtGroups];

  //! Part flags, see `Part::Flags`.
  uint32_t _flags;

  //! Used to store children parts, can be introspected as well.
  PipePart* _children[2];

  //! Number of persistent registers the part requires.
  PipeRegUsage _persistentRegs;
  //! Number of persistent registers the part can spill to decrease the pressure.
  PipeRegUsage _spillableRegs;
  //! Number of temporary registers the part uses.
  PipeRegUsage _temporaryRegs;

  //! A global initialization hook.
  //!
  //! This hook is acquired during initialization phase of the part. Please do
  //! not confuse this with loop initializers that contain another hook that
  //! is used during the loop only. Initialization hooks define an entry for
  //! the part where an additional  code can be injected at any time during
  //! pipeline construction.
  asmjit::BaseNode* _globalHook;

  PipePart(PipeCompiler* pc, uint32_t partType) noexcept;

  template<typename T>
  inline T* as() noexcept { return static_cast<T*>(this); }
  template<typename T>
  inline const T* as() const noexcept { return static_cast<const T*>(this); }

  //! Tests whether the part is initialized
  inline bool isPartInitialized() const noexcept { return _globalHook != nullptr; }
  //! Returns the part type.
  inline uint32_t partType() const noexcept { return _partType; }

  //! Tests whether the part should restrict using GP registers.
  inline uint8_t hasLowRegs(uint32_t rKind) const noexcept { return _hasLowRegs[rKind]; }

  //! Tests whether the part should restrict using GP registers.
  inline uint8_t hasLowGpRegs() const noexcept { return hasLowRegs(x86::Reg::kGroupGp); }
  //! Tests whether the part should restrict using MM registers.
  inline uint8_t hasLowMmRegs() const noexcept { return hasLowRegs(x86::Reg::kGroupMm); }
  //! Tests whether the part should restrict using XMM/YMM registers.
  inline uint8_t hasLowVecRegs() const noexcept { return hasLowRegs(x86::Reg::kGroupVec); }

  inline uint32_t flags() const noexcept { return _flags; }

  //! Returns the number children parts.
  inline uint32_t childrenCount() const noexcept { return _childrenCount; }
  //! Returns children parts as an array.
  inline PipePart** children() const noexcept { return (PipePart**)_children; }

  //! Prepares the part - it would call `prepare()` on all child parts.
  virtual void preparePart() noexcept;

  //! Calls `preparePart()` on all children and also prevents calling it
  //! multiple times.
  void prepareChildren() noexcept;

  inline void _initGlobalHook(asmjit::BaseNode* node) noexcept {
    // Can be initialized only once.
    BL_ASSERT(_globalHook == nullptr);
    _globalHook = node;
  }

  inline void _finiGlobalHook() noexcept {
    // Initialized by `_initGlobalHook()`, cannot be null here.
    BL_ASSERT(_globalHook != nullptr);
    _globalHook = nullptr;
  }
};

} // {BLPipeGen}

//! \}
//! \endcond

#endif // BLEND2D_PIPEGEN_PIPEPART_P_H
