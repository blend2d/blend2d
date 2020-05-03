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

#ifndef BLEND2D_PIPEGEN_PIPEGENCORE_P_H_INCLUDED
#define BLEND2D_PIPEGEN_PIPEGENCORE_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../compop_p.h"
#include "../format_p.h"
#include "../image_p.h"
#include "../pipedefs_p.h"
#include "../rgba_p.h"
#include "../support_p.h"
#include "../tables_p.h"

// External dependencies of BLPipeGen.
#if BL_TARGET_ARCH_X86
  #include <asmjit/x86.h>
#endif

//! \cond INTERNAL
//! \addtogroup blend2d_internal_pipegen
//! \{

//! Everything related to pipeline generator is within `BLPipeGen` namespace.
namespace BLPipeGen {

// ============================================================================
// [Forward Declarations]
// ============================================================================

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

// ============================================================================
// [AsmJit-Integration]
// ============================================================================

namespace x86 { using namespace ::asmjit::x86; }

using asmjit::imm;
using asmjit::Imm;
using asmjit::Label;
using asmjit::Operand;
using asmjit::Operand_;

// ============================================================================
// [BLPipeGen::Limits]
// ============================================================================

enum Limits {
  kNumVirtGroups = asmjit::BaseReg::kGroupVirt
};

// ============================================================================
// [BLPipeGen::CMaskLoopType]
// ============================================================================

//! Pipeline generator loop-type, used by fillers & compositors.
enum CMaskLoopType : uint32_t  {
  //! Not in a loop-mode.
  kCMaskLoopTypeNone    = 0,
  //! CMask opaque loop (alpha is 1.0).
  kCMaskLoopTypeOpaque  = 1,
  //! CMask masked loop (alpha is not 1.0).
  kCMaskLoopTypeMask    = 2
};

// ============================================================================
// [BLPipeGen::BLPipeOptLevel]
// ============================================================================

//! Pipeline optimization level.
enum OptLevel : uint32_t {
  //! Safest optimization level.
  kOptLevel_None        = 0,

  //! SSE2+ optimization level (minimum on X86).
  kOptLevel_X86_SSE2    = 1,
  //! SSE3+ optimization level.
  kOptLevel_X86_SSE3    = 2,
  //! SSSE3+ optimization level.
  kOptLevel_X86_SSSE3   = 3,
  //! SSE4.1+ optimization level.
  kOptLevel_X86_SSE4_1  = 4,
  //! SSE4.2+ optimization level.
  kOptLevel_X86_SSE4_2  = 5,
  //! AVX+ optimization level.
  kOptLevel_X86_AVX     = 6,
  //! AVX2+ optimization level.
  kOptLevel_X86_AVX2    = 7
};

// ============================================================================
// [BLPipeGen::OpArray]
// ============================================================================

class OpArray {
public:
  enum { kMaxSize = 4 };

  BL_INLINE OpArray() noexcept { reset(); }
  BL_INLINE explicit OpArray(const Operand_& op0) noexcept { init(op0); }
  BL_INLINE OpArray(const Operand_& op0, const Operand_& op1) noexcept { init(op0, op1); }
  BL_INLINE OpArray(const Operand_& op0, const Operand_& op1, const Operand_& op2) noexcept { init(op0, op1, op2); }
  BL_INLINE OpArray(const Operand_& op0, const Operand_& op1, const Operand_& op2, const Operand_& op3) noexcept { init(op0, op1, op2, op3); }
  BL_INLINE OpArray(const OpArray& other) noexcept { init(other); }

  BL_INLINE OpArray& operator=(const OpArray& other) noexcept {
    init(other);
    return *this;
  }

protected:
  // Used internally to implement `low()`, `high()`, `even()`, and `odd()`.
  BL_INLINE OpArray(const OpArray& other, uint32_t from, uint32_t inc, uint32_t limit) noexcept {
    uint32_t di = 0;
    for (uint32_t si = from; si < limit; si += inc)
      v[di++] = other[si];
    _size = di;
  }

public:
  BL_INLINE void init(const Operand_& op0) noexcept {
    _size = 1;
    v[0] = op0;
    v[1].reset();
    v[2].reset();
    v[3].reset();
  }

  BL_INLINE void init(const Operand_& op0, const Operand_& op1) noexcept {
    _size = 2;
    v[0] = op0;
    v[1] = op1;
    v[2].reset();
    v[3].reset();
  }

  BL_INLINE void init(const Operand_& op0, const Operand_& op1, const Operand_& op2) noexcept {
    _size = 3;
    v[0] = op0;
    v[1] = op1;
    v[2] = op2;
    v[3].reset();
  }

  BL_INLINE void init(const Operand_& op0, const Operand_& op1, const Operand_& op2, const Operand_& op3) noexcept {
    _size = 4;
    v[0] = op0;
    v[1] = op1;
    v[2] = op2;
    v[3] = op3;
  }

  BL_INLINE void init(const OpArray& other) noexcept {
    _size = other._size;
    if (_size)
      memcpy(v, other.v, _size * sizeof(Operand_));
  }

  //! Resets `OpArray` to a default construction state.
  BL_INLINE void reset() noexcept {
    _size = 0;
    for (uint32_t i = 0; i < kMaxSize; i++)
      v[i].reset();
  }

  //! Tests whether the vector is empty (has no elements).
  BL_INLINE bool empty() const noexcept { return _size == 0; }
  //! Tests whether the vector has only one element, which makes it scalar.
  BL_INLINE bool isScalar() const noexcept { return _size == 1; }
  //! Tests whether the vector has more than 1 element, which means that
  //! calling `high()` and `odd()` won't return an empty vector.
  BL_INLINE bool isVector() const noexcept { return _size > 1; }

  //! Returns the number of vector elements.
  BL_INLINE uint32_t size() const noexcept { return _size; }

  BL_INLINE Operand_& operator[](size_t index) noexcept {
    BL_ASSERT(index < _size);
    return v[index];
  }

  BL_INLINE const Operand_& operator[](size_t index) const noexcept {
    BL_ASSERT(index < _size);
    return v[index];
  }

  BL_INLINE OpArray lo() const noexcept { return OpArray(*this, 0, 1, (_size + 1) / 2); }
  BL_INLINE OpArray hi() const noexcept { return OpArray(*this, _size > 1 ? (_size + 1) / 2 : 0, 1, _size); }
  BL_INLINE OpArray even() const noexcept { return OpArray(*this, 0, 2, _size); }
  BL_INLINE OpArray odd() const noexcept { return OpArray(*this, _size > 1, 2, _size); }

  //! Returns a new vector consisting of either even (from == 0) or odd
  //! (from == 1) elements. It's like calling `even()` and `odd()`, but
  //! can be used within a loop that performs the same operation for both.
  BL_INLINE OpArray even_odd(uint32_t from) const noexcept { return OpArray(*this, _size > 1 ? from : 0, 2, _size); }

  uint32_t _size;
  Operand_ v[kMaxSize];
};

template<typename T>
class OpArrayT : public OpArray {
public:
  BL_INLINE OpArrayT() noexcept : OpArray() {}
  BL_INLINE explicit OpArrayT(const T& op0) noexcept : OpArray(op0) {}
  BL_INLINE OpArrayT(const T& op0, const T& op1) noexcept : OpArray(op0, op1) {}
  BL_INLINE OpArrayT(const T& op0, const T& op1, const T& op2) noexcept : OpArray(op0, op1, op2) {}
  BL_INLINE OpArrayT(const T& op0, const T& op1, const T& op2, const T& op3) noexcept : OpArray(op0, op1, op2, op3) {}
  BL_INLINE OpArrayT(const OpArrayT<T>& other) noexcept : OpArray(other) {}

  BL_INLINE OpArrayT& operator=(const OpArrayT<T>& other) noexcept {
    init(other);
    return *this;
  }

protected:
  BL_INLINE OpArrayT(const OpArrayT<T>& other, uint32_t n, uint32_t from, uint32_t inc) noexcept : OpArray(other, n, from, inc) {}

public:
  BL_INLINE void init(const T& op0) noexcept { OpArray::init(op0); }
  BL_INLINE void init(const T& op0, const T& op1) noexcept { OpArray::init(op0, op1); }
  BL_INLINE void init(const T& op0, const T& op1, const T& op2) noexcept { OpArray::init(op0, op1, op2); }
  BL_INLINE void init(const T& op0, const T& op1, const T& op2, const T& op3) noexcept { OpArray::init(op0, op1, op2, op3); }
  BL_INLINE void init(const OpArrayT<T>& other) noexcept { OpArray::init(other); }

  BL_INLINE T& operator[](size_t index) noexcept {
    BL_ASSERT(index < _size);
    return static_cast<T&>(v[index]);
  }

  BL_INLINE const T& operator[](size_t index) const noexcept {
    BL_ASSERT(index < _size);
    return static_cast<const T&>(v[index]);
  }

  BL_INLINE T& atUnrestricted(size_t index) noexcept {
    BL_ASSERT(index < kMaxSize);
    return static_cast<T&>(v[index]);
  }

  BL_INLINE const T& atUnrestricted(size_t index) const noexcept {
    BL_ASSERT(index < kMaxSize);
    return static_cast<const T&>(v[index]);
  }

  BL_INLINE OpArrayT<T> lo() const noexcept { return OpArrayT<T>(*this, 0, 1, (_size + 1) / 2); }
  BL_INLINE OpArrayT<T> hi() const noexcept { return OpArrayT<T>(*this, _size > 1 ? (_size + 1) / 2 : 0, 1, _size); }
  BL_INLINE OpArrayT<T> even() const noexcept { return OpArrayT<T>(*this, 0, 2, _size); }
  BL_INLINE OpArrayT<T> odd() const noexcept { return OpArrayT<T>(*this, _size > 1, 2, _size); }
  BL_INLINE OpArrayT<T> even_odd(uint32_t from) const noexcept { return OpArrayT<T>(*this, _size > 1 ? from : 0, 2, _size); }
};

typedef OpArrayT<x86::Vec> VecArray;

// ============================================================================
// [BLPipeGen::OpAccess]
// ============================================================================

BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)

template<typename T>
struct OpAccessImpl {
  typedef T Input;
  typedef T Output;

  static BL_INLINE uint32_t size(const Input& op) noexcept { return 1; }
  static BL_INLINE Output& at(Input& op, size_t i) noexcept { return op; }
  static BL_INLINE const Output& at(const Input& op, size_t i) noexcept { return op; }
};

template<>
struct OpAccessImpl<OpArray> {
  typedef OpArray Input;
  typedef Operand_ Output;

  static BL_INLINE uint32_t size(const Input& op) noexcept { return op.size(); }
  static BL_INLINE Output& at(Input& op, size_t i) noexcept { return op[i]; }
  static BL_INLINE const Output& at(const Input& op, size_t i) noexcept { return op[i]; }
};

struct OpAccess {
  template<typename OpT>
  static BL_INLINE uint32_t opCount(const OpT& op) noexcept { return OpAccessImpl<OpT>::size(op); }
  template<typename OpT>
  static BL_INLINE typename OpAccessImpl<OpT>::Output& at(OpT& op, size_t i) noexcept { return OpAccessImpl<OpT>::at(op, i); }
  template<typename OpT>
  static BL_INLINE const typename OpAccessImpl<OpT>::Output& at(const OpT& op, size_t i) noexcept { return OpAccessImpl<OpT>::at(op, i); }
};

BL_DIAGNOSTIC_POP

// ============================================================================
// [BLPipeGen::ScopedInjector]
// ============================================================================

class ScopedInjector {
public:
  asmjit::BaseCompiler* cc;
  asmjit::BaseNode** hook;
  asmjit::BaseNode* prev;

  BL_NONCOPYABLE(ScopedInjector)

  BL_INLINE ScopedInjector(asmjit::BaseCompiler* cc, asmjit::BaseNode** hook) noexcept
    : cc(cc),
      hook(hook),
      prev(cc->setCursor(*hook)) {}

  BL_INLINE ~ScopedInjector() noexcept {
    *hook = cc->setCursor(prev);
  }
};

// ============================================================================
// [BLPipeGen::JitUtils]
// ============================================================================

//! Utilities used by PipeCompiler and other parts of the library.
struct JitUtils {
  template<typename T>
  static BL_INLINE void resetVarArray(T* array, size_t size) noexcept {
    for (size_t i = 0; i < size; i++)
      array[i].reset();
  }

  template<typename T>
  static BL_INLINE void resetVarStruct(T* data, size_t size = sizeof(T)) noexcept {
    resetVarArray(reinterpret_cast<asmjit::BaseReg*>(data), size / sizeof(asmjit::BaseReg));
  }
};

// ============================================================================
// [BLPipeGen::Pixel]
// ============================================================================

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
  enum Type : uint8_t {
    kTypeNone = 0,
    kTypeAlpha = 1,
    kTypeRGBA = 2
  };

  enum BaseFlags : uint32_t {
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

  uint32_t _type;
  uint32_t _flags;
  uint32_t _count;

  //! Scalar alpha component (single value only, no packing/unpacking here).
  x86::Gp sa;
  //! Packed alpha components.
  VecArray pa;
  //! Unpacked alpha components.
  VecArray ua;
  //! Unpacked and inverted alpha components.
  VecArray uia;
  //! Packed ARGB32 pixel(s), maximum 8.
  VecArray pc;
  //! Unpacked ARGB32 pixel(s), maximum 8.
  VecArray uc;

  BL_INLINE Pixel(uint32_t type = kTypeNone) noexcept
    : _type(type),
      _flags(0),
      _count(0) {}

  BL_INLINE void reset(uint32_t type = kTypeNone) noexcept {
    _type = type;
    resetAllExceptType();
  }

  BL_INLINE void resetAllExceptType() noexcept {
    _flags = 0;
    _count = 0;
    sa.reset();
    pa.reset();
    ua.reset();
    uia.reset();
    pc.reset();
    uc.reset();
  }

  BL_INLINE uint32_t type() const noexcept { return _type; }
  BL_INLINE uint32_t flags() const noexcept { return _flags; }
  BL_INLINE uint32_t count() const noexcept { return _count; }

  BL_INLINE void setType(uint32_t type) noexcept { _type = type; }
  BL_INLINE void setCount(uint32_t count) noexcept { _count = count; }

  BL_INLINE bool isAlpha() const noexcept { return _type == kTypeAlpha; }
  BL_INLINE bool isRGBA() const noexcept { return _type == kTypeRGBA; }

  //! Tests whether all members are immutable (solid fills).
  BL_INLINE bool isImmutable() const noexcept { return (_flags & kImmutable) != 0; }
  //! Tests whether this pixel was a partial fetch (the last pixel could be missing).
  BL_INLINE bool isLastPartial() const noexcept { return (_flags & kLastPartial) != 0; }

  BL_INLINE void makeImmutable() noexcept { _flags |= kImmutable; }

  BL_INLINE void setImmutable(bool immutable) noexcept {
    _flags = (_flags & ~kImmutable) | (immutable ? kImmutable : 0u);
  }
};

// ============================================================================
// [BLPipeGen::SolidPixel]
// ============================================================================

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

// ============================================================================
// [BLPipeGen::PipeCMask]
// ============================================================================

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

} // {BLPipeGen}

//! \}
//! \endcond

#endif // BLEND2D_PIPEGEN_PIPEGENCORE_P_H_INCLUDED
