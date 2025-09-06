// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_JITBASE_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_JITBASE_P_H_INCLUDED

#include "../../api-internal_p.h"
#include "../../support/intops_p.h"

// External dependencies of `bl::Pipeline::JIT`.
#if defined(BL_JIT_ARCH_X86)
  #include <asmjit/x86.h>
#endif

#if defined(BL_JIT_ARCH_A64)
  #include <asmjit/a64.h>
#endif

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace bl::Pipeline::JIT {

// AsmJit Integration
// ==================

using asmjit::AlignMode;
using asmjit::CpuFeatures;
using asmjit::InstId;
using asmjit::imm;
using asmjit::Imm;
using asmjit::JumpAnnotation;
using asmjit::Label;
using asmjit::Operand;
using asmjit::Operand_;
using asmjit::OperandSignature;
using asmjit::Reg;
using asmjit::RegMask;
using asmjit::RegType;
using asmjit::RegGroup;

#if defined(BL_JIT_ARCH_X86)
namespace x86 { using namespace ::asmjit::x86; }
using Gp = x86::Gp;
using Vec = x86::Vec;
using KReg = x86::KReg;
using Mem = x86::Mem;
using CondCode = x86::CondCode;
using AsmCompiler = x86::Compiler;

static BL_INLINE_NODEBUG Mem mem_ptr(const Gp& base, int32_t disp = 0) noexcept { return x86::ptr(base, disp); }
static BL_INLINE_NODEBUG Mem mem_ptr(const Gp& base, const Gp& index, uint32_t shift = 0, int32_t disp = 0) noexcept { return x86::ptr(base, index, shift, disp); }
#endif

#if defined(BL_JIT_ARCH_A64)
namespace a64 { using namespace ::asmjit::a64; }
using Gp = a64::Gp;
using Vec = a64::Vec;
using Mem = a64::Mem;
using CondCode = a64::CondCode;
using AsmCompiler = a64::Compiler;

static BL_INLINE_NODEBUG Mem mem_ptr(const Gp& base, int32_t disp = 0) noexcept { return a64::ptr(base, disp); }
static BL_INLINE_NODEBUG Mem mem_ptr(const Gp& base, const Gp& index, uint32_t shift = 0) noexcept { return a64::ptr(base, index, a64::lsl(shift)); }
#endif

// Strong Enums
// ------------

BL_DEFINE_STRONG_TYPE(PixelCount, uint32_t)
BL_DEFINE_STRONG_TYPE(Alignment, uint32_t)

// SIMD Width & Utilities
// ----------------------

//! Vector register width.
enum class VecWidth : uint8_t {
  //! 128-bit vector register (baseline, SSE/AVX, NEON, ASIMD, etc...).
  k128 = 0,
  //! 256-bit vector register (AVX2+).
  k256 = 1,
  //! 512-bit vector register (AVX512_DQ & AVX512_BW & AVX512_VL).
  k512 = 2,

#if defined(BL_JIT_ARCH_X86)
  kMaxPlatformWidth = k512
#else
  kMaxPlatformWidth = k128
#endif
};

//! SIMD data width.
enum class DataWidth : uint8_t {
  k8 = 0,
  k16 = 1,
  k32 = 2,
  k64 = 3,
  k128 = 4
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

namespace VecWidthUtils {

static BL_INLINE OperandSignature signature_of(VecWidth vw) noexcept {
#if defined(BL_JIT_ARCH_X86)
  static const OperandSignature table[] = {
    OperandSignature{asmjit::RegTraits<asmjit::RegType::kVec128>::kSignature},
    OperandSignature{asmjit::RegTraits<asmjit::RegType::kVec256>::kSignature},
    OperandSignature{asmjit::RegTraits<asmjit::RegType::kVec512>::kSignature}
  };
  return table[size_t(vw)];
#endif

#if defined(BL_JIT_ARCH_A64)
  bl_unused(vw);
  return OperandSignature{asmjit::RegTraits<asmjit::RegType::kVec128>::kSignature};
#endif
}

static BL_INLINE asmjit::TypeId type_id_of(VecWidth vw) noexcept {
#if defined(BL_JIT_ARCH_X86)
  static const asmjit::TypeId table[] = {
    asmjit::TypeId::kInt32x4,
    asmjit::TypeId::kInt32x8,
    asmjit::TypeId::kInt32x16
  };

  return table[size_t(vw)];
#endif

#if defined(BL_JIT_ARCH_A64)
  bl_unused(vw);
  return asmjit::TypeId::kInt32x4;
#endif
}

//! Calculates ideal SIMD width for the requested `byte_count` considering the given `max_vec_width`.
//!
//! The returned VecWidth is at most `max_vec_width`, but could be lesser in case
//! the whole VecWidth is not required to process the requested `byte_count`.
static BL_INLINE_NODEBUG VecWidth vec_width_for_byte_count(VecWidth max_vec_width, uint32_t byte_count) noexcept {
  return bl_min(max_vec_width, VecWidth((byte_count + 15) >> 5));
}

//! Calculates the number of registers that would be necessary to hold the requested `byte_count`, considering
//! the given `max_vec_width`.
static BL_INLINE_NODEBUG uint32_t vec_count_for_byte_count(VecWidth max_vec_width, uint32_t byte_count) noexcept {
  uint32_t shift = uint32_t(max_vec_width) + 4u;
  return (byte_count + ((1u << shift) - 1u)) >> shift;
}

static BL_INLINE_NODEBUG VecWidth vec_width_of(const Vec& reg) noexcept {
  return VecWidth(uint32_t(reg.reg_type()) - uint32_t(RegType::kVec128));
}

static BL_INLINE_NODEBUG VecWidth vec_width_of(VecWidth vw, DataWidth data_width, uint32_t n) noexcept {
  return VecWidth(bl_min<uint32_t>((n << uint32_t(data_width)) >> 5, uint32_t(vw)));
}

static BL_INLINE uint32_t vec_count_of(VecWidth vw, DataWidth data_width, uint32_t n) noexcept {
  uint32_t shift = uint32_t(vw) + 4;
  uint32_t total_width = n << uint32_t(data_width);
  return (total_width + ((1 << shift) - 1)) >> shift;
}

static BL_INLINE uint32_t vec_count_of(VecWidth vw, DataWidth data_width, PixelCount n) noexcept {
  return vec_count_of(vw, data_width, n.value());
}

static BL_INLINE Vec clone_vec_as(const Vec& src, VecWidth vw) noexcept {
  Vec result(src);
  result.set_signature(signature_of(vw));
  return result;
}

} // {VecWidthUtils}

// AsmJit Helpers
// ==============

//! Operand array used by SIMD pipeline.
//!
//! Can hold up to `kMaxSize` registers, however, the number of actual registers is dynamic and depends
//! on initialization.
class OpArray {
public:
  using Op = Operand_;
  static inline constexpr uint32_t kMaxSize = 8;

  uint32_t _size;
  Operand_ v[kMaxSize];

  BL_INLINE_NODEBUG OpArray() noexcept { reset(); }

  BL_INLINE_NODEBUG explicit OpArray(const Op& op0) noexcept {
    init(op0);
  }

  BL_INLINE_NODEBUG OpArray(const Op& op0, const Op& op1) noexcept {
    init(op0, op1);
  }

  BL_INLINE_NODEBUG OpArray(const Op& op0, const Op& op1, const Op& op2) noexcept {
    init(op0, op1, op2);
  }

  BL_INLINE_NODEBUG OpArray(const Op& op0, const Op& op1, const Op& op2, const Op& op3) noexcept {
    init(op0, op1, op2, op3);
  }

  BL_INLINE_NODEBUG OpArray(const Op& op0, const Op& op1, const Op& op2, const Op& op3, const Op& op4) noexcept {
    init(op0, op1, op2, op3, op4);
  }

  BL_INLINE_NODEBUG OpArray(const Op& op0, const Op& op1, const Op& op2, const Op& op3, const Op& op4, const Op& op5) noexcept {
    init(op0, op1, op2, op3, op4, op5);
  }

  BL_INLINE_NODEBUG OpArray(const Op& op0, const Op& op1, const Op& op2, const Op& op3, const Op& op4, const Op& op5, const Op& op6) noexcept {
    init(op0, op1, op2, op3, op4, op5, op6);
  }

  BL_INLINE_NODEBUG OpArray(const Op& op0, const Op& op1, const Op& op2, const Op& op3, const Op& op4, const Op& op5, const Op& op6, const Op& op7) noexcept {
    init(op0, op1, op2, op3, op4, op5, op6, op7);
  }

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

  BL_INLINE_NODEBUG void _reset_from(uint32_t index) noexcept {
    for (uint32_t i = index; i < kMaxSize; i++) {
      v[index].reset();
    }
  }

public:
  BL_INLINE_NODEBUG void init(const Op& op0) noexcept {
    _size = 1;
    v[0] = op0;
    _reset_from(1);
  }

  BL_INLINE_NODEBUG void init(const Op& op0, const Op& op1) noexcept {
    _size = 2;
    v[0] = op0;
    v[1] = op1;
    _reset_from(2);
  }

  BL_INLINE_NODEBUG void init(const Op& op0, const Op& op1, const Op& op2) noexcept {
    _size = 3;
    v[0] = op0;
    v[1] = op1;
    v[2] = op2;
    _reset_from(3);
  }

  BL_INLINE_NODEBUG void init(const Op& op0, const Op& op1, const Op& op2, const Op& op3) noexcept {
    _size = 4;
    v[0] = op0;
    v[1] = op1;
    v[2] = op2;
    v[3] = op3;
    _reset_from(4);
  }

  BL_INLINE_NODEBUG void init(const Op& op0, const Op& op1, const Op& op2, const Op& op3, const Op& op4) noexcept {
    _size = 5;
    v[0] = op0;
    v[1] = op1;
    v[2] = op2;
    v[3] = op3;
    v[4] = op4;
    _reset_from(5);
  }

  BL_INLINE_NODEBUG void init(const Op& op0, const Op& op1, const Op& op2, const Op& op3, const Op& op4, const Op& op5) noexcept {
    _size = 6;
    v[0] = op0;
    v[1] = op1;
    v[2] = op2;
    v[3] = op3;
    v[4] = op4;
    v[5] = op5;
    _reset_from(6);
  }

  BL_INLINE_NODEBUG void init(const Op& op0, const Op& op1, const Op& op2, const Op& op3, const Op& op4, const Op& op5, const Op& op6) noexcept {
    _size = 7;
    v[0] = op0;
    v[1] = op1;
    v[2] = op2;
    v[3] = op3;
    v[4] = op4;
    v[5] = op5;
    v[6] = op6;
    _reset_from(7);
  }

  BL_INLINE_NODEBUG void init(const Op& op0, const Op& op1, const Op& op2, const Op& op3, const Op& op4, const Op& op5, const Op& op6, const Op& op7) noexcept {
    _size = 7;
    v[0] = op0;
    v[1] = op1;
    v[2] = op2;
    v[3] = op3;
    v[4] = op4;
    v[5] = op5;
    v[6] = op6;
    v[7] = op7;
  }

  BL_INLINE_NODEBUG void init(const Op* array, uint32_t size) noexcept {
    _size = size;
    if (size)
      memcpy(v, array, size * sizeof(Op));
  }

  BL_INLINE_NODEBUG void init(const OpArray& other) noexcept {
    init(other.v, other._size);
  }

  //! Resets `OpArray` to a default construction state.
  BL_INLINE_NODEBUG void reset() noexcept {
    _size = 0;
    for (uint32_t i = 0; i < kMaxSize; i++)
      v[i].reset();
  }

  //! Tests whether the vector is empty (has no elements).
  BL_INLINE_NODEBUG bool is_empty() const noexcept { return _size == 0; }
  //! Tests whether the vector has only one element, which makes it scalar.
  BL_INLINE_NODEBUG bool is_scalar() const noexcept { return _size == 1; }
  //! Tests whether the vector has more than 1 element, which means that
  //! calling `high()` and `odd()` won't return an empty vector.
  BL_INLINE_NODEBUG bool is_vector() const noexcept { return _size > 1; }

  //! Returns the number of vector elements.
  BL_INLINE_NODEBUG uint32_t size() const noexcept { return _size; }
  //! Returns the maximum size of vector elements.
  BL_INLINE_NODEBUG uint32_t max_size() const noexcept { return kMaxSize; }

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
  BL_INLINE_NODEBUG OpArray half() const noexcept { return OpArray(*this, 0, 1, (_size + 1) / 2); }
  BL_INLINE_NODEBUG OpArray every_nth(uint32_t n) const noexcept { return OpArray(*this, 0, n, _size); }

  //! Returns a new vector consisting of either even (from == 0) or odd
  //! (from == 1) elements. It's like calling `even()` and `odd()`, but
  //! can be used within a loop that performs the same operation for both.
  BL_INLINE_NODEBUG OpArray even_odd(uint32_t from) const noexcept { return OpArray(*this, _size > 1 ? from : 0, 2, _size); }
};

class VecArray : public OpArray {
public:
  BL_INLINE_NODEBUG VecArray() noexcept : OpArray() {}

  BL_INLINE_NODEBUG explicit VecArray(const Vec& op0) noexcept
    : OpArray(op0) {}

  BL_INLINE_NODEBUG VecArray(const Vec& op0, const Vec& op1) noexcept
    : OpArray(op0, op1) {}

  BL_INLINE_NODEBUG VecArray(const Vec& op0, const Vec& op1, const Vec& op2) noexcept
    : OpArray(op0, op1, op2) {}

  BL_INLINE_NODEBUG VecArray(const Vec& op0, const Vec& op1, const Vec& op2, const Vec& op3) noexcept
    : OpArray(op0, op1, op2, op3) {}

  BL_INLINE_NODEBUG VecArray(const Vec& op0, const Vec& op1, const Vec& op2, const Vec& op3, const Vec& op4) noexcept
    : OpArray(op0, op1, op2, op3, op4) {}

  BL_INLINE_NODEBUG VecArray(const Vec& op0, const Vec& op1, const Vec& op2, const Vec& op3, const Vec& op4, const Vec& op5) noexcept
    : OpArray(op0, op1, op2, op3, op4, op5) {}

  BL_INLINE_NODEBUG VecArray(const Vec& op0, const Vec& op1, const Vec& op2, const Vec& op3, const Vec& op4, const Vec& op5, const Vec& op6) noexcept
    : OpArray(op0, op1, op2, op3, op4, op5, op6) {}

  BL_INLINE_NODEBUG VecArray(const Vec& op0, const Vec& op1, const Vec& op2, const Vec& op3, const Vec& op4, const Vec& op5, const Vec& op6, const Vec& op7) noexcept
    : OpArray(op0, op1, op2, op3, op4, op5, op6, op7) {}

  BL_INLINE_NODEBUG VecArray(const VecArray& other) noexcept
    : OpArray(other) {}

  BL_INLINE_NODEBUG VecArray& operator=(const VecArray& other) noexcept {
    init(other);
    return *this;
  }

protected:
  BL_INLINE_NODEBUG VecArray(const VecArray& other, uint32_t from, uint32_t inc, uint32_t limit) noexcept
    : OpArray(other, from, inc, limit) {}

public:
  BL_INLINE_NODEBUG void init(const Vec& op0) noexcept {
    OpArray::init(op0);
  }

  BL_INLINE_NODEBUG void init(const Vec& op0, const Vec& op1) noexcept {
    OpArray::init(op0, op1);
  }

  BL_INLINE_NODEBUG void init(const Vec& op0, const Vec& op1, const Vec& op2) noexcept {
    OpArray::init(op0, op1, op2);
  }

  BL_INLINE_NODEBUG void init(const Vec& op0, const Vec& op1, const Vec& op2, const Vec& op3) noexcept {
    OpArray::init(op0, op1, op2, op3);
  }

  BL_INLINE_NODEBUG void init(const Vec& op0, const Vec& op1, const Vec& op2, const Vec& op3, const Vec& op4) noexcept {
    OpArray::init(op0, op1, op2, op3, op4);
  }

  BL_INLINE_NODEBUG void init(const Vec& op0, const Vec& op1, const Vec& op2, const Vec& op3, const Vec& op4, const Vec& op5) noexcept {
    OpArray::init(op0, op1, op2, op3, op4, op5);
  }

  BL_INLINE_NODEBUG void init(const Vec& op0, const Vec& op1, const Vec& op2, const Vec& op3, const Vec& op4, const Vec& op5, const Vec& op6) noexcept {
    OpArray::init(op0, op1, op2, op3, op4, op5, op6);
  }

  BL_INLINE_NODEBUG void init(const Vec& op0, const Vec& op1, const Vec& op2, const Vec& op3, const Vec& op4, const Vec& op5, const Vec& op6, const Vec& op7) noexcept {
    OpArray::init(op0, op1, op2, op3, op4, op5, op6, op7);
  }

  BL_INLINE_NODEBUG void init(const Vec* array, uint32_t size) noexcept {
    OpArray::init(array, size);
  }

  BL_INLINE_NODEBUG void init(const VecArray& other) noexcept {
    OpArray::init(other);
  }

  BL_INLINE Vec& operator[](size_t index) noexcept {
    BL_ASSERT(index < _size);
    return static_cast<Vec&>(v[index]);
  }

  BL_INLINE const Vec& operator[](size_t index) const noexcept {
    BL_ASSERT(index < _size);
    return static_cast<const Vec&>(v[index]);
  }

  BL_INLINE Vec& at_unrestricted(size_t index) noexcept {
    BL_ASSERT(index < kMaxSize);
    return static_cast<Vec&>(v[index]);
  }

  BL_INLINE const Vec& at_unrestricted(size_t index) const noexcept {
    BL_ASSERT(index < kMaxSize);
    return static_cast<const Vec&>(v[index]);
  }

  BL_INLINE void reassign(size_t index, const Vec& new_vec) noexcept {
    BL_ASSERT(index < kMaxSize);
    v[index] = new_vec;
  }

  BL_INLINE_NODEBUG VecArray lo() const noexcept { return VecArray(*this, 0, 1, (_size + 1) / 2); }
  BL_INLINE_NODEBUG VecArray hi() const noexcept { return VecArray(*this, _size > 1 ? (_size + 1) / 2 : 0, 1, _size); }
  BL_INLINE_NODEBUG VecArray even() const noexcept { return VecArray(*this, 0, 2, _size); }
  BL_INLINE_NODEBUG VecArray odd() const noexcept { return VecArray(*this, _size > 1, 2, _size); }
  BL_INLINE_NODEBUG VecArray even_odd(uint32_t from) const noexcept { return VecArray(*this, _size > 1 ? from : 0, 2, _size); }
  BL_INLINE_NODEBUG VecArray half() const noexcept { return VecArray(*this, 0, 1, (_size + 1) / 2); }
  BL_INLINE_NODEBUG VecArray every_nth(uint32_t n) const noexcept { return VecArray(*this, 0, n, _size); }

  BL_INLINE_NODEBUG void truncate(uint32_t new_size) noexcept {
    _size = bl_min(_size, new_size);
  }

  BL_INLINE VecArray clone_as(asmjit::OperandSignature signature) const noexcept {
    VecArray out(*this);
    for (uint32_t i = 0; i < out.size(); i++) {
      out.v[i].set_signature(signature);
    }
    return out;
  }

  BL_INLINE_NODEBUG VecWidth vec_width() const noexcept { return VecWidthUtils::vec_width_of(v[0].as<Vec>()); }

  BL_INLINE_NODEBUG void set_vec_width(VecWidth vw) noexcept {
    asmjit::OperandSignature signature = VecWidthUtils::signature_of(vw);
    for (uint32_t i = 0; i < size(); i++) {
      v[i].set_signature(signature);
    }
  }

  BL_INLINE_NODEBUG VecArray clone_as(VecWidth vw) const noexcept { return clone_as(VecWidthUtils::signature_of(vw)); }
  BL_INLINE_NODEBUG VecArray clone_as(const asmjit::Reg& reg) const noexcept { return clone_as(reg.signature()); }

  BL_INLINE_NODEBUG bool is_vec128() const noexcept { return v[0].is_vec128(); }
  BL_INLINE_NODEBUG bool is_vec256() const noexcept { return v[0].is_vec256(); }
  BL_INLINE_NODEBUG bool is_vec512() const noexcept { return v[0].is_vec512(); }

#if defined(BL_JIT_ARCH_X86)
  BL_INLINE_NODEBUG VecArray xmm() const noexcept { return clone_as(asmjit::OperandSignature{asmjit::RegTraits<asmjit::RegType::kVec128>::kSignature}); }
  BL_INLINE_NODEBUG VecArray ymm() const noexcept { return clone_as(asmjit::OperandSignature{asmjit::RegTraits<asmjit::RegType::kVec256>::kSignature}); }
  BL_INLINE_NODEBUG VecArray zmm() const noexcept { return clone_as(asmjit::OperandSignature{asmjit::RegTraits<asmjit::RegType::kVec512>::kSignature}); }
#endif

  // Iterator compatibility.
  BL_INLINE_NODEBUG Vec* begin() noexcept { return reinterpret_cast<Vec*>(v); }
  BL_INLINE_NODEBUG Vec* end() noexcept { return reinterpret_cast<Vec*>(v) + _size; }

  BL_INLINE_NODEBUG const Vec* begin() const noexcept { return reinterpret_cast<const Vec*>(v); }
  BL_INLINE_NODEBUG const Vec* end() const noexcept { return reinterpret_cast<const Vec*>(v) + _size; }

  BL_INLINE_NODEBUG const Vec* cbegin() const noexcept { return reinterpret_cast<const Vec*>(v); }
  BL_INLINE_NODEBUG const Vec* cend() const noexcept { return reinterpret_cast<const Vec*>(v) + _size; }
};

//! JIT Utilities used by PipeCompiler and other parts of the library.
namespace JitUtils {

template<typename T>
static BL_INLINE void reset_var_array(T* array, size_t size) noexcept {
  for (size_t i = 0; i < size; i++)
    array[i].reset();
}

template<typename T>
static BL_INLINE void reset_var_struct(T* data, size_t size = sizeof(T)) noexcept {
  reset_var_array(reinterpret_cast<asmjit::Reg*>(data), size / sizeof(asmjit::Reg));
}

static BL_INLINE_NODEBUG const Operand_& first_op(const Operand_& operand) noexcept { return operand; }
static BL_INLINE_NODEBUG const Operand_& first_op(const OpArray& op_array) noexcept { return op_array[0]; }

static BL_INLINE_NODEBUG uint32_t count_op(const Operand_&) noexcept { return 1u; }
static BL_INLINE_NODEBUG uint32_t count_op(const OpArray& op_array) noexcept { return op_array.size(); }

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

static BL_INLINE uint8_t perm_2x128_imm(Perm2x128 hi, Perm2x128 lo) noexcept {
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
  asmjit::BaseCompiler* cc {};
  asmjit::BaseNode** hook {};
  asmjit::BaseNode* prev {};
  bool hook_was_cursor = false;

  BL_NONCOPYABLE(ScopedInjector)

  BL_INLINE ScopedInjector(asmjit::BaseCompiler* cc, asmjit::BaseNode** hook) noexcept
    : cc(cc),
      hook(hook),
      prev(cc->set_cursor(*hook)),
      hook_was_cursor(*hook == prev) {}

  BL_INLINE ~ScopedInjector() noexcept {
    if (!hook_was_cursor)
      *hook = cc->set_cursor(prev);
    else
      *hook = cc->cursor();
  }
};

} // {bl::Pipeline::JIT}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_JITBASE_P_H_INCLUDED
