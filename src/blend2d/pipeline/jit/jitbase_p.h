// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_JITBASE_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_JITBASE_P_H_INCLUDED

#include "../../api-internal_p.h"

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

// AsmJit Helpers
// ==============

//! Operand array used by SIMD pipeline.
//!
//! Can hold up to `kMaxSize` registers, however, the number of actual registers is dynamic and depends
//! on initialization.
class OpArray {
public:
  enum : uint32_t { kMaxSize = 4 };

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

} // {JitUtils}

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

// SIMD Constants
// --------------

//! SIMD data width.
enum class SimdWidth : uint8_t {
  //! 128-bit SIMD (baseline, SSE/AVX, NEON, ASIMD, etc...).
  k128 = 0,
  //! 256-bit SIMD (AVX2+).
  k256 = 1,
  //! 512-bit SIMD (AVX512_DQ & AVX512_BW).
  k512 = 2
};

//! SIMD data arrangement.
//!
//! This is designed to avoid expensive permutations between LO/HI parts of YMM registers in AVX2 case.
//! AVX512_F offers new instructions, which make data packing and unpacking easier.
enum class SimdArrangement : uint8_t {
  //! Any data arrangement - data is either repeated, not unpacked, or SIMD width is not big enough to affect
  //! packing.
  kAny = 0,

  //! Data is arranged / unpacked into pairs of continuous data, which looks like:
  //!
  //! ```
  //! Packed:
  //!   YMM0 == [31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16|15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00]
  //!
  //! Unpacked:
  //!   YMM0 == [__ 15 __ 14 __ 13 __ 12 __ 11 __ 10 __ 09 __ 08|__ 07 __ 06 __ 05 __ 04 __ 03 __ 02 __ 01 __ 00]
  //!   YMM1 == [__ 31 __ 30 __ 29 __ 28 __ 27 __ 26 __ 25 __ 24|__ 23 __ 22 __ 21 __ 20 __ 19 __ 18 __ 17 __ 16]
  //!
  //! Vpackuswb YMM0, YMM0, YMM1:
  //!   YMM0 == [31 30 29 28 27 26 25 24 15 14 13 12 11 10 09 08|23 22 21 20 19 18 17 16 07 06 05 04 03 02 01 00]
  //! ```
  //!
  //!
  kContinuous = 1,

  //! Data is arranged / unpacked into low & high pairs, which looks like:
  //!
  //! ```
  //! Packed (bytes):
  //!   YMM0 == [31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16|15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00]
  //!
  //! Unpacked (words):
  //!   YMM0 == [__ 23 __ 22 __ 21 __ 20 __ 19 __ 18 __ 17 __ 16|__ 07 __ 06 __ 05 __ 04 __ 03 __ 02 __ 01 __ 00]
  //!   YMM1 == [__ 31 __ 30 __ 29 __ 28 __ 27 __ 26 __ 25 __ 24|__ 15 __ 14 __ 13 __ 12 __ 11 __ 10 __ 09 __ 08]
  //!
  //! Vpackuswb YMM0, YMM0, YMM1:
  //!   YMM0 == [31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16|15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00]
  //! ```
  //!
  //! This representation is easy to pack / unpack, but in general it's required that the packed register has a
  //! full width.
  kLowHigh = 2
};

} // {JIT}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_JITBASE_P_H_INCLUDED
