// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../support/arenaallocator_p.h"
#include "../support/intops_p.h"

// ArenaAllocator - API
// ====================

// Zero size block used by `BLArenaAllocator` that doesn't have any memory allocated.
// Should be allocated in read-only memory and should never be modified.
const BLArenaAllocator::ZeroBlock BLArenaAllocator::_zeroBlock = { { 0 }, { nullptr, nullptr, 0 } };

void BLArenaAllocator::_init(size_t blockSize, size_t blockAlignment, void* staticData, size_t staticSize) noexcept {
  BL_ASSERT(blockSize >= kMinBlockSize);
  BL_ASSERT(blockSize <= kMaxBlockSize);
  BL_ASSERT(blockAlignment <= 64);

  _assignZeroBlock();
  _blockSize = blockSize & BLIntOps::nonZeroLsbMask<size_t>(BLIntOps::bitSizeOf<size_t>() - 4);
  _hasStaticBlock = staticData != nullptr;
  _blockAlignmentShift = BLIntOps::ctz(blockAlignment) & 0x7;

  // Setup the first [temporary] block, if necessary.
  if (staticData) {
    Block* block = static_cast<Block*>(staticData);
    block->prev = nullptr;
    block->next = nullptr;

    BL_ASSERT(staticSize >= kBlockSize);
    block->size = staticSize - kBlockSize;

    _assignBlock(block);
  }
}

void BLArenaAllocator::reset() noexcept {
  // Can't be altered.
  Block* cur = _block;
  if (cur == &_zeroBlock.block)
    return;

  Block* initial = const_cast<BLArenaAllocator::Block*>(&_zeroBlock.block);
  _ptr = initial->data();
  _end = initial->data();
  _block = initial;

  // Since cur can be in the middle of the double-linked list, we have to traverse both directions separately.
  Block* next = cur->next;
  do {
    Block* prev = cur->prev;

    // If this is the first block and this AllocatorTmp is temporary then the first block is statically allocated.
    // We cannot free it and it makes sense to keep it even when this is hard reset.
    if (prev == nullptr && _hasStaticBlock) {
      cur->prev = nullptr;
      cur->next = nullptr;
      _assignBlock(cur);
      break;
    }

    free(cur);
    cur = prev;
  } while (cur);

  cur = next;
  while (cur) {
    next = cur->next;
    free(cur);
    cur = next;
  }
}

void* BLArenaAllocator::_alloc(size_t size, size_t alignment) noexcept {
  Block* curBlock = _block;
  Block* next = curBlock->next;

  size_t rawBlockAlignment = blockAlignment();
  size_t minimumAlignment = blMax<size_t>(alignment, rawBlockAlignment);

  // If the `BLArenaAllocator` has been cleared the current block doesn't have to be the last one. Check if there is
  // a block that can be used instead of allocating a new one. If there is a `next` block it's completely unused,
  // we don't have to check for remaining bytes in that case.
  if (next) {
    uint8_t* ptr = BLIntOps::alignUp(next->data(), minimumAlignment);
    uint8_t* end = next->data() + next->size;

    if (size <= (size_t)(end - ptr)) {
      _block = next;
      _ptr = ptr + size;
      _end = next->data() + next->size;
      return static_cast<void*>(ptr);
    }
  }

  // Prevent arithmetic overflow.
  size_t newSize = blMax(blockSize(), size);
  if (BL_UNLIKELY(newSize > SIZE_MAX - kBlockSize - kMaxAlignment))
    return nullptr;

  // Allocate new block - we add alignment overhead to `newSize`, which becomes the new block size, and we also add
  // `kBlockOverhead` to the allocator as it includes members of `BLArenaAllocator::Block` structure.
  newSize += kMaxAlignment;
  Block* newBlock = static_cast<Block*>(malloc(newSize + kBlockSize));

  if (BL_UNLIKELY(!newBlock))
    return nullptr;

  // Adjust newSize so the end of the block is aligned to maximum alignment. We will lose some bytes at the end of
  // the block, but we will never go beyond the end of the block on alignment allocation request.
  //
  // NOTE: There was a bug in the past regarding this, do not remove this code.
  newSize = (size_t)(BLIntOps::alignDown(newBlock->data() + newSize, kMaxAlignment) - newBlock->data());

  // Align the pointer to `minimumAlignment` and adjust the size of this block accordingly. It's the same as using
  // `minimumAlignment - BLIntOps::alignUpDiff()`, just written differently.
  newBlock->prev = nullptr;
  newBlock->next = nullptr;
  newBlock->size = newSize;

  if (curBlock != &_zeroBlock.block) {
    newBlock->prev = curBlock;
    curBlock->next = newBlock;

    // Does only happen if there is a next block, but the requested memory can't fit into it. In this case a new
    // buffer is allocated and inserted between the current block and the next one.
    if (next) {
      newBlock->next = next;
      next->prev = newBlock;
    }
  }

  uint8_t* ptr = BLIntOps::alignUp(newBlock->data(), minimumAlignment);
  uint8_t* end = newBlock->data() + newSize;

  _ptr = ptr + size;
  _end = end;
  _block = newBlock;

  BL_ASSERT(_ptr <= _end);
  return static_cast<void*>(ptr);
}

void* BLArenaAllocator::allocZeroed(size_t size, size_t alignment) noexcept {
  void* p = alloc(size, alignment);
  if (BL_UNLIKELY(!p))
    return p;
  return memset(p, 0, size);
}
