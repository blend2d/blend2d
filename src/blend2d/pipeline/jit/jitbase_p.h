// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_JITBASE_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_JITBASE_P_H_INCLUDED

#include "../../api-internal_p.h"
#include "../../support/intops_p.h"

// External dependencies of BLPipeline.
#if BL_TARGET_ARCH_X86
  #include <asmjit/x86.h>
#endif

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace BLPipeline {
namespace JIT {

// AsmJit Integration
// ==================

namespace x86 { using namespace ::asmjit::x86; }

using asmjit::AlignMode;
using asmjit::CpuFeatures;
using asmjit::InstId;
using asmjit::RegMask;
using asmjit::RegType;
using asmjit::RegGroup;
using asmjit::imm;
using asmjit::Imm;
using asmjit::Label;
using asmjit::Operand;
using asmjit::Operand_;
using asmjit::OperandSignature;
using asmjit::JumpAnnotation;

// Strong Enums
// ------------

BL_DEFINE_STRONG_TYPE(PixelCount, uint32_t)
BL_DEFINE_STRONG_TYPE(Alignment, uint32_t)

// SIMD Width & Utilities
// ----------------------

//! SIMD register width.
enum class SimdWidth : uint8_t {
  //! 128-bit SIMD (baseline, SSE/AVX, NEON, ASIMD, etc...).
  k128 = 0,
  //! 256-bit SIMD (AVX2+).
  k256 = 1,
  //! 512-bit SIMD (AVX512_DQ & AVX512_BW & AVX512_VL).
  k512 = 2
};

//! SIMD data width.
enum class DataWidth : uint8_t {
  k8 = 0,
  k16 = 1,
  k32 = 2,
  k64 = 3
};

//! Broadcast width.
enum class Bcst : uint8_t {
  k8 = 0,
  k16 = 1,
  k32 = 2,
  k64 = 3,
  kNA = 0xFE,
  kNA_Unique = 0xFF
};

namespace SimdWidthUtils {

static BL_INLINE OperandSignature signatureOf(SimdWidth simdWidth) noexcept {
  static const OperandSignature table[] = {
    OperandSignature{asmjit::x86::Xmm::kSignature},
    OperandSignature{asmjit::x86::Ymm::kSignature},
    OperandSignature{asmjit::x86::Zmm::kSignature}
  };

  return table[size_t(simdWidth)];
}

static BL_INLINE asmjit::TypeId typeIdOf(SimdWidth simdWidth) noexcept {
  static const asmjit::TypeId table[] = {
    asmjit::TypeId::kInt32x4,
    asmjit::TypeId::kInt32x8,
    asmjit::TypeId::kInt32x16
  };

  return table[size_t(simdWidth)];
}

static BL_INLINE SimdWidth simdWidthOf(const x86::Vec& reg) noexcept {
  return SimdWidth(uint32_t(reg.type()) - uint32_t(RegType::kX86_Xmm));
}

static BL_INLINE SimdWidth simdWidthOf(SimdWidth simdWidth, DataWidth dataWidth, uint32_t n) noexcept {
  return SimdWidth(blMin<uint32_t>((n << uint32_t(dataWidth)) >> 5, uint32_t(simdWidth)));
}

static BL_INLINE uint32_t regCountOf(SimdWidth simdWidth, DataWidth dataWidth, uint32_t n) noexcept {
  uint32_t shift = uint32_t(simdWidth) + 4;
  uint32_t totalWidth = n << uint32_t(dataWidth);
  return (totalWidth + ((1 << shift) - 1)) >> shift;
}

static BL_INLINE uint32_t regCountOf(SimdWidth simdWidth, DataWidth dataWidth, PixelCount n) noexcept {
  return regCountOf(simdWidth, dataWidth, n.value());
}

static BL_INLINE x86::Vec cloneVecAs(const x86::Vec& src, SimdWidth simdWidth) noexcept {
  x86::Vec result(src);
  result.setSignature(signatureOf(simdWidth));
  return result;
}

} // {SimdWidthUtils}

// AsmJit Helpers
// ==============

//! Operand array used by SIMD pipeline.
//!
//! Can hold up to `kMaxSize` registers, however, the number of actual registers is dynamic and depends
//! on initialization.
class OpArray {
public:
  enum : uint32_t { kMaxSize = 4 };

  uint32_t _size;
  Operand_ v[kMaxSize];

  BL_INLINE_NODEBUG OpArray() noexcept { reset(); }
  BL_INLINE_NODEBUG explicit OpArray(const Operand_& op0) noexcept { init(op0); }
  BL_INLINE_NODEBUG OpArray(const Operand_& op0, const Operand_& op1) noexcept { init(op0, op1); }
  BL_INLINE_NODEBUG OpArray(const Operand_& op0, const Operand_& op1, const Operand_& op2) noexcept { init(op0, op1, op2); }
  BL_INLINE_NODEBUG OpArray(const Operand_& op0, const Operand_& op1, const Operand_& op2, const Operand_& op3) noexcept { init(op0, op1, op2, op3); }
  BL_INLINE_NODEBUG OpArray(const OpArray& other) noexcept { init(other); }

  BL_INLINE_NODEBUG OpArray& operator=(const OpArray& other) noexcept {
    init(other);
    return *this;
  }

protected:
  // Used internally to implement `low()`, `high()`, `even()`, and `odd()`.
  BL_INLINE_NODEBUG OpArray(const OpArray& other, uint32_t from, uint32_t inc, uint32_t limit) noexcept {
    uint32_t di = 0;
    for (uint32_t si = from; si < limit; si += inc)
      v[di++] = other[si];
    _size = di;
  }

public:
  BL_INLINE_NODEBUG void init(const Operand_& op0) noexcept {
    _size = 1;
    v[0] = op0;
    v[1].reset();
    v[2].reset();
    v[3].reset();
  }

  BL_INLINE_NODEBUG void init(const Operand_& op0, const Operand_& op1) noexcept {
    _size = 2;
    v[0] = op0;
    v[1] = op1;
    v[2].reset();
    v[3].reset();
  }

  BL_INLINE_NODEBUG void init(const Operand_& op0, const Operand_& op1, const Operand_& op2) noexcept {
    _size = 3;
    v[0] = op0;
    v[1] = op1;
    v[2] = op2;
    v[3].reset();
  }

  BL_INLINE_NODEBUG void init(const Operand_& op0, const Operand_& op1, const Operand_& op2, const Operand_& op3) noexcept {
    _size = 4;
    v[0] = op0;
    v[1] = op1;
    v[2] = op2;
    v[3] = op3;
  }

  BL_INLINE_NODEBUG void init(const OpArray& other) noexcept {
    _size = other._size;
    if (_size)
      memcpy(v, other.v, _size * sizeof(Operand_));
  }

  //! Resets `OpArray` to a default construction state.
  BL_INLINE_NODEBUG void reset() noexcept {
    _size = 0;
    for (uint32_t i = 0; i < kMaxSize; i++)
      v[i].reset();
  }

  //! Tests whether the vector is empty (has no elements).
  BL_INLINE_NODEBUG bool empty() const noexcept { return _size == 0; }
  //! Tests whether the vector has only one element, which makes it scalar.
  BL_INLINE_NODEBUG bool isScalar() const noexcept { return _size == 1; }
  //! Tests whether the vector has more than 1 element, which means that
  //! calling `high()` and `odd()` won't return an empty vector.
  BL_INLINE_NODEBUG bool isVector() const noexcept { return _size > 1; }

  //! Returns the number of vector elements.
  BL_INLINE_NODEBUG uint32_t size() const noexcept { return _size; }

  BL_INLINE bool equals(const OpArray& other) const noexcept {
    size_t count = size();
    if (count != other.size())
      return false;

    for (uint32_t i = 0; i < count; i++)
      if (v[i] != other.v[i])
        return false;

    return true;
  }

  BL_INLINE_NODEBUG bool operator==(const OpArray& other) const noexcept { return  equals(other); }
  BL_INLINE_NODEBUG bool operator!=(const OpArray& other) const noexcept { return !equals(other); }

  BL_INLINE Operand_& operator[](size_t index) noexcept {
    BL_ASSERT(index < _size);
    return v[index];
  }

  BL_INLINE const Operand_& operator[](size_t index) const noexcept {
    BL_ASSERT(index < _size);
    return v[index];
  }

  BL_INLINE_NODEBUG OpArray lo() const noexcept { return OpArray(*this, 0, 1, (_size + 1) / 2); }
  BL_INLINE_NODEBUG OpArray hi() const noexcept { return OpArray(*this, _size > 1 ? (_size + 1) / 2 : 0, 1, _size); }
  BL_INLINE_NODEBUG OpArray even() const noexcept { return OpArray(*this, 0, 2, _size); }
  BL_INLINE_NODEBUG OpArray odd() const noexcept { return OpArray(*this, _size > 1, 2, _size); }

  //! Returns a new vector consisting of either even (from == 0) or odd
  //! (from == 1) elements. It's like calling `even()` and `odd()`, but
  //! can be used within a loop that performs the same operation for both.
  BL_INLINE_NODEBUG OpArray even_odd(uint32_t from) const noexcept { return OpArray(*this, _size > 1 ? from : 0, 2, _size); }
};

template<typename T>
class OpArrayT : public OpArray {
public:
  BL_INLINE_NODEBUG OpArrayT() noexcept : OpArray() {}
  BL_INLINE_NODEBUG explicit OpArrayT(const T& op0) noexcept : OpArray(op0) {}
  BL_INLINE_NODEBUG OpArrayT(const T& op0, const T& op1) noexcept : OpArray(op0, op1) {}
  BL_INLINE_NODEBUG OpArrayT(const T& op0, const T& op1, const T& op2) noexcept : OpArray(op0, op1, op2) {}
  BL_INLINE_NODEBUG OpArrayT(const T& op0, const T& op1, const T& op2, const T& op3) noexcept : OpArray(op0, op1, op2, op3) {}
  BL_INLINE_NODEBUG OpArrayT(const OpArrayT<T>& other) noexcept : OpArray(other) {}

  BL_INLINE_NODEBUG OpArrayT& operator=(const OpArrayT<T>& other) noexcept {
    init(other);
    return *this;
  }

protected:
  BL_INLINE_NODEBUG OpArrayT(const OpArrayT<T>& other, uint32_t n, uint32_t from, uint32_t inc) noexcept : OpArray(other, n, from, inc) {}

public:
  BL_INLINE_NODEBUG void init(const T& op0) noexcept { OpArray::init(op0); }
  BL_INLINE_NODEBUG void init(const T& op0, const T& op1) noexcept { OpArray::init(op0, op1); }
  BL_INLINE_NODEBUG void init(const T& op0, const T& op1, const T& op2) noexcept { OpArray::init(op0, op1, op2); }
  BL_INLINE_NODEBUG void init(const T& op0, const T& op1, const T& op2, const T& op3) noexcept { OpArray::init(op0, op1, op2, op3); }
  BL_INLINE_NODEBUG void init(const OpArrayT<T>& other) noexcept { OpArray::init(other); }

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

  BL_INLINE_NODEBUG OpArrayT<T> lo() const noexcept { return OpArrayT<T>(*this, 0, 1, (_size + 1) / 2); }
  BL_INLINE_NODEBUG OpArrayT<T> hi() const noexcept { return OpArrayT<T>(*this, _size > 1 ? (_size + 1) / 2 : 0, 1, _size); }
  BL_INLINE_NODEBUG OpArrayT<T> even() const noexcept { return OpArrayT<T>(*this, 0, 2, _size); }
  BL_INLINE_NODEBUG OpArrayT<T> odd() const noexcept { return OpArrayT<T>(*this, _size > 1, 2, _size); }
  BL_INLINE_NODEBUG OpArrayT<T> even_odd(uint32_t from) const noexcept { return OpArrayT<T>(*this, _size > 1 ? from : 0, 2, _size); }

  BL_INLINE OpArrayT<T> cloneAs(asmjit::OperandSignature signature) const noexcept {
    OpArrayT<T> out(*this);
    for (uint32_t i = 0; i < out.size(); i++)
      out.v[i].setSignature(signature);
    return out;
  }

  BL_INLINE_NODEBUG OpArrayT<T> cloneAs(SimdWidth simdWidth) const noexcept { return cloneAs(SimdWidthUtils::signatureOf(simdWidth)); }
  BL_INLINE_NODEBUG OpArrayT<T> cloneAs(const asmjit::BaseReg& reg) const noexcept { return cloneAs(reg.signature()); }
  BL_INLINE_NODEBUG OpArrayT<T> xmm() const noexcept { return cloneAs(asmjit::OperandSignature{x86::Xmm::kSignature}); }
  BL_INLINE_NODEBUG OpArrayT<T> ymm() const noexcept { return cloneAs(asmjit::OperandSignature{x86::Ymm::kSignature}); }
  BL_INLINE_NODEBUG OpArrayT<T> zmm() const noexcept { return cloneAs(asmjit::OperandSignature{x86::Zmm::kSignature}); }

  // Iterator compatibility.
  BL_INLINE_NODEBUG T* begin() noexcept { return reinterpret_cast<T*>(v); }
  BL_INLINE_NODEBUG T* end() noexcept { return reinterpret_cast<T*>(v) + _size; }

  BL_INLINE_NODEBUG const T* begin() const noexcept { return reinterpret_cast<const T*>(v); }
  BL_INLINE_NODEBUG const T* end() const noexcept { return reinterpret_cast<const T*>(v) + _size; }

  BL_INLINE_NODEBUG const T* cbegin() const noexcept { return reinterpret_cast<const T*>(v); }
  BL_INLINE_NODEBUG const T* cend() const noexcept { return reinterpret_cast<const T*>(v) + _size; }
};

typedef OpArrayT<x86::Vec> VecArray;

//! JIT Utilities used by PipeCompiler and other parts of the library.
namespace JitUtils {

template<typename T>
static BL_INLINE void resetVarArray(T* array, size_t size) noexcept {
  for (size_t i = 0; i < size; i++)
    array[i].reset();
}

template<typename T>
static BL_INLINE void resetVarStruct(T* data, size_t size = sizeof(T)) noexcept {
  resetVarArray(reinterpret_cast<asmjit::BaseReg*>(data), size / sizeof(asmjit::BaseReg));
}

static BL_INLINE_NODEBUG const Operand_& firstOp(const Operand_& operand) noexcept { return operand; }
static BL_INLINE_NODEBUG const Operand_& firstOp(const OpArray& opArray) noexcept { return opArray[0]; }

static BL_INLINE_NODEBUG uint32_t countOp(const Operand_&) noexcept { return 1u; }
static BL_INLINE_NODEBUG uint32_t countOp(const OpArray& opArray) noexcept { return opArray.size(); }

template<typename T> static BL_INLINE bool isXmm(const T& op) noexcept { return x86::Reg::isXmm(firstOp(op)); }
template<typename T> static BL_INLINE bool isYmm(const T& op) noexcept { return x86::Reg::isYmm(firstOp(op)); }
template<typename T> static BL_INLINE bool isZmm(const T& op) noexcept { return x86::Reg::isZmm(firstOp(op)); }

} // {JitUtils}

// Permutations
// ------------

enum class Perm2x128 : uint32_t {
  kALo = 0,
  kAHi = 1,
  kBLo = 2,
  kBHi = 3,
  kZero = 8
};

static BL_INLINE uint8_t perm2x128Imm(Perm2x128 hi, Perm2x128 lo) noexcept {
  return uint8_t((uint32_t(hi) << 4) | (uint32_t(lo)));
}

// Injecting
// ---------


//! Provides scope-based code injection.
//!
//! Scope-based code injection is used in some places to inject code at a specific point in the generated code.
//! When a pipeline is initialized each part can remember certain place where it could inject code in the future
//! as at the initialization phase it still doesn't know whether everything required to generate the code in place.
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

} // {JIT}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_JITBASE_P_H_INCLUDED
