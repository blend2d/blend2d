// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/fetchutilscoverage_p.h"
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
#if defined(BL_JIT_ARCH_A64)
  // Predicated load of 1-3 elements can be simplified to the following on AArch64:
  //   - load the first element at [0]    (always valid).
  //   - load the last element at [i - 1] (always valid, possibly overlapping with the first element if count==2).
  //   - load the mid element by using CINC instruction (incrementing when count >= 2).
  a64::Compiler* cc = pc->cc;

  Gp mid = pc->newGpPtr("@mid");
  Gp last = pc->newGpPtr("@last");

  cc->cmp(count, 2);
  cc->cinc(mid, sPtr, CondCode::kUnsignedGE);

  if (advanceMode == AdvanceMode::kAdvance) {
    cc->ld1r(dVec.b16(), a64::ptr_post(sPtr, count.cloneAs(sPtr)));
  }
  else {
    cc->ldr(dVec.b(), a64::ptr(sPtr));
  }

  cc->ld1(dVec.b(1), a64::ptr(mid));
  cc->cinc(last, mid, CondCode::kUnsignedGT);
  cc->ld1(dVec.b(2), a64::ptr(last));

#else
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
    pc->umin(mid, mid, end);
    pc->load_u8(tmp, mem_ptr(mid, -1));
    add_shifted(pc, acc, tmp, 8);
  }

  pc->s_mov_u32(dVec, acc);
#endif
}

// Predicated load of 1-7 bytes.
static void fetchPredicatedVec8_1To7(PipeCompiler* pc, const Vec& dVec, Gp sPtr, AdvanceMode advanceMode, const Gp& count) noexcept {
#if defined(BL_JIT_ARCH_X86)
  if (pc->is32Bit()) {
    // Not optimized, probably not worth spending time on trying to optimize this version as we don't expect 32-bit
    // targets to be important.
    Label L_Iter = pc->newLabel();
    Label L_Done = pc->newLabel();

    Gp i = pc->newGp32("@fetch_x");
    Gp acc = pc->newGp32("@fetch_acc");
    Vec tmp = pc->newV128("@fetch_tmp");

    pc->mov(i, count);
    pc->mov(acc, 0);
    pc->v_xor_i32(dVec, dVec, dVec);
    pc->j(L_Iter, ucmp_lt(i, 4));

    pc->v_loadu32(dVec, x86::ptr(sPtr, i, 0, -4));
    pc->j(L_Done, sub_z(i, 4));

    pc->bind(L_Iter);
    pc->load_shift_u8(acc, x86::ptr(sPtr, i, 0, -1));
    pc->v_slli_u64(dVec, dVec, 8);
    pc->j(L_Iter, sub_nz(i, 1));

    pc->bind(L_Done);
    pc->s_mov(tmp, acc);
    pc->v_or_i32(dVec, dVec, tmp);

    if (advanceMode == AdvanceMode::kAdvance) {
      pc->add(sPtr, sPtr, count.cloneAs(sPtr));
    }

    return;
  }
#endif // BL_JIT_ARCH_X86

  // This implementation uses a single branch to skip the loading of the rest when `count == 1`. The reason is that we
  // want to use 3x 16-bit fetches to fetch 2..6 bytes, and combine that with the first byte if `count & 1 == 1`. This
  // approach seems to be good and it's also pretty short. Since the branch depends on `count == 1` it should also make
  // branch predictor happier as we expect that `count == 2..7` case should be much more likely than `count == 1`.
  Label L_Done = pc->newLabel();

  Gp acc = pc->newGpPtr("@fetch_acc");
  Gp index0 = pc->newGpPtr("@fetch_index0");
  Gp index1 = pc->newGpPtr("@fetch_index1");

  pc->load_u8(acc, ptr(sPtr));
  pc->j(L_Done, cmp_eq(count.r32(), 1));

  // This is how indexes are calculated for count:
  //   - count == 2 -> index0 = 0 | index0' = 0 | index1 = 0
  //   - count == 3 -> index0 = 1 | index0' = 1 | index1 = 1
  //   - count == 4 -> index0 = 2 | index0' = 2 | index1 = 0
  //   - count == 5 -> index0 = 3 | index0' = 3 | index1 = 1
  //   - count == 6 -> index0 = 4 | index0' = 2 | index1 = 0
  //   - count == 7 -> index0 = 5 | index0' = 3 | index1 = 1
  pc->shl(acc, acc, 24);
  pc->sub(index0.r32(), count.r32(), 2);
  pc->and_(index1.r32(), count.r32(), 0x1);
  pc->load_merge_u16(acc, ptr(sPtr, index0));

  pc->add(index1.r32(), index1.r32(), 2);
  pc->umin(index0.r32(), index0.r32(), index1.r32());
  pc->load_shift_u16(acc, ptr(sPtr, index0));

  pc->and_(index1.r32(), index1.r32(), 1);
  pc->load_shift_u16(acc, ptr(sPtr, index1));

  pc->shl(index1.r32(), index1.r32(), 3);
  pc->rol(acc, acc, index1.r64());

  pc->bind(L_Done);
  pc->s_mov_u64(dVec, acc);

  if (advanceMode == AdvanceMode::kAdvance) {
    pc->add(sPtr, sPtr, count.cloneAs(sPtr));
  }
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

  if (n <= 8) {
    fetchPredicatedVec8_1To7(pc, dVec[0], sPtr, advanceMode, predicate.count());
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
      x86::Mem mem = pc->_getMemConst(pc->ct.k_msk64_data);
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
  if (n <= 16u) {
    sVec[0] = sVec[0].v128();
  }
  else if (n <= 32u && sVec.size() == 1u) {
    sVec[0] = sVec[0].v256();
  }

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
    pc->add(dPtr, dPtr, 4);
    pc->shiftOrRotateRight(vLast, vLast, 4);
    pc->bind(L_StoreSkip4);

    remaining -= 4u;
  }

  Gp gpLast = pc->newGp32("@gpLast");
  pc->s_mov_u32(gpLast, vLast);

  if (remaining > 2u) {
    Label L_StoreSkip2 = pc->newLabel();
    pc->j(L_StoreSkip2, bt_z(count, 1));
    pc->store_u16(dMem, gpLast);
    pc->add(dPtr, dPtr, 2);
    pc->shr(gpLast, gpLast, 16);
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

static void multiplyPackedMaskWithGlobalAlpha(PipeCompiler* pc, VecArray vm, uint32_t n, GlobalAlpha* ga) noexcept {
  BL_ASSERT(vm.size() > 0u);
  BL_ASSERT(ga != nullptr);

  uint32_t vc = calculateVecCount(vm[0].size(), n);
  vm.truncate(vc);

#if defined(BL_JIT_ARCH_A64)
  const Vec& pa = ga->pa();

  if (n <= 8u) {
    pc->v_mulw_lo_u8(vm, vm, pa);
    pc->v_srli_rnd_acc_u16(vm, vm, 8);
    pc->v_srlni_rnd_lo_u16(vm, vm, 8);
  }
  else {
    VecArray vt;
    pc->newVecArray(vt, vc, vm.vecWidth(), "@vt0");

    pc->v_mulw_hi_u8(vt, vm, pa);
    pc->v_mulw_lo_u8(vm, vm, pa);

    pc->v_srli_rnd_acc_u16(vm, vm, 8);
    pc->v_srli_rnd_acc_u16(vt, vt, 8);

    pc->v_srlni_rnd_lo_u16(vm, vm, 8);
    pc->v_srlni_rnd_hi_u16(vm, vt, 8);
  }
#else
  Vec ua = ga->ua().cloneAs(vm[0]);

  if (n <= 8u) {
    pc->v_cvt_u8_lo_to_u16(vm, vm);
    pc->v_mul_u16(vm, vm, ua);
    pc->v_div255_u16(vm);
    pc->v_packs_i16_u8(vm, vm, vm);
  }
  else {
    Operand zero = pc->simdConst(&pc->ct.i_0000000000000000, Bcst::kNA, vm[0]);

    VecArray vt;
    pc->newVecArray(vt, vc, vm.vecWidth(), "@vt0");

    pc->v_interleave_hi_u8(vt, vm, zero);
    pc->v_interleave_lo_u8(vm, vm, zero);
    pc->v_mul_u16(vt, vt, ua);
    pc->v_mul_u16(vm, vm, ua);
    pc->v_div255_u16(vt);
    pc->v_div255_u16(vm);
    pc->v_packs_i16_u8(vm, vm, vt);
  }
#endif
}

void fetchMaskA8IntoPA(PipeCompiler* pc, VecArray& dVec, const Gp& sPtr, PixelCount n, AdvanceMode advanceMode, PixelPredicate& predicate, GlobalAlpha* ga) noexcept {
  BL_ASSERT(dVec.size() >= pc->vecCountOf(DataWidth::k8, n) && dVec.vecWidth() == pc->vecWidthOf(DataWidth::k8, n));

  fetchVec8(pc, dVec, sPtr, n.value(), advanceMode, predicate);

  if (ga) {
    multiplyPackedMaskWithGlobalAlpha(pc, dVec, n.value(), ga);
  }
}

void fetchMaskA8IntoUA(PipeCompiler* pc, VecArray& dVec, const Gp& sPtr, PixelCount n, AdvanceMode advanceMode, PixelPredicate& predicate, GlobalAlpha* ga) noexcept {
  BL_ASSERT(dVec.size() >= pc->vecCountOf(DataWidth::k16, n) && dVec.vecWidth() == pc->vecWidthOf(DataWidth::k16, n));

  uint32_t vc = pc->vecCountOf(DataWidth::k16, n);
  Mem m = ptr(sPtr);

  if (predicate.empty()) {
    switch (n.value()) {
      case 2:
#if defined(BL_JIT_ARCH_X86)
        if (pc->hasAVX2()) {
          pc->v_broadcast_u16(dVec[0], m);
        }
        else
#endif // BL_JIT_ARCH_X86
        {
          pc->v_loadu16(dVec[0], m);
        }
        pc->v_cvt_u8_lo_to_u16(dVec[0], dVec[0]);
        break;

      case 4:
        pc->v_loada32(dVec[0], m);
        pc->v_cvt_u8_lo_to_u16(dVec[0], dVec[0]);
        break;

      case 8:
        pc->v_cvt_u8_lo_to_u16(dVec[0], m);
        break;

      default: {
        for (uint32_t i = 0; i < vc; i++) {
          pc->v_cvt_u8_lo_to_u16(dVec[i], m);
          m.addOffsetLo32(dVec[i].size() / 2u);
        }
        break;
      }
    }

    if (advanceMode == AdvanceMode::kAdvance) {
      pc->add(sPtr, sPtr, n.value());
    }
  }
  else {
    if (n.value() <= 8) {
      fetchPredicatedVec8(pc, dVec, sPtr, n.value(), advanceMode, predicate);
      pc->v_cvt_u8_lo_to_u16(dVec[0], dVec[0]);
    }
#if defined(BL_JIT_ARCH_X86)
    else if (dVec[0].size() > 16u) {
      VecArray lo = dVec.cloneAs(VecWidth(uint32_t(dVec.vecWidth()) - 1u));
      fetchPredicatedVec8(pc, lo, sPtr, n.value(), advanceMode, predicate);
      pc->v_cvt_u8_lo_to_u16(dVec, dVec);
    }
#endif // BL_JIT_ARCH_X86
    else {
      VecArray even = dVec.even();
      VecArray odd = dVec.odd();

      fetchPredicatedVec8(pc, even, sPtr, n.value(), advanceMode, predicate);

      pc->v_cvt_u8_hi_to_u16(odd, even);
      pc->v_cvt_u8_lo_to_u16(even, even);
    }
  }

  if (ga) {
    pc->v_mul_i16(dVec, dVec, ga->ua().cloneAs(dVec[0]));
    pc->v_div255_u16(dVec);
  }
}

#if defined(BL_JIT_ARCH_X86)
// Works for SSE4.1, AVX/AVX2, and AVX-512 cases.
static void fetchMaskA8IntoPCByExpandingTo32Bits(PipeCompiler* pc, VecArray& dVec, const Gp& sPtr, PixelCount n, AdvanceMode advanceMode, GlobalAlpha* ga) noexcept {
  pc->v_loaduvec_u8_to_u32(dVec, ptr(sPtr));

  if (advanceMode == AdvanceMode::kAdvance) {
    pc->add(sPtr, sPtr, n.value());
  }

  // TODO: [JIT] We can save some multiplications if we only extend to 16 bits, then multiply, and then shuffle.
  if (ga) {
    pc->v_mul_u16(dVec, dVec, ga->ua());
    pc->v_div255_u16(dVec);
  }

  pc->v_swizzlev_u8(dVec, dVec, pc->simdConst(&pc->ct.swizu8_xxx3xxx2xxx1xxx0_to_3333222211110000, Bcst::kNA, dVec));
}

// AVX2 and AVX-512 code using YMM/ZMM registers require a different approach compared to 128-bit registers as we
// are going to cross 128-bit boundaries, which usually require either zero-extension or using one of AVX2/AVX-512
// permute instructions.
static void expandA8MaskToPC_YmmZmm(PipeCompiler* pc, VecArray& dVec, VecArray& aVec) noexcept {
  // Number of 4-vec chunks for swizzling - each 4-vec chunk is swizzled/unpacked independently.
  uint32_t qCount = (dVec.size() + 3) / 4;

  // AVX512_VBMI provides VPERMB, which we want to use - on modern micro-architectures such as Zen4+ it's as fast as
  // VPSHUFB.
  if (dVec.isVec512() && pc->hasAVX512_VBMI()) {
    Vec predicate0 = pc->simdVecConst(&pc->ct.permu8_a8_to_rgba32_pc, Bcst::kNA_Unique, dVec);
    Vec predicate1;

    if (dVec.size() >= 2u) {
      predicate1 = pc->simdVecConst(&pc->ct.permu8_a8_to_rgba32_pc_second, Bcst::kNA_Unique, dVec);
    }

    for (uint32_t q = 0; q < qCount; q++) {
      uint32_t d = q * 4;
      uint32_t remain = blMin<uint32_t>(dVec.size() - d, 4);

      if (remain >= 3) {
        pc->v_extract_v256(dVec[d + 2], aVec[q], 1);
      }

      if (remain >= 2) {
        pc->v_permute_u8(dVec[d + 1], predicate1, aVec[q]);
      }

      pc->v_permute_u8(dVec[d], predicate0, aVec[q]);

      if (remain >= 4) {
        pc->v_permute_u8(dVec[d + 3], predicate1, dVec[d + 2]);
      }

      if (remain >= 3) {
        pc->v_permute_u8(dVec[d + 2], predicate0, dVec[d + 2]);
      }
    }
  }
  else if (dVec.isVec512()) {
    Vec predicate = pc->simdVecConst(&pc->ct.swizu8_xxx3xxx2xxx1xxx0_to_3333222211110000, Bcst::kNA, dVec);

    for (uint32_t q = 0; q < qCount; q++) {
      uint32_t d = q * 4;
      uint32_t remain = blMin<uint32_t>(dVec.size() - d, 4);

      for (uint32_t i = 1; i < remain; i++) {
        Vec& dv = dVec[d + i];
        pc->v_extract_v128(dv, aVec[q], i);
      }

      for (uint32_t i = 0; i < remain; i++) {
        Vec& dv = dVec[d + i];
        pc->v_cvt_u8_to_u32(dv, i == 0 ? aVec[q] : dv);
        pc->v_swizzlev_u8(dv, dv, predicate);
      }
    }
  }
  else {
    BL_ASSERT(dVec.isVec256());

    Vec predicate = pc->simdVecConst(&pc->ct.swizu8_xxx3xxx2xxx1xxx0_to_3333222211110000, Bcst::kNA, dVec);

    for (uint32_t q = 0; q < qCount; q++) {
      uint32_t d = q * 4;
      uint32_t remain = blMin<uint32_t>(dVec.size() - d, 4);

      if (remain >= 3) {
        pc->v_swizzle_u64x4(dVec[d + 2], aVec[q], swizzle(1, 0, 3, 2));
      }

      if (remain >= 2) {
        pc->v_swizzle_u32x4(dVec[d + 1], aVec[q], swizzle(1, 0, 3, 2));
      }

      if (remain >= 4) {
        pc->v_swizzle_u32x4(dVec[d + 3], dVec[d + 2], swizzle(1, 0, 3, 2));
      }

      for (uint32_t i = 0; i < remain; i++) {
        Vec& dv = dVec[d + i];
        pc->v_cvt_u8_to_u32(dv, i == 0 ? aVec[q] : dv);
        pc->v_swizzlev_u8(dv, dv, predicate);
      }
    }
  }
}
#endif // BL_JIT_ARCH_X86

void fetchMaskA8IntoPC(PipeCompiler* pc, VecArray dVec, const Gp& sPtr, PixelCount n, AdvanceMode advanceMode, PixelPredicate& predicate, GlobalAlpha* ga) noexcept {
  VecWidth vw = dVec.vecWidth();
  uint32_t vc = VecWidthUtils::vecCountOf(vw, DataWidth::k32, n);

  BL_ASSERT(dVec.size() >= vc);
  dVec.truncate(vc);

#if defined(BL_JIT_ARCH_X86)
  // The easiest way to do this is to extend BYTE to DWORD and then to use a single VPSHUFB predicate to expand
  // alpha values to all required lanes. This saves registers that would be otherwise used to hold more predicates.
  //
  // NOTE: This approach is only suitable for X86 as we can zero extend BYTE to DWORD during the load itself, which
  // makes it the best approach as we can use a single predicate to duplicate the alpha to all required lanes.
  if (predicate.empty() && pc->hasSSE4_1() && n >= 4u) {
    fetchMaskA8IntoPCByExpandingTo32Bits(pc, dVec, sPtr, n, advanceMode, ga);
    return;
  }
#endif // BL_JIT_ARCH_X86

  VecArray aVec = dVec.every_nth(4);
  fetchVec8(pc, aVec, sPtr, n.value(), advanceMode, predicate);

  // TODO: [JIT] This is not optimal in X86 case - we should zero extend to U16, multiply, and then expand to U32.
  if (ga) {
    multiplyPackedMaskWithGlobalAlpha(pc, aVec, n.value(), ga);
  }

#if defined(BL_JIT_ARCH_X86)
  if (!dVec.isVec128()) {
    // At least 8 pixels should be fetched in order to use YMM registers and 16 pixels in order to use ZMM registers.
    BL_ASSERT(n >= dVec[0].size() / 4u);

    expandA8MaskToPC_YmmZmm(pc, dVec, aVec);
    return;
  }
#endif // BL_JIT_ARCH_X86

  // Number of 4-vec chunks for swizzling - each 4-vec chunk is swizzled/unpacked independently.
  uint32_t qCount = (dVec.size() + 3) / 4;

  // We have two choices - use interleave sequences (2 interleaves are required to expand one A8 to 4 channels)
  // or use VPSHUFB/TBL (table lookup) instructions to do only a single table lookup per register.
  bool useInterleaveSequence = n <= 8;

#if defined(BL_JIT_ARCH_X86)
  if (!pc->hasSSSE3()) {
    useInterleaveSequence = true;
  }
#endif // BL_JIT_ARCH_X86

  if (useInterleaveSequence) {
    for (uint32_t q = 0; q < qCount; q++) {
      uint32_t d = q * 4;
      uint32_t remain = vc - d;

      Vec a0 = aVec[q];

      if (remain >= 4) {
        pc->v_interleave_hi_u8(dVec[d + 2], a0, a0);
      }

      pc->v_interleave_lo_u8(dVec[d + 0], a0, a0);

      if (remain >= 2) {
        pc->v_interleave_hi_u16(dVec[d + 1], dVec[d + 0], dVec[d + 0]);
      }

      pc->v_interleave_lo_u16(dVec[d + 0], dVec[d + 0], dVec[d + 0]);

      if (remain >= 4) {
        pc->v_interleave_hi_u16(dVec[d + 3], dVec[d + 2], dVec[d + 2]);
      }

      if (remain >= 3) {
        pc->v_interleave_lo_u16(dVec[d + 2], dVec[d + 2], dVec[d + 2]);
      }
    }
  }
  else {
    // Maximum number of registers in VecArray is 8, thus we can have up to 2 valid registers in
    // dVec that we are going to shuffle to 1-8 registers by using a table lookup (VPSHUFB or TBL).
    bool limitPredicateCount = pc->vecRegCount() < 32u;

    Operand swiz[4];
    swiz[0] = pc->simdConst(&pc->ct.swizu8_xxxxxxxxxxxx3210_to_3333222211110000, Bcst::kNA, dVec);

    if (vc >= 2u) {
      swiz[1] = pc->simdConst(&pc->ct.swizu8_xxxxxxxx3210xxxx_to_3333222211110000, Bcst::kNA, dVec);
    }

    if (vc >= 3u) {
      swiz[2] = limitPredicateCount ? swiz[0] : pc->simdConst(&pc->ct.swizu8_xxxx3210xxxxxxxx_to_3333222211110000, Bcst::kNA, dVec);
    }

    if (vc >= 4u) {
      swiz[3] = limitPredicateCount ? swiz[1] : pc->simdConst(&pc->ct.swizu8_3210xxxxxxxxxxxx_to_3333222211110000, Bcst::kNA, dVec);
    }

    for (uint32_t q = 0; q < qCount; q++) {
      uint32_t d = q * 4;
      uint32_t remain = vc - d;

      Vec a0 = aVec[q];

      if (remain >= 3u) {
        Vec a1 = a0;
        if (limitPredicateCount) {
          a1 = dVec[d + 2];
          pc->v_swizzle_u32x4(a1, a0, swizzle(3, 2, 3, 2));
        }

        if (remain >= 4) {
          pc->v_swizzlev_u8(dVec[d + 3], a1, swiz[3]);
        }

        pc->v_swizzlev_u8(dVec[d + 2], a1, swiz[2]);
      }

      if (remain >= 2u) {
        pc->v_swizzlev_u8(dVec[d + 1], a0, swiz[1]);
      }

      pc->v_swizzlev_u8(dVec[d], a0, swiz[0]);
    }
  }
}

void fetchMaskA8IntoUC(PipeCompiler* pc, VecArray& dVec, const Gp& sPtr, PixelCount n, AdvanceMode advanceMode, PixelPredicate& predicate, GlobalAlpha* ga) noexcept {
  BL_ASSERT(dVec.size() >= pc->vecCountOf(DataWidth::k64, n) && dVec.vecWidth() == pc->vecWidthOf(DataWidth::k64, n));

#if defined(BL_JIT_ARCH_X86)
  VecWidth vecWidth = pc->vecWidthOf(DataWidth::k64, n);
#endif // BL_JIT_ARCH_X86

  uint32_t vecCount = pc->vecCountOf(DataWidth::k64, n);
  Mem m = ptr(sPtr);

  // Maybe unused on AArch64 in release mode.
  blUnused(vecCount);

  switch (n.value()) {
    case 1: {
      BL_ASSERT(predicate.empty());
      BL_ASSERT(vecCount == 1);

#if defined(BL_JIT_ARCH_X86)
      if (!pc->hasAVX2()) {
        pc->v_load8(dVec[0], m);
        if (advanceMode == AdvanceMode::kAdvance) {
          pc->add(sPtr, sPtr, n.value());
        }
        pc->v_swizzle_lo_u16x4(dVec[0], dVec[0], swizzle(0, 0, 0, 0));
      }
      else
#endif // BL_JIT_ARCH_X86
      {
        pc->v_broadcast_u8(dVec[0], m);
        if (advanceMode == AdvanceMode::kAdvance) {
          pc->add(sPtr, sPtr, n.value());
        }
        pc->v_cvt_u8_lo_to_u16(dVec[0], dVec[0]);
      }

      if (ga) {
        pc->v_mul_i16(dVec[0], dVec[0], ga->ua().cloneAs(dVec[0]));
        pc->v_div255_u16(dVec[0]);
      }
      break;
    }

    case 2: {
      BL_ASSERT(vecCount == 1);

#if defined(BL_JIT_ARCH_X86)
      if (!predicate.empty() || !pc->hasAVX2()) {
        fetchVec8(pc, dVec, sPtr, n.value(), advanceMode, predicate);
        pc->v_interleave_lo_u8(dVec[0], dVec[0], dVec[0]);
        pc->v_interleave_lo_u16(dVec[0], dVec[0], dVec[0]);
        pc->v_cvt_u8_lo_to_u16(dVec[0], dVec[0]);
      }
      else {
        pc->v_loadu16_u8_to_u64(dVec[0], m);
        if (advanceMode == AdvanceMode::kAdvance) {
          pc->add(sPtr, sPtr, n.value());
        }
        pc->v_swizzlev_u8(dVec[0], dVec[0], pc->simdConst(&pc->ct.swizu8_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA, dVec[0]));
      }
#else
      fetchVec8(pc, dVec, sPtr, n.value(), advanceMode, predicate);
      pc->v_swizzlev_u8(dVec[0], dVec[0], pc->simdConst(&pc->ct.swizu8_xxxxxxxxxxxxxx10_to_z1z1z1z1z0z0z0z0, Bcst::kNA, dVec[0]));
#endif // BL_JIT_ARCH_X86

      if (ga) {
        pc->v_mul_i16(dVec[0], dVec[0], ga->ua().cloneAs(dVec[0]));
        pc->v_div255_u16(dVec[0]);
      }
      break;
    }

    case 4: {
#if defined(BL_JIT_ARCH_X86)
      if (vecWidth >= VecWidth::k256) {
        if (predicate.empty()) {
          pc->v_loadu32_u8_to_u64(dVec[0], m);
          if (advanceMode == AdvanceMode::kAdvance) {
            pc->add(sPtr, sPtr, n.value());
          }
        }
        else {
          fetchVec8(pc, dVec, sPtr, n.value(), advanceMode, predicate);
          pc->cc->vpmovzxbq(dVec[0], dVec[0].xmm());
        }
        pc->v_swizzlev_u8(dVec[0], dVec[0], pc->simdConst(&pc->ct.swizu8_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA, dVec[0]));

        if (ga) {
          pc->v_mul_i16(dVec[0], dVec[0], ga->ua().cloneAs(dVec[0]));
          pc->v_div255_u16(dVec[0]);
        }
      }
      else
#endif // BL_JIT_ARCH_X86
      {
        fetchVec8(pc, dVec, sPtr, n.value(), advanceMode, predicate);
        pc->v_cvt_u8_lo_to_u16(dVec[0], dVec[0]);

        if (ga) {
          pc->v_mul_i16(dVec[0], dVec[0], ga->ua().cloneAs(dVec[0]));
          pc->v_div255_u16(dVec[0]);
        }

        pc->v_interleave_lo_u16(dVec[0], dVec[0], dVec[0]);         // dVec[0] = [M3 M3 M2 M2 M1 M1 M0 M0]
        pc->v_swizzle_u32x4(dVec[1], dVec[0], swizzle(3, 3, 2, 2)); // dVec[1] = [M3 M3 M3 M3 M2 M2 M2 M2]
        pc->v_swizzle_u32x4(dVec[0], dVec[0], swizzle(1, 1, 0, 0)); // dVec[0] = [M1 M1 M1 M1 M0 M0 M0 M0]
      }
      break;
    }

    default: {
#if defined(BL_JIT_ARCH_X86)
      if (vecWidth >= VecWidth::k256) {
        if (predicate.empty()) {
          for (uint32_t i = 0; i < vecCount; i++) {
            pc->v_loaduvec_u8_to_u64(dVec[i], m);
            m.addOffsetLo32(dVec[i].size() / 8u);
          }

          if (advanceMode == AdvanceMode::kAdvance) {
            pc->add(sPtr, sPtr, n.value());
          }

          if (ga) {
            Vec ua = ga->ua().cloneAs(dVec[0]);
            if (pc->hasOptFlag(PipeOptFlags::kFastVpmulld)) {
              pc->v_mul_i32(dVec, dVec, ua);
              pc->v_div255_u16(dVec);
              pc->v_swizzle_u32x4(dVec, dVec, swizzle(2, 2, 0, 0));
            }
            else {
              pc->v_mul_i16(dVec, dVec, ua);
              pc->v_div255_u16(dVec);
              pc->v_swizzlev_u8(dVec, dVec, pc->simdConst(&pc->ct.swizu8_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA, dVec[0]));
            }
          }
          else {
            pc->v_swizzlev_u8(dVec, dVec, pc->simdConst(&pc->ct.swizu8_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA, dVec[0]));
          }
        }
        else {
          VecArray pm;
          VecArray um;

          pc->newVecArray(pm, pc->vecCountOf(DataWidth::k8, n), pc->vecWidthOf(DataWidth::k8, n), "pm");
          pc->newVecArray(um, pc->vecCountOf(DataWidth::k16, n), pc->vecWidthOf(DataWidth::k16, n), "um");

          fetchVec8(pc, pm, sPtr, n.value(), advanceMode, predicate);

          if (um.size() == 1) {
            pc->v_cvt_u8_lo_to_u16(um, pm);
          }
          else {
            pc->v_cvt_u8_hi_to_u16(um.odd(), pm);
            pc->v_cvt_u8_lo_to_u16(um.even(), pm);
          }

          if (ga) {
            pc->v_mul_i16(um, um, ga->ua().cloneAs(um[0]));
            pc->v_div255_u16(um);
          }

          if (dVec[0].isZmm()) {
            if (pc->hasAVX512_VBMI()) {
              // Extract 128-bit vectors and then use VPERMB to permute 8 elements to 512-bit width.
              Vec pred = pc->simdVecConst(&pc->ct.permu8_4xu8_lo_to_rgba32_uc, Bcst::kNA_Unique, dVec);

              for (uint32_t i = 1; i < dVec.size(); i++) {
                pc->v_extract_v128(dVec[i], um[0], i);
              }

              for (uint32_t i = 0; i < dVec.size(); i++) {
                pc->v_permute_u8(dVec[i], pred, (i == 0 ? um[0] : dVec[i]).cloneAs(dVec[i]));
              }
            }
            else {
              Vec pred = pc->simdVecConst(&pc->ct.swizu8_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA_Unique, dVec);
              for (uint32_t i = 1; i < dVec.size(); i++) {
                pc->v_extract_v128(dVec[i], um[0], i);
              }

              for (uint32_t i = 0; i < dVec.size(); i++) {
                pc->v_cvt_u8_to_u32(dVec[i], i == 0 ? um[0] : dVec[i]);
                pc->v_swizzlev_u8(dVec[i], dVec[i], pred);
              }
            }
          }
          else if (dVec[0].isYmm()) {
            Vec pred = pc->simdVecConst(&pc->ct.swizu8_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA_Unique, dVec);

            if (dVec.size() >= 2)
              pc->v_swizzle_u64x2(dVec[1].xmm(), um[0].xmm(), swizzle(0, 1));

            if (dVec.size() >= 3)
              pc->v_extract_v128(dVec[2].xmm(), um[0], 1);

            if (dVec.size() >= 4)
              pc->v_swizzle_u64x4(dVec[3], um[0], swizzle(3, 3, 3, 3));

            for (uint32_t i = 0; i < dVec.size(); i++) {
              pc->v_cvt_u8_to_u32(dVec[i], i == 0 ? um[0] : dVec[i]);
              pc->v_swizzlev_u8(dVec[i], dVec[i], pred);
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
          pc->v_cvt_u8_lo_to_u16(dVec[0], m);
          if (advanceMode == AdvanceMode::kAdvance) {
            pc->add(sPtr, sPtr, n.value());
          }
        }
        else {
          fetchVec8(pc, VecArray(dVec[0]), sPtr, n.value(), advanceMode, predicate);
          pc->v_cvt_u8_lo_to_u16(dVec[0], dVec[0]);
        }

        if (ga) {
          pc->v_mul_i16(dVec[0], dVec[0], ga->ua().cloneAs(dVec[0]));
          pc->v_div255_u16(dVec[0]);
        }

        pc->v_interleave_hi_u16(dVec[2], dVec[0], dVec[0]);         // dVec[2] = [M7 M7 M6 M6 M5 M5 M4 M4]
        pc->v_interleave_lo_u16(dVec[0], dVec[0], dVec[0]);         // dVec[0] = [M3 M3 M2 M2 M1 M1 M0 M0]
        pc->v_swizzle_u32x4(dVec[3], dVec[2], swizzle(3, 3, 2, 2)); // dVec[3] = [M7 M7 M7 M7 M6 M6 M6 M6]
        pc->v_swizzle_u32x4(dVec[1], dVec[0], swizzle(3, 3, 2, 2)); // dVec[1] = [M3 M3 M3 M3 M2 M2 M2 M2]
        pc->v_swizzle_u32x4(dVec[0], dVec[0], swizzle(1, 1, 0, 0)); // dVec[0] = [M1 M1 M1 M1 M0 M0 M0 M0]
        pc->v_swizzle_u32x4(dVec[2], dVec[2], swizzle(1, 1, 0, 0)); // dVec[2] = [M5 M5 M5 M5 M4 M4 M4 M4]
      }
      break;
    }
  }
}

void fetchMaskA8(PipeCompiler* pc, VecArray& dVec, const Gp& sPtr, PixelCount n, PixelType pixelType, PixelCoverageFormat coverageFormat, AdvanceMode advanceMode, PixelPredicate& predicate, GlobalAlpha* ga) noexcept {
  switch (pixelType) {
    case PixelType::kA8: {
      BL_ASSERT(n != 1u);

      if (coverageFormat == PixelCoverageFormat::kPacked) {
        VecWidth vecWidth = pc->vecWidthOf(DataWidth::k8, n);
        uint32_t vecCount = pc->vecCountOf(DataWidth::k8, n);

        pc->newVecArray(dVec, vecCount, vecWidth, "vm");
        fetchMaskA8IntoPA(pc, dVec, sPtr, n, advanceMode, predicate, ga);
      }
      else {
        VecWidth vecWidth = pc->vecWidthOf(DataWidth::k16, n);
        uint32_t vecCount = pc->vecCountOf(DataWidth::k16, n);

        pc->newVecArray(dVec, vecCount, vecWidth, "vm");
        fetchMaskA8IntoUA(pc, dVec, sPtr, n, advanceMode, predicate, ga);
      }
      break;
    }

    case PixelType::kRGBA32: {
      if (coverageFormat == PixelCoverageFormat::kPacked) {
        VecWidth vecWidth = pc->vecWidthOf(DataWidth::k32, n);
        uint32_t vecCount = pc->vecCountOf(DataWidth::k32, n);

        pc->newVecArray(dVec, vecCount, vecWidth, "vm");
        fetchMaskA8IntoPC(pc, dVec, sPtr, n, advanceMode, predicate, ga);
      }
      else {
        VecWidth vecWidth = pc->vecWidthOf(DataWidth::k64, n);
        uint32_t vecCount = pc->vecCountOf(DataWidth::k64, n);

        pc->newVecArray(dVec, vecCount, vecWidth, "vm");
        fetchMaskA8IntoUC(pc, dVec, sPtr, n, advanceMode, predicate, ga);
      }

      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

// bl::Pipeline::Jit::FetchUtils - Fetch Pixel(s)
// ==============================================

#if defined(BL_JIT_ARCH_X86)
static void v_permute_op(PipeCompiler* pc, Vec dst, Vec predicate, Operand src, OpcodeVVV op, uint32_t byteQuantity) noexcept {
  OperandSignature sgn;

  if (byteQuantity == 64u) {
    sgn = OperandSignature{Zmm::kSignature};
  }
  else if (byteQuantity == 32u) {
    sgn = OperandSignature{Ymm::kSignature};
  }
  else {
    sgn = OperandSignature{Xmm::kSignature};
  }

  dst.setSignature(sgn);;
  predicate.setSignature(sgn);

  if (src.isReg()) {
    src.setSignature(sgn);
  }

  pc->emit_3v(op, dst, predicate, src);
};

static void v_prgb32_to_pa_vpermb(PipeCompiler* pc, const Vec& dst, const Vec& predicate, const Operand& src, PixelCount n) noexcept {
  return v_permute_op(pc, dst, predicate, src, OpcodeVVV::kPermuteU8, n.value() * 4u);
};

static void v_prgb32_to_ua_vpermw(PipeCompiler* pc, const Vec& dst, const Vec& predicate, const Operand& src, PixelCount n) noexcept {
  return v_permute_op(pc, dst, predicate, src, OpcodeVVV::kPermuteU16, n.value() * 4u);
};

static void fetchPRGB32IntoPA_AVX512(PipeCompiler* pc, VecArray& dVec, const Gp& sPtr, PixelCount n, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  VecWidth pcWidth = VecWidthUtils::vecWidthOf(VecWidth::k512, DataWidth::k32, n.value());
  uint32_t pcCount = VecWidthUtils::vecCountOf(pcWidth, DataWidth::k32, n.value());

  uint32_t dShift = uint32_t(dVec.vecWidth());
  uint32_t dMask = (1u << dShift) - 1u;

  uint32_t iter = 0;
  uint32_t remaining = n.value();

  if (pc->hasAVX512_VBMI()) {
    // In AVX512_VBMI case we can use VPERMT2B to shuffle two registers at once (and micro-architecturally the cost
    // is either the same as VPERMB [AMD] or 2xVPERMB [Intel]). This approach seems to be the most efficient.
    Vec bytePerm = pc->simdVecConst(&pc->ct.permu8_pc_to_pa, Bcst::kNA_Unique, pcWidth);

    if (predicate.empty()) {
      Mem m = ptr(sPtr);

      if (pcWidth == VecWidth::k128 || pcCount == 1u) {
        // If there is only a single register to load or all destination registers are XMMs it's actually very simple.
        do {
          uint32_t quantity = blMin<uint32_t>(remaining, 16u);
          const Vec& dv = dVec[iter];

          v_prgb32_to_pa_vpermb(pc, dv, bytePerm, m, PixelCount{quantity});
          m.addOffsetLo32(quantity * 4u);

          iter++;
          remaining -= quantity;
        } while (remaining > 0u);
      }
      else {
        do {
          uint32_t quantity = blMin<uint32_t>(remaining, 64u);
          const Vec& dv = dVec[iter];

          if (quantity >= 64u) {
            // Four ZMM registers to permute.
            Vec tv = pc->newVec(pcWidth, "@tmp_vec");
            pc->v_loadu512(dv.zmm(), m);
            pc->cc->vpermt2b(dv.zmm(), bytePerm.zmm(), m.cloneAdjusted(64));

            pc->v_loadu512(tv.zmm(), m.cloneAdjusted(128));
            pc->cc->vpermt2b(tv.zmm(), bytePerm.zmm(), m.cloneAdjusted(192));

            pc->v_insert_v256(dv, dv, tv, 1);
          }
          else if (quantity >= 32u) {
            // Two ZMM registers to permute.
            pc->v_loadu512(dv.zmm(), m);
            pc->cc->vpermt2b(dv.zmm(), bytePerm, m.cloneAdjusted(64));
          }
          else {
            v_prgb32_to_pa_vpermb(pc, dv, bytePerm, m, PixelCount{quantity});
          }

          m.addOffsetLo32(quantity * 4u);

          iter++;
          remaining -= quantity;
        } while (remaining > 0u);
      }

      if (advanceMode == AdvanceMode::kAdvance) {
        pc->add(sPtr, sPtr, n.value() * 4u);
      }
    }
    else {
      VecArray pcVec;
      pc->newVecArray(pcVec, pcCount, pcWidth, "@tmp_pc_vec");

      // We really want each fourth register to point to the original dVec (so we don't have to move afterwards).
      for (uint32_t i = 0; i < pcCount; i += 4) {
        pcVec.reassign(i, dVec[i / 4u]);
      }

      fetchVec32(pc, pcVec, sPtr, n.value(), advanceMode, predicate);

      if (pcWidth == VecWidth::k128 || pcCount == 1u) {
        // If there is only a single register to load or all destination registers are XMMs it's actually very simple.
        do {
          uint32_t quantity = blMin<uint32_t>(remaining, 16u);
          const Vec& dv = dVec[iter];

          v_prgb32_to_pa_vpermb(pc, dv, bytePerm, pcVec[iter], PixelCount{quantity});

          iter++;
          remaining -= quantity;
        } while (remaining > 0u);
      }
      else {
        uint32_t pcIdx = 0;
        do {
          uint32_t quantity = blMin<uint32_t>(remaining, 64u);
          const Vec& dv = dVec[iter];

          if (quantity >= 64u) {
            // Four ZMM registers to permute.
            pc->cc->vpermt2b(pcVec[pcIdx + 0].zmm(), bytePerm.zmm(), pcVec[pcIdx + 1].zmm());
            pc->cc->vpermt2b(pcVec[pcIdx + 2].zmm(), bytePerm.zmm(), pcVec[pcIdx + 3].zmm());

            pc->v_insert_v256(dv, pcVec[pcIdx + 0], pcVec[pcIdx + 2], 1);
            pcIdx += 4;
          }
          else if (quantity >= 32u) {
            // Two ZMM registers to permute.
            pc->cc->vpermt2b(pcVec[pcIdx].zmm(), bytePerm.zmm(), pcVec[pcIdx + 1].zmm());
            BL_ASSERT(dv.id() == pcVec[pcIdx].id());

            pcIdx += 2;
          }
          else {
            v_prgb32_to_pa_vpermb(pc, dv, bytePerm, pcVec[pcIdx], PixelCount{quantity});
            pcIdx++;
          }

          iter++;
          remaining -= quantity;
        } while (remaining > 0u);
      }
    }
  }
  else if (predicate.empty()) {
    Mem m = ptr(sPtr);
    Vec secondary;

    if (pcCount > 1u) {
      secondary = pc->newVec(pcWidth, "@tmp_vec");
    }

    do {
      uint32_t quantity = blMin<uint32_t>(remaining, 16u);
      uint32_t fraction = iter & dMask;

      const Vec& dv = dVec[iter >> dShift];
      const Vec& tv = fraction ? secondary : dv;

      if (quantity >= 16u) {
        pc->v_srli_u32(tv.zmm(), m, 24);
        pc->cc->vpmovdb(tv.xmm(), tv.zmm());
      }
      else if (quantity >= 8u) {
        pc->v_srli_u32(tv.ymm(), m, 24);
        pc->cc->vpmovdb(tv.xmm(), tv.ymm());
      }
      else if (quantity >= 4u) {
        pc->v_srli_u32(tv.xmm(), m, 24);
        pc->cc->vpmovdb(tv.xmm(), tv.xmm());
      }
      else {
        BL_NOT_REACHED();
      }

      if (fraction == 1u) {
        pc->v_insert_v128(dv.ymm(), dv.ymm(), tv.xmm(), fraction);
      }
      else if (fraction > 1u) {
        pc->v_insert_v128(dv.zmm(), dv.zmm(), tv.xmm(), fraction);
      }

      m.addOffsetLo32(quantity * 4u);

      iter++;
      remaining -= quantity;
    } while (remaining > 0u);

    if (advanceMode == AdvanceMode::kAdvance) {
      pc->add(sPtr, sPtr, n.value() * 4u);
    }
  }
  else {
    VecArray tVec;

    pc->newVecArray(tVec, pcCount, pcWidth, "@tmp_vec");
    fetchVec32(pc, tVec, sPtr, n.value(), advanceMode, predicate);

    do {
      uint32_t quantity = blMin<uint32_t>(remaining, 16u);
      uint32_t fraction = iter & dMask;

      const Vec& dv = dVec[iter >> dShift];
      const Vec& tv = fraction ? tVec[iter] : dv;

      if (quantity >= 16u) {
        pc->v_srli_u32(tv.zmm(), tv.zmm(), 24);
        pc->cc->vpmovdb(tv.zmm(), tVec[iter].zmm());
      }
      else if (quantity >= 8u) {
        pc->v_srli_u32(tv.ymm(), tv.ymm(), 24);
        pc->cc->vpmovdb(tv.ymm(), tVec[iter].ymm());
      }
      else if (quantity >= 4u) {
        pc->v_srli_u32(tv.xmm(), tv.xmm(), 24);
        pc->cc->vpmovdb(tv.xmm(), tVec[iter].xmm());
      }
      else {
        BL_NOT_REACHED();
      }

      if (fraction == 1u) {
        pc->v_insert_v128(dv.ymm(), dv.ymm(), tv.xmm(), fraction);
      }
      else if (fraction > 1u) {
        pc->v_insert_v128(dv.zmm(), dv.zmm(), tv.xmm(), fraction);
      }

      iter++;
      remaining -= quantity;
    } while (remaining > 0u);
  }
}

static void fetchPRGB32IntoPA_AVX2(PipeCompiler* pc, VecArray& dVec, const Gp& sPtr, PixelCount n, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  VecWidth pcWidth = VecWidthUtils::vecWidthOf(VecWidth::k256, DataWidth::k32, n.value());
  uint32_t pcCount = VecWidthUtils::vecCountOf(pcWidth, DataWidth::k32, n.value());

  VecArray tVec;
  pc->newVecArray(tVec, pcCount, pcWidth, "@tmp_vec");
  fetchVec32(pc, tVec, sPtr, n.value(), advanceMode, predicate);

  uint32_t dIdx = 0;
  uint32_t tIdx = 0;
  uint32_t remaining = n.value();

  pc->v_srli_u32(tVec, tVec, 24);

  do {
    const Vec& dv = dVec[dIdx];
    uint32_t quantity = blMin<uint32_t>(remaining, 32u);

    if (quantity >= 16u) {
      Vec vpermdPred = pc->simdVecConst(&pc->ct.permu32_fix_2x_pack_avx2, Bcst::kNA_Unique, pcWidth);

      const Vec& sv0 = tVec[tIdx + 0];
      const Vec& sv1 = tVec[tIdx + 1];

      if (quantity == 32u) {
        const Vec& sv2 = tVec[tIdx + 2];
        const Vec& sv3 = tVec[tIdx + 3];

        pc->v_packs_i32_u16(sv0, sv0, sv1);
        pc->v_packs_i32_u16(sv2, sv2, sv3);
        pc->v_packs_i16_u8(sv0, sv0, sv2);
        pc->cc->vpermd(dv, vpermdPred, sv0);

        tIdx += 4;
      }
      else if (quantity == 16) {
        pc->v_packs_i32_u16(sv0, sv0, sv1);
        pc->v_packs_i16_u8(sv0, sv0, sv0);
        pc->cc->vpermd(dv, vpermdPred, sv0);

        tIdx += 2;
      }
    }
    else {
      const Vec& sv = tVec[tIdx];

      if (quantity == 8u) {
        pc->v_packs_i32_u16(sv, sv, sv);
        pc->v_swizzle_u64x4(sv, sv, swizzle(3, 1, 2, 0));
        pc->v_packs_i16_u8(dv.xmm(), sv.xmm(), sv.xmm());
      }
      else {
        pc->v_packs_i32_u16(dv.xmm(), sv.xmm(), sv.xmm());
        pc->v_packs_i16_u8(dv.xmm(), dv.xmm(), dv.xmm());
      }

      tIdx++;
    }

    dIdx++;
    remaining -= quantity;
  } while (remaining > 0u);
}

static void fetchPRGB32IntoUA_AVX512(PipeCompiler* pc, VecArray& dVec, const Gp& sPtr, PixelCount n, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  VecWidth pcWidth = VecWidthUtils::vecWidthOf(VecWidth::k512, DataWidth::k32, n.value());
  uint32_t pcCount = VecWidthUtils::vecCountOf(pcWidth, DataWidth::k32, n.value());

  uint32_t iter = 0;
  uint32_t remaining = n.value();

  // A baseline AVX512 ISA offers VPERMT2W to shuffle two registers at once at 16-bit quantities, which is sufficient
  // for our case (converting a 32-bit ARGB pixel into an unpacked 16-bit alpha). We always want to shift by 8 at the
  // end as that means shifting half registers in case we load multiple ones.
  Vec permutePredicate = pc->simdVecConst(&pc->ct.permu16_pc_to_ua, Bcst::kNA_Unique, pcWidth);

  if (predicate.empty()) {
    Mem m = ptr(sPtr);

    if (pcWidth == VecWidth::k128 || pcCount == 1u) {
      // If there is only a single register to load or all destination registers are XMMs it's actually very simple.
      do {
        uint32_t quantity = blMin<uint32_t>(remaining, 16u);
        const Vec& dv = dVec[iter];

        v_prgb32_to_ua_vpermw(pc, dv, permutePredicate, m, PixelCount{quantity});
        m.addOffsetLo32(quantity * 4u);

        iter++;
        remaining -= quantity;
      } while (remaining > 0u);
    }
    else {
      do {
        uint32_t quantity = blMin<uint32_t>(remaining, 64u);
        const Vec& dv = dVec[iter];

        if (quantity >= 64u) {
          // Four ZMM registers to permute.
          Vec tv = pc->newVec(pcWidth, "@tmp_vec");
          pc->v_loadu512(dv.zmm(), m);
          pc->cc->vpermt2w(dv.zmm(), permutePredicate.zmm(), m.cloneAdjusted(64));

          pc->v_loadu512(tv.zmm(), m.cloneAdjusted(128));
          pc->cc->vpermt2w(tv.zmm(), permutePredicate.zmm(), m.cloneAdjusted(192));

          pc->v_insert_v256(dv, dv, tv, 1);
        }
        else if (quantity >= 32u) {
          // Two ZMM registers to permute.
          pc->v_loadu512(dv.zmm(), m);
          pc->cc->vpermt2w(dv.zmm(), permutePredicate, m.cloneAdjusted(64));
        }
        else {
          v_prgb32_to_ua_vpermw(pc, dv, permutePredicate, m, PixelCount{quantity});
        }

        m.addOffsetLo32(quantity * 4u);

        iter++;
        remaining -= quantity;
      } while (remaining > 0u);
    }

    if (advanceMode == AdvanceMode::kAdvance) {
      pc->add(sPtr, sPtr, n.value() * 4u);
    }
  }
  else {
    VecArray pcVec;
    pc->newVecArray(pcVec, pcCount, pcWidth, "@tmp_pc_vec");

    // We really want each second register to point to the original dVec (so we don't have to move afterwards).
    for (uint32_t i = 0; i < pcCount; i += 2) {
      pcVec.reassign(i, dVec[i / 2u]);
    }

    fetchVec32(pc, pcVec, sPtr, n.value(), advanceMode, predicate);

    if (pcWidth == VecWidth::k128 || pcCount == 1u) {
      // If there is only a single register to load or all destination registers are XMMs it's actually very simple.
      do {
        uint32_t quantity = blMin<uint32_t>(remaining, 16u);
        const Vec& dv = dVec[iter];

        v_prgb32_to_ua_vpermw(pc, dv, permutePredicate, pcVec[iter], PixelCount{quantity});

        iter++;
        remaining -= quantity;
      } while (remaining > 0u);
    }
    else {
      uint32_t pcIdx = 0;
      do {
        uint32_t quantity = blMin<uint32_t>(remaining, 64u);
        const Vec& dv = dVec[iter];

        if (quantity >= 32u) {
          // Two ZMM registers to permute.
          pc->cc->vpermt2w(pcVec[pcIdx].zmm(), permutePredicate.zmm(), pcVec[pcIdx + 1].zmm());
          BL_ASSERT(dv.id() == pcVec[pcIdx].id());

          pcIdx += 2;
        }
        else {
          v_prgb32_to_ua_vpermw(pc, dv, permutePredicate, pcVec[pcIdx], PixelCount{quantity});
          pcIdx++;
        }

        iter++;
        remaining -= quantity;
      } while (remaining > 0u);
    }
  }

  // Apply the final shift by 8 to get unpacked alpha from [Ax] packed data.
  pc->v_srli_u16(dVec, dVec, 8);
}

static void fetchPRGB32IntoUA_AVX2(PipeCompiler* pc, VecArray& dVec, const Gp& sPtr, PixelCount n, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  VecWidth pcWidth = VecWidthUtils::vecWidthOf(VecWidth::k256, DataWidth::k32, n.value());
  uint32_t pcCount = VecWidthUtils::vecCountOf(pcWidth, DataWidth::k32, n.value());

  VecArray tVec;
  pc->newVecArray(tVec, pcCount, pcWidth, "@tmp_vec");
  fetchVec32(pc, tVec, sPtr, n.value(), advanceMode, predicate);

  uint32_t dIdx = 0;
  uint32_t tIdx = 0;
  uint32_t remaining = n.value();

  pc->v_srli_u32(tVec, tVec, 24);

  do {
    uint32_t quantity = blMin<uint32_t>(remaining, 16u);

    const Vec& dv = dVec[dIdx];
    const Vec& sv0 = tVec[tIdx + 0];

    if (quantity == 16u) {
      const Vec& sv1 = tVec[tIdx + 1];

      pc->v_packs_i32_u16(sv0, sv0, sv1);
      pc->v_swizzle_u64x4(dv.ymm(), sv0, swizzle(3, 1, 2, 0));

      tIdx += 2;
    }
    else if (quantity == 8u) {
      pc->v_packs_i32_u16(sv0, sv0, sv0);
      pc->v_swizzle_u64x4(dv.ymm(), sv0, swizzle(3, 1, 2, 0));

      tIdx++;
    }
    else {
      pc->v_packs_i32_u16(dv.xmm(), sv0.xmm(), sv0.xmm());

      tIdx++;
    }

    dIdx++;
    remaining -= quantity;
  } while (remaining > 0u);
}
#endif // BL_JIT_ARCH_X86

void fetchPRGB32IntoPA(PipeCompiler* pc, VecArray& dVec, const Gp& sPtr, PixelCount n, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
#if defined(BL_JIT_ARCH_X86)
  if (pc->hasAVX512() && n >= 4u) {
    fetchPRGB32IntoPA_AVX512(pc, dVec, sPtr, n, advanceMode, predicate);
    return;
  }
  else if (pc->hasAVX2() && dVec.isVec256() && n >= 8u) {
    fetchPRGB32IntoPA_AVX2(pc, dVec, sPtr, n, advanceMode, predicate);
    return;
  }
#endif // BL_JIT_ARCH_X86

  VecWidth pcWidth = VecWidthUtils::vecWidthOf(VecWidth::k128, DataWidth::k32, n.value());
  uint32_t pcCount = VecWidthUtils::vecCountOf(pcWidth, DataWidth::k32, n.value());

  VecArray tVec;
  pc->newVecArray(tVec, pcCount, pcWidth, "@tmp_vec");

  // We really want each fourth register to point to the original dVec (so we don't have to move afterwards).
  for (uint32_t i = 0; i < pcCount; i += 4) {
    tVec.reassign(i, dVec[i / 4u]);
  }

  fetchVec32(pc, tVec, sPtr, n.value(), advanceMode, predicate);

  uint32_t dIdx = 0;
  uint32_t tIdx = 0;
  uint32_t remaining = n.value();

  pc->v_srli_u32(tVec, tVec, 24);

  do {
    const Vec& dv = dVec[dIdx];
    uint32_t quantity = blMin<uint32_t>(remaining, 16u);

    if (quantity > 8u) {
      pc->v_packs_i32_u16(tVec[tIdx + 0], tVec[tIdx + 0], tVec[tIdx + 1]);
      pc->v_packs_i32_u16(tVec[tIdx + 2], tVec[tIdx + 2], tVec[tIdx + 3]);
      pc->v_packs_i16_u8(dv, tVec[tIdx + 0], tVec[tIdx + 2]);

      tIdx += 4;
    }
    else if (quantity > 4u) {
      pc->v_packs_i32_u16(dv, tVec[tIdx + 0], tVec[tIdx + 1]);
      pc->v_packs_i16_u8(dv, dv, dv);

      tIdx += 2;
    }
    else {
      pc->v_packs_i32_u16(dv, tVec[tIdx + 0], tVec[tIdx + 0]);
      pc->v_packs_i16_u8(dv, dv, dv);

      tIdx++;
    }

    dIdx++;
    remaining -= quantity;
  } while (remaining > 0u);
}

void fetchPRGB32IntoUA(PipeCompiler* pc, VecArray& dVec, const Gp& sPtr, PixelCount n, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
#if defined(BL_JIT_ARCH_X86)
  if (pc->hasAVX512() && n >= 8u) {
    fetchPRGB32IntoUA_AVX512(pc, dVec, sPtr, n, advanceMode, predicate);
    return;
  }
  else if (pc->hasAVX2() && dVec.isVec256() && n >= 8u) {
    fetchPRGB32IntoUA_AVX2(pc, dVec, sPtr, n, advanceMode, predicate);
    return;
  }
#endif // BL_JIT_ARCH_X86

  VecWidth pcWidth = VecWidthUtils::vecWidthOf(VecWidth::k128, DataWidth::k32, n.value());
  uint32_t pcCount = VecWidthUtils::vecCountOf(pcWidth, DataWidth::k32, n.value());

  VecArray tVec;
  pc->newVecArray(tVec, pcCount, pcWidth, "@tmp_vec");

  // We really want each second register to point to the original dVec (so we don't have to move afterwards).
  for (uint32_t i = 0; i < pcCount; i += 2) {
    tVec.reassign(i, dVec[i / 2u]);
  }

  fetchVec32(pc, tVec, sPtr, n.value(), advanceMode, predicate);

  uint32_t dIdx = 0;
  uint32_t tIdx = 0;
  uint32_t remaining = n.value();

  pc->v_srli_u32(tVec, tVec, 24);

  do {
    const Vec& dv = dVec[dIdx];
    uint32_t quantity = blMin<uint32_t>(remaining, 16u);

    if (quantity > 4u) {
      pc->v_packs_i32_u16(dv, tVec[tIdx + 0], tVec[tIdx + 1]);
      tIdx += 2;
    }
    else {
      pc->v_packs_i32_u16(dv, tVec[tIdx + 0], tVec[tIdx + 0]);
      tIdx++;
    }

    dIdx++;
    remaining -= quantity;
  } while (remaining > 0u);
}

static void fetchPixelsA8(PipeCompiler* pc, Pixel& p, PixelCount n, PixelFlags flags, PixelFetchInfo fInfo, Gp sPtr, Alignment alignment, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  BL_ASSERT(p.isA8());
  BL_ASSERT(n.value() > 1u);

  // TODO: Do we need it in general?
  blUnused(alignment);

  p.setCount(n);

  // It's forbidden to use PA in single-pixel case (scalar mode) and SA in multiple-pixel case (vector mode).
  BL_ASSERT(uint32_t(n.value() != 1) ^ uint32_t(blTestFlag(flags, PixelFlags::kSA)));

  // It's forbidden to request both - PA and UA.
  BL_ASSERT((flags & (PixelFlags::kPA | PixelFlags::kUA)) != (PixelFlags::kPA | PixelFlags::kUA));

  VecWidth paWidth = pc->vecWidthOf(DataWidth::k8, n);
  uint32_t paCount = pc->vecCountOf(DataWidth::k8, n);

  VecWidth uaWidth = pc->vecWidthOf(DataWidth::k16, n);
  uint32_t uaCount = pc->vecCountOf(DataWidth::k16, n);

  switch (fInfo.format()) {
    // A8 <- PRGB32.
    case FormatExt::kPRGB32: {
      if (blTestFlag(flags, PixelFlags::kPA)) {
        pc->newVecArray(p.pa, paCount, paWidth, p.name(), "pa");
        fetchPRGB32IntoPA(pc, p.pa, sPtr, n, advanceMode, predicate);
      }
      else {
        pc->newVecArray(p.ua, uaCount, uaWidth, p.name(), "ua");
        fetchPRGB32IntoUA(pc, p.ua, sPtr, n, advanceMode, predicate);
      }
      break;
    }

    // A8 <- A8.
    case FormatExt::kA8: {
      if (blTestFlag(flags, PixelFlags::kPA)) {
        pc->newVecArray(p.pa, paCount, paWidth, p.name(), "pa");
        fetchMaskA8IntoPA(pc, p.pa, sPtr, n, advanceMode, predicate);
      }
      else {
        pc->newVecArray(p.ua, uaCount, uaWidth, p.name(), "ua");
        fetchMaskA8IntoUA(pc, p.ua, sPtr, n, advanceMode, predicate);
      }
      break;
    }

    default:
      BL_NOT_REACHED();
  }

  satisfyPixelsA8(pc, p, flags);
}

static void fetchPixelsRGBA32(PipeCompiler* pc, Pixel& p, PixelCount n, PixelFlags flags, PixelFetchInfo fInfo, Gp sPtr, Alignment alignment, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  BL_ASSERT(p.isRGBA32());
  BL_ASSERT(n.value() > 1u);

  p.setCount(n);

  Mem sMem = ptr(sPtr);
  uint32_t srcBPP = fInfo.bpp();

  VecWidth pcWidth = pc->vecWidthOf(DataWidth::k32, n);
  uint32_t pcCount = VecWidthUtils::vecCountOf(pcWidth, DataWidth::k32, n);

  VecWidth ucWidth = pc->vecWidthOf(DataWidth::k64, n);
  uint32_t ucCount = VecWidthUtils::vecCountOf(ucWidth, DataWidth::k64, n);

  switch (fInfo.format()) {
    // RGBA32 <- PRGB32 | XRGB32.
    case FormatExt::kPRGB32:
    case FormatExt::kXRGB32: {
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

        if (advanceMode == AdvanceMode::kAdvance) {
          pc->add(sPtr, sPtr, n.value() * srcBPP);
        }
      }

      if (fInfo.format() == FormatExt::kXRGB32) {
        fillAlphaChannel(pc, p);
      }

      break;
    }

    // RGBA32 <- A8.
    case FormatExt::kA8: {
      if (blTestFlag(flags, PixelFlags::kPC)) {
        pc->newVecArray(p.pc, pcCount, pcWidth, p.name(), "pc");
        fetchMaskA8IntoPC(pc, p.pc, sPtr, n, advanceMode, predicate);
      }
      else {
        pc->newVecArray(p.uc, ucCount, ucWidth, p.name(), "uc");
        fetchMaskA8IntoUC(pc, p.uc, sPtr, n, advanceMode, predicate);
      }

      break;
    }

    // RGBA32 <- Unknown?
    default:
      BL_NOT_REACHED();
  }

  satisfyPixelsRGBA32(pc, p, flags);
}

void fetchPixel(PipeCompiler* pc, Pixel& p, PixelFlags flags, PixelFetchInfo fInfo, Mem sMem) noexcept {
  p.setCount(PixelCount{1u});

  switch (p.type()) {
    case PixelType::kA8: {
      switch (fInfo.format()) {
        case FormatExt::kPRGB32: {
          p.sa = pc->newGp32("a");
#if defined(BL_JIT_ARCH_X86)
          sMem.addOffset(fInfo.fetchAlphaOffset());
          pc->load_u8(p.sa, sMem);
#else
          if (fInfo.fetchAlphaOffset() == 0) {
            pc->load_u8(p.sa, sMem);
          }
          else {
            pc->load_u32(p.sa, sMem);
            pc->shr(p.sa, p.sa, 24);
          }
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
      switch (fInfo.format()) {
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

void fetchPixels(PipeCompiler* pc, Pixel& p, PixelCount n, PixelFlags flags, PixelFetchInfo fInfo, const Gp& sPtr, Alignment alignment, AdvanceMode advanceMode) noexcept {
  fetchPixels(pc, p, n, flags, fInfo, sPtr, alignment, advanceMode, pc->emptyPredicate());
}

void fetchPixels(PipeCompiler* pc, Pixel& p, PixelCount n, PixelFlags flags, PixelFetchInfo fInfo, const Gp& sPtr, Alignment alignment, AdvanceMode advanceMode, PixelPredicate& predicate) noexcept {
  if (n == 1u) {
    BL_ASSERT(predicate.empty());
    fetchPixel(pc, p, flags, fInfo, mem_ptr(sPtr));

    if (advanceMode == AdvanceMode::kAdvance) {
      pc->add(sPtr, sPtr, fInfo.bpp());
    }
    return;
  }

  switch (p.type()) {
    case PixelType::kA8:
      fetchPixelsA8(pc, p, n, flags, fInfo, sPtr, alignment, advanceMode, predicate);
      break;

    case PixelType::kRGBA32:
      fetchPixelsRGBA32(pc, p, n, flags, fInfo, sPtr, alignment, advanceMode, predicate);
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
          pc->v_swizzlev_u8(ux[i], px[i / 2u], pc->simdConst(&pc->ct.swizu8_76543210xxxxxxxx_to_z7z6z5z4z3z2z1z0, Bcst::kNA, ux[i]));
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
      pc->v_swizzlev_u8(ux[i], px[i / 2u], pc->simdConst(&pc->ct.swizu8_76543210xxxxxxxx_to_z7z6z5z4z3z2z1z0, Bcst::kNA, ux[i]));
    else
      pc->v_cvt_u8_lo_to_u16(ux[i], px[i / 2u]);
  }
#endif
}

void x_fetch_unpacked_a8_2x(PipeCompiler* pc, const Vec& dst, PixelFetchInfo fInfo, const Mem& src1, const Mem& src0) noexcept {
#if defined(BL_JIT_ARCH_X86)
  Mem m0 = src0;
  Mem m1 = src1;

  if (fInfo.format() == FormatExt::kPRGB32) {
    m0.addOffset(fInfo.fetchAlphaOffset());
    m1.addOffset(fInfo.fetchAlphaOffset());
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

  if (fInfo.format() == FormatExt::kPRGB32 && fInfo.fetchAlphaOffset()) {
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

  switch (bpp) {
    case 1: {
      if (!predicate.empty()) {
        // Predicated pixel count must be greater than 1!
        BL_ASSERT(n != 1);

        satisfyPixels(pc, p, PixelFlags::kPA | PixelFlags::kImmutable);
        storePredicatedVec8(pc, dPtr, p.pa, n.value(), AdvanceMode::kAdvance, predicate);
      }
      else {
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
