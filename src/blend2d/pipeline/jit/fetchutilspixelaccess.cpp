// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/fetchutilspixelaccess_p.h"

namespace bl {
namespace Pipeline {
namespace JIT {
namespace FetchUtils {

static void satisfyPixelsA8(PipeCompiler* pc, Pixel& p, PixelFlags flags) noexcept;
static void satisfyPixelsRGBA32(PipeCompiler* pc, Pixel& p, PixelFlags flags) noexcept;

// bl::Pipeline::Jit::FetchUtils - Fetch & Store
// =============================================

static uint32_t calculateVecCount(uint32_t vecSize, uint32_t n) noexcept {
  uint32_t shift = IntOps::ctz(vecSize);
  return (n + vecSize - 1) >> shift;
}

#if defined(BL_JIT_ARCH_A64)
// Provides a specialized AArch64 implementation of a byte granularity vector fetch/store.
static void fetchVec8AArch64(PipeCompiler* pc, VecArray& dVec, const Gp& sPtr, uint32_t n, AdvanceMode advanceMode) noexcept {
  AsmCompiler* cc = pc->cc;
  uint32_t i = 0;

  if (advanceMode == AdvanceMode::kNoAdvance) {
    while (i < n) {
      uint32_t idx = i / 16u;
      uint32_t remaining = n - i;

      if (remaining >= 32u) {
        cc->ldp(dVec[idx], dVec[idx + 1], a64::ptr(sPtr, i));
        i += 32;
      }
      else {
        uint32_t count = blMin<uint32_t>(n - i, 16u);
        pc->v_load_iany(dVec[idx], a64::ptr(sPtr, i), count, Alignment{1});
        i += count;
      }
    }
  }
  else {
    while (i < n) {
      uint32_t idx = i / 16u;
      uint32_t remaining = n - i;

      if (remaining >= 32u) {
        cc->ldp(dVec[idx], dVec[idx + 1], a64::ptr_post(sPtr, 32));
        i += 32;
      }
      else {
        uint32_t count = blMin<uint32_t>(n - i, 16u);
        pc->v_load_iany(dVec[idx], mem_ptr(sPtr), count, Alignment{1});
        pc->add(sPtr, sPtr, count);

        i += count;
      }
    }
  }
}

static void storeVec8AArch64(PipeCompiler* pc, const Gp& dPtr, const VecArray& sVec, uint32_t n, AdvanceMode advanceMode) noexcept {
  AsmCompiler* cc = pc->cc;
  uint32_t i = 0;

  if (advanceMode == AdvanceMode::kNoAdvance) {
    while (i < n) {
      uint32_t idx = i / 16u;
      uint32_t remaining = n - i;

      if (remaining >= 32u) {
        cc->stp(sVec[idx], sVec[idx + 1], a64::ptr(dPtr, i));
        i += 32;
      }
      else {
        uint32_t count = blMin<uint32_t>(n - i, 16u);
        pc->v_load_iany(sVec[idx], a64::ptr(dPtr, i), count, Alignment{1});
        i += count;
      }
    }
  }
  else {
    while (i < n) {
      uint32_t idx = i / 16u;
      uint32_t remaining = n - i;

      if (remaining >= 32u) {
        cc->stp(sVec[idx], sVec[idx + 1], a64::ptr_post(dPtr, 32));
        i += 32;
      }
      else {
        uint32_t count = blMin<uint32_t>(n - i, 16u);
        pc->v_load_iany(sVec[idx], mem_ptr(dPtr), count, Alignment{1});
        pc->add(dPtr, dPtr, count);

        i += count;
      }
    }
  }
}
#endif // BL_JIT_ARCH_A64

void fetchVec8(PipeCompiler* pc, const VecArray& dVec_, Gp sPtr, uint32_t n, AdvanceMode advanceMode) noexcept {
  VecArray dVec(dVec_);
  dVec.truncate(calculateVecCount(dVec[0].size(), n));

  BL_ASSERT(!dVec.empty());

#if defined(BL_JIT_ARCH_A64)
  fetchVec8AArch64(pc, dVec, sPtr, n, advanceMode);
#else
  uint32_t offset = 0;

  for (uint32_t idx = 0; idx < dVec.size(); idx++) {
    uint32_t remaining = n - offset;
    uint32_t fetchSize = blMin<uint32_t>(dVec[idx].size(), remaining);

    pc->v_load_iany(dVec[idx], mem_ptr(sPtr, offset), fetchSize, Alignment{1});
    offset += fetchSize;

    if (offset >= n)
      break;
  }

  if (advanceMode == AdvanceMode::kAdvance) {
    pc->add(sPtr, sPtr, n);
  }
#endif
}

void storeVec8(PipeCompiler* pc, const Gp& dPtr, const VecArray& sVec_, uint32_t n, AdvanceMode advanceMode) noexcept {
  VecArray sVec(sVec_);
  sVec.truncate(calculateVecCount(sVec[0].size(), n));

  BL_ASSERT(!sVec.empty());

#if defined(BL_JIT_ARCH_A64)
  storeVec8AArch64(pc, dPtr, sVec, n, advanceMode);
#else
  uint32_t offset = 0;

  for (uint32_t idx = 0; idx < sVec.size(); idx++) {
    uint32_t remaining = n - offset;
    uint32_t storeSize = blMin<uint32_t>(sVec[idx].size(), remaining);

    pc->v_store_iany(mem_ptr(dPtr, offset), sVec[idx], storeSize, Alignment{1});
    offset += storeSize;

    if (offset >= n)
      break;
  }

  if (advanceMode == AdvanceMode::kAdvance) {
    pc->add(dPtr, dPtr, n);
  }
#endif
}

void fetchVec32(PipeCompiler* pc, const VecArray& dVec, Gp sPtr, uint32_t n, AdvanceMode advanceMode) noexcept {
  fetchVec8(pc, dVec, sPtr, n * 4u, advanceMode);
}

void storeVec32(PipeCompiler* pc, const Gp& dPtr, const VecArray& sVec, uint32_t n, AdvanceMode advanceMode) noexcept {
  storeVec8(pc, dPtr, sVec, n * 4u, advanceMode);
}

void fetchVec8(PipeCompiler* pc, const VecArray& dVec, Gp sPtr, uint32_t n, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  if (predicate.empty())
    fetchVec8(pc, dVec, sPtr, n, advanceMode);
  else
    fetchPredicatedVec8(pc, dVec, sPtr, n, advanceMode, predicate);
}

void fetchVec32(PipeCompiler* pc, const VecArray& dVec, Gp sPtr, uint32_t n, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  if (predicate.empty())
    fetchVec32(pc, dVec, sPtr, n, advanceMode);
  else
    fetchPredicatedVec32(pc, dVec, sPtr, n, advanceMode, predicate);
}

void storeVec8(PipeCompiler* pc, const Gp& dPtr, const VecArray& sVec, uint32_t n, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  if (predicate.empty())
    storeVec8(pc, dPtr, sVec, n, advanceMode);
  else
    storePredicatedVec8(pc, dPtr, sVec, n, advanceMode, predicate);
}

void storeVec32(PipeCompiler* pc, const Gp& dPtr, const VecArray& sVec, uint32_t n, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  if (predicate.empty())
    storeVec32(pc, dPtr, sVec, n, advanceMode);
  else
    storePredicatedVec32(pc, dPtr, sVec, n, advanceMode, predicate);
}

// bl::Pipeline::Jit::FetchUtils - Fetch Miscellaneous
// ===================================================

void fetchSecond32BitElement(PipeCompiler* pc, const Vec& vec, const Mem& src) noexcept {
#if defined(BL_JIT_ARCH_X86)
  if (!pc->hasSSE4_1()) {
    Vec tmp = pc->newV128("@tmp");
    pc->v_loadu32(tmp, src);
    pc->v_interleave_lo_u32(vec, vec, tmp);
  }
  else
#endif
  {
    pc->v_insert_u32(vec, src, 1);
  }
}

void fetchThird32BitElement(PipeCompiler* pc, const Vec& vec, const Mem& src) noexcept {
#if defined(BL_JIT_ARCH_X86)
  if (!pc->hasSSE4_1()) {
    Vec tmp = pc->newV128("@tmp");
    pc->v_loadu32(tmp, src);
    pc->v_interleave_lo_u64(vec, vec, tmp);
  }
  else
#endif
  {
    pc->v_insert_u32(vec, src, 2);
  }
}

// bl::Pipeline::Jit::FetchUtils - Predicated Fetch
// ================================================

static void add_shifted(PipeCompiler* pc, const Gp& dst, const Gp& src, uint32_t shift) noexcept {
#if defined(BL_JIT_ARCH_X86)
  pc->shl(src, src, shift);
  pc->add(dst, dst, src);
#else
  pc->add_scaled(dst, src, 1 << shift);
#endif
};

static void fetchPredicatedVec8_1To3(PipeCompiler* pc, const Vec& dVec, Gp sPtr, AdvanceMode advanceMode, const Gp& count) noexcept {
  Gp acc = pc->newGp32("@acc");
  Gp tmp = pc->newGp32("@tmp");
  Gp mid = pc->newGpPtr("@mid");

  if (advanceMode == AdvanceMode::kAdvance) {
    pc->load_u8(acc, mem_ptr(sPtr));
    pc->add(mid, sPtr, 2);
    pc->add(sPtr, sPtr, count.cloneAs(sPtr));
    pc->load_u8(tmp, mem_ptr(sPtr, -1));
    pc->umin(mid, mid, sPtr);
    add_shifted(pc, acc, tmp, 16);
    pc->load_u8(tmp, mem_ptr(mid, -1));
    add_shifted(pc, acc, tmp, 8);
  }
  else {
    Gp end = pc->newGpPtr("@end");

    pc->add(end, sPtr, count.cloneAs(sPtr));
    pc->load_u8(tmp, mem_ptr(end, -1));
    pc->load_u8(acc, mem_ptr(sPtr));
    add_shifted(pc, acc, tmp, 16);

    pc->add(mid, sPtr, 2);
    pc->umin(mid, sPtr, end);
    pc->load_u8(tmp, mem_ptr(mid, -1));
    add_shifted(pc, acc, tmp, 8);
  }

  pc->s_mov_u32(dVec, acc);
}

static void fetchPredicatedVec8_4To7(PipeCompiler* pc, const Vec& dVec, Gp sPtr, AdvanceMode advanceMode, const Gp& count) noexcept {
  Gp highAcc = pc->newGp32("@highAcc");
  Gp shift = pc->newGp32("@shift");
  Mem highPtr;

  pc->mov(shift, 8);
  pc->v_loadu32(dVec, mem_ptr(sPtr));
  pc->sub(shift, shift, count.cloneAs(shift));

  if (advanceMode == AdvanceMode::kAdvance) {
    pc->add(sPtr, sPtr, count.cloneAs(sPtr));
    highPtr = mem_ptr(sPtr, -4);
  }
  else {
#if defined(BL_JIT_ARCH_X86)
    highPtr = x86::ptr(sPtr, count.cloneAs(sPtr), 0, -4);
#else
    Gp end = pc->newGpPtr("@end");
    pc->add(end, sPtr, count.cloneAs(sPtr));
    highPtr = mem_ptr(end, -4);
#endif
  }

  pc->shl(shift, shift, 3);
  pc->load_u32(highAcc, highPtr);
  pc->shr(highAcc, highAcc, shift);

#if defined(BL_JIT_ARCH_X86)
  if (!pc->hasSSE4_1()) {
    Vec highVec = pc->newSimilarReg(dVec, "@high");
    pc->s_mov_u32(highVec, highAcc);
    pc->v_interleave_lo_u32(dVec, dVec, highVec);
  }
  else
#endif
  {
    pc->s_insert_u32(dVec, highAcc, 1);
  }
}

static void fetchPredicatedVec8_1To7(PipeCompiler* pc, const Vec& dVec, Gp sPtr, AdvanceMode advanceMode, const Gp& count) noexcept {
  Label L_LessThan4 = pc->newLabel();
  Label L_Done = pc->newLabel();

  pc->j(L_LessThan4, ucmp_lt(count, 4));
  fetchPredicatedVec8_4To7(pc, dVec, sPtr, advanceMode, count);
  pc->j(L_Done);

  pc->bind(L_LessThan4);
  fetchPredicatedVec8_1To3(pc, dVec, sPtr, advanceMode, count);
  pc->bind(L_Done);
}

static void fetchPredicatedVec8_4To15(PipeCompiler* pc, const Vec& dVec, Gp sPtr, AdvanceMode advanceMode, const Gp& count) noexcept {
  Gp end = pc->newGpPtr("@end");

#if defined(BL_JIT_ARCH_X86)
  if (!pc->hasSSE3()) {
    AsmCompiler* cc = pc->cc;

    Vec acc = pc->newV128("@acc");
    Vec tmp = pc->newV128("@tmp");
    Gp shift = pc->newGp32("@shift");

    Label L_Done = pc->newLabel();
    Label L_LessThan8 = pc->newLabel();

    pc->neg(shift, count.cloneAs(shift));
    pc->shl(shift, shift, 3);
    pc->j(L_LessThan8, ucmp_lt(count, 8));

    pc->add(shift, shift, 16 * 8);
    pc->v_loadu64(dVec, mem_ptr(sPtr));
    pc->s_mov_u32(tmp, shift);

    if (advanceMode == AdvanceMode::kAdvance) {
      pc->add(sPtr, sPtr, count.cloneAs(sPtr));
      pc->v_loadu64(acc, x86::ptr(sPtr, -8));
    }
    else {
      pc->v_loadu64(acc, x86::ptr(sPtr, count.cloneAs(sPtr), 0, -8));
    }

    cc->psrlq(acc.as<Xmm>(), tmp.as<Xmm>());
    pc->v_interleave_lo_u64(dVec, dVec, acc);
    pc->j(L_Done);

    pc->bind(L_LessThan8);
    pc->add(shift, shift, 8 * 8);
    pc->v_loadu32(dVec, mem_ptr(sPtr));
    pc->s_mov_u32(tmp, shift);

    if (advanceMode == AdvanceMode::kAdvance) {
      pc->add(sPtr, sPtr, count.cloneAs(sPtr));
      pc->v_loadu32(acc, x86::ptr(sPtr, -4));
    }
    else {
      pc->v_loadu32(acc, x86::ptr(sPtr, count.cloneAs(sPtr), 0, -4));
    }

    cc->psrld(acc.as<Xmm>(), tmp.as<Xmm>());
    pc->v_interleave_lo_u32(dVec, dVec, acc);

    pc->bind(L_Done);
    return;
  }
#endif // BL_JIT_ARCH_X86

  // Common implementation that targets both X86 and AArch64.
  Vec vPred = pc->newV128("@pred");
  Mem mPred = pc->simdMemConst(pc->ct.swizu8_load_tail_0_to_16, Bcst::kNA_Unique, vPred);
  mPred.setIndex(count.cloneAs(sPtr));

#if defined(BL_JIT_ARCH_X86)
  // Temporaries needed to compose a single 128-bit vector from 4 32-bit elements.
  Vec tmp0;
  Vec tmp1;

  if (!pc->hasSSE4_1()) {
    tmp0 = pc->newV128("tmp0");
    tmp1 = pc->newV128("tmp1");
  }
#endif // BL_JIT_ARCH_X86

  auto&& fetch_next_32 = [&](const Gp& src, uint32_t i) noexcept {
    Mem p = mem_ptr(src);
#if defined(BL_JIT_ARCH_X86)
    if (!pc->hasSSE4_1()) {
      switch (i) {
        case 1:
          pc->v_loadu32(tmp0, p);
          pc->v_interleave_lo_u32(dVec, dVec, tmp0);
          break;
        case 2:
          pc->v_loadu32(tmp0, p);
          break;
        case 3:
          pc->v_loadu32(tmp1, p);
          pc->v_interleave_lo_u32(tmp0, tmp0, tmp1);
          pc->v_interleave_lo_u64(dVec, dVec, tmp0);
          break;
      }
      return;
    }
#endif // BL_JIT_ARCH_X86
    pc->v_insert_u32(dVec, p, i);
  };

  pc->v_loadu32(dVec, mem_ptr(sPtr));
  pc->add_ext(end, sPtr, count.cloneAs(sPtr), 1, -4);
  pc->v_loada128(vPred, mPred);

  if (advanceMode == AdvanceMode::kAdvance) {
    pc->add(sPtr, sPtr, 4);
    pc->umin(sPtr, sPtr, end);
    fetch_next_32(sPtr, 1);

    pc->add(sPtr, sPtr, 4);
    pc->umin(sPtr, sPtr, end);
    fetch_next_32(sPtr, 2);

    pc->add(sPtr, sPtr, 4);
    pc->umin(sPtr, sPtr, end);
    fetch_next_32(sPtr, 3);

    pc->add(sPtr, sPtr, 4);
  }
  else {
    Gp mid = pc->newGpPtr("@mid");

    pc->add(mid, sPtr, 4);
    pc->umin(mid, mid, end);
    fetch_next_32(mid, 1);

    pc->add(mid, sPtr, 8);
    pc->umin(mid, mid, end);
    fetch_next_32(mid, 2);

    pc->add(mid, sPtr, 12);
    pc->umin(mid, mid, end);
    fetch_next_32(mid, 3);
  }

  pc->v_swizzlev_u8(dVec, dVec, vPred);
}

static void fetchPredicatedVec8_1To15(PipeCompiler* pc, const Vec& dVec, Gp sPtr, AdvanceMode advanceMode, const Gp& count) noexcept {
  Label L_LessThan4 = pc->newLabel();
  Label L_Done = pc->newLabel();

  pc->j(L_LessThan4, ucmp_lt(count, 4));
  fetchPredicatedVec8_4To15(pc, dVec, sPtr, advanceMode, count);
  pc->j(L_Done);

  pc->bind(L_LessThan4);
  fetchPredicatedVec8_1To3(pc, dVec, sPtr, advanceMode, count);
  pc->bind(L_Done);
}

static void fetchPredicatedVec8_V128(PipeCompiler* pc, const VecArray& dVec, Gp sPtr, uint32_t n, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  uint32_t vecCount = dVec.size();
  Gp count = predicate.count();

  // Handle small cases first.
  if (n <= 2) {
    // Never empty & never full -> there is exactly a single element to load.
    pc->v_load8(dVec[0], mem_ptr(sPtr));

    if (advanceMode == AdvanceMode::kAdvance) {
      pc->add(sPtr, sPtr, predicate.count().cloneAs(sPtr));
    }
  }
  else if (n <= 4) {
    fetchPredicatedVec8_1To3(pc, dVec[0], sPtr, advanceMode, count);
  }
  else if (n <= 8) {
    fetchPredicatedVec8_1To7(pc, dVec[0], sPtr, advanceMode, count);
  }
  else if (n <= 16) {
    fetchPredicatedVec8_1To15(pc, dVec[0], sPtr, advanceMode, count);
  }
  else {
    BL_ASSERT(vecCount > 1);

    // TODO: [JIT] UNIMPLEMENTED: Predicated fetch - multiple vector registers.
    blUnused(vecCount);
    BL_NOT_REACHED();
  }
}

#if defined(BL_JIT_ARCH_X86)
static void fetchPredicatedVec8_AVX(PipeCompiler* pc, const VecArray& dVec, Gp sPtr, uint32_t n, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  uint32_t vecCount = dVec.size();

  if (n <= 4) {
    fetchPredicatedVec8_1To3(pc, dVec[0], sPtr, advanceMode, predicate.count());
    return;
  }

  AsmCompiler* cc = pc->cc;
  InstId loadInstId = pc->hasAVX2() ? x86::Inst::kIdVpmaskmovd : x86::Inst::kIdVmaskmovps;
  uint32_t vecElementCount = dVec[0].size() / 4u;

  Label L_LessThan4 = pc->newLabel();
  Label L_Done = pc->newLabel();

  Gp count = predicate.count();
  Gp countDiv4 = pc->newGp32("@countDiv4");
  Gp tailPixels = pc->newGp32("@tailPixels");
  Gp tailShift = pc->newGp32("@tailShift");

  Vec vTail = pc->newSimilarReg(dVec[0], "vTail");
  Vec vPred = pc->newSimilarReg(dVec[0], "vPred");
  Mem mPred = pc->simdMemConst(pc->ct.loadstore16_lo8_msk8(), Bcst::kNA_Unique, vPred);
  mPred.setIndex(countDiv4.cloneAs(sPtr));

  pc->j(L_LessThan4, ucmp_lt(count, 4));
  pc->neg(tailShift, count.r32());
  pc->shl(tailShift, tailShift, 3);
  pc->load_u32(tailPixels, x86::ptr(sPtr, count.cloneAs(sPtr), 0, -4));
  pc->shr(tailPixels, tailPixels, tailShift);
  pc->shr(countDiv4, count, 2);
  pc->v_broadcast_u32(vTail, tailPixels);

  Mem sMem = mem_ptr(sPtr);
  for (uint32_t i = 0; i < vecCount; i++) {
    cc->vpmovsxbd(vPred, mPred);
    cc->emit(loadInstId, dVec[i], vPred, sMem);
    cc->vpblendvb(dVec[i], vTail, dVec[i], vPred);

    sMem.addOffset(int32_t(dVec[i].size()));
    mPred.addOffset(-int32_t(vecElementCount * 8));
  }

  pc->j(L_Done);

  pc->bind(L_LessThan4);
  fetchPredicatedVec8_1To3(pc, dVec[0], sPtr, AdvanceMode::kNoAdvance, count);
  for (uint32_t i = 1; i < vecCount; i++) {
    pc->v_zero_i(dVec[i]);
  }

  pc->bind(L_Done);

  if (advanceMode == AdvanceMode::kAdvance) {
    pc->add(sPtr, sPtr, count.cloneAs(sPtr));
  }
}

static void fetchPredicatedVec8_AVX512(PipeCompiler* pc, const VecArray& dVec, Gp sPtr, uint32_t n, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  uint32_t vecCount = dVec.size();
  uint32_t vecElementCount = dVec[0].size();

  Gp count = predicate.count();

  if (vecCount == 1u) {
    pc->v_load_predicated_u8(dVec[0], mem_ptr(sPtr), n, predicate);

    if (advanceMode == AdvanceMode::kAdvance) {
      pc->add(sPtr, sPtr, count.cloneAs(sPtr));
    }
  }
  else {
    BL_ASSERT(n >= 64);
    BL_ASSERT(dVec[0].isZmm());

    AsmCompiler* cc = pc->cc;

    Gp nMov = pc->newGpPtr("nMov");
    Gp nPred = pc->newGpPtr("nPred");
    KReg kPred = cc->newKq("kPred");

    if (vecElementCount <= 32 || pc->is64Bit()) {
      // NOTE: BHZI instruction is used to create the load mask. It's a pretty interesting instruction as unlike others
      // it uses 8 bits of index, which are basically saturated to OperandSize. This is great for our use as the maximum
      // registers we load is 4, which is 256-1 bytes total (we decrement one byte as predicated is not intended to load
      // ALL bytes).
      //
      // Additionally, we use POPCNT to count bits in the mask, which are then used to decrement nPred and possibly
      // increment the source pointer.
      Gp gpPred = pc->newGpPtr("gpPred");
      pc->mov(gpPred, -1);
      pc->mov(nPred.cloneAs(count), count);

      for (uint32_t i = 0; i < vecCount; i++) {
        Gp nDec = nPred;
        if (i != vecCount - 1) {
          nDec = nMov;
        }

        if (vecElementCount == 64u) {
          cc->bzhi(gpPred, gpPred, nPred);
          cc->kmovq(kPred, gpPred);
          if (i != vecCount - 1) {
            cc->popcnt(nMov, gpPred);
          }
        }
        else if (vecElementCount == 32u) {
          cc->bzhi(gpPred.r32(), gpPred.r32(), nPred.r32());
          cc->kmovd(kPred, gpPred.r32());
          if (i != vecCount - 1) {
            cc->popcnt(nMov.r32(), gpPred.r32());
          }
        }
        else {
          cc->bzhi(gpPred.r32(), gpPred.r32(), nPred.r32());
          cc->kmovw(kPred, gpPred.r32());
          if (i != vecCount - 1) {
            cc->movzx(nMov.r32(), gpPred.r16());
            cc->popcnt(nMov.r32(), nMov.r32());
          }
        }

        if (advanceMode == AdvanceMode::kAdvance) {
          cc->k(kPred).z().vmovdqu8(dVec[i], mem_ptr(sPtr));
          cc->add(sPtr, nDec);
        }
        else {
          cc->k(kPred).z().vmovdqu8(dVec[i], mem_ptr(sPtr, i * vecElementCount));
        }

        if (i < vecCount - 1u) {
          cc->sub(nPred, nDec);
        }
      }
    }
    else {
      x86::Mem mem = pc->_getMemConst(commonTable.k_msk64_data);
      mem.setIndex(nMov);
      mem.setShift(3);
      pc->mov(nPred.cloneAs(count), count);

      for (uint32_t i = 0; i < vecCount; i++) {
        pc->umin(nMov, nPred, vecElementCount);

        if (vecElementCount == 64u)
          cc->kmovq(kPred, mem);
        else if (vecElementCount == 32u)
          cc->kmovd(kPred, mem);
        else
          cc->kmovw(kPred, mem);

        if (advanceMode == AdvanceMode::kAdvance) {
          cc->k(kPred).z().vmovdqu8(dVec[i], mem_ptr(sPtr));
          cc->add(sPtr, nMov);
        }
        else {
          cc->k(kPred).z().vmovdqu8(dVec[i], mem_ptr(sPtr, i * vecElementCount));
        }

        if (i < vecCount - 1u) {
          cc->sub(nPred, nMov);
        }
      }
    }
  }
}

static void fetchPredicatedVec32_AVX(PipeCompiler* pc, const VecArray& dVec, Gp sPtr, uint32_t n, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  uint32_t vecCount = dVec.size();
  uint32_t vecElementCount = dVec[0].size() / 4u;

  Gp count = predicate.count();
  Mem sMem = mem_ptr(sPtr);

  if (vecCount == 1u) {
    pc->v_load_predicated_u32(dVec[0], sMem, n, predicate);
  }
  else {
    AsmCompiler* cc = pc->cc;
    InstId loadInstId = pc->hasAVX2() ? x86::Inst::kIdVpmaskmovd : x86::Inst::kIdVmaskmovps;

    Vec vPred = pc->newSimilarReg(dVec[0], "vPred");
    Mem mPred = pc->simdMemConst(pc->ct.loadstore16_lo8_msk8(), Bcst::kNA_Unique, vPred);
    mPred.setIndex(count.cloneAs(sPtr), 3);

    for (uint32_t i = 0; i < vecCount; i++) {
      cc->vpmovsxbd(vPred, mPred);
      cc->emit(loadInstId, dVec[i], vPred, sMem);

      sMem.addOffset(int32_t(dVec[i].size()));
      mPred.addOffset(-int32_t(vecElementCount * 8));
    }
  }

  if (advanceMode == AdvanceMode::kAdvance) {
    pc->add_scaled(sPtr, count.cloneAs(sPtr), 4u);
  }
}

static void fetchPredicatedVec32_AVX512(PipeCompiler* pc, const VecArray& dVec, Gp sPtr, uint32_t n, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  uint32_t vecCount = dVec.size();
  uint32_t vecElementCount = dVec[0].size() / 4u;

  Gp count = predicate.count();
  Mem sMem = mem_ptr(sPtr);

  if (vecCount == 1u) {
    pc->v_load_predicated_u32(dVec[0], sMem, n, predicate);
  }
  else {
    AsmCompiler* cc = pc->cc;
    Gp gpPred;
    KReg kPred;

    if (vecCount <= 2u) {
      gpPred = pc->newGp32("gpPred");
      kPred = cc->newKd("kPred");
    }
    else {
      gpPred = pc->newGp64("gpPred");
      kPred = cc->newKq("kPred");
    }

    pc->mov(gpPred, -1);
    cc->bzhi(gpPred, gpPred, count.cloneAs(gpPred));

    if (vecCount <= 2u)
      cc->kmovd(kPred, gpPred);
    else
      cc->kmovq(kPred, gpPred);

    for (uint32_t i = 0; i < vecCount; i++) {
      cc->k(kPred).z().vmovdqu32(dVec[i], sMem);
      sMem.addOffset(int32_t(dVec[i].size()));

      if (i + 1u != vecCount) {
        if (vecCount <= 2u)
          cc->kshiftrd(kPred, kPred, vecElementCount);
        else
          cc->kshiftrq(kPred, kPred, vecElementCount);
      }
    }
  }

  if (advanceMode == AdvanceMode::kAdvance) {
    pc->add_scaled(sPtr, count.cloneAs(sPtr), 4u);
  }
}
#endif // BL_JIT_ARCH_X86

// The following code implements fetching 128-bit vectors without any kind of hardware support. We employ two
// strategies. If the number of vectors to fetch is greater than 1 we branch to the implementation depending
// on whether we can fetch at least one FULL vector - and then we fetch the rest without branches. If we cannot
// fetch a FULL vector, we would use branches to fetch individual lanes.
static void fetchPredicatedVec32_V128(PipeCompiler* pc, const VecArray& dVec, Gp sPtr, uint32_t n, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  blUnused(n);

  uint32_t vecCount = dVec.size();

  Gp count = predicate.count();

  // Zero all vectors except the first, which is guaranteed to be modified by the fetch.
  //
  // NOTE: We have to zero the registers as otherwise they would contain garbage, which would then be processed.
  // The garbage is actually only the part of the problem - much bigger problem would be AsmJit not being to
  // compute exact liveness, which could possible make the life of dVec[1..N] to span across most of the function.
  for (uint32_t i = 1u; i < vecCount; i++) {
    pc->v_zero_i(dVec[i]);
  }

  Label L_Done;

  Gp adjusted1 = pc->newGpPtr("@adjusted1");
  Gp adjusted2 = pc->newGpPtr("@adjusted2");

  pc->add_ext(adjusted2, sPtr, count.cloneAs(sPtr), 4, -4);

  if (vecCount > 1u) {
    // TODO: [JIT] UNIMPLEMENTED: Not expected to have more than 2 - 2 vectors would be unpacked to 4, which is the limit.
    BL_ASSERT(vecCount == 2);

    L_Done = pc->newLabel();

    Label L_TailOnly = pc->newLabel();
    pc->j(L_TailOnly, ucmp_lt(count, 4));

    pc->add(adjusted1, sPtr, 16);
    pc->umin(adjusted1, adjusted1, adjusted2);

    pc->v_loadu128(dVec[0], mem_ptr(sPtr));
    pc->v_loadu32(dVec[1], mem_ptr(adjusted1));

    pc->add(adjusted1, sPtr, 20);
    pc->umin(adjusted1, adjusted1, adjusted2);
    fetchSecond32BitElement(pc, dVec[1], mem_ptr(adjusted1));
    fetchThird32BitElement(pc, dVec[1], mem_ptr(adjusted2));

    pc->j(L_Done);
    pc->bind(L_TailOnly);
  }

  {
    pc->v_loadu32(dVec[0], mem_ptr(sPtr));
    pc->add(adjusted1, sPtr, 4);
    pc->umin(adjusted1, adjusted1, adjusted2);
    fetchSecond32BitElement(pc, dVec[0], mem_ptr(adjusted1));
    fetchThird32BitElement(pc, dVec[0], mem_ptr(adjusted2));
  }

  if (L_Done.isValid()) {
    pc->bind(L_Done);
  }

  if (advanceMode == AdvanceMode::kAdvance) {
    pc->add(sPtr, adjusted2, 4);
  }

  predicate.addMaterializedEndPtr(sPtr, adjusted1, adjusted2);
}

void fetchPredicatedVec8(PipeCompiler* pc, const VecArray& dVec_, Gp sPtr, uint32_t n, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  // Restrict the number of vectors to match `n` exactly.
  VecArray dVec(dVec_);
  dVec.truncate(calculateVecCount(dVec[0].size(), n));

  BL_ASSERT(!dVec.empty());
  BL_ASSERT(n >= 2u);

#if defined(BL_JIT_ARCH_X86)
  if (n <= 16)
    dVec[0] = dVec[0].v128();
  else if (n <= 32u && dVec.size() == 1u)
    dVec[0] = dVec[0].v256();

  // Don't spoil the generic implementation with 256-bit and 512-bit vectors. In AVX/AVX2/AVX-512 cases we always
  // want to use masked loads as they are always relatively cheap and should be cheaper than branching or scalar loads.
  if (pc->hasAVX512()) {
    fetchPredicatedVec8_AVX512(pc, dVec, sPtr, n, advanceMode, predicate);
    return;
  }

  // Must be XMM/YMM if AVX-512 is not available.
  BL_ASSERT(!dVec[0].isZmm());

  if (pc->hasAVX()) {
    fetchPredicatedVec8_AVX(pc, dVec, sPtr, n, advanceMode, predicate);
    return;
  }

  // Must be XMM if AVX is not available.
  BL_ASSERT(dVec[0].isXmm());
#endif // BL_JIT_ARCH_X86

  fetchPredicatedVec8_V128(pc, dVec, sPtr, n, advanceMode, predicate);
}

void fetchPredicatedVec32(PipeCompiler* pc, const VecArray& dVec_, Gp sPtr, uint32_t n, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  // Restrict the number of vectors to match `n` exactly.
  VecArray dVec(dVec_);
  dVec.truncate(calculateVecCount(dVec[0].size(), n * 4u));

  BL_ASSERT(!dVec.empty());
  BL_ASSERT(n >= 2u);

#if defined(BL_JIT_ARCH_X86)
  if (n <= 4)
    dVec[0] = dVec[0].v128();
  else if (n <= 8u && dVec.size() == 1u)
    dVec[0] = dVec[0].v256();

  // Don't spoil the generic implementation with 256-bit and 512-bit vectors. In AVX/AVX2/AVX-512 cases we always
  // want to use masked loads as they are always relatively cheap and should be cheaper than branching or scalar loads.
  if (pc->hasAVX512()) {
    fetchPredicatedVec32_AVX512(pc, dVec, sPtr, n, advanceMode, predicate);
    return;
  }

  // Must be XMM/YMM if AVX-512 is not available.
  BL_ASSERT(!dVec[0].isZmm());

  if (pc->hasAVX()) {
    fetchPredicatedVec32_AVX(pc, dVec, sPtr, n, advanceMode, predicate);
    return;
  }

  // Must be XMM if AVX is not available.
  BL_ASSERT(dVec[0].isXmm());
#endif // BL_JIT_ARCH_X86

  fetchPredicatedVec32_V128(pc, dVec, sPtr, n, advanceMode, predicate);
}

// bl::Pipeline::Jit::FetchUtils - Predicated Store
// ================================================

#if defined(BL_JIT_ARCH_X86)
static void storePredicatedVec8_AVX512(PipeCompiler* pc, Gp dPtr, VecArray sVec, uint32_t n, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  AsmCompiler* cc = pc->cc;

  uint32_t vecCount = sVec.size();
  uint32_t vecElementCount = sVec[0].size();

  Gp count = predicate.count();

  // If there is a multiple of input vectors and they are not ZMMs, convert to ZMMs first so we can use as little
  // writes as possible. We are compiling for AVX-512 machine so we have 512-bit SIMD.
  if (vecCount > 1u) {
    if (sVec[0].isVec128()) {
      Vec v256 = pc->newV512("@store_256");
      if (vecCount == 4u) {
        Vec v512 = pc->newV512("@store_512");

        pc->v_insert_v128(v512.ymm(), sVec[0].ymm(), sVec[1].xmm(), 1);
        pc->v_insert_v128(v256.ymm(), sVec[2].ymm(), sVec[3].xmm(), 1);
        pc->v_insert_v256(v512, v512, v256, 1);

        sVec.init(v512);
        vecCount = 1;
        vecElementCount = 64;
      }
      else if (vecCount == 2u) {
        pc->v_insert_v128(v256, sVec[0].ymm(), sVec[1].xmm(), 1);

        sVec.init(v256);
        vecCount = 1;
        vecElementCount = 32;
      }
      else {
        // 3 elements? No...
        BL_NOT_REACHED();
      }
    }
    else if (sVec[0].isVec256()) {
      VecArray newVec;
      uint32_t newCount = (vecCount + 1u) / 2u;

      pc->newVecArray(newVec, newCount, VecWidth::k512, "@store_vec");
      pc->v_insert_v256(newVec, sVec.even(), sVec.odd(), 1);

      sVec = newVec;
      vecCount = newCount;
      vecElementCount = 64;
    }
  }

  // Simplified case used when there is only one vector to store.
  if (vecCount == 1u) {
    pc->v_store_predicated_u8(mem_ptr(dPtr), sVec[0], n, predicate);
    if (advanceMode == AdvanceMode::kAdvance) {
      pc->add(dPtr, dPtr, count.cloneAs(dPtr));
    }
    return;
  }

  // Predicated writes are very expensive on all modern HW due to store forwarding. In general we want to minimize
  // the number of write operations that involve predication so we try to store as many vectors as possible by using
  // regular stores. This complicates the code a bit, but improved the performance on all the hardware tested.
  Vec vTail = pc->newSimilarReg(sVec[0], "@vTail");
  Gp remaining = pc->newGpPtr("@remaining");

  if (advanceMode == AdvanceMode::kNoAdvance) {
    Gp dPtrCopy = pc->newSimilarReg(dPtr, "@dPtrCopy");
    pc->mov(dPtrCopy, dPtr);
    dPtr = dPtrCopy;
  }

  Label L_Tail = pc->newLabel();
  Label L_Done = pc->newLabel();

  pc->mov(remaining.r32(), count.r32());
  for (uint32_t i = 0; i < vecCount - 1; i++) {
    pc->v_mov(vTail, sVec[i]);
    pc->j(L_Tail, sub_c(remaining.r32(), vecElementCount));
    pc->v_store_iany(mem_ptr(dPtr), sVec[i], vecElementCount, Alignment(1));
    pc->add(dPtr, dPtr, vecElementCount);
  }
  pc->v_mov(vTail, sVec[vecCount - 1]);

  pc->bind(L_Tail);
  pc->j(L_Done, add_z(remaining.r32(), vecElementCount));
  KReg kPred = pc->makeMaskPredicate(predicate, vecElementCount, remaining);
  cc->k(kPred).vmovdqu8(mem_ptr(dPtr), vTail);

  pc->bind(L_Done);
}
#endif // BL_JIT_ARCH_X86

void storePredicatedVec8(PipeCompiler* pc, const Gp& dPtr_, const VecArray& sVec_, uint32_t n, AdvanceMode advanceMode_, PixelPredicate& predicate) noexcept {
  // Restrict the number of vectors to match `n` exactly.
  VecArray sVec(sVec_);
  sVec.truncate(calculateVecCount(sVec[0].size(), n));

  AdvanceMode advanceMode(advanceMode_);

  BL_ASSERT(!sVec.empty());
  BL_ASSERT(n >= 2u);

#if defined(BL_JIT_ARCH_X86)
  if (n <= 16u)
    sVec[0] = sVec[0].v128();
  else if (n <= 32u && sVec.size() == 1u)
    sVec[0] = sVec[0].v256();

  if (pc->hasAVX512()) {
    storePredicatedVec8_AVX512(pc, dPtr_, sVec, n, advanceMode, predicate);
    return;
  }
#endif // BL_JIT_ARCH_X86

  Gp dPtr = dPtr_;
  Gp count = predicate.count();

  Mem dMem = mem_ptr(dPtr);
  uint32_t sizeMinusOne = sVec.size() - 1u;

  Vec vLast = sVec[sizeMinusOne];
  bool tailCanBeEmpty = false;

  uint32_t remaining = n;
  uint32_t elementCount = vLast.size();

  auto makeDPtrCopy = [&]() noexcept {
    BL_ASSERT(advanceMode == AdvanceMode::kNoAdvance);

    dPtr = pc->newSimilarReg(dPtr, "@dPtrCopy");
    advanceMode = AdvanceMode::kAdvance;

    pc->mov(dPtr, dPtr_);
  };

  if (sizeMinusOne || !vLast.isVec128()) {
    count = pc->newSimilarReg(count, "@count");
    vLast = pc->newSimilarReg(vLast, "@vLast");
    tailCanBeEmpty = true;

    pc->mov(count, predicate.count());
    pc->v_mov(vLast, sVec[0]);

    if (advanceMode == AdvanceMode::kNoAdvance)
      makeDPtrCopy();
  }

  // Process whole vectors in case that there is more than one vector in `sVec`.
  if (sizeMinusOne) {
    Label L_Tail = pc->newLabel();
    uint32_t requiredCount = elementCount;

    for (uint32_t i = 0; i < sizeMinusOne; i++) {
      pc->j(L_Tail, ucmp_lt(count, requiredCount));
      pc->v_storeuvec_u32(dMem, sVec[i]);
      pc->add(dPtr, dPtr, vLast.size());
      pc->v_mov(vLast, sVec[i + 1]);

      BL_ASSERT(remaining >= elementCount);
      remaining -= elementCount;
      requiredCount += elementCount;
    }

    pc->bind(L_Tail);
  }

#if defined(BL_JIT_ARCH_X86)
  if (vLast.isZmm()) {
    BL_ASSERT(remaining > 32u);

    Label L_StoreSkip32 = pc->newLabel();
    pc->j(L_StoreSkip32, bt_z(count, 5));
    pc->v_storeu256(dMem, vLast.ymm());
    pc->v_extract_v256(vLast.ymm(), vLast, 1);
    pc->add(dPtr, dPtr, 32);
    pc->bind(L_StoreSkip32);

    vLast = vLast.ymm();
    remaining -= 32u;
  }

  if (vLast.isYmm()) {
    BL_ASSERT(remaining > 16u);

    Label L_StoreSkip16 = pc->newLabel();
    pc->j(L_StoreSkip16, bt_z(count, 4));
    pc->v_storeu128(dMem, vLast.xmm());
    pc->v_extract_v128(vLast.xmm(), vLast, 1);
    pc->add(dPtr, dPtr, 16);
    pc->bind(L_StoreSkip16);

    vLast = vLast.xmm();
    remaining -= 16u;
  }
#endif // BL_JIT_ARCH_X86

  if (remaining > 8u) {
    Label L_StoreSkip8 = pc->newLabel();
    pc->j(L_StoreSkip8, bt_z(count, 3));
    pc->v_storeu64(dMem, vLast);
    pc->shiftOrRotateRight(vLast, vLast, 8);
    pc->add(dPtr, dPtr, 8);
    pc->bind(L_StoreSkip8);

    remaining -= 8u;
  }

  if (remaining > 4u) {
    Label L_StoreSkip4 = pc->newLabel();
    pc->j(L_StoreSkip4, bt_z(count, 2));
    pc->v_storeu32(dMem, vLast);
    pc->shiftOrRotateRight(vLast, vLast, 4);
    pc->add(dPtr, dPtr, 4);
    pc->bind(L_StoreSkip4);

    remaining -= 4u;
  }

  Gp gpLast = pc->newGp32("@gpLast");
  pc->s_mov_u32(gpLast, vLast);

  if (remaining > 2u) {
    Label L_StoreSkip2 = pc->newLabel();
    pc->j(L_StoreSkip2, bt_z(count, 1));
    pc->store_u16(dMem, gpLast);
    pc->shr(gpLast, gpLast, 16);
    pc->add(dPtr, dPtr, 2);
    pc->bind(L_StoreSkip2);

    remaining -= 2u;
  }

  Label L_StoreSkip1 = pc->newLabel();
  pc->j(L_StoreSkip1, bt_z(count, 0));
  pc->store_u8(dMem, gpLast);
  pc->add(dPtr, dPtr, 1);
  pc->bind(L_StoreSkip1);

  // Fix a warning that a variable is set, but never used. It's used in asserts and on x86 target.
  blUnused(remaining);

  // Let's keep it if for some reason we would need it in the future.
  blUnused(tailCanBeEmpty);
}

void storePredicatedVec32(PipeCompiler* pc, const Gp& dPtr_, const VecArray& sVec_, uint32_t n, AdvanceMode advanceMode_, PixelPredicate& predicate) noexcept {
  // Restrict the number of vectors to match `n` exactly.
  VecArray sVec(sVec_);
  sVec.truncate(calculateVecCount(sVec[0].size(), n * 4u));

  BL_ASSERT(!sVec.empty());
  BL_ASSERT(n >= 2u);

#if defined(BL_JIT_ARCH_X86)
  if (n <= 4)
    sVec[0] = sVec[0].v128();
  else if (n <= 8u && sVec.size() == 1u)
    sVec[0] = sVec[0].v256();
#endif // BL_JIT_ARCH_X86

  AdvanceMode advanceMode(advanceMode_);

  Gp dPtr = dPtr_;
  Gp count = predicate.count();

  Mem dMem = mem_ptr(dPtr);
  uint32_t sizeMinusOne = sVec.size() - 1u;

  Vec vLast = sVec[sizeMinusOne];
  bool tailCanBeEmpty = false;

  uint32_t remaining = n;
  uint32_t elementCount = vLast.size() / 4u;

  auto makeDPtrCopy = [&]() noexcept {
    BL_ASSERT(advanceMode == AdvanceMode::kNoAdvance);

    dPtr = pc->newSimilarReg(dPtr, "@dPtrCopy");
    advanceMode = AdvanceMode::kAdvance;

    pc->mov(dPtr, dPtr_);
  };

  if (sizeMinusOne || !vLast.isVec128()) {
    count = pc->newSimilarReg(count, "@count");
    vLast = pc->newSimilarReg(vLast, "@vLast");
    tailCanBeEmpty = true;

    pc->mov(count, predicate.count());
    pc->v_mov(vLast, sVec[0]);

    if (advanceMode == AdvanceMode::kNoAdvance)
      makeDPtrCopy();
  }

  // Process whole vectors in case that there is more than one vector in `sVec`. It makes no sense to process
  // ALL vectors with a predicate as that would be unnecessarily complicated and possibly not that efficient
  // considering the high cost of predicated stores of tested micro-architectures.
  if (sizeMinusOne) {
    Label L_Tail = pc->newLabel();
    uint32_t requiredCount = elementCount;

    for (uint32_t i = 0; i < sizeMinusOne; i++) {
      pc->j(L_Tail, ucmp_lt(count, requiredCount));
      pc->v_storeuvec_u32(dMem, sVec[i]);
      pc->add(dPtr, dPtr, vLast.size());
      pc->v_mov(vLast, sVec[i + 1]);

      BL_ASSERT(remaining >= elementCount);
      remaining -= elementCount;
      requiredCount += elementCount;
    }

    pc->bind(L_Tail);
  }

#if defined(BL_JIT_ARCH_X86)
  // Let's use AVX/AVX2/AVX-512 masking stores if fast store with mask is enabled.
  if (pc->hasOptFlag(PipeOptFlags::kFastStoreWithMask)) {
    pc->v_store_predicated_u32(dMem, vLast, remaining, predicate);

    // Local advancing can be true, however, if we stored with predicate it means that the initial pointer
    // can be untouched. So check against the passed `advanceMode_` instead of advance, which would be true
    // if there was multiple vector registers to store.
    if (advanceMode_ == AdvanceMode::kAdvance) {
      pc->add_scaled(dPtr, count.cloneAs(dPtr), 4u);
    }

    return;
  }

  if (vLast.isZmm()) {
    BL_ASSERT(remaining > 8u);

    Label L_StoreSkip8 = pc->newLabel();
    pc->j(L_StoreSkip8, bt_z(count, 3));
    pc->v_storeu256(dMem, vLast.ymm());
    pc->v_extract_v256(vLast.ymm(), vLast, 1);
    pc->add(dPtr, dPtr, 32);
    pc->bind(L_StoreSkip8);

    vLast = vLast.ymm();
    remaining -= 8u;
  }

  if (vLast.isYmm()) {
    BL_ASSERT(remaining > 4u);

    Label L_StoreSkip4 = pc->newLabel();
    pc->j(L_StoreSkip4, bt_z(count, 2));
    pc->v_storeu128(dMem, vLast.xmm());
    pc->v_extract_v128(vLast.xmm(), vLast, 1);
    pc->add(dPtr, dPtr, 16);
    pc->bind(L_StoreSkip4);

    vLast = vLast.xmm();
    remaining -= 4u;
  }
#endif // BL_JIT_ARCH_X86

  Label L_TailDone;

  if (tailCanBeEmpty)
    L_TailDone = pc->newLabel();

  if (count.id() != predicate.count().id()) {
    if (!tailCanBeEmpty)
      pc->and_(count, count, 0x3);
    else
      pc->j(L_TailDone, and_z(count, 0x3));
  }
  else if (tailCanBeEmpty) {
    pc->j(L_TailDone, cmp_eq(count, 0));
  }

  Gp adjusted1;
  Gp adjusted2;

  if (const PixelPredicate::MaterializedEndPtr* materialized = predicate.findMaterializedEndPtr(dPtr_)) {
    adjusted1 = materialized->adjusted1;
    adjusted2 = materialized->adjusted2;
  }
  else {
    adjusted1 = pc->newGpPtr("@adjusted1");
    adjusted2 = pc->newGpPtr("@adjusted2");

    pc->add_ext(adjusted2, dPtr, count.cloneAs(dPtr), 4, -4);
    pc->add(adjusted1, dPtr, 4);
    pc->umin(adjusted1, adjusted1, adjusted2);
  }

  pc->v_store_extract_u32(mem_ptr(adjusted2), vLast, 2);
  pc->v_store_extract_u32(mem_ptr(adjusted1), vLast, 1);
  pc->v_storeu32_u32(mem_ptr(dPtr), vLast);

  if (advanceMode_ == AdvanceMode::kAdvance)
    pc->add(dPtr, adjusted2, 4);

  if (tailCanBeEmpty) {
    pc->bind(L_TailDone);
  }

  // Fix a warning that a variable is set, but never used. It's used in asserts and on x86 target.
  blUnused(remaining);
}

// bl::Pipeline::Jit::FetchUtils - Fetch Mask
// ==========================================

#if defined(BL_JIT_ARCH_A64)
static void multiplyMaskWithGlobalAlpha(PipeCompiler* pc, VecArray vm, const Vec& globalAlpha, uint32_t n) noexcept {
  vm.truncate((n + 15u) / 16u);

  VecArray vt;
  pc->newVecArray(vt, vm.size(), VecWidth::k128, "@vt0");

  if (n > 8u)
    pc->v_mulw_hi_u8(vt, vm, globalAlpha);
  pc->v_mulw_lo_u8(vm, vm, globalAlpha);

  pc->v_srli_rnd_acc_u16(vm, vm, 8);
  if (n > 8u)
    pc->v_srli_rnd_acc_u16(vt, vt, 8);

  pc->v_srlni_rnd_lo_u16(vm, vm, 8);
  if (n > 8u)
    pc->v_srlni_rnd_hi_u16(vm, vt, 8);
}
#endif

void fetchMaskA8AndAdvance(PipeCompiler* pc, VecArray& vm, const Gp& mPtr, PixelCount n, PixelType pixelType, PixelCoverageFormat coverageFormat, const Vec& globalAlpha, PixelPredicate& predicate) noexcept {
  // Not used on X86.
  blUnused(coverageFormat);

  Mem m = mem_ptr(mPtr);

  switch (pixelType) {
    case PixelType::kA8: {
      BL_ASSERT(n != 1u);

#if defined(BL_JIT_ARCH_A64)
      if (coverageFormat == PixelCoverageFormat::kPacked) {
        VecWidth vecWidth = pc->vecWidthOf(DataWidth::k8, n);
        uint32_t vecCount = pc->vecCountOf(DataWidth::k8, n);

        pc->newVecArray(vm, vecCount, vecWidth, "vm");
        fetchVec8(pc, vm, mPtr, n.value(), AdvanceMode::kAdvance, predicate);

        if (globalAlpha.isValid()) {
          multiplyMaskWithGlobalAlpha(pc, vm, globalAlpha, n.value());
        }
      }
      else
#endif // BL_JIT_ARCH_A64
      {
        VecWidth vecWidth = pc->vecWidthOf(DataWidth::k16, n);
        uint32_t vecCount = pc->vecCountOf(DataWidth::k16, n);

        pc->newVecArray(vm, vecCount, vecWidth, "vm");

        if (predicate.empty()) {
          switch (n.value()) {
            case 2:
#if defined(BL_JIT_ARCH_X86)
              if (pc->hasAVX2()) {
                pc->v_broadcast_u16(vm[0], m);
              }
              else
#endif // BL_JIT_ARCH_X86
              {
                pc->v_loadu16(vm[0], m);
              }
              pc->v_cvt_u8_lo_to_u16(vm[0], vm[0]);
              break;

            case 4:
              pc->v_loada32(vm[0], m);
              pc->v_cvt_u8_lo_to_u16(vm[0], vm[0]);
              break;

            case 8:
              pc->v_cvt_u8_lo_to_u16(vm[0], m);
              break;

            default: {
              for (uint32_t i = 0; i < vecCount; i++) {
                pc->v_cvt_u8_lo_to_u16(vm[i], m);
                m.addOffsetLo32(vm[i].size() / 2u);
              }
              break;
            }
          }

          pc->add(mPtr, mPtr, n.value());
        }
        else {
          if (n.value() <= 8) {
            fetchPredicatedVec8(pc, vm, mPtr, n.value(), AdvanceMode::kAdvance, predicate);
            pc->v_cvt_u8_lo_to_u16(vm[0], vm[0]);
          }
#if defined(BL_JIT_ARCH_X86)
          else if (vm[0].size() > 16u) {
            VecArray lo = vm.cloneAs(VecWidth(uint32_t(vm.vecWidth()) - 1u));
            fetchPredicatedVec8(pc, lo, mPtr, n.value(), AdvanceMode::kAdvance, predicate);
            pc->v_cvt_u8_lo_to_u16(vm, vm);
          }
#endif
          else {
            VecArray even = vm.even();
            VecArray odd = vm.odd();
            fetchPredicatedVec8(pc, even, mPtr, n.value(), AdvanceMode::kAdvance, predicate);
            pc->v_cvt_u8_hi_to_u16(odd, even);
            pc->v_cvt_u8_lo_to_u16(even, even);
          }
        }

        if (globalAlpha.isValid()) {
          pc->v_mul_i16(vm, vm, globalAlpha.cloneAs(vm[0]));
          pc->v_div255_u16(vm);
        }
      }

      break;
    }

    case PixelType::kRGBA32: {
#if defined(BL_JIT_ARCH_A64)
      if (coverageFormat == PixelCoverageFormat::kPacked) {
        VecWidth vecWidth = pc->vecWidthOf(DataWidth::k32, n);
        uint32_t vecCount = pc->vecCountOf(DataWidth::k32, n);

        BL_ASSERT(vecCount <= 4u);

        pc->newVecArray(vm, vecCount, vecWidth, "vm");
        fetchVec8(pc, vm, mPtr, n.value(), AdvanceMode::kAdvance, predicate);

        if (globalAlpha.isValid()) {
          multiplyMaskWithGlobalAlpha(pc, vm, globalAlpha, n.value());
        }

        if (vecCount == 4) {
          pc->v_swizzlev_u8(vm[3], vm[0], pc->simdConst(&pc->ct.swizu8_3210xxxxxxxxxxxx_to_3333222211110000, Bcst::kNA, vm[0]));
        }

        if (vecCount >= 3) {
          pc->v_swizzlev_u8(vm[2], vm[0], pc->simdConst(&pc->ct.swizu8_xxxx3210xxxxxxxx_to_3333222211110000, Bcst::kNA, vm[0]));
        }

        if (vecCount >= 2) {
          pc->v_swizzlev_u8(vm[1], vm[0], pc->simdConst(&pc->ct.swizu8_xxxxxxxx3210xxxx_to_3333222211110000, Bcst::kNA, vm[0]));
        }

        pc->v_swizzlev_u8(vm[0], vm[0], pc->simdConst(&pc->ct.swizu8_xxxxxxxxxxxx3210_to_3333222211110000, Bcst::kNA, vm[0]));
      }
      else
#endif // BL_JIT_ARCH_A64
      {
        VecWidth vecWidth = pc->vecWidthOf(DataWidth::k64, n);
        uint32_t vecCount = pc->vecCountOf(DataWidth::k64, n);

        pc->newVecArray(vm, vecCount, vecWidth, "vm");

        switch (n.value()) {
          case 1: {
            BL_ASSERT(predicate.empty());
            BL_ASSERT(vecCount == 1);

#if defined(BL_JIT_ARCH_X86)
            if (!pc->hasAVX2()) {
              pc->v_load8(vm[0], m);
              pc->add(mPtr, mPtr, n.value());
              pc->v_swizzle_lo_u16x4(vm[0], vm[0], swizzle(0, 0, 0, 0));
            }
            else
#endif // BL_JIT_ARCH_X86
            {
              pc->v_broadcast_u8(vm[0], m);
              pc->add(mPtr, mPtr, n.value());
              pc->v_cvt_u8_lo_to_u16(vm[0], vm[0]);
            }

            if (globalAlpha.isValid()) {
              pc->v_mul_i16(vm[0], vm[0], globalAlpha.cloneAs(vm[0]));
              pc->v_div255_u16(vm[0]);
            }
            break;
          }

          case 2: {
            BL_ASSERT(vecCount == 1);

#if defined(BL_JIT_ARCH_X86)
            if (!predicate.empty() || !pc->hasAVX2()) {
              fetchVec8(pc, vm, mPtr, n.value(), AdvanceMode::kAdvance, predicate);
              pc->v_interleave_lo_u8(vm[0], vm[0], vm[0]);
              pc->v_interleave_lo_u16(vm[0], vm[0], vm[0]);
              pc->v_cvt_u8_lo_to_u16(vm[0], vm[0]);
            }
            else
            {
              pc->v_loadu16_u8_to_u64(vm[0], m);
              pc->add(mPtr, mPtr, n.value());
              pc->v_swizzlev_u8(vm[0], vm[0], pc->simdConst(&pc->ct.swizu8_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA, vm[0]));
            }
#else
            fetchVec8(pc, vm, mPtr, n.value(), AdvanceMode::kAdvance, predicate);
            pc->v_swizzlev_u8(vm[0], vm[0], pc->simdConst(&pc->ct.swizu8_xxxxxxxxxxxxxx10_to_z1z1z1z1z0z0z0z0, Bcst::kNA, vm[0]));
#endif // BL_JIT_ARCH_X86

            if (globalAlpha.isValid()) {
              pc->v_mul_i16(vm[0], vm[0], globalAlpha.cloneAs(vm[0]));
              pc->v_div255_u16(vm[0]);
            }
            break;
          }

          case 4: {
#if defined(BL_JIT_ARCH_X86)
            if (vecWidth >= VecWidth::k256) {
              if (predicate.empty()) {
                pc->v_loadu32_u8_to_u64(vm[0], m);
                pc->add(mPtr, mPtr, n.value());
              }
              else {
                fetchVec8(pc, vm, mPtr, n.value(), AdvanceMode::kAdvance, predicate);
                pc->cc->vpmovzxbq(vm[0], vm[0].xmm());
              }
              pc->v_swizzlev_u8(vm[0], vm[0], pc->simdConst(&pc->ct.swizu8_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA, vm[0]));

              if (globalAlpha.isValid()) {
                pc->v_mul_i16(vm[0], vm[0], globalAlpha.cloneAs(vm[0]));
                pc->v_div255_u16(vm[0]);
              }
            }
            else
#endif // BL_JIT_ARCH_X86
            {
              fetchVec8(pc, vm, mPtr, n.value(), AdvanceMode::kAdvance, predicate);
              pc->v_cvt_u8_lo_to_u16(vm[0], vm[0]);

              if (globalAlpha.isValid()) {
                pc->v_mul_i16(vm[0], vm[0], globalAlpha.cloneAs(vm[0]));
                pc->v_div255_u16(vm[0]);
              }

              pc->v_interleave_lo_u16(vm[0], vm[0], vm[0]);           // vm[0] = [M3 M3 M2 M2 M1 M1 M0 M0]
              pc->v_swizzle_u32x4(vm[1], vm[0], swizzle(3, 3, 2, 2)); // vm[1] = [M3 M3 M3 M3 M2 M2 M2 M2]
              pc->v_swizzle_u32x4(vm[0], vm[0], swizzle(1, 1, 0, 0)); // vm[0] = [M1 M1 M1 M1 M0 M0 M0 M0]
            }
            break;
          }

          default: {
#if defined(BL_JIT_ARCH_X86)
            if (vecWidth >= VecWidth::k256) {
              if (predicate.empty()) {
                for (uint32_t i = 0; i < vecCount; i++) {
                  pc->v_loaduvec_u8_to_u64(vm[i], m);
                  m.addOffsetLo32(vm[i].size() / 8u);
                }
                pc->add(mPtr, mPtr, n.value());

                if (globalAlpha.isValid()) {
                  if (pc->hasOptFlag(PipeOptFlags::kFastVpmulld)) {
                    pc->v_mul_i32(vm, vm, globalAlpha.cloneAs(vm[0]));
                    pc->v_div255_u16(vm);
                    pc->v_swizzle_u32x4(vm, vm, swizzle(2, 2, 0, 0));
                  }
                  else {
                    pc->v_mul_i16(vm, vm, globalAlpha.cloneAs(vm[0]));
                    pc->v_div255_u16(vm);
                    pc->v_swizzlev_u8(vm, vm, pc->simdConst(&pc->ct.swizu8_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA, vm[0]));
                  }
                }
                else {
                  pc->v_swizzlev_u8(vm, vm, pc->simdConst(&pc->ct.swizu8_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA, vm[0]));
                }
              }
              else {
                VecArray pm;
                VecArray um;

                pc->newVecArray(pm, pc->vecCountOf(DataWidth::k8, n), pc->vecWidthOf(DataWidth::k8, n), "pm");
                pc->newVecArray(um, pc->vecCountOf(DataWidth::k16, n), pc->vecWidthOf(DataWidth::k16, n), "um");

                fetchVec8(pc, pm, mPtr, n.value(), AdvanceMode::kAdvance, predicate);

                if (um.size() == 1) {
                  pc->v_cvt_u8_lo_to_u16(um, pm);
                }
                else {
                  pc->v_cvt_u8_hi_to_u16(um.odd(), pm);
                  pc->v_cvt_u8_lo_to_u16(um.even(), pm);
                }

                if (globalAlpha.isValid()) {
                  pc->v_mul_i16(um, um, globalAlpha.cloneAs(um[0]));
                  pc->v_div255_u16(um);
                }

                if (vm[0].isZmm()) {
                  if (pc->hasAVX512_VBMI()) {
                    // Extract 128-bit vectors and then use VPERMB to permute 8 elements to 512-bit width.
                    Vec pred = pc->simdVecConst(&pc->ct.permu8_4xu8_lo_to_rgba32_uc, Bcst::kNA_Unique, vm);

                    for (uint32_t i = 1; i < vm.size(); i++) {
                      pc->v_extract_v128(vm[i], um[0], i);
                    }

                    for (uint32_t i = 0; i < vm.size(); i++) {
                      pc->v_permute_u8(vm[i], pred, (i == 0 ? um[0] : vm[i]).cloneAs(vm[i]));
                    }
                  }
                  else {
                    Vec pred = pc->simdVecConst(&pc->ct.swizu8_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA_Unique, vm);
                    for (uint32_t i = 1; i < vm.size(); i++) {
                      pc->v_extract_v128(vm[i], um[0], i);
                    }

                    for (uint32_t i = 0; i < vm.size(); i++) {
                      pc->v_cvt_u8_to_u32(vm[i], i == 0 ? um[0] : vm[i]);
                      pc->v_swizzlev_u8(vm[i], vm[i], pred);
                    }
                  }
                }
                else if (vm[0].isYmm()) {
                  Vec pred = pc->simdVecConst(&pc->ct.swizu8_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA_Unique, vm);

                  if (vm.size() >= 2)
                    pc->v_swizzle_u64x2(vm[1].xmm(), um[0].xmm(), swizzle(0, 1));

                  if (vm.size() >= 3)
                    pc->v_extract_v128(vm[2].xmm(), um[0], 1);

                  if (vm.size() >= 4)
                    pc->v_swizzle_u64x4(vm[3], um[0], swizzle(3, 3, 3, 3));

                  for (uint32_t i = 0; i < vm.size(); i++) {
                    pc->v_cvt_u8_to_u32(vm[i], i == 0 ? um[0] : vm[i]);
                    pc->v_swizzlev_u8(vm[i], vm[i], pred);
                  }
                }
                else {
                  BL_NOT_REACHED();
                }
              }
            }
            else
#endif // BL_JIT_ARCH_X86
            {
              // Maximum pixels for 128-bit SIMD is 8 - there are no registers for more...
              BL_ASSERT(n == 8);

              if (predicate.empty()) {
                pc->v_cvt_u8_lo_to_u16(vm[0], m);
                pc->add(mPtr, mPtr, n.value());
              }
              else {
                fetchVec8(pc, VecArray(vm[0]), mPtr, n.value(), AdvanceMode::kAdvance, predicate);
                pc->v_cvt_u8_lo_to_u16(vm[0], vm[0]);
              }

              if (globalAlpha.isValid()) {
                pc->v_mul_i16(vm[0], vm[0], globalAlpha.cloneAs(vm[0]));
                pc->v_div255_u16(vm[0]);
              }

              pc->v_interleave_hi_u16(vm[2], vm[0], vm[0]);           // vm[2] = [M7 M7 M6 M6 M5 M5 M4 M4]
              pc->v_interleave_lo_u16(vm[0], vm[0], vm[0]);           // vm[0] = [M3 M3 M2 M2 M1 M1 M0 M0]
              pc->v_swizzle_u32x4(vm[3], vm[2], swizzle(3, 3, 2, 2)); // vm[3] = [M7 M7 M7 M7 M6 M6 M6 M6]
              pc->v_swizzle_u32x4(vm[1], vm[0], swizzle(3, 3, 2, 2)); // vm[1] = [M3 M3 M3 M3 M2 M2 M2 M2]
              pc->v_swizzle_u32x4(vm[0], vm[0], swizzle(1, 1, 0, 0)); // vm[0] = [M1 M1 M1 M1 M0 M0 M0 M0]
              pc->v_swizzle_u32x4(vm[2], vm[2], swizzle(1, 1, 0, 0)); // vm[2] = [M5 M5 M5 M5 M4 M4 M4 M4]
            }
            break;
          }
        }
      }

      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

// bl::Pipeline::Jit::FetchUtils - Fetch Pixel(s)
// ==============================================

static void fetchPixelsA8(PipeCompiler* pc, Pixel& p, PixelCount n, PixelFlags flags, FormatExt format, Mem sMem, Alignment alignment, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  BL_ASSERT(p.isA8());
  BL_ASSERT(n.value() > 1u);

  p.setCount(n);

  Gp sPtr = sMem.baseReg().as<Gp>();

  uint32_t srcBPP = blFormatInfo[uint32_t(format)].depth / 8u;
  VecWidth paWidth = pc->vecWidthOf(DataWidth::k8, n);

#if defined(BL_JIT_ARCH_X86)
  VecWidth uaWidth = pc->vecWidthOf(DataWidth::k16, n);
#endif

  // It's forbidden to use PA in single-pixel case (scalar mode) and SA in multiple-pixel case (vector mode).
  BL_ASSERT(uint32_t(n.value() != 1) ^ uint32_t(blTestFlag(flags, PixelFlags::kSA)));

  // It's forbidden to request both - PA and UA.
  BL_ASSERT((flags & (PixelFlags::kPA | PixelFlags::kUA)) != (PixelFlags::kPA | PixelFlags::kUA));

  switch (format) {
    case FormatExt::kPRGB32: {
      Vec predicatedPixel;
      VecWidth p32Width = pc->vecWidthOf(DataWidth::k32, n);

      if (!predicate.empty()) {
        predicatedPixel = pc->newVec(p32Width, p.name(), "pred");
        fetchPredicatedVec32(pc, VecArray(predicatedPixel), sPtr, n.value(), advanceMode, predicate);

        // Don't advance again...
        advanceMode = AdvanceMode::kNoAdvance;
      }

      auto fetch4Shifted = [](PipeCompiler* pc, const Vec& dst, const Mem& src, Alignment alignment, const Vec& predicatedPixel) noexcept {
        if (predicatedPixel.isValid()) {
          pc->v_srli_u32(dst, predicatedPixel, 24);
        }
        else {
#if defined(BL_JIT_ARCH_X86)
          if (pc->hasAVX512()) {
            pc->v_srli_u32(dst, src, 24);
          }
          else
#endif // BL_JIT_ARCH_X86
          {
            pc->v_loada128(dst, src, alignment);
            pc->v_srli_u32(dst, dst, 24);
          }
        }
      };

      switch (n.value()) {
        case 4: {
          if (blTestFlag(flags, PixelFlags::kPA)) {
            pc->newVecArray(p.pa, 1, VecWidth::k128, p.name(), "pa");
            Vec a = p.pa[0];

            fetch4Shifted(pc, a, sMem, alignment, predicatedPixel);
#if defined(BL_JIT_ARCH_X86)
            if (pc->hasAVX512()) {
              pc->cc->vpmovdb(a, a);
            }
            else
#endif // BL_JIT_ARCH_X86
            {
              pc->v_packs_i32_i16(a, a, a);
              pc->v_packs_i16_u8(a, a, a);
            }

            p.pa.init(a);
          }
          else {
            pc->newVecArray(p.ua, 1, VecWidth::k128, p.name(), "ua");
            Vec a = p.ua[0];

            fetch4Shifted(pc, a, sMem, alignment, predicatedPixel);
            pc->v_packs_i32_i16(a, a, a);

            p.ua.init(a);
          }

          break;
        }

        case 8: {
          Vec a0 = pc->newV128("pa");

#if defined(BL_JIT_ARCH_X86)
          if (pc->hasAVX512()) {
            Vec aTmp = pc->newV256("a.tmp");
            pc->v_srli_u32(aTmp, sMem, 24);

            if (blTestFlag(flags, PixelFlags::kPA)) {
              pc->cc->vpmovdb(a0, aTmp);
              p.pa.init(a0);
              pc->rename(p.pa, p.name(), "pa");
            }
            else {
              pc->cc->vpmovdw(a0, aTmp);
              p.ua.init(a0);
              pc->rename(p.ua, p.name(), "ua");
            }
          }
          else
#endif // BL_JIT_ARCH_X86
          {
            Vec a1 = pc->newV128("paHi");

            fetch4Shifted(pc, a0, sMem, alignment, predicatedPixel);
            sMem.addOffsetLo32(16);
            fetch4Shifted(pc, a1, sMem, alignment, predicatedPixel);
            pc->v_packs_i32_i16(a0, a0, a1);

            if (blTestFlag(flags, PixelFlags::kPA)) {
              pc->v_packs_i16_u8(a0, a0, a0);
              p.pa.init(a0);
              pc->rename(p.pa, p.name(), "pa");
            }
            else {
              p.ua.init(a0);
              pc->rename(p.ua, p.name(), "ua");
            }
          }
          break;
        }

        case 16:
        case 32:
        case 64: {
#if defined(BL_JIT_ARCH_X86)
          uint32_t p32RegCount = VecWidthUtils::vecCountOf(p32Width, DataWidth::k32, n);
          BL_ASSERT(p32RegCount < VecArray::kMaxSize);

          if (pc->hasAVX512()) {
            VecArray p32;
            pc->newVecArray(p32, p32RegCount, p32Width, p.name(), "p32");

            auto multiVecUnpack = [](PipeCompiler* pc, VecArray& dst, VecArray src, uint32_t srcWidth) noexcept {
              uint32_t dstVecSize = dst[0].size();

              // Number of bytes in dst registers after this is done.
              uint32_t dstWidth = blMin<uint32_t>(dst.size() * dstVecSize, src.size() * srcWidth) / dst.size();

              for (;;) {
                VecArray out;
                BL_ASSERT(srcWidth < dstWidth);

                bool isLastStep = (srcWidth * 2u == dstWidth);
                uint32_t outRegCount = blMax<uint32_t>(src.size() / 2u, 1u);

                switch (srcWidth) {
                  case 4:
                    if (isLastStep)
                      out = dst.xmm();
                    else
                      pc->newV128Array(out, outRegCount, "tmp");
                    pc->v_interleave_lo_u32(out, src.even(), src.odd());
                    break;

                  case 8:
                    if (isLastStep)
                      out = dst.xmm();
                    else
                      pc->newV128Array(out, outRegCount, "tmp");
                    pc->v_interleave_lo_u64(out, src.even(), src.odd());
                    break;

                  case 16:
                    if (isLastStep)
                      out = dst.ymm();
                    else
                      pc->newV256Array(out, outRegCount, "tmp");
                    pc->v_insert_v128(out.ymm(), src.even().ymm(), src.odd().xmm(), 1);
                    break;

                  case 32:
                    BL_ASSERT(isLastStep);
                    out = dst.zmm();
                    pc->v_insert_v256(out.zmm(), src.even().zmm(), src.odd().ymm(), 1);
                    break;
                }

                srcWidth *= 2u;
                if (isLastStep)
                  break;

                src = out;
                srcWidth *= 2u;
              }
            };

            for (const Vec& v : p32) {
              if (predicatedPixel.isValid())
                pc->v_srli_u32(v, predicatedPixel, 24);
              else
                pc->v_srli_u32(v, sMem, 24);

              sMem.addOffset(v.size());
              if (blTestFlag(flags, PixelFlags::kPA))
                pc->cc->vpmovdb(v.xmm(), v);
              else
                pc->cc->vpmovdw(v.half(), v);
            }

            if (blTestFlag(flags, PixelFlags::kPA)) {
              uint32_t paRegCount = VecWidthUtils::vecCountOf(paWidth, DataWidth::k8, n);
              BL_ASSERT(paRegCount <= OpArray::kMaxSize);

              if (p32RegCount == 1) {
                p.pa.init(p32[0]);
                pc->rename(p.pa, p.name(), "pa");
              }
              else {
                pc->newVecArray(p.pa, paRegCount, paWidth, p.name(), "pa");
                multiVecUnpack(pc, p.pa, p32, p32[0].size() / 4u);
              }
            }
            else {
              uint32_t uaRegCount = VecWidthUtils::vecCountOf(paWidth, DataWidth::k16, n);
              BL_ASSERT(uaRegCount <= OpArray::kMaxSize);

              if (p32RegCount == 1) {
                p.ua.init(p32[0]);
                pc->rename(p.ua, p.name(), "ua");
              }
              else {
                pc->newVecArray(p.ua, uaRegCount, uaWidth, p.name(), "ua");
                multiVecUnpack(pc, p.ua, p32, p32[0].size() / 2u);
              }
            }
          }
          else
#endif // BL_JIT_ARCH_X86
          {
            BL_NOT_REACHED();
          }

          break;
        }

        default:
          BL_NOT_REACHED();
      }

      break;
    }

    case FormatExt::kA8: {
      uint32_t paRegCount = VecWidthUtils::vecCountOf(paWidth, DataWidth::k8, n);
      BL_ASSERT(paRegCount <= OpArray::kMaxSize);

      if (!predicate.empty()) {
        pc->newVecArray(p.pa, paRegCount, paWidth, p.name(), "pa");
        fetchPredicatedVec8(pc, p.pa, sPtr, n.value(), advanceMode, predicate);
        break;
      }

      switch (n.value()) {
        case 4: {
          Vec a = pc->newV128("a");
          sMem.setSize(4);
          pc->v_loada32(a, sMem);

          if (blTestFlag(flags, PixelFlags::kPA)) {
            p.pa.init(a);
          }
          else {
            pc->v_cvt_u8_lo_to_u16(a, a);
            p.ua.init(a);
          }

          break;
        }

        case 8: {
          Vec a = pc->newV128("a");
          sMem.setSize(8);

          if (blTestFlag(flags, PixelFlags::kPA)) {
            pc->v_loadu64(a, sMem);
            p.pa.init(a);
          }
          else {
            pc->v_loadu64_u8_to_u16(a, sMem);
            p.ua.init(a);
          }

          break;
        }

        case 16:
        case 32:
        case 64: {
#if defined(BL_JIT_ARCH_X86)
          uint32_t uaRegCount = VecWidthUtils::vecCountOf(uaWidth, DataWidth::k16, n);
          BL_ASSERT(uaRegCount <= OpArray::kMaxSize);

          if (pc->vecWidth() >= VecWidth::k256) {
            if (blTestFlag(flags, PixelFlags::kPA)) {
              pc->newVecArray(p.pa, paRegCount, paWidth, p.name(), "pa");
              for (uint32_t i = 0; i < paRegCount; i++) {
                pc->v_loadavec(p.pa[i], sMem, alignment);
                sMem.addOffsetLo32(p.pa[i].size());
              }
            }
            else {
              pc->newVecArray(p.ua, uaRegCount, uaWidth, p.name(), "ua");
              for (uint32_t i = 0; i < uaRegCount; i++) {
                pc->v_cvt_u8_lo_to_u16(p.ua[i], sMem);
                sMem.addOffsetLo32(p.ua[i].size() / 2u);
              }
            }
          }
          else if (!blTestFlag(flags, PixelFlags::kPA) && pc->hasSSE4_1()) {
            pc->newV128Array(p.ua, uaRegCount, p.name(), "ua");
            for (uint32_t i = 0; i < uaRegCount; i++) {
              pc->v_cvt_u8_lo_to_u16(p.ua[i], sMem);
              sMem.addOffsetLo32(8);
            }
          }
          else
#endif // BL_JIT_ARCH_X86
          {
            pc->newV128Array(p.pa, paRegCount, p.name(), "pa");
            for (uint32_t i = 0; i < paRegCount; i++) {
              pc->v_loada128(p.pa[i], sMem, alignment);
              sMem.addOffsetLo32(16);
            }
          }

          break;
        }

        default:
          BL_NOT_REACHED();
      }

      break;
    }

    default:
      BL_NOT_REACHED();
  }

  if (advanceMode == AdvanceMode::kAdvance) {
    pc->add(sPtr, sPtr, n.value() * srcBPP);
  }

  satisfyPixelsA8(pc, p, flags);
}

static void fetchPixelsRGBA32(PipeCompiler* pc, Pixel& p, PixelCount n, PixelFlags flags, FormatExt format, Mem sMem, Alignment alignment, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  BL_ASSERT(p.isRGBA32());
  BL_ASSERT(n.value() > 1u);

  p.setCount(n);

  Gp sPtr = sMem.baseReg().as<Gp>();
  uint32_t srcBPP = blFormatInfo[uint32_t(format)].depth / 8u;

  switch (format) {
    // RGBA32 <- PRGB32 | XRGB32.
    case FormatExt::kPRGB32:
    case FormatExt::kXRGB32: {
      VecWidth pcWidth = pc->vecWidthOf(DataWidth::k32, n);
      uint32_t pcCount = VecWidthUtils::vecCountOf(pcWidth, DataWidth::k32, n);
      BL_ASSERT(pcCount <= OpArray::kMaxSize);

#if defined(BL_JIT_ARCH_X86)
      VecWidth ucWidth = pc->vecWidthOf(DataWidth::k64, n);
      uint32_t ucCount = VecWidthUtils::vecCountOf(ucWidth, DataWidth::k64, n);
      BL_ASSERT(ucCount <= OpArray::kMaxSize);
#endif // BL_JIT_ARCH_X86

      if (!predicate.empty()) {
        pc->newVecArray(p.pc, pcCount, pcWidth, p.name(), "pc");
        fetchPredicatedVec32(pc, p.pc, sPtr, n.value(), advanceMode, predicate);
      }
      else {
        switch (n.value()) {
          case 1: {
            pc->newV128Array(p.pc, 1, p.name(), "pc");
            pc->v_loada32(p.pc[0], sMem);

            break;
          }

          case 2: {
#if defined(BL_JIT_ARCH_X86)
            if (blTestFlag(flags, PixelFlags::kUC) && pc->hasSSE4_1()) {
              pc->newV128Array(p.uc, 1, p.name(), "uc");
              pc->v_cvt_u8_lo_to_u16(p.pc[0].xmm(), sMem);
            }
            else
#endif // BL_JIT_ARCH_X86
            {
              pc->newV128Array(p.pc, 1, p.name(), "pc");
              pc->v_loadu64(p.pc[0], sMem);
            }

            break;
          }

          case 4: {
#if defined(BL_JIT_ARCH_X86)
            if (!blTestFlag(flags, PixelFlags::kPC) && pc->use256BitSimd()) {
              pc->newV256Array(p.uc, 1, p.name(), "uc");
              pc->v_cvt_u8_lo_to_u16(p.uc[0].ymm(), sMem);
            }
            else if (!blTestFlag(flags, PixelFlags::kPC) && pc->hasSSE4_1()) {
              pc->newV128Array(p.uc, 2, p.name(), "uc");
              pc->v_cvt_u8_lo_to_u16(p.uc[0].xmm(), sMem);
              sMem.addOffsetLo32(8);
              pc->v_cvt_u8_lo_to_u16(p.uc[1].xmm(), sMem);
            }
            else
#endif // BL_JIT_ARCH_X86
            {
              pc->newV128Array(p.pc, 1, p.name(), "pc");
              pc->v_loada128(p.pc[0], sMem, alignment);
            }

            break;
          }

          case 8:
          case 16:
          case 32: {
#if defined(BL_JIT_ARCH_X86)
            if (pc->vecWidth() >= VecWidth::k256) {
              if (blTestFlag(flags, PixelFlags::kPC)) {
                pc->newVecArray(p.pc, pcCount, pcWidth, p.name(), "pc");
                for (uint32_t i = 0; i < pcCount; i++) {
                  pc->v_loadavec(p.pc[i], sMem, alignment);
                  sMem.addOffsetLo32(p.pc[i].size());
                }
              }
              else {
                pc->newVecArray(p.uc, ucCount, ucWidth, p.name(), "uc");
                for (uint32_t i = 0; i < ucCount; i++) {
                  pc->v_cvt_u8_lo_to_u16(p.uc[i], sMem);
                  sMem.addOffsetLo32(p.uc[i].size() / 2u);
                }
              }
            }
            else if (!blTestFlag(flags, PixelFlags::kPC) && pc->hasSSE4_1()) {
              pc->newV128Array(p.uc, ucCount, p.name(), "uc");
              for (uint32_t i = 0; i < ucCount; i++) {
                pc->v_cvt_u8_lo_to_u16(p.uc[i], sMem);
                sMem.addOffsetLo32(8);
              }
            }
            else
#endif // BL_JIT_ARCH_X86
            {
              pc->newV128Array(p.pc, pcCount, p.name(), "pc");
              pc->v_loadavec(p.pc, sMem, alignment);
            }

            break;
          }

          default:
            BL_NOT_REACHED();
        }
      }

      if (format == FormatExt::kXRGB32)
        fillAlphaChannel(pc, p);

      break;
    }

    // RGBA32 <- A8.
    case FormatExt::kA8: {
      BL_ASSERT(predicate.empty());

      switch (n.value()) {
        case 1: {
          BL_ASSERT(predicate.empty());

          if (blTestFlag(flags, PixelFlags::kPC)) {
            pc->newV128Array(p.pc, 1, p.name(), "pc");

#if defined(BL_JIT_ARCH_X86)
            if (!pc->hasAVX2()) {
              Gp tmp = pc->newGp32("tmp");
              pc->load_u8(tmp, sMem);
              pc->mul(tmp, tmp, 0x01010101u);
              pc->s_mov_u32(p.pc[0], tmp);
            }
            else
#endif // BL_JIT_ARCH_X86
            {
              pc->v_broadcast_u8(p.pc[0].v128(), sMem);
            }
          }
          else {
            pc->newV128Array(p.uc, 1, p.name(), "uc");
#if defined(BL_JIT_ARCH_X86)
            if (!pc->hasAVX2()) {
              pc->v_load8(p.uc[0], sMem);
              pc->v_swizzle_lo_u16x4(p.uc[0], p.uc[0], swizzle(0, 0, 0, 0));
            }
            else
#endif
            {
              pc->v_broadcast_u8(p.uc[0], sMem);
              pc->v_cvt_u8_lo_to_u16(p.uc[0], p.uc[0]);
            }
          }

          break;
        }

        case 2: {
          if (blTestFlag(flags, PixelFlags::kPC)) {
            pc->newV128Array(p.pc, 1, p.name(), "pc");
#if defined(BL_JIT_ARCH_X86)
            if (!pc->hasAVX2()) {
              if (pc->hasSSE4_1()) {
                pc->v_loadu16_u8_to_u64(p.pc[0], sMem);
                pc->v_swizzlev_u8(p.pc[0], p.pc[0], pc->simdConst(&pc->ct.swizu8_xxxxxxx1xxxxxxx0_to_zzzzzzzz11110000, Bcst::kNA, p.pc[0]));
              }
              else {
                Gp tmp = pc->newGp32("tmp");
                pc->load_u16(tmp, sMem);
                pc->s_mov_u32(p.pc[0], tmp);
                pc->v_interleave_lo_u8(p.pc[0], p.pc[0], p.pc[0]);
                pc->v_interleave_lo_u16(p.pc[0], p.pc[0], p.pc[0]);
              }
            }
            else
#endif // BL_JIT_ARCH_X86
            {
              pc->v_broadcast_u16(p.pc[0].v128(), sMem);
              pc->v_swizzlev_u8(p.pc[0], p.pc[0], pc->simdConst(&pc->ct.swizu8_xxxxxxxxxxxx3210_to_3333222211110000, Bcst::kNA, p.pc[0]));
            }
          }
          else {
            // TODO: [JIT] UNIMPLEMENTED: Unfinished code, should not be hit as we never do 2 pixel quantity.
          }

          break;
        }

        case 4: {
          if (blTestFlag(flags, PixelFlags::kPC)) {
            pc->newV128Array(p.pc, 1, p.name(), "pc");

            pc->v_loada32(p.pc[0], sMem);
#if defined(BL_JIT_ARCH_X86)
            if (!pc->hasSSSE3()) {
              pc->v_interleave_lo_u8(p.pc[0], p.pc[0], p.pc[0]);
              pc->v_interleave_lo_u16(p.pc[0], p.pc[0], p.pc[0]);
            }
            else
#endif // BL_JIT_ARCH_X86
            {
              pc->v_swizzlev_u8(p.pc[0], p.pc[0], pc->simdConst(&pc->ct.swizu8_xxxxxxxxxxxx3210_to_3333222211110000, Bcst::kNA, p.pc[0]));
            }
          }
          else {
#if defined(BL_JIT_ARCH_X86)
            if (pc->use256BitSimd()) {
              pc->newV256Array(p.uc, 1, p.name(), "uc");

              pc->v_loadu32_u8_to_u64(p.uc, sMem);
              pc->v_swizzlev_u8(p.pc[0], p.pc[0], pc->simdConst(&pc->ct.swizu8_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA, p.pc[0]));
            }
            else
#endif // BL_JIT_ARCH_X86
            {
              pc->newV128Array(p.uc, 2, p.name(), "uc");

              pc->v_loada32(p.uc[0], sMem);
              pc->v_interleave_lo_u8(p.uc[0], p.uc[0], p.uc[0]);
              pc->v_cvt_u8_lo_to_u16(p.uc[0], p.uc[0]);

              pc->v_swizzle_u32x4(p.uc[1], p.uc[0], swizzle(3, 3, 2, 2));
              pc->v_swizzle_u32x4(p.uc[0], p.uc[0], swizzle(1, 1, 0, 0));
            }
          }

          break;
        }

        case 8:
        case 16: {
#if defined(BL_JIT_ARCH_X86)
          if (pc->use256BitSimd()) {
            if (blTestFlag(flags, PixelFlags::kPC)) {
              uint32_t pcCount = pc->vecCountOf(DataWidth::k32, n);
              BL_ASSERT(pcCount <= OpArray::kMaxSize);

              pc->newV256Array(p.pc, pcCount, p.name(), "pc");
              for (uint32_t i = 0; i < pcCount; i++) {
                pc->v_cvt_u8_to_u32(p.pc[i], sMem);
                sMem.addOffsetLo32(8);
              }

              pc->v_swizzlev_u8(p.pc, p.pc, pc->simdConst(&pc->ct.swizu8_xxx3xxx2xxx1xxx0_to_3333222211110000, Bcst::kNA, p.pc));
            }
            else {
              uint32_t ucCount = pc->vecCountOf(DataWidth::k64, n);
              BL_ASSERT(ucCount <= OpArray::kMaxSize);

              pc->newV256Array(p.uc, ucCount, p.name(), "uc");
              for (uint32_t i = 0; i < ucCount; i++) {
                pc->v_loadu32_u8_to_u64(p.uc[i], sMem);
                sMem.addOffsetLo32(4);
              }

              pc->v_swizzlev_u8(p.uc, p.uc, pc->simdConst(&pc->ct.swizu8_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA, p.uc));
            }
          }
          else
#endif // BL_JIT_ARCH_X86
          {
            if (blTestFlag(flags, PixelFlags::kPC)) {
              uint32_t pcCount = pc->vecCountOf(DataWidth::k32, n);
              BL_ASSERT(pcCount <= OpArray::kMaxSize);

              pc->newV128Array(p.pc, pcCount, p.name(), "pc");

              for (uint32_t i = 0; i < pcCount; i++) {
                pc->v_loada32(p.pc[i], sMem);
                sMem.addOffsetLo32(4);
              }

#if defined(BL_JIT_ARCH_X86)
              if (!pc->hasSSSE3()) {
                pc->v_interleave_lo_u8(p.pc, p.pc, p.pc);
                pc->v_interleave_lo_u16(p.pc, p.pc, p.pc);
              }
              else
#endif // BL_JIT_ARCH_X86
              {
                pc->v_swizzlev_u8(p.pc, p.pc, pc->simdConst(&pc->ct.swizu8_xxxxxxxxxxxx3210_to_3333222211110000, Bcst::kNA, p.pc));
              }
            }
            else {
              uint32_t ucCount = pc->vecCountOf(DataWidth::k64, n);
              BL_ASSERT(ucCount == 4);

              pc->newV128Array(p.uc, ucCount, p.name(), "uc");

              pc->v_loada32(p.uc[0], sMem);
              sMem.addOffsetLo32(4);
              pc->v_loada32(p.uc[2], sMem);

              pc->v_interleave_lo_u8(p.uc[0], p.uc[0], p.uc[0]);
              pc->v_interleave_lo_u8(p.uc[2], p.uc[2], p.uc[2]);

              pc->v_cvt_u8_lo_to_u16(p.uc[0], p.uc[0]);
              pc->v_cvt_u8_lo_to_u16(p.uc[2], p.uc[2]);

              pc->v_swizzle_u32x4(p.uc[1], p.uc[0], swizzle(3, 3, 2, 2));
              pc->v_swizzle_u32x4(p.uc[3], p.uc[2], swizzle(3, 3, 2, 2));
              pc->v_swizzle_u32x4(p.uc[0], p.uc[0], swizzle(1, 1, 0, 0));
              pc->v_swizzle_u32x4(p.uc[2], p.uc[2], swizzle(1, 1, 0, 0));
            }
          }

          break;
        }

#if defined(BL_JIT_ARCH_X86)
        case 32: {
          if (pc->use512BitSimd()) {
            if (blTestFlag(flags, PixelFlags::kPC)) {
              uint32_t pcCount = pc->vecCountOf(DataWidth::k32, n);
              BL_ASSERT(pcCount <= OpArray::kMaxSize);

              pc->newV512Array(p.pc, pcCount, p.name(), "pc");
              for (uint32_t i = 0; i < pcCount; i++) {
                pc->v_loadu128_u8_to_u32(p.pc[i], sMem);
                sMem.addOffsetLo32(16);
              }

              pc->v_swizzlev_u8(p.pc, p.pc, pc->simdConst(&pc->ct.swizu8_xxx3xxx2xxx1xxx0_to_3333222211110000, Bcst::kNA, p.pc));
            }
            else {
              uint32_t ucCount = pc->vecCountOf(DataWidth::k64, n);
              BL_ASSERT(ucCount <= OpArray::kMaxSize);

              pc->newV512Array(p.uc, ucCount, p.name(), "uc");
              for (uint32_t i = 0; i < ucCount; i++) {
                pc->v_loadu64_u8_to_u64(p.uc[i], sMem);
                sMem.addOffsetLo32(8);
              }

              pc->v_swizzlev_u8(p.uc, p.uc, pc->simdConst(&pc->ct.swizu8_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA, p.uc));
            }
          }
          else {
            BL_ASSERT(pc->use256BitSimd());

            if (blTestFlag(flags, PixelFlags::kPC)) {
              uint32_t pcCount = pc->vecCountOf(DataWidth::k32, n);
              BL_ASSERT(pcCount <= OpArray::kMaxSize);

              pc->newV256Array(p.pc, pcCount, p.name(), "pc");

              for (uint32_t i = 0; i < pcCount; i++) {
                pc->v_loadu64_u8_to_u32(p.pc[i], sMem);
                sMem.addOffsetLo32(8);
              }

              pc->v_swizzlev_u8(p.pc, p.pc, pc->simdConst(&pc->ct.swizu8_xxx3xxx2xxx1xxx0_to_3333222211110000, Bcst::kNA, p.pc));
            }
            else {
              // There is not enough registers for this.
              BL_NOT_REACHED();
            }
          }

          break;
        }
#endif // BL_JIT_ARCH_X86

        default:
          BL_NOT_REACHED();
      }

      break;
    }

    // RGBA32 <- Unknown?
    default:
      BL_NOT_REACHED();
  }

  // Predicated fetcher offers advance as an option, which we pass to it, so don't advance if already advanced.
  if (advanceMode == AdvanceMode::kAdvance && predicate.empty()) {
    pc->add(sPtr, sPtr, n.value() * srcBPP);
  }

  satisfyPixelsRGBA32(pc, p, flags);
}

void fetchPixel(PipeCompiler* pc, Pixel& p, PixelFlags flags, FormatExt format, Mem sMem) noexcept {
  p.setCount(PixelCount{1u});

  switch (p.type()) {
    case PixelType::kA8: {
      switch (format) {
        case FormatExt::kPRGB32: {
          p.sa = pc->newGp32("a");
#if defined(BL_JIT_ARCH_X86)
          sMem.addOffset(3);
          pc->load_u8(p.sa, sMem);
#else
          pc->load_u32(p.sa, sMem);
          pc->shr(p.sa, p.sa, 24);
#endif
          break;
        }

        case FormatExt::kXRGB32: {
          p.sa = pc->newGp32("a");
          pc->mov(p.sa, 255);
          break;
        }

        case FormatExt::kA8: {
          p.sa = pc->newGp32("a");
          pc->load_u8(p.sa, sMem);
          break;
        }

        default:
          BL_NOT_REACHED();
      }

      satisfyPixelsA8(pc, p, flags);
      return;
    }

    case PixelType::kRGBA32: {
      switch (format) {
        case FormatExt::kA8: {
          if (blTestFlag(flags, PixelFlags::kPC)) {
            pc->newV128Array(p.pc, 1, p.name(), "pc");

#if defined(BL_JIT_ARCH_X86)
            if (!pc->hasAVX2()) {
              Gp tmp = pc->newGp32("tmp");
              pc->load_u8(tmp, sMem);
              pc->mul(tmp, tmp, 0x01010101u);
              pc->s_mov_u32(p.pc[0], tmp);
            }
            else
            {
              pc->v_broadcast_u8(p.pc[0], sMem);
            }
#else
            pc->v_load8(p.pc[0], sMem);
#endif // BL_JIT_ARCH_X86
          }
          else {
            pc->newV128Array(p.uc, 1, p.name(), "uc");

#if defined(BL_JIT_ARCH_X86)
            if (!pc->hasAVX2()) {
              pc->v_load8(p.uc[0], sMem);
              pc->v_swizzle_lo_u16x4(p.uc[0], p.uc[0], swizzle(0, 0, 0, 0));
            }
            else
            {
              pc->v_broadcast_u8(p.uc[0], sMem);
              pc->v_cvt_u8_lo_to_u16(p.uc[0], p.uc[0]);
            }
#else
            pc->v_load8(p.pc[0], sMem);
            pc->v_broadcast_u16(p.pc[0], p.pc[0]);
#endif
          }
          break;
        }

        // RGBA32 <- PRGB32 | XRGB32.
        case FormatExt::kPRGB32:
        case FormatExt::kXRGB32: {
          pc->newV128Array(p.pc, 1, p.name(), "pc");
          pc->v_loada32(p.pc[0], sMem);
          break;
        }

        default:
          BL_NOT_REACHED();
      }

      satisfyPixelsRGBA32(pc, p, flags);
      return;
    }

    default:
      BL_NOT_REACHED();
  }
}

void fetchPixels(PipeCompiler* pc, Pixel& p, PixelCount n, PixelFlags flags, FormatExt format, const Mem& sMem, Alignment alignment) noexcept {
  if (n == 1u) {
    fetchPixel(pc, p, flags, format, sMem);
    return;
  }

  PixelPredicate noPredicate;

  switch (p.type()) {
    case PixelType::kA8:
      fetchPixelsA8(pc, p, n, flags, format, sMem, alignment, AdvanceMode::kNoAdvance, noPredicate);
      break;

    case PixelType::kRGBA32:
      fetchPixelsRGBA32(pc, p, n, flags, format, sMem, alignment, AdvanceMode::kNoAdvance, noPredicate);
      break;

    default:
      BL_NOT_REACHED();
  }
}

void fetchPixels(PipeCompiler* pc, Pixel& p, PixelCount n, PixelFlags flags, FormatExt format, const Gp& sPtr, Alignment alignment, AdvanceMode advanceMode) noexcept {
  fetchPixels(pc, p, n, flags, format, sPtr, alignment, advanceMode, pc->emptyPredicate());
}

void fetchPixels(PipeCompiler* pc, Pixel& p, PixelCount n, PixelFlags flags, FormatExt format, const Gp& sPtr, Alignment alignment, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  if (n == 1u) {
    BL_ASSERT(predicate.empty());
    fetchPixel(pc, p, flags, format, mem_ptr(sPtr));

    if (advanceMode == AdvanceMode::kAdvance) {
      pc->add(sPtr, sPtr, blFormatInfo[uint32_t(format)].depth / 8u);
    }
    return;
  }

  switch (p.type()) {
    case PixelType::kA8:
      fetchPixelsA8(pc, p, n, flags, format, mem_ptr(sPtr), alignment, advanceMode, predicate);
      break;

    case PixelType::kRGBA32:
      fetchPixelsRGBA32(pc, p, n, flags, format, mem_ptr(sPtr), alignment, advanceMode, predicate);
      break;

    default:
      BL_NOT_REACHED();
  }
}

// bl::Pipeline::Jit::FetchUtils - Satisfy Pixels
// ==============================================

static void satisfyPixelsA8(PipeCompiler* pc, Pixel& p, PixelFlags flags) noexcept {
  BL_ASSERT(p.type() == PixelType::kA8);
  BL_ASSERT(p.count() != 0);

  // Scalar mode uses only SA.
  if (p.count() == 1) {
    BL_ASSERT( blTestFlag(flags, PixelFlags::kSA));
    BL_ASSERT(!blTestFlag(flags, PixelFlags::kPA | PixelFlags::kUA));

    return;
  }

  if (blTestFlag(flags, PixelFlags::kPA) && p.pa.empty()) {
    // Either PA or UA, but never both.
    BL_ASSERT(!blTestFlag(flags, PixelFlags::kUA));

    _x_pack_pixel(pc, p.pa, p.ua, p.count().value(), p.name(), "pa");
  }
  else if (blTestFlag(flags, PixelFlags::kUA) && p.ua.empty()) {
    // Either PA or UA, but never both.
    BL_ASSERT(!blTestFlag(flags, PixelFlags::kPA));

    _x_unpack_pixel(pc, p.ua, p.pa, p.count().value(), p.name(), "ua");
  }

  if (blTestFlag(flags, PixelFlags::kPI) && p.pi.empty()) {
    if (!p.pa.empty()) {
      pc->newVecArray(p.pi, p.pa.size(), p.pa[0], p.name(), "pi");
      pc->v_not_u32(p.pi, p.pa);
    }
    else {
      // TODO: [JIT] UNIMPLEMENTED: A8 pipeline - finalize satisfy-pixel.
      BL_ASSERT(false);
    }
  }

  if (blTestFlag(flags, PixelFlags::kUA | PixelFlags::kUI)) {
    if (p.ua.empty()) {
      // TODO: [JIT] UNIMPLEMENTED: A8 pipeline - finalize satisfy-pixel.
      BL_ASSERT(false);
    }
  }
}

static void satisfyPixelsRGBA32(PipeCompiler* pc, Pixel& p, PixelFlags flags) noexcept {
  BL_ASSERT(p.type() == PixelType::kRGBA32);
  BL_ASSERT(p.count() != 0);

  if (blTestFlag(flags, PixelFlags::kPA | PixelFlags::kPI))
    flags |= PixelFlags::kPC;

  // Quick reject if all flags were satisfied already or no flags were given.
  if ((!blTestFlag(flags, PixelFlags::kPC) || !p.pc.empty()) &&
      (!blTestFlag(flags, PixelFlags::kPA) || !p.pa.empty()) &&
      (!blTestFlag(flags, PixelFlags::kPI) || !p.pi.empty()) &&
      (!blTestFlag(flags, PixelFlags::kUC) || !p.uc.empty()) &&
      (!blTestFlag(flags, PixelFlags::kUA) || !p.ua.empty()) &&
      (!blTestFlag(flags, PixelFlags::kUI) || !p.ui.empty())) {
    return;
  }

  // Only fetch unpacked alpha if we already have unpacked pixels. Wait otherwise as fetch flags may contain
  // `PixelFlags::kUC`, which is handled below. This is an optimization for cases in which the caller wants
  // packed RGBA and unpacked alpha.
  if (blTestFlag(flags, PixelFlags::kUA | PixelFlags::kUI) && p.ua.empty() && !p.uc.empty()) {
    // Emit pshuflw/pshufhw sequence for every unpacked pixel.
    pc->newVecArray(p.ua, p.uc.size(), p.uc[0], p.name(), "ua");

#if defined(BL_JIT_ARCH_X86)
    if (!pc->hasAVX()) {
      pc->v_expand_alpha_16(p.ua, p.uc, true);
    }
    else
#endif // BL_JIT_ARCH_X86
    {
      pc->v_swizzlev_u8(p.ua, p.uc, pc->simdConst(&pc->ct.swizu8_32xxxxxx10xxxxxx_to_3232323210101010, Bcst::kNA, p.ua));
    }
  }

  // Pack or unpack a sequence.
  //
  // The following code handles packing or unpacking pixels. Typically, depending on a fetcher, either
  // packed or unpacked pixels are assigned to a `Pixel`. Then, the consumer of that pixel decides which
  // format to use. So, if there is a mismatch, we have to emit a pack/unpack sequence. Unpacked pixels
  // are needed for almost everything except some special cases like SRC_COPY and PLUS without a mask.

  // Either PC or UC, but never both.
  BL_ASSERT((flags & (PixelFlags::kPC | PixelFlags::kUC)) != (PixelFlags::kPC | PixelFlags::kUC));

  if (blTestFlag(flags, PixelFlags::kPC) && p.pc.empty()) {
    _x_pack_pixel(pc, p.pc, p.uc, p.count().value() * 4u, p.name(), "pc");
  }
  else if (blTestFlag(flags, PixelFlags::kUC) && p.uc.empty()) {
    _x_unpack_pixel(pc, p.uc, p.pc, p.count().value() * 4, p.name(), "uc");
  }

  if (blTestFlag(flags, PixelFlags::kPA | PixelFlags::kPI)) {
    if (blTestFlag(flags, PixelFlags::kPA) && p.pa.empty()) {
      pc->newVecArray(p.pa, p.pc.size(), p.pc[0], p.name(), "pa");
      pc->v_swizzlev_u8(p.pa, p.pc, pc->simdConst(&pc->ct.swizu8_3xxx2xxx1xxx0xxx_to_3333222211110000, Bcst::kNA, p.pc));
    }

    if (blTestFlag(flags, PixelFlags::kPI) && p.pi.empty()) {
      pc->newVecArray(p.pi, p.pc.size(), p.pc[0], p.name(), "pi");
      if (p.pa.size()) {
        pc->v_not_u32(p.pi, p.pa);
      }
      else {
        pc->v_swizzlev_u8(p.pi, p.pc, pc->simdConst(&pc->ct.swizu8_3xxx2xxx1xxx0xxx_to_3333222211110000, Bcst::kNA, p.pc));
        pc->v_not_u32(p.pi, p.pi);
      }
    }
  }

  // Unpack alpha from either packed or unpacked pixels.
  if (blTestFlag(flags, PixelFlags::kUA | PixelFlags::kUI) && p.ua.empty()) {
    // This time we have to really fetch A8/IA8, if we haven't before.
    BL_ASSERT(!p.pc.empty() || !p.uc.empty());

    uint32_t uaCount = pc->vecCountOf(DataWidth::k64, p.count());
    BL_ASSERT(uaCount <= OpArray::kMaxSize);

    if (!p.uc.empty()) {
      pc->newVecArray(p.ua, uaCount, p.uc[0], p.name(), "ua");
#if defined(BL_JIT_ARCH_X86)
      if (!pc->hasAVX()) {
        pc->v_expand_alpha_16(p.ua, p.uc, p.count() > 1);
      }
      else
#endif // BL_JIT_ARCH_X86
      {
        pc->v_swizzlev_u8(p.ua, p.uc, pc->simdConst(&pc->ct.swizu8_32xxxxxx10xxxxxx_to_3232323210101010, Bcst::kNA, p.ua));
      }
    }
    else {
      if (p.count() <= 2) {
        pc->newV128Array(p.ua, uaCount, p.name(), "ua");
#if defined(BL_JIT_ARCH_X86)
        if (p.count() == 1) {
          pc->v_swizzle_lo_u16x4(p.ua[0], p.pc[0], swizzle(1, 1, 1, 1));
          pc->v_srli_u16(p.ua[0], p.ua[0], 8);
        }
        else if (pc->hasAVX()) {
          pc->v_swizzlev_u8(p.ua[0], p.pc[0], pc->simdConst(&pc->ct.swizu8_xxxxxxxx1xxx0xxx_to_z1z1z1z1z0z0z0z0, Bcst::kNA, p.ua[0]));
        }
        else {
          pc->v_swizzle_lo_u16x4(p.ua[0], p.pc[0], swizzle(3, 3, 1, 1));
          pc->v_swizzle_u32x4(p.ua[0], p.ua[0], swizzle(1, 1, 0, 0));
          pc->v_srli_u16(p.ua[0], p.ua[0], 8);
        }
#else
        pc->v_swizzlev_u8(p.ua[0], p.pc[0], pc->simdConst(&pc->ct.swizu8_xxxxxxxx1xxx0xxx_to_z1z1z1z1z0z0z0z0, Bcst::kNA, p.ua[0]));
#endif
      }
      else {
        VecWidth ucWidth = pc->vecWidthOf(DataWidth::k64, p.count());
        pc->newVecArray(p.ua, uaCount, ucWidth, p.name(), "ua");

#if defined(BL_JIT_ARCH_X86)
        if (ucWidth == VecWidth::k512) {
          if (uaCount == 1) {
            pc->v_cvt_u8_lo_to_u16(p.ua[0], p.pc[0].ymm());
          }
          else {
            pc->v_extract_v256(p.ua.odd().ymm(), p.pc.zmm(), 1);
            pc->v_cvt_u8_lo_to_u16(p.ua.even(), p.pc.ymm());
            pc->v_cvt_u8_lo_to_u16(p.ua.odd(), p.ua.odd().ymm());
          }

          pc->v_swizzlev_u8(p.ua, p.ua, pc->simdConst(&pc->ct.swizu8_32xxxxxx10xxxxxx_to_3232323210101010, Bcst::kNA, p.ua));
        }
        else if (ucWidth == VecWidth::k256) {
          if (uaCount == 1) {
            pc->v_cvt_u8_lo_to_u16(p.ua[0], p.pc[0].xmm());
          }
          else {
            pc->v_extract_v128(p.ua.odd().xmm(), p.pc.ymm(), 1);
            pc->v_cvt_u8_lo_to_u16(p.ua.even(), p.pc.xmm());
            pc->v_cvt_u8_lo_to_u16(p.ua.odd(), p.ua.odd().xmm());
          }

          pc->v_swizzlev_u8(p.ua, p.ua, pc->simdConst(&pc->ct.swizu8_32xxxxxx10xxxxxx_to_3232323210101010, Bcst::kNA, p.ua));
        }
        else
#endif // BL_JIT_ARCH_X86
        {
          for (uint32_t i = 0; i < p.pc.size(); i++)
            pc->xExtractUnpackedAFromPackedARGB32_4(p.ua[i * 2], p.ua[i * 2 + 1], p.pc[i]);
        }
      }
    }
  }

  if (blTestFlag(flags, PixelFlags::kUI) && p.ui.empty()) {
    if (pc->hasNonDestructiveSrc() || blTestFlag(flags, PixelFlags::kUA)) {
      pc->newVecArray(p.ui, p.ua.size(), p.ua[0], p.name(), "ui");
      pc->v_inv255_u16(p.ui, p.ua);
    }
    else {
      p.ui.init(p.ua);
      pc->v_inv255_u16(p.ui, p.ua);

      p.ua.reset();
      pc->rename(p.ui, p.name(), "ui");
    }
  }
}

void satisfyPixels(PipeCompiler* pc, Pixel& p, PixelFlags flags) noexcept {
  BL_ASSERT(p.count() != 0);

  switch (p.type()) {
    case PixelType::kA8:
      satisfyPixelsA8(pc, p, flags);
      break;

    case PixelType::kRGBA32:
      satisfyPixelsRGBA32(pc, p, flags);
      break;

    default:
      BL_NOT_REACHED();
  }
}

// bl::Pipeline::Jit::FetchUtils - Satisfy Solid Pixels
// ====================================================

static void satisfySolidPixelsA8(PipeCompiler* pc, Pixel& p, PixelFlags flags) noexcept {
  BL_ASSERT(p.type() == PixelType::kA8);
  BL_ASSERT(p.count() != 0);

  VecWidth vw = pc->vecWidth();

  if (blTestFlag(flags, PixelFlags::kPA) && p.pa.empty()) {
    BL_ASSERT(!p.ua.empty());
    pc->newVecArray(p.pa, 1, vw, p.name(), "pa");
    pc->v_packs_i16_u8(p.pa[0], p.ua[0], p.ua[0]);
  }

  if (blTestFlag(flags, PixelFlags::kPI) && p.pi.empty()) {
    if (!p.pa.empty()) {
      pc->newVecArray(p.pi, 1, vw, p.name(), "pi");
      pc->v_not_u32(p.pi[0], p.pa[0]);
    }
    else {
      BL_ASSERT(!p.ua.empty());
      pc->newVecArray(p.pi, 1, vw, p.name(), "pi");
      pc->v_packs_i16_u8(p.pi[0], p.ua[0], p.ua[0]);
      pc->v_not_u32(p.pi[0], p.pi[0]);
    }
  }

  // TODO: [JIT] UNIMPLEMENTED: A8 pipeline - finalize solid-alpha.
}

static void satisfySolidPixelsRGBA32(PipeCompiler* pc, Pixel& p, PixelFlags flags) noexcept {
  BL_ASSERT(p.type() == PixelType::kRGBA32);
  BL_ASSERT(p.count() != 0);

  VecWidth vw = pc->vecWidth();

  if (blTestFlag(flags, PixelFlags::kPC) && p.pc.empty()) {
    BL_ASSERT(!p.uc.empty());

    pc->newVecArray(p.pc, 1, vw, p.name(), "pc");
    pc->v_mov(p.pc[0], p.uc[0]);
    pc->v_packs_i16_u8(p.pc[0], p.pc[0], p.pc[0]);
  }

  if (blTestFlag(flags, PixelFlags::kUC) && p.uc.empty()) {
    BL_ASSERT(!p.pc.empty());

    pc->newVecArray(p.uc, 1, vw, p.name(), "uc");
    pc->v_cvt_u8_lo_to_u16(p.uc[0], p.pc[0]);
  }

  if (blTestFlag(flags, PixelFlags::kPA | PixelFlags::kPI) && p.pa.empty()) {
    BL_ASSERT(!p.pc.empty() || !p.uc.empty());

    // TODO: [JIT] PORTABILITY: Requires SSSE3 on X86.
    pc->newVecArray(p.pa, 1, vw, p.name(), "pa");
    if (!p.pc.empty()) {
      pc->v_swizzlev_u8(p.pa[0], p.pc[0], pc->simdConst(&pc->ct.swizu8_3xxx2xxx1xxx0xxx_to_3333222211110000, Bcst::kNA, p.pa[0]));
    }
    else if (!p.uc.empty()) {
      pc->v_swizzlev_u8(p.pa[0], p.uc[0], pc->simdConst(&pc->ct.swizu8_x1xxxxxxx0xxxxxx_to_1111000011110000, Bcst::kNA, p.pa[0]));
    }
  }

  if (blTestFlag(flags, PixelFlags::kUA) && p.ua.empty()) {
    pc->newVecArray(p.ua, 1, vw, p.name(), "ua");

    if (!p.pa.empty()) {
      pc->v_cvt_u8_lo_to_u16(p.ua[0], p.pa[0]);
    }
    else if (!p.uc.empty()) {
      pc->v_swizzle_lo_u16x4(p.ua[0], p.uc[0], swizzle(3, 3, 3, 3));
      pc->v_swizzle_u32x4(p.ua[0], p.ua[0], swizzle(1, 0, 1, 0));
    }
    else {
      pc->v_swizzle_lo_u16x4(p.ua[0], p.pc[0], swizzle(1, 1, 1, 1));
      pc->v_swizzle_u32x4(p.ua[0], p.ua[0], swizzle(1, 0, 1, 0));
      pc->v_srli_u16(p.ua[0], p.ua[0], 8);
    }
  }

  if (blTestFlag(flags, PixelFlags::kPI)) {
    if (!p.pa.empty()) {
      pc->newVecArray(p.pi, 1, vw, p.name(), "pi");
      pc->v_not_u32(p.pi[0], p.pa[0]);
    }
  }

  if (blTestFlag(flags, PixelFlags::kUI) && p.ui.empty()) {
    pc->newVecArray(p.ui, 1, vw, p.name(), "ui");

    if (!p.ua.empty()) {
      pc->v_inv255_u16(p.ui[0], p.ua[0]);
    }
    else if (!p.uc.empty()) {
      pc->v_swizzle_lo_u16x4(p.ui[0], p.uc[0], swizzle(3, 3, 3, 3));
      pc->v_swizzle_u32x4(p.ui[0], p.ui[0], swizzle(1, 0, 1, 0));
      pc->v_inv255_u16(p.ui[0], p.ui[0]);
    }
    else {
      pc->v_swizzle_lo_u16x4(p.ui[0], p.pc[0], swizzle(1, 1, 1, 1));
      pc->v_swizzle_u32x4(p.ui[0], p.ui[0], swizzle(1, 0, 1, 0));
      pc->v_srli_u16(p.ui[0], p.ui[0], 8);
      pc->v_inv255_u16(p.ui[0], p.ui[0]);
    }
  }
}

void satisfySolidPixels(PipeCompiler* pc, Pixel& p, PixelFlags flags) noexcept {
  BL_ASSERT(p.count() != 0);

  switch (p.type()) {
    case PixelType::kA8:
      satisfySolidPixelsA8(pc, p, flags);
      break;

    case PixelType::kRGBA32:
      satisfySolidPixelsRGBA32(pc, p, flags);
      break;

    default:
      BL_NOT_REACHED();
  }
}

// bl::Pipeline::Jit::FetchUtils - Miscellaneous
// =============================================

// Emits a pixel packing sequence.
void _x_pack_pixel(PipeCompiler* pc, VecArray& px, VecArray& ux, uint32_t n, const char* prefix, const char* pxName) noexcept {
  BL_ASSERT( px.empty());
  BL_ASSERT(!ux.empty());

#if defined(BL_JIT_ARCH_X86)
  if (pc->hasAVX512() && ux[0].type() >= asmjit::RegType::kX86_Ymm) {
    VecWidth pxWidth = pc->vecWidthOf(DataWidth::k8, n);
    uint32_t pxCount = pc->vecCountOf(DataWidth::k8, n);
    BL_ASSERT(pxCount <= OpArray::kMaxSize);

    pc->newVecArray(px, pxCount, pxWidth, prefix, pxName);

    if (ux.size() == 1) {
      // Pack ZMM->YMM or YMM->XMM.
      BL_ASSERT(pxCount == 1);
      pc->cc->vpmovwb(px[0], ux[0]);
      ux.reset();
      return;
    }
    else if (ux[0].type() >= asmjit::RegType::kX86_Zmm) {
      // Pack ZMM to ZMM.
      VecArray pxTmp;
      pc->newV256Array(pxTmp, ux.size(), prefix, "pxTmp");

      for (uint32_t i = 0; i < ux.size(); i++)
        pc->cc->vpmovwb(pxTmp[i].ymm(), ux[i]);

      for (uint32_t i = 0; i < ux.size(); i += 2)
        pc->cc->vinserti32x8(px[i / 2u].zmm(), pxTmp[i].zmm(), pxTmp[i + 1u].ymm(), 1);

      ux.reset();
      return;
    }
  }

  if (pc->hasAVX()) {
    uint32_t pxCount = pc->vecCountOf(DataWidth::k8, n);
    BL_ASSERT(pxCount <= OpArray::kMaxSize);

    if (ux[0].type() >= asmjit::RegType::kX86_Ymm) {
      if (ux.size() == 1) {
        // Pack YMM to XMM.
        BL_ASSERT(pxCount == 1);

        Vec pTmp = pc->newV256("pTmp");
        pc->newV128Array(px, pxCount, prefix, pxName);

        pc->v_packs_i16_u8(pTmp, ux[0], ux[0]);
        pc->v_swizzle_u64x4(px[0].ymm(), pTmp, swizzle(3, 1, 2, 0));
      }
      else {
        pc->newV256Array(px, pxCount, prefix, pxName);
        pc->v_packs_i16_u8(px, ux.even(), ux.odd());
        pc->v_swizzle_u64x4(px, px, swizzle(3, 1, 2, 0));
      }
    }
    else {
      pc->newV128Array(px, pxCount, prefix, pxName);
      pc->v_packs_i16_u8(px, ux.even(), ux.odd());
    }
    ux.reset();
  }
  else {
    // NOTE: This is only used by a non-AVX pipeline. Renaming makes no sense when in AVX mode. Additionally,
    // we may need to pack to XMM register from two YMM registers, so the register types don't have to match
    // if the pipeline is using 256-bit SIMD or higher.
    px.init(ux.even());
    pc->rename(px, prefix, pxName);

    pc->v_packs_i16_u8(px, ux.even(), ux.odd());
    ux.reset();
  }
#else
  uint32_t pxCount = pc->vecCountOf(DataWidth::k8, n);
  BL_ASSERT(pxCount <= OpArray::kMaxSize);

  pc->newV128Array(px, pxCount, prefix, pxName);
  pc->v_packs_i16_u8(px, ux.even(), ux.odd());

  ux.reset();
#endif
}

// Emits a pixel unpacking sequence.
void _x_unpack_pixel(PipeCompiler* pc, VecArray& ux, VecArray& px, uint32_t n, const char* prefix, const char* uxName) noexcept {
  BL_ASSERT( ux.empty());
  BL_ASSERT(!px.empty());

#if defined(BL_JIT_ARCH_X86)
  VecWidth uxWidth = pc->vecWidthOf(DataWidth::k16, n);
  uint32_t uxCount = pc->vecCountOf(DataWidth::k16, n);
  BL_ASSERT(uxCount <= OpArray::kMaxSize);

  if (pc->hasAVX()) {
    pc->newVecArray(ux, uxCount, uxWidth, prefix, uxName);

    if (uxWidth == VecWidth::k512) {
      if (uxCount == 1) {
        pc->v_cvt_u8_lo_to_u16(ux[0], px[0].ymm());
      }
      else {
        pc->v_extract_v256(ux.odd().ymm(), px, 1);
        pc->v_cvt_u8_lo_to_u16(ux.even(), px.ymm());
        pc->v_cvt_u8_lo_to_u16(ux.odd(), ux.odd().ymm());
      }
    }
    else if (uxWidth == VecWidth::k256 && n >= 16) {
      if (uxCount == 1) {
        pc->v_cvt_u8_lo_to_u16(ux[0], px[0].xmm());
      }
      else {
        pc->v_extract_v128(ux.odd().xmm(), px, 1);
        pc->v_cvt_u8_lo_to_u16(ux.even(), px.xmm());
        pc->v_cvt_u8_lo_to_u16(ux.odd(), ux.odd().xmm());
      }
    }
    else {
      for (uint32_t i = 0; i < uxCount; i++) {
        if (i & 1)
          pc->v_swizzlev_u8(ux[i], px[i / 2u], pc->simdConst(&commonTable.swizu8_76543210xxxxxxxx_to_z7z6z5z4z3z2z1z0, Bcst::kNA, ux[i]));
        else
          pc->v_cvt_u8_lo_to_u16(ux[i], px[i / 2u]);
      }
    }
  }
  else {
    if (n <= 8) {
      ux.init(px[0]);
      pc->v_cvt_u8_lo_to_u16(ux[0], ux[0]);
    }
    else {
      ux._size = px.size() * 2;
      for (uint32_t i = 0; i < px.size(); i++) {
        ux[i * 2 + 0] = px[i];
        ux[i * 2 + 1] = pc->newV128();
        pc->xMovzxBW_LoHi(ux[i * 2 + 0], ux[i * 2 + 1], ux[i * 2 + 0]);
      }
    }

    px.reset();
    pc->rename(ux, prefix, uxName);
  }
#else
  uint32_t count = pc->vecCountOf(DataWidth::k16, n);
  BL_ASSERT(count <= OpArray::kMaxSize);

  pc->newVecArray(ux, count, VecWidth::k128, prefix, uxName);

  for (uint32_t i = 0; i < count; i++) {
    if (i & 1)
      pc->v_swizzlev_u8(ux[i], px[i / 2u], pc->simdConst(&commonTable.swizu8_76543210xxxxxxxx_to_z7z6z5z4z3z2z1z0, Bcst::kNA, ux[i]));
    else
      pc->v_cvt_u8_lo_to_u16(ux[i], px[i / 2u]);
  }
#endif
}

void x_fetch_unpacked_a8_2x(PipeCompiler* pc, const Vec& dst, FormatExt format, const Mem& src1, const Mem& src0) noexcept {
#if defined(BL_JIT_ARCH_X86)
  Mem m0 = src0;
  Mem m1 = src1;

  if (format == FormatExt::kPRGB32) {
    m0.addOffset(3);
    m1.addOffset(3);
  }

  if (pc->hasSSE4_1()) {
    pc->v_load8(dst, m0);
    pc->v_insert_u8(dst, m1, 2);
  }
  else {
    Gp aGp = pc->newGp32("aGp");
    pc->load_u8(aGp, m1);
    pc->shl(aGp, aGp, 16);
    pc->load_merge_u8(aGp, m0);
    pc->s_mov_u32(dst, aGp);
  }
#else
  Vec tmp = pc->newSimilarReg(dst, "@tmp");

  if (format == FormatExt::kPRGB32) {
    pc->v_loadu32(dst, src0);
    pc->v_loadu32(tmp, src1);
    pc->v_srli_u32(dst, dst, 24);
    pc->cc->ins(dst.b(2), tmp.b(3));
  }
  else {
    pc->v_load8(dst, src0);
    pc->v_load8(tmp, src1);
    pc->cc->ins(dst.b(2), tmp.b(0));
  }
#endif
}

void x_assign_unpacked_alpha_values(PipeCompiler* pc, Pixel& p, PixelFlags flags, const Vec& vec) noexcept {
  blUnused(flags);

  BL_ASSERT(p.type() != PixelType::kNone);
  BL_ASSERT(p.count() != 0);

  Vec v0 = vec;

  if (p.isRGBA32()) {
    switch (p.count().value()) {
      case 1: {
        pc->v_swizzle_lo_u16x4(v0, v0, swizzle(0, 0, 0, 0));

        p.uc.init(v0);
        break;
      }

      case 2: {
        pc->v_interleave_lo_u16(v0, v0, v0);
        pc->v_swizzle_u32x4(v0, v0, swizzle(1, 1, 0, 0));

        p.uc.init(v0);
        break;
      }

      case 4: {
        Vec v1 = pc->newV128("@v1");

        pc->v_interleave_lo_u16(v0, v0, v0);
        pc->v_swizzle_u32x4(v1, v0, swizzle(3, 3, 2, 2));
        pc->v_swizzle_u32x4(v0, v0, swizzle(1, 1, 0, 0));

        p.uc.init(v0, v1);
        break;
      }

      case 8: {
        Vec v1 = pc->newV128("@v1");
        Vec v2 = pc->newV128("@v2");
        Vec v3 = pc->newV128("@v3");

        pc->v_interleave_hi_u16(v2, v0, v0);
        pc->v_interleave_lo_u16(v0, v0, v0);

        pc->v_swizzle_u32x4(v1, v0, swizzle(3, 3, 2, 2));
        pc->v_swizzle_u32x4(v0, v0, swizzle(1, 1, 0, 0));
        pc->v_swizzle_u32x4(v3, v2, swizzle(3, 3, 2, 2));
        pc->v_swizzle_u32x4(v2, v2, swizzle(1, 1, 0, 0));

        p.uc.init(v0, v1, v2, v3);
        break;
      }

      default:
        BL_NOT_REACHED();
    }

    pc->rename(p.uc, "uc");
  }
  else {
    switch (p.count().value()) {
      case 1: {
        BL_ASSERT(blTestFlag(flags, PixelFlags::kSA));

        Gp sa = pc->newGp32("sa");
        pc->s_extract_u16(sa, vec, 0);

        p.sa = sa;
        break;
      }

      default: {
        p.ua.init(vec);
        pc->rename(p.ua, p.name(), "ua");
        break;
      }
    }
  }
}

void fillAlphaChannel(PipeCompiler* pc, Pixel& p) noexcept {
  switch (p.type()) {
    case PixelType::kRGBA32:
      if (!p.pc.empty()) pc->vFillAlpha255B(p.pc, p.pc);
      if (!p.uc.empty()) pc->vFillAlpha255W(p.uc, p.uc);
      break;

    case PixelType::kA8:
      break;

    default:
      BL_NOT_REACHED();
  }
}

void storePixelsAndAdvance(PipeCompiler* pc, const Gp& dPtr, Pixel& p, PixelCount n, uint32_t bpp, Alignment alignment, PixelPredicate& predicate) noexcept {
  Mem dMem = mem_ptr(dPtr);

  blUnused(predicate);

  switch (bpp) {
    case 1: {
#if defined(BL_JIT_ARCH_X86)
      if (!predicate.empty()) {
        // Predicated pixel count must be greater than 1!
        BL_ASSERT(n != 1);

        satisfyPixels(pc, p, PixelFlags::kPA | PixelFlags::kImmutable);
        storePredicatedVec8(pc, dPtr, p.pa, n.value(), AdvanceMode::kAdvance, predicate);
      }
      else
#endif // BL_JIT_ARCH_X86
      {
        if (n == 1) {
          satisfyPixels(pc, p, PixelFlags::kSA | PixelFlags::kImmutable);
          pc->store_u8(dMem, p.sa);
        }
        else {
          satisfyPixels(pc, p, PixelFlags::kPA | PixelFlags::kImmutable);

          if (n <= 16) {
            pc->v_store_iany(dMem, p.pa[0], n.value(), alignment);
          }
          else {
            satisfyPixels(pc, p, PixelFlags::kPA | PixelFlags::kImmutable);

            // TODO: [JIT] OPTIMIZATION: AArch64 - Use v_storeavec with multiple Vec registers to take advantage of STP where possible.
            uint32_t pcIndex = 0;
            uint32_t vecSize = p.pa[0].size();
            uint32_t pixelsPerReg = vecSize;

            for (uint32_t i = 0; i < n.value(); i += pixelsPerReg) {
              pc->v_storeavec(dMem, p.pa[pcIndex], alignment);
              if (++pcIndex >= p.pa.size())
                pcIndex = 0;
              dMem.addOffset(vecSize);
            }
          }
        }

        pc->add(dPtr, dPtr, n.value());
      }

      break;
    }

    case 4: {
      if (!predicate.empty()) {
        satisfyPixels(pc, p, PixelFlags::kPC | PixelFlags::kImmutable);
        storePredicatedVec32(pc, dPtr, p.pc, n.value(), AdvanceMode::kAdvance, predicate);
      }
#if defined(BL_JIT_ARCH_X86)
      else if (pc->hasAVX512() && n >= 2 && !p.uc.empty() && p.pc.empty()) {
        uint32_t ucIndex = 0;
        uint32_t vecSize = p.uc[0].size();
        uint32_t pixelsPerReg = vecSize / 8u;

        for (uint32_t i = 0; i < n.value(); i += pixelsPerReg) {
          pc->cc->vpmovwb(dMem, p.uc[ucIndex]);
          if (++ucIndex >= p.uc.size())
            ucIndex = 0;
          dMem.addOffset(vecSize / 2u);
        }
        pc->add(dPtr, dPtr, n.value() * 4);
      }
#endif
      else {
        satisfyPixels(pc, p, PixelFlags::kPC | PixelFlags::kImmutable);

        if (n <= 4) {
          pc->v_store_iany(dMem, p.pc[0], n.value() * 4u, alignment);
        }
        else {
          // TODO: [JIT] OPTIMIZATION: AArch64 - Use v_storeavec with multiple Vec registers to take advantage of STP where possible.
          uint32_t pcIndex = 0;
          uint32_t vecSize = p.pc[0].size();
          uint32_t pixelsPerReg = vecSize / 4u;

          for (uint32_t i = 0; i < n.value(); i += pixelsPerReg) {
            pc->v_storeavec(dMem, p.pc[pcIndex], alignment);
            if (++pcIndex >= p.pc.size())
              pcIndex = 0;
            dMem.addOffset(vecSize);
          }
        }
        pc->add(dPtr, dPtr, n.value() * 4);
      }

      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

} // {FetchUtils}
} // {JIT}
} // {Pipeline}
} // {bl}

#endif // !BL_BUILD_NO_JIT
