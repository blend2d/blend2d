// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/fetchutilsinlineloops_p.h"

namespace bl {
namespace Pipeline {
namespace JIT {
namespace FetchUtils {

// bl::Pipeline::JIT::FetchUtils - FillSpan & FillRect Loops
// =========================================================

static BL_NOINLINE void emitMemFillSequence(PipeCompiler* pc, Mem dPtr, Vec sVec, uint32_t numBytes, AdvanceMode advanceMode) noexcept {
  uint32_t n = numBytes;

#if defined(BL_JIT_ARCH_X86)
  if (sVec.size() > 32 && n <= 32)
    sVec = sVec.ymm();

  if (sVec.size() > 16 && n <= 16)
    sVec = sVec.xmm();

  uint32_t vecSize = sVec.size();
  for (uint32_t i = 0; i < n; i += vecSize) {
    pc->v_storeuvec(dPtr, sVec);
    dPtr.addOffsetLo32(vecSize);
  }

  if (advanceMode == AdvanceMode::kAdvance) {
    Gp dPtrBase = dPtr.baseReg().as<Gp>();
    pc->add(dPtrBase, dPtrBase, numBytes);
  }
#elif defined(BL_JIT_ARCH_A64)
  AsmCompiler* cc = pc->cc;

  bool postIndex = advanceMode == AdvanceMode::kAdvance && !dPtr.hasOffset();
  if (postIndex) {
    dPtr.setOffsetMode(a64::OffsetMode::kPostIndex);
  }

  while (n >= 32u) {
    if (postIndex)
      dPtr.setOffsetLo32(32);

    cc->stp(sVec, sVec, dPtr);
    if (!postIndex)
      dPtr.addOffsetLo32(32);

    n -= 32u;
  }

  for (uint32_t count = 16; count != 0; count >>= 1) {
    if (n >= count) {
      Vec v = sVec;

      if (postIndex) {
        dPtr.setOffsetLo32(int32_t(count));
      }

      pc->v_store_iany(dPtr, v, count, Alignment{1});
      if (!postIndex)
        dPtr.addOffsetLo32(count);

      n -= count;
    }
  }

  // In case that any of the two pointers had an offset, we have to advance here...
  if (advanceMode == AdvanceMode::kAdvance && !postIndex) {
    Gp dPtrBase = dPtr.baseReg().as<Gp>();
    pc->add(dPtrBase, dPtrBase, numBytes);
  }
#else
  #error "Unknown architecture"
#endif
}

void inlineFillSpanLoop(
  PipeCompiler* pc,
  Gp dst,
  Vec src,
  Gp i,
  uint32_t mainLoopSize, uint32_t itemSize, uint32_t itemGranularity) noexcept {

  BL_ASSERT(IntOps::isPowerOf2(itemSize));
  BL_ASSERT(itemSize <= 16u);

  uint32_t granularityInBytes = itemSize * itemGranularity;
  uint32_t mainStepInItems = mainLoopSize / itemSize;

  BL_ASSERT(IntOps::isPowerOf2(granularityInBytes));
  BL_ASSERT(mainStepInItems * itemSize == mainLoopSize);

  BL_ASSERT(mainLoopSize >= 16u);
  BL_ASSERT(mainLoopSize >= granularityInBytes);

  uint32_t k;
  uint32_t vecSize = src.size();

  // Granularity >= 16 Bytes
  // -----------------------

  if (granularityInBytes >= 16u) {
    Label L_End = pc->newLabel();

    // MainLoop
    // --------

    {
      Label L_MainIter = pc->newLabel();
      Label L_MainSkip = pc->newLabel();

      pc->j(L_MainSkip, sub_c(i, mainStepInItems));
      pc->bind(L_MainIter);
      emitMemFillSequence(pc, mem_ptr(dst), src, mainLoopSize, AdvanceMode::kAdvance);
      pc->j(L_MainIter, sub_nc(i, mainStepInItems));

      pc->bind(L_MainSkip);
      pc->j(L_End, add_z(i, mainStepInItems));
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize * 2 > granularityInBytes) {
      Label L_TailIter = pc->newLabel();

      pc->bind(L_TailIter);
      emitMemFillSequence(pc, mem_ptr(dst), src, granularityInBytes, AdvanceMode::kAdvance);
      pc->j(L_TailIter, sub_nz(i, itemGranularity));
    }
    else if (mainLoopSize * 2 == granularityInBytes) {
      emitMemFillSequence(pc, mem_ptr(dst), src, granularityInBytes, AdvanceMode::kAdvance);
    }

    pc->bind(L_End);
    return;
  }

  // Granularity == 4 Bytes
  // ----------------------

  if (granularityInBytes == 4u) {
    BL_ASSERT(itemSize <= 4u);

    uint32_t sizeShift = IntOps::ctz(itemSize);
    uint32_t alignPattern = ((vecSize - 1u) * itemSize) & (vecSize - 1u);

    uint32_t oneStepInItems = 4u >> sizeShift;
    uint32_t tailStepInItems = 16u >> sizeShift;

    if (vecSize >= 32u) {
      // Make `i` contain the number of 32-bit units to fill.
      Gp i_ptr = i.cloneAs(dst);
      if (itemSize != 4u)
        pc->shr(i, i, 2u - sizeShift);

#if defined(BL_JIT_ARCH_X86)
      if (pc->hasMaskedAccessOf(4) && pc->hasOptFlag(PipeOptFlags::kFastStoreWithMask)) {
        Label L_MainIter = pc->newLabel();
        Label L_MainSkip = pc->newLabel();
        Label L_TailIter = pc->newLabel();
        Label L_TailSkip = pc->newLabel();
        Label L_End = pc->newLabel();

        pc->j(L_MainSkip, sub_c(i_ptr, vecSize));

        pc->bind(L_MainIter);
        emitMemFillSequence(pc, mem_ptr(dst), src, vecSize * 4u, AdvanceMode::kAdvance);
        pc->j(L_MainIter, sub_nc(i_ptr, vecSize));

        pc->bind(L_MainSkip);
        pc->j(L_TailSkip, add_s(i_ptr, vecSize - vecSize / 4u));

        pc->bind(L_TailIter);
        emitMemFillSequence(pc, mem_ptr(dst), src, vecSize, AdvanceMode::kAdvance);
        pc->j(L_TailIter, sub_nc(i_ptr, vecSize / 4u));

        pc->bind(L_TailSkip);
        pc->j(L_End, add_z(i_ptr, vecSize / 4u));

        PixelPredicate predicate(vecSize / 4u, PredicateFlags::kNeverFull, i);
        pc->v_store_predicated_u32(mem_ptr(dst), src, vecSize / 4u, predicate);

        pc->lea(dst, mem_ptr(dst, i_ptr, 2));
        pc->bind(L_End);
      }
      else
#endif // BL_JIT_ARCH_X86
      {
        Label L_LargeIter = pc->newLabel();
        Label L_SmallIter = pc->newLabel();
        Label L_SmallCheck = pc->newLabel();
        Label L_TinyCase16 = pc->newLabel();
        Label L_TinyCase8 = pc->newLabel();
        Label L_TinyCase4 = pc->newLabel();
        Label L_TinyCase2 = pc->newLabel();
        Label L_End = pc->newLabel();

        pc->j(vecSize == 64 ? L_TinyCase16 : L_TinyCase8, sub_c(i_ptr, vecSize / 4u));
        pc->j(L_SmallIter, ucmp_lt(i_ptr, vecSize));

        // Align to a vecSize, but keep two LSB bits in case the alignment is unfixable.
        pc->v_storeuvec(mem_ptr(dst), src);
        pc->add(dst, dst, vecSize);
        pc->lea(i_ptr, mem_ptr(dst, i_ptr, 2));
        pc->and_(dst, dst, -int(vecSize) | 0x3);
        pc->sub(i_ptr, i_ptr, dst);
        pc->sar(i_ptr, i_ptr, 2);
        pc->sub(i_ptr, i_ptr, vecSize);

        pc->bind(L_LargeIter);
        emitMemFillSequence(pc, mem_ptr(dst), src, vecSize * 4, AdvanceMode::kAdvance);
        pc->j(L_LargeIter, sub_ugt(i_ptr, vecSize));

        pc->add(i_ptr, i_ptr, vecSize);
        pc->j(L_SmallCheck);

        pc->bind(L_SmallIter);
        pc->v_storeuvec(mem_ptr(dst), src);
        pc->add(dst, dst, vecSize);
        pc->bind(L_SmallCheck);
        pc->j(L_SmallIter, sub_ugt(i_ptr, vecSize / 4u));

        pc->add_ext(dst, dst, i_ptr, 4, vecSize);
        pc->v_storeuvec(mem_ptr(dst, -int(vecSize)), src);
        pc->j(L_End);

        if (vecSize == 64) {
          pc->bind(L_TinyCase16);
          pc->j(L_TinyCase8, bt_z(i, 3));
          pc->v_storeu256(mem_ptr(dst), src);
          pc->add(dst, dst, 32);
        }

        pc->bind(L_TinyCase8);
        pc->j(L_TinyCase4, bt_z(i, 2));
        pc->v_storeu128(mem_ptr(dst), src);
        pc->add(dst, dst, 16);

        pc->bind(L_TinyCase4);
        pc->j(L_TinyCase2, bt_z(i, 1));
        pc->v_storeu64(mem_ptr(dst), src);
        pc->add(dst, dst, 8);

        pc->bind(L_TinyCase2);
        pc->and_(i, i, 0x1);
        pc->shl(i, i, 2);
        pc->add(dst, dst, i_ptr);
        pc->v_storea32(mem_ptr(dst, -4), src);

        pc->bind(L_End);
      }
    }
    else {
      Label L_Finalize = pc->newLabel();
      Label L_End = pc->newLabel();

      // Preparation / Alignment
      // -----------------------

      {
        pc->j(L_Finalize, ucmp_lt(i, oneStepInItems * (vecSize / 4u)));

        Gp i_ptr = i.cloneAs(dst);
        if (sizeShift)
          pc->shl(i_ptr, i_ptr, sizeShift);
        pc->add(i_ptr, i_ptr, dst);

        pc->v_storeuvec(mem_ptr(dst), src);

        pc->add(dst, dst, src.size());
        pc->and_(dst, dst, -1 ^ int(alignPattern));

        if (sizeShift == 0) {
          pc->j(L_End, sub_z(i_ptr, dst));
        }
        else {
          pc->sub(i_ptr, i_ptr, dst);
          pc->j(L_End, shr_z(i_ptr, sizeShift));
        }
      }

      // MainLoop
      // --------

      {
        Label L_MainIter = pc->newLabel();
        Label L_MainSkip = pc->newLabel();

        pc->j(L_MainSkip, sub_c(i, mainStepInItems));

        pc->bind(L_MainIter);
        emitMemFillSequence(pc, mem_ptr(dst), src.v128(), mainLoopSize, AdvanceMode::kAdvance);
        pc->j(L_MainIter, sub_nc(i, mainStepInItems));

        pc->bind(L_MainSkip);
        pc->j(L_End, add_z(i, mainStepInItems));
      }

      // TailLoop / TailSequence
      // -----------------------

      if (mainLoopSize > vecSize * 2u) {
        Label L_TailIter = pc->newLabel();
        Label L_TailSkip = pc->newLabel();

        pc->j(L_TailSkip, sub_c(i, tailStepInItems));

        pc->bind(L_TailIter);
        pc->v_storeavec(mem_ptr(dst), src);
        pc->add(dst, dst, vecSize);
        pc->j(L_TailIter, sub_nc(i, tailStepInItems));

        pc->bind(L_TailSkip);
        pc->j(L_End, add_z(i, tailStepInItems));
      }
      else if (mainLoopSize >= vecSize * 2u) {
        pc->j(L_Finalize, ucmp_lt(i, tailStepInItems));

        pc->v_storeavec(mem_ptr(dst), src);
        pc->add(dst, dst, vecSize);
        pc->j(L_End, sub_z(i, tailStepInItems));
      }

      // Finalize
      // --------

      {
        Label L_Store1 = pc->newLabel();

        pc->bind(L_Finalize);
        pc->j(L_Store1, ucmp_lt(i, 8u / itemSize));

        pc->v_storeu64(mem_ptr(dst), src);
        pc->add(dst, dst, 8);
        pc->j(L_End, sub_z(i, 8u / itemSize));

        pc->bind(L_Store1);
        pc->v_storea32(mem_ptr(dst), src);
        pc->add(dst, dst, 4);
      }

      pc->bind(L_End);
    }

    return;
  }

  // Granularity == 1 Byte
  // ---------------------

  if (granularityInBytes == 1) {
    BL_ASSERT(itemSize == 1u);

    Label L_Finalize = pc->newLabel();
    Label L_End      = pc->newLabel();

    // Preparation / Alignment
    // -----------------------

    {
      Label L_Small = pc->newLabel();
      Label L_Large = pc->newLabel();
      Gp srcGp = pc->newGp32("srcGp");

      pc->j(L_Large, ucmp_gt(i, 15));
      pc->s_mov_u32(srcGp, src);

      pc->bind(L_Small);
      pc->store_u8(ptr(dst), srcGp);
      pc->inc(dst);
      pc->j(L_Small, sub_nz(i, 1));
      pc->j(L_End);

      pc->bind(L_Large);
      Gp i_ptr = i.cloneAs(dst);
      pc->add(i_ptr, i_ptr, dst);

      pc->v_storeu128(mem_ptr(dst), src);
      pc->add(dst, dst, 16);
      pc->and_(dst, dst, -16);

      pc->j(L_End, sub_z(i_ptr, dst));
    }

    // MainLoop
    // --------

    {
      Label L_MainIter = pc->newLabel();
      Label L_MainSkip = pc->newLabel();

      pc->j(L_MainSkip, sub_c(i, mainLoopSize));

      pc->bind(L_MainIter);
      for (k = 0; k < mainLoopSize; k += 16u)
        pc->v_storea128(mem_ptr(dst, int(k)), src);
      pc->add(dst, dst, mainLoopSize);
      pc->j(L_MainIter, sub_nc(i, mainLoopSize));

      pc->bind(L_MainSkip);
      pc->j(L_End, add_z(i, mainLoopSize));
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize > 32) {
      Label L_TailIter = pc->newLabel();
      Label L_TailSkip = pc->newLabel();

      pc->j(L_TailSkip, sub_c(i, 16));

      pc->bind(L_TailIter);
      pc->v_storea128(mem_ptr(dst), src);
      pc->add(dst, dst, 16);
      pc->j(L_TailIter, sub_nc(i, 16));

      pc->bind(L_TailSkip);
      pc->j(L_End, add_z(i, 16));
    }
    else if (mainLoopSize >= 32) {
      pc->j(L_Finalize, scmp_lt(i, 16));
      pc->v_storea128(mem_ptr(dst, int(k)), src);
      pc->add(dst, dst, 16);
      pc->j(L_End, sub_z(i, 16));
    }

    // Finalize
    // --------

    {
      pc->add(dst, dst, i.cloneAs(dst));
      pc->v_storeu128(mem_ptr(dst, -16), src);
    }

    pc->bind(L_End);
    return;
  }

  BL_NOT_REACHED();
}

// Inlines a whole FillRect loop that uses axis-aligned (AA) coordinates, taking advantage of the width of the
// rectangle. In many cases the rendering context has to deal with axis aligned rectangles of various sizes.
// When the width is large it pays off to align the destination pointer, however, when the size is relatively
// small, it doesn't matter whether the destination pointer is aligned or not, and aligning it explicitly can be
// a waste of cycles as all the instructions to align it can be much more expensive than few unaligned stores.
//
// Additionally, when filling a rectangle it pays off to specialize for various widths before we enter the
// scanline loop, because if we don't do that then each scanline iteration would require to do the same checks
// again and again. Based on testing if the number of bytes per iteration is less than 192 or 256 (this depends
// on target architecture and micro-architecture) then it pays off to specialize. Widths larger than 256 bytes
// don't need width specialization and generally benefit from aligning the destination pointer to native vector
// width.
void inlineFillRectLoop(
  PipeCompiler* pc,
  Gp dstPtr,
  Gp stride,
  Gp w,
  Gp h,
  Vec src,
  uint32_t itemSize, const Label& end) noexcept {

  Label L_End(end);
  Label L_Width_LE_256 = pc->newLabel();
  Label L_Width_LE_192 = pc->newLabel();
  Label L_Width_LE_160 = pc->newLabel();
  Label L_Width_LE_128 = pc->newLabel();
  Label L_Width_LE_96 = pc->newLabel();
  Label L_Width_LE_64 = pc->newLabel();
  Label L_Width_LE_32 = pc->newLabel();
  Label L_Width_LE_16 = pc->newLabel();
  Label L_Width_LT_8 = pc->newLabel();

  Label L_Width_LT_4; // Only used if necessary unit size is less than 4 bytes.
  Label L_Width_LT_2; // Only used if necessary unit size is less than 2 bytes.

  BL_ASSERT(IntOps::isPowerOf2(itemSize));
  uint32_t sizeShift = IntOps::ctz(itemSize);
  uint32_t sizeMask = itemSize - 1u;

  uint32_t storeAlignment = src.size();
  uint32_t storeAlignmentMask = storeAlignment - 1;

  if (!L_End.isValid())
    L_End = pc->newLabel();

  Gp endIndexA = pc->newGpPtr("endIndexA");
  Gp endIndexB = pc->newGpPtr("endIndexB");
  Gp src32b = pc->newGp32("src32");

  VecArray src256b;
  VecArray src512b;
  VecArray srcAlignSize;

#if defined(BL_JIT_ARCH_X86)
  if (src.isVec128()) {
    src256b.init(src, src);
    src512b.init(src, src, src, src);
  }
  else {
    src256b.init(src.v256());
    if (src.isVec256())
      src512b.init(src, src);
    else
      src512b.init(src);
  }
  src = src.v128();
#else
  src256b.init(src, src);
  src512b.init(src, src, src, src);
#endif

  if (storeAlignment <= 16)
    srcAlignSize.init(src);
  else if (storeAlignment <= 32)
    srcAlignSize = src256b;
  else
    srcAlignSize = src512b;

  pc->mul(endIndexA.r32(), w, itemSize);

  pc->j(L_Width_LE_32, ucmp_le(w, 32 >> sizeShift));
  pc->j(L_Width_LE_256, ucmp_le(w, 256 >> sizeShift));

  // Fill Rect - Width > 256 Bytes
  // -----------------------------

  {
    Label L_ScanlineLoop = pc->newLabel();
    Label L_ScanlineEnd = pc->newLabel();
    Label L_MainLoop = pc->newLabel();
    Label L_MainLoop4x = pc->newLabel();
    Label L_MainSkip4x = pc->newLabel();

    Gp dstAligned = pc->newGpPtr("dstAligned");
    Gp i = pc->newGpPtr("i");

    pc->bind(L_ScanlineLoop);
    pc->add(i, dstPtr, endIndexA);
    pc->add(dstAligned, dstPtr, storeAlignment);
    pc->v_storeuvec(mem_ptr(dstPtr), srcAlignSize);
    pc->and_(dstAligned, dstAligned, ~uint64_t(storeAlignmentMask ^ sizeMask));

    if (storeAlignment == 64)
      pc->v_storeuvec(mem_ptr(i, -64), src512b);
    else
      pc->v_storeuvec(mem_ptr(i, -32), src256b);

    pc->sub(i, i, dstAligned);
    pc->shr(i, i, storeAlignment == 64u ? 6 : 5);
    pc->j(L_MainSkip4x, sub_c(i.r32(), 4));

    pc->bind(L_MainLoop4x);
    if (storeAlignment == 64) {
      pc->v_storeuvec(mem_ptr(dstAligned), src512b);
      pc->v_storeuvec(mem_ptr(dstAligned, 64), src512b);
      pc->v_storeuvec(mem_ptr(dstAligned, 128), src512b);
      pc->v_storeuvec(mem_ptr(dstAligned, 192), src512b);
      pc->add(dstAligned, dstAligned, 256);
    }
    else {
      pc->v_storeuvec(mem_ptr(dstAligned), src512b);
      pc->v_storeuvec(mem_ptr(dstAligned, 64), src512b);
      pc->add(dstAligned, dstAligned, 128);
    }
    pc->j(L_MainLoop4x, sub_nc(i.r32(), 4));

    pc->bind(L_MainSkip4x);
    pc->j(L_ScanlineEnd, add_z(i.r32(), 4));

    pc->bind(L_MainLoop);
    if (storeAlignment == 64) {
      pc->v_storeuvec(mem_ptr(dstAligned), src512b);
      pc->add(dstAligned, dstAligned, 64);
    }
    else {
      pc->v_storeuvec(mem_ptr(dstAligned), src256b);
      pc->add(dstAligned, dstAligned, 32);
    }
    pc->j(L_MainLoop, sub_nz(i.r32(), 1));

    pc->bind(L_ScanlineEnd);
    pc->add(dstPtr, dstPtr, stride);
    pc->j(L_ScanlineLoop, sub_nz(h, 1));

    pc->j(L_End);
  }

  // Fill Rect - Width > 192 && Width <= 256 Bytes
  // ---------------------------------------------

  pc->bind(L_Width_LE_256);

  pc->sub(endIndexB, endIndexA, 32);
  pc->sub(endIndexA, endIndexA, 16);

  pc->j(L_Width_LE_128, ucmp_le(w, 128 >> sizeShift));
  pc->j(L_Width_LE_192, ucmp_le(w, 192 >> sizeShift));

  {
    Label L_ScanlineLoop = pc->newLabel();
    Gp dstEnd = pc->newGpPtr("dstEnd");

    pc->bind(L_ScanlineLoop);
    pc->v_storeuvec(mem_ptr(dstPtr), src512b);
    pc->add(dstEnd, dstPtr, endIndexB);
    pc->v_storeuvec(mem_ptr(dstPtr, 64), src512b);
    pc->v_storeuvec(mem_ptr(dstPtr, 128), src512b);
    pc->add(dstPtr, dstPtr, stride);
    pc->v_storeuvec(mem_ptr(dstEnd, -32), src512b);
    pc->j(L_ScanlineLoop, sub_nz(h, 1));

    pc->j(L_End);
  }

  // Fill Rect - Width > 160 && Width <= 192 Bytes
  // ---------------------------------------------

  // NOTE: This one was added as it seems that memory store pressure is bottlenecking
  // more than an additional branch, especially if the height is not super small.
  pc->bind(L_Width_LE_192);
  pc->j(L_Width_LE_160, ucmp_le(w, 160 >> sizeShift));

  {
    Label L_ScanlineLoop = pc->newLabel();
    Gp dstEnd = pc->newGpPtr("dstEnd");

    pc->bind(L_ScanlineLoop);
    pc->v_storeuvec(mem_ptr(dstPtr), src512b);
    pc->add(dstEnd, dstPtr, endIndexB);
    pc->v_storeuvec(mem_ptr(dstPtr, 64), src512b);
    pc->add(dstPtr, dstPtr, stride);
    pc->v_storeuvec(mem_ptr(dstEnd, -32), src512b);
    pc->j(L_ScanlineLoop, sub_nz(h, 1));

    pc->j(L_End);
  }

  // Fill Rect - Width > 128 && Width <= 160 Bytes
  // ---------------------------------------------

  // NOTE: This one was added as it seems that memory store pressure is bottlenecking
  // more than an additional branch, especially if the height is not super small.
  pc->bind(L_Width_LE_160);

  {
    Label L_ScanlineLoop = pc->newLabel();
    Gp dstEnd = pc->newGpPtr("dstEnd");

    pc->bind(L_ScanlineLoop);
    pc->v_storeuvec(mem_ptr(dstPtr), src512b);
    pc->add(dstEnd, dstPtr, endIndexB);
    pc->v_storeuvec(mem_ptr(dstPtr, 64), src512b);
    pc->add(dstPtr, dstPtr, stride);
    pc->v_storeuvec(mem_ptr(dstEnd), src256b);
    pc->j(L_ScanlineLoop, sub_nz(h, 1));

    pc->j(L_End);
  }

  // Fill Rect - Width > 96 && Width <= 128 Bytes
  // --------------------------------------------

  pc->bind(L_Width_LE_128);
  pc->j(L_Width_LE_64, ucmp_le(w, 64 >> sizeShift));
  pc->j(L_Width_LE_96, ucmp_le(w, 96 >> sizeShift));

  {
    Label L_ScanlineLoop2x = pc->newLabel();
    Gp dstAlt = pc->newGpPtr("dstAlt");
    Gp dstEnd = pc->newGpPtr("dstEnd");

    pc->j(L_ScanlineLoop2x, test_z(h, 0x1));
    pc->add(dstEnd, dstPtr, endIndexB);
    pc->v_storeuvec(mem_ptr(dstPtr,  0), src512b);
    pc->v_storeuvec(mem_ptr(dstPtr, 64), src256b);
    pc->add(dstPtr, dstPtr, stride);
    pc->v_storeuvec(mem_ptr(dstEnd    ), src256b);
    pc->j(L_End, sub_z(h, 1));

    pc->bind(L_ScanlineLoop2x);
    pc->add(dstAlt, dstPtr, stride);
    pc->v_storeuvec(mem_ptr(dstPtr,  0), src512b);
    pc->add(dstEnd, dstPtr, endIndexB);
    pc->v_storeuvec(mem_ptr(dstPtr, 64), src256b);
    pc->add_scaled(dstPtr, stride, 2);
    pc->v_storeuvec(mem_ptr(dstEnd    ), src256b);
    pc->add(dstEnd, dstAlt, endIndexB);
    pc->v_storeuvec(mem_ptr(dstAlt,  0), src512b);
    pc->v_storeuvec(mem_ptr(dstAlt, 64), src256b);
    pc->v_storeuvec(mem_ptr(dstEnd    ), src256b);
    pc->j(L_ScanlineLoop2x, sub_nz(h, 2));

    pc->j(L_End);
  }

  // Fill Rect - Width > 64 && Width <= 96 Bytes
  // --------------------------------------------

  pc->bind(L_Width_LE_96);

  {
    Label L_ScanlineLoop2x = pc->newLabel();
    Gp dstAlt = pc->newGpPtr("dstAlt");
    Gp dstEnd = pc->newGpPtr("dstEnd");

    pc->j(L_ScanlineLoop2x, test_z(h, 0x1));
    pc->add(dstEnd, dstPtr, endIndexB);
    pc->v_storeuvec(mem_ptr(dstPtr), src512b);
    pc->add(dstPtr, dstPtr, stride);
    pc->v_storeuvec(mem_ptr(dstEnd), src256b);
    pc->j(L_End, sub_z(h, 1));

    pc->bind(L_ScanlineLoop2x);
    pc->add(dstAlt, dstPtr, stride);
    pc->v_storeuvec(mem_ptr(dstPtr), src512b);
    pc->add(dstEnd, dstPtr, endIndexB);
    pc->add_scaled(dstPtr, stride, 2);
    pc->v_storeuvec(mem_ptr(dstAlt), src512b);
    pc->add(dstAlt, dstAlt, endIndexB);
    pc->v_storeuvec(mem_ptr(dstEnd), src256b);
    pc->v_storeuvec(mem_ptr(dstAlt), src256b);
    pc->j(L_ScanlineLoop2x, sub_nz(h, 2));

    pc->j(L_End);
  }

  // Fill Rect - Width > 32 && Width <= 64 Bytes
  // -------------------------------------------

  pc->bind(L_Width_LE_64);

  {
    Label L_ScanlineLoop2x = pc->newLabel();
    Gp dstAlt = pc->newGpPtr("dstAlt");

    pc->j(L_ScanlineLoop2x, test_z(h, 0x1));
    pc->v_storeu128(mem_ptr(dstPtr, endIndexA), src);
    pc->v_storeu128(mem_ptr(dstPtr, endIndexB), src);
    pc->v_storeuvec(mem_ptr(dstPtr), src256b);
    pc->add(dstPtr, dstPtr, stride);
    pc->j(L_End, sub_z(h, 1));

    pc->bind(L_ScanlineLoop2x);
    pc->add(dstAlt, dstPtr, stride);
    pc->v_storeu128(mem_ptr(dstPtr, endIndexA), src);
    pc->v_storeu128(mem_ptr(dstPtr, endIndexB), src);
    pc->v_storeuvec(mem_ptr(dstPtr), src256b);
    pc->add(dstPtr, dstAlt, stride);
    pc->v_storeu128(mem_ptr(dstAlt, endIndexA), src);
    pc->v_storeu128(mem_ptr(dstAlt, endIndexB), src);
    pc->v_storeuvec(mem_ptr(dstAlt), src256b);
    pc->j(L_ScanlineLoop2x, sub_nz(h, 2));

    pc->j(L_End);
  }

  // Fill Rect - Width > 16 && Width <= 32 Bytes
  // -------------------------------------------

  pc->bind(L_Width_LE_32);
  pc->j(L_Width_LE_16, ucmp_le(w, 16 >> sizeShift));

  {
    Label L_ScanlineLoop2x = pc->newLabel();
    Gp dstAlt = pc->newGpPtr("dstAlt");

    pc->sub(endIndexA, endIndexA, 16);

    pc->j(L_ScanlineLoop2x, test_z(h, 0x1));
    pc->v_storeu128(mem_ptr(dstPtr, endIndexA), src);
    pc->v_storeu128(mem_ptr(dstPtr), src);
    pc->add(dstPtr, dstPtr, stride);
    pc->j(L_End, sub_z(h, 1));

    pc->bind(L_ScanlineLoop2x);
    pc->add(dstAlt, dstPtr, stride);
    pc->v_storeu128(mem_ptr(dstPtr), src);
    pc->v_storeu128(mem_ptr(dstPtr, endIndexA), src);
    pc->add(dstPtr, dstAlt, stride);
    pc->v_storeu128(mem_ptr(dstAlt), src);
    pc->v_storeu128(mem_ptr(dstAlt, endIndexA), src);
    pc->j(L_ScanlineLoop2x, sub_nz(h, 2));

    pc->j(L_End);
  }

  // Fill Rect - Width >= 8 && Width <= 16 Bytes
  // ------------------------------------------

  pc->bind(L_Width_LE_16);
  pc->j(L_Width_LT_8, ucmp_lt(w, 8 >> sizeShift));

  {
    Label L_ScanlineLoop2x = pc->newLabel();
    Gp dstAlt = pc->newGpPtr("dstAlt");

    pc->sub(endIndexA, endIndexA, 8);

    pc->j(L_ScanlineLoop2x, test_z(h, 0x1));
    pc->v_storeu64(mem_ptr(dstPtr, endIndexA), src);
    pc->v_storeu64(mem_ptr(dstPtr), src);
    pc->add(dstPtr, dstPtr, stride);
    pc->j(L_End, sub_z(h, 1));

    pc->bind(L_ScanlineLoop2x);
    pc->add(dstAlt, dstPtr, stride);
    pc->v_storeu64(mem_ptr(dstPtr, endIndexA), src);
    pc->v_storeu64(mem_ptr(dstPtr), src);
    pc->add(dstPtr, dstAlt, stride);
    pc->v_storeu64(mem_ptr(dstAlt, endIndexA), src);
    pc->v_storeu64(mem_ptr(dstAlt), src);
    pc->j(L_ScanlineLoop2x, sub_nz(h, 2));

    pc->j(L_End);
  }

  // Fill Rect - Width < 8 Bytes
  // ---------------------------

  if (itemSize <= 4) {
    BL_ASSERT(L_Width_LT_8.isValid());

    pc->bind(L_Width_LT_8);
    pc->s_mov_u32(src32b, src);

    if (itemSize == 4) {
      // We know that if the unit size is 4 bytes or more it's only one item at a time.
      Label L_ScanlineLoop2x = pc->newLabel();

      pc->j(L_ScanlineLoop2x, test_z(h, 0x1));
      pc->store_u32(mem_ptr(dstPtr), src32b);
      pc->add(dstPtr, dstPtr, stride);
      pc->j(L_End, sub_z(h, 1));

      pc->bind(L_ScanlineLoop2x);
      pc->store_u32(mem_ptr(dstPtr), src32b);
      pc->store_u32(mem_ptr(dstPtr, stride), src32b);
      pc->add_ext(dstPtr, dstPtr, stride, 2);
      pc->j(L_ScanlineLoop2x, sub_nz(h, 2));

      pc->j(L_End);
    }
    else {
      // Fill Rect - Width >= 4 && Width < 8 Bytes
      Label L_ScanlineLoop2x = pc->newLabel();
      L_Width_LT_4 = pc->newLabel();

      Gp dstAlt = pc->newGpPtr("dstAlt");

      pc->j(L_Width_LT_4, ucmp_lt(w, 4 >> sizeShift));
      pc->sub(endIndexA, endIndexA, 4);

      pc->j(L_ScanlineLoop2x, test_z(h, 0x1));
      pc->store_u32(mem_ptr(dstPtr), src32b);
      pc->store_u32(mem_ptr(dstPtr, endIndexA), src32b);
      pc->add(dstPtr, dstPtr, stride);
      pc->j(L_End, sub_z(h, 1));

      pc->bind(L_ScanlineLoop2x);
      pc->add(dstAlt, dstPtr, stride);
      pc->store_u32(mem_ptr(dstPtr, endIndexA), src32b);
      pc->store_u32(mem_ptr(dstPtr), src32b);
      pc->add(dstPtr, dstAlt, stride);
      pc->store_u32(mem_ptr(dstAlt, endIndexA), src32b);
      pc->store_u32(mem_ptr(dstAlt), src32b);
      pc->j(L_ScanlineLoop2x, sub_nz(h, 2));

      pc->j(L_End);
    }
  }

  // Fill Rect - Width < 4 Bytes
  // ---------------------------

  if (itemSize <= 2) {
    BL_ASSERT(L_Width_LT_4.isValid());

    pc->bind(L_Width_LT_4);

    if (itemSize == 2) {
      // We know that if the unit size is 2 bytes or more it's only one item at a time.
      Label L_ScanlineLoop2x = pc->newLabel();

      pc->j(L_ScanlineLoop2x, test_z(h, 0x1));
      pc->store_u16(mem_ptr(dstPtr), src32b);
      pc->add(dstPtr, dstPtr, stride);
      pc->j(L_End, sub_z(h, 1));

      pc->bind(L_ScanlineLoop2x);
      pc->store_u16(mem_ptr(dstPtr), src32b);
      pc->store_u16(mem_ptr(dstPtr, stride), src32b);
      pc->add_ext(dstPtr, dstPtr, stride, 2);
      pc->j(L_ScanlineLoop2x, sub_nz(h, 2));

      pc->j(L_End);
    }
    else {
      // Fill Rect - Width >= 2 && Width < 4 Bytes
      Label L_ScanlineLoop2x = pc->newLabel();
      L_Width_LT_2 = pc->newLabel();

      Gp dstAlt = pc->newGpPtr("dstAlt");

      pc->j(L_Width_LT_2, ucmp_lt(w, 2));
      pc->sub(endIndexA, endIndexA, 2);

      pc->j(L_ScanlineLoop2x, test_z(h, 0x1));
      pc->store_u16(mem_ptr(dstPtr), src32b);
      pc->store_u16(mem_ptr(dstPtr, endIndexA), src32b);
      pc->add(dstPtr, dstPtr, stride);
      pc->j(L_End, sub_z(h, 1));

      pc->bind(L_ScanlineLoop2x);
      pc->add(dstAlt, dstPtr, stride);
      pc->store_u16(mem_ptr(dstPtr, endIndexA), src32b);
      pc->store_u16(mem_ptr(dstPtr), src32b);
      pc->add(dstPtr, dstAlt, stride);
      pc->store_u16(mem_ptr(dstAlt, endIndexA), src32b);
      pc->store_u16(mem_ptr(dstAlt), src32b);
      pc->j(L_ScanlineLoop2x, sub_nz(h, 2));

      pc->j(L_End);
    }
  }

  // Fill Rect - Width < 2 Bytes
  // ---------------------------

  if (itemSize == 1) {
    BL_ASSERT(L_Width_LT_2.isValid());

    Label L_ScanlineLoop2x = pc->newLabel();

    pc->bind(L_Width_LT_2);
    pc->j(L_ScanlineLoop2x, test_z(h, 0x1));
    pc->store_u8(mem_ptr(dstPtr), src32b);
    pc->add(dstPtr, dstPtr, stride);
    pc->j(L_End, sub_z(h, 1));

    pc->bind(L_ScanlineLoop2x);
    pc->store_u8(mem_ptr(dstPtr), src32b);
    pc->store_u8(mem_ptr(dstPtr, stride), src32b);
    pc->add_ext(dstPtr, dstPtr, stride, 2);
    pc->j(L_ScanlineLoop2x, sub_nz(h, 2));

    pc->j(L_End);
  }

  // Fill Rect - End
  // ---------------

  if (end.isValid())
    pc->j(end);
  else
    pc->bind(L_End);
}

// bl::Pipeline::JIT::FetchUtils - CopySpan & CopyRect Loops
// =========================================================

static BL_NOINLINE void emitMemCopySequence(
  PipeCompiler* pc,
  Mem dPtr, bool dstAligned,
  Mem sPtr, bool srcAligned, uint32_t numBytes, const Vec& fillMask, AdvanceMode advanceMode) noexcept {

#if defined(BL_JIT_ARCH_X86)
  AsmCompiler* cc = pc->cc;

  VecArray t;

  uint32_t n = numBytes / 16;
  uint32_t limit = 2;
  pc->newV128Array(t, blMin(n, limit), "t");

  uint32_t fetchInst = pc->hasAVX() ? x86::Inst::kIdVmovaps : x86::Inst::kIdMovaps;
  uint32_t storeInst = pc->hasAVX() ? x86::Inst::kIdVmovaps : x86::Inst::kIdMovaps;

  if (!srcAligned) fetchInst = pc->hasAVX() ? x86::Inst::kIdVmovups : x86::Inst::kIdMovups;
  if (!dstAligned) storeInst = pc->hasAVX() ? x86::Inst::kIdVmovups : x86::Inst::kIdMovups;

  do {
    uint32_t i;
    uint32_t count = blMin<uint32_t>(n, limit);

    if (pc->hasAVX() && fillMask.isValid()) {
      // Shortest code for this use case. AVX allows to read from unaligned
      // memory, so if we use VEC instructions we are generally safe here.
      for (i = 0; i < count; i++) {
        pc->v_or_i32(t[i], fillMask, sPtr);
        sPtr.addOffsetLo32(16);
      }

      for (i = 0; i < count; i++) {
        cc->emit(storeInst, dPtr, t[i]);
        dPtr.addOffsetLo32(16);
      }
    }
    else {
      for (i = 0; i < count; i++) {
        cc->emit(fetchInst, t[i], sPtr);
        sPtr.addOffsetLo32(16);
      }

      for (i = 0; i < count; i++)
        if (fillMask.isValid())
          pc->v_or_i32(t[i], t[i], fillMask);

      for (i = 0; i < count; i++) {
        cc->emit(storeInst, dPtr, t[i]);
        dPtr.addOffsetLo32(16);
      }
    }

    n -= count;
  } while (n > 0);

  if (advanceMode == AdvanceMode::kAdvance) {
    Gp sPtrBase = sPtr.baseReg().as<Gp>();
    Gp dPtrBase = dPtr.baseReg().as<Gp>();

    pc->add(sPtrBase, sPtrBase, numBytes);
    pc->add(dPtrBase, dPtrBase, numBytes);
  }
#elif defined(BL_JIT_ARCH_A64)
  blUnused(dstAligned, srcAligned);

  AsmCompiler* cc = pc->cc;
  uint32_t n = numBytes;

  VecArray t;
  pc->newV128Array(t, blMin<uint32_t>((n + 15u) / 16u, 4u), "t");

  bool postIndex = (advanceMode == AdvanceMode::kAdvance) && !dPtr.hasOffset() && !sPtr.hasOffset();
  if (postIndex) {
    dPtr.setOffsetMode(a64::OffsetMode::kPostIndex);
    sPtr.setOffsetMode(a64::OffsetMode::kPostIndex);
  }

  while (n >= 32u) {
    uint32_t vecCount = blMin<uint32_t>(n / 32u, 2u) * 2u;

    if (postIndex) {
      // Always emit a pair of ldp/stp if we are using post-index as this seems to be
      // faster on many CPUs (the dependency of post-indexing is hidden in this case).
      vecCount = 2;
      dPtr.setOffsetLo32(32);
      sPtr.setOffsetLo32(32);
    }

    for (uint32_t i = 0; i < vecCount; i += 2) {
      cc->ldp(t[i + 0], t[i + 1], sPtr);
      if (!postIndex)
        sPtr.addOffsetLo32(32);
    }

    if (fillMask.isValid()) {
      for (uint32_t i = 0; i < vecCount; i++)
        pc->v_or_i32(t[i], t[i], fillMask);
    }

    for (uint32_t i = 0; i < vecCount; i += 2) {
      cc->stp(t[i + 0], t[i + 1], dPtr);
      if (!postIndex)
        dPtr.addOffsetLo32(32);
    }

    n -= vecCount * 16u;
  }

  for (uint32_t count = 16; count != 0; count >>= 1) {
    if (n >= count) {
      Vec v = t[0];

      if (postIndex) {
        dPtr.setOffsetLo32(int32_t(count));
        sPtr.setOffsetLo32(int32_t(count));
      }

      pc->v_load_iany(v, sPtr, count, Alignment{1});
      if (!postIndex)
        sPtr.addOffsetLo32(count);

      if (fillMask.isValid())
        pc->v_or_i32(t[0], t[0], fillMask);

      pc->v_store_iany(dPtr, v, count, Alignment{1});
      if (!postIndex)
        dPtr.addOffsetLo32(count);

      n -= count;
    }
  }

  // In case that any of the two pointers had an offset, we have to advance here...
  if (advanceMode == AdvanceMode::kAdvance && !postIndex) {
    Gp sPtrBase = sPtr.baseReg().as<Gp>();
    Gp dPtrBase = dPtr.baseReg().as<Gp>();

    pc->add(sPtrBase, sPtrBase, numBytes);
    pc->add(dPtrBase, dPtrBase, numBytes);
  }
#else
  #error "Unknown architecture"
#endif
}

void inlineCopySpanLoop(
  PipeCompiler* pc,
  Gp dst,
  Gp src,
  Gp i,
  uint32_t mainLoopSize, uint32_t itemSize, uint32_t itemGranularity, FormatExt format) noexcept {

  BL_ASSERT(IntOps::isPowerOf2(itemSize));
  BL_ASSERT(itemSize <= 16u);

  uint32_t granularityInBytes = itemSize * itemGranularity;
  uint32_t mainStepInItems = mainLoopSize / itemSize;

  BL_ASSERT(IntOps::isPowerOf2(granularityInBytes));
  BL_ASSERT(mainStepInItems * itemSize == mainLoopSize);

  BL_ASSERT(mainLoopSize >= 16u);
  BL_ASSERT(mainLoopSize >= granularityInBytes);

  Vec t0 = pc->newV128("t0");
  Vec fillMask;

  if (format == FormatExt::kXRGB32)
    fillMask = pc->simdVecConst(&commonTable.i_FF000000FF000000, Bcst::k64, t0);

  // Granularity >= 16 Bytes
  // -----------------------

  if (granularityInBytes >= 16u) {
    Label L_End = pc->newLabel();

    // MainLoop
    // --------

    {
      Label L_MainIter = pc->newLabel();
      Label L_MainSkip = pc->newLabel();

      pc->j(L_MainSkip, sub_c(i, mainStepInItems));

      pc->bind(L_MainIter);
      emitMemCopySequence(pc, mem_ptr(dst), false, mem_ptr(src), false, mainLoopSize, fillMask, AdvanceMode::kAdvance);
      pc->j(L_MainIter, sub_nc(i, mainStepInItems));

      pc->bind(L_MainSkip);
      pc->j(L_End, add_z(i, mainStepInItems));
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize * 2 > granularityInBytes) {
      Label L_TailIter = pc->newLabel();

      pc->bind(L_TailIter);
      emitMemCopySequence(pc, mem_ptr(dst), false, mem_ptr(src), false, granularityInBytes, fillMask, AdvanceMode::kAdvance);
      pc->j(L_TailIter, sub_nz(i, itemGranularity));
    }
    else if (mainLoopSize * 2 == granularityInBytes) {
      emitMemCopySequence(pc, mem_ptr(dst), false, mem_ptr(src), false, granularityInBytes, fillMask, AdvanceMode::kAdvance);
    }

    pc->bind(L_End);
    return;
  }

  // Granularity == 4 Bytes
  // ----------------------

  if (granularityInBytes == 4u) {
    BL_ASSERT(itemSize <= 4u);
    uint32_t sizeShift = IntOps::ctz(itemSize);
    uint32_t alignPattern = (15u * itemSize) & 15u;

    uint32_t oneStepInItems = 4u >> sizeShift;
    uint32_t tailStepInItems = 16u >> sizeShift;

    Label L_Finalize = pc->newLabel();
    Label L_End      = pc->newLabel();

    // Preparation / Alignment
    // -----------------------

    {
      pc->j(L_Finalize, ucmp_lt(i, oneStepInItems * 4u));

      Gp i_ptr = i.cloneAs(dst);
      pc->v_loadu128(t0, mem_ptr(src));
      if (sizeShift)
        pc->shl(i_ptr, i_ptr, sizeShift);

      pc->add(i_ptr, i_ptr, dst);
      pc->sub(src, src, dst);
      pc->v_storeu128(mem_ptr(dst), t0);
      pc->add(dst, dst, 16);
      pc->and_(dst, dst, -1 ^ int(alignPattern));
      pc->add(src, src, dst);

      if (sizeShift == 0) {
        pc->j(L_End, sub_z(i_ptr, dst));
      }
      else {
        pc->sub(i_ptr, i_ptr, dst);
        pc->j(L_End, shr_z(i_ptr, sizeShift));
      }
    }

    // MainLoop
    // --------

    {
      Label L_MainIter = pc->newLabel();
      Label L_MainSkip = pc->newLabel();

      pc->j(L_MainSkip, sub_c(i, mainStepInItems));

      pc->bind(L_MainIter);
      emitMemCopySequence(pc, mem_ptr(dst), true, mem_ptr(src), false, mainLoopSize, fillMask, AdvanceMode::kAdvance);
      pc->j(L_MainIter, sub_nc(i, mainStepInItems));

      pc->bind(L_MainSkip);
      pc->j(L_End, add_z(i, mainStepInItems));
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize > 32) {
      Label L_TailIter = pc->newLabel();
      Label L_TailSkip = pc->newLabel();

      pc->j(L_TailSkip, sub_c(i, tailStepInItems));

      pc->bind(L_TailIter);
      emitMemCopySequence(pc, mem_ptr(dst), true, mem_ptr(src), false, 16, fillMask, AdvanceMode::kAdvance);
      pc->j(L_TailIter, sub_nc(i, tailStepInItems));

      pc->bind(L_TailSkip);
      pc->j(L_End, add_z(i, tailStepInItems));
    }
    else if (mainLoopSize >= 32) {
      pc->j(L_Finalize, ucmp_lt(i, tailStepInItems));

      emitMemCopySequence(pc, mem_ptr(dst), true, mem_ptr(src), false, 16, fillMask, AdvanceMode::kAdvance);
      pc->j(L_End, sub_z(i, tailStepInItems));
    }

    // Finalize
    // --------

    {
      Label L_Store1 = pc->newLabel();

      pc->bind(L_Finalize);
      pc->j(L_Store1, ucmp_lt(i, 8u / itemSize));

      pc->v_loadu64(t0, mem_ptr(src));
      pc->add(src, src, 8);
      pc->v_storeu64(mem_ptr(dst), t0);
      pc->add(dst, dst, 8);
      pc->j(L_End, sub_z(i, 8u / itemSize));

      pc->bind(L_Store1);
      pc->v_loada32(t0, mem_ptr(src));
      pc->add(src, src, 4);
      pc->v_storea32(mem_ptr(dst), t0);
      pc->add(dst, dst, 4);
    }

    pc->bind(L_End);
    return;
  }

  // Granularity == 1 Byte
  // ---------------------

  if (granularityInBytes == 1) {
    BL_ASSERT(itemSize == 1u);

    Label L_Finalize = pc->newLabel();
    Label L_End      = pc->newLabel();

    // Preparation / Alignment
    // -----------------------

    {
      Label L_Small = pc->newLabel();
      Label L_Large = pc->newLabel();

      Gp i_ptr = i.cloneAs(dst);
      Gp byte_val = pc->newGp32("@byte_val");

      pc->j(L_Large, ucmp_gt(i, 15));

      pc->bind(L_Small);
      pc->load_u8(byte_val, ptr(src));
      pc->inc(src);
      pc->store_u8(ptr(dst), byte_val);
      pc->inc(dst);
      pc->j(L_Small, sub_nz(i, 1));
      pc->j(L_End);

      pc->bind(L_Large);
      pc->v_loadu128(t0, mem_ptr(src));
      pc->add(i_ptr, i_ptr, dst);
      pc->sub(src, src, dst);

      pc->v_storeu128(mem_ptr(dst), t0);
      pc->add(dst, dst, 16);
      pc->and_(dst, dst, -16);

      pc->add(src, src, dst);
      pc->j(L_End, sub_z(i_ptr, dst));
    }

    // MainLoop
    // --------

    {
      Label L_MainIter = pc->newLabel();
      Label L_MainSkip = pc->newLabel();

      pc->j(L_MainSkip, sub_c(i, mainLoopSize));

      pc->bind(L_MainIter);
      emitMemCopySequence(pc, mem_ptr(dst), true, mem_ptr(src), false, mainLoopSize, fillMask, AdvanceMode::kAdvance);
      pc->j(L_MainIter, sub_nc(i, mainLoopSize));

      pc->bind(L_MainSkip);
      pc->j(L_End, add_z(i, mainLoopSize));
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize > 32) {
      Label L_TailIter = pc->newLabel();
      Label L_TailSkip = pc->newLabel();

      pc->j(L_TailSkip, sub_c(i, 16));

      pc->bind(L_TailIter);
      emitMemCopySequence(pc, mem_ptr(dst), true, mem_ptr(src), false, 16, fillMask, AdvanceMode::kAdvance);
      pc->j(L_TailIter, sub_nc(i, 16));

      pc->bind(L_TailSkip);
      pc->j(L_End, add_z(i, 16));
    }
    else if (mainLoopSize >= 32) {
      pc->j(L_Finalize, ucmp_lt(i, 16));

      emitMemCopySequence(pc, mem_ptr(dst), true, mem_ptr(src), false, 16, fillMask, AdvanceMode::kAdvance);
      pc->j(L_End, sub_z(i, 16));
    }

    // Finalize
    // --------

    {
      pc->add(src, src, i.cloneAs(src));
      pc->add(dst, dst, i.cloneAs(dst));
      emitMemCopySequence(pc, mem_ptr(dst, -16), false, mem_ptr(src, -16), false, 16, fillMask, AdvanceMode::kNoAdvance);
    }

    pc->bind(L_End);
    return;
  }
}

} // {FetchUtils}
} // {JIT}
} // {Pipeline}
} // {bl}

#endif // !BL_BUILD_NO_JIT
