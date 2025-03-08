// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../support/arenaallocator_p.h"
#include "../support/intops_p.h"

namespace bl {

// bl::ArenaAllocator - API
// ========================

//! Zero block, used by a default constructed `ArenaAllocator`, which doesn't hold any allocated block. This block
//! must be properly aligned so when arena allocator aligns its current pointer to check for aligned allocation it
//! would not overflow past the end of the block - which is the same as the beginning of the block as it has no size.
struct alignas(64) ArenaAllocatorZeroBlock {
  uint8_t padding[64 - sizeof(ArenaAllocator::Block)];
  ArenaAllocator::Block block;
};

// Zero size block used by `ArenaAllocator` that doesn't have any memory allocated.
// Should be allocated in read-only memory and should never be modified.
static const ArenaAllocatorZeroBlock kArenaAllocatorZeroBlock = { { 0 }, { nullptr, nullptr, 0 } };

static BL_INLINE void ArenaAllocator_assignZeroBlock(ArenaAllocator* self) noexcept {
  ArenaAllocator::Block* block = const_cast<ArenaAllocator::Block*>(&kArenaAllocatorZeroBlock.block);
  self->_ptr = block->data();
  self->_end = block->data();
  self->_block = block;
}

static BL_INLINE void ArenaAllocator_assignBlock(ArenaAllocator* self, ArenaAllocator::Block* block) noexcept {
  size_t alignment = self->blockAlignment();
  self->_ptr = IntOps::alignUp(block->data(), alignment);
  self->_end = block->data() + block->size;
  self->_block = block;
}

void ArenaAllocator::_init(size_t blockSize, size_t blockAlignment, void* staticData, size_t staticSize) noexcept {
  BL_ASSERT(blockSize >= kMinBlockSize);
  BL_ASSERT(blockSize <= kMaxBlockSize);
  BL_ASSERT(blockAlignment <= 64);

  ArenaAllocator_assignZeroBlock(this);

  size_t blockSizeShift = IntOps::bitSizeOf<size_t>() - IntOps::clz(blockSize);
  size_t blockAlignmentShift = IntOps::bitSizeOf<size_t>() - IntOps::clz(blockAlignment | (size_t(1) << 3));

  _blockAlignmentShift = uint8_t(blockAlignmentShift);
  _minimumBlockSizeShift = uint8_t(blockSizeShift);
  _maximumBlockSizeShift = uint8_t(25); // (1 << 25) Equals 32 MiB blocks (should be enough for all cases)
  _hasStaticBlock = uint8_t(staticData != nullptr);
  _reserved = uint8_t(0u);
  _blockCount = size_t(staticData != nullptr);

  // Setup the first [temporary] block, if necessary.
  if (staticData) {
    Block* block = static_cast<Block*>(staticData);
    block->prev = nullptr;
    block->next = nullptr;

    BL_ASSERT(staticSize >= kBlockSize);
    block->size = staticSize - kBlockSize;

    ArenaAllocator_assignBlock(this, block);
    _blockCount = 1u;
  }
}

void ArenaAllocator::reset() noexcept {
  // Can't be altered.
  Block* cur = _block;
  if (cur == &kArenaAllocatorZeroBlock.block)
    return;

  Block* initial = const_cast<ArenaAllocator::Block*>(&kArenaAllocatorZeroBlock.block);
  _ptr = initial->data();
  _end = initial->data();
  _block = initial;
  _blockCount = 0u;

  // Since cur can be in the middle of the double-linked list, we have to traverse both directions separately.
  Block* next = cur->next;
  do {
    Block* prev = cur->prev;

    // If this is the first block and this AllocatorTmp is temporary then the first block is statically allocated.
    // We cannot free it and it makes sense to keep it even when this is hard reset.
    if (prev == nullptr && _hasStaticBlock) {
      cur->prev = nullptr;
      cur->next = nullptr;
      ArenaAllocator_assignBlock(this, cur);
      break;
    }

    ::free(cur);
    cur = prev;
  } while (cur);

  cur = next;
  while (cur) {
    next = cur->next;
    ::free(cur);
    cur = next;
  }
}

void ArenaAllocator::clear() noexcept {
  Block* cur = _block;
  while (cur->prev) {
    cur = cur->prev;
  }
  ArenaAllocator_assignBlock(this, cur);
}

void* ArenaAllocator::_alloc(size_t size, size_t alignment) noexcept {
  Block* curBlock = _block;
  Block* next = curBlock->next;

  size_t defaultBlockAlignment = blockAlignment();
  size_t requiredBlockAlignment = blMax<size_t>(alignment, defaultBlockAlignment);

  // If the `Zone` has been cleared the current block doesn't have to be the last one. Check if there is a block
  // that can be used instead of allocating a new one. If there is a `next` block it's completely unused, we don't
  // have to check for remaining bytes in that case.
  if (next) {
    uint8_t* ptr = IntOps::alignUp(next->data(), requiredBlockAlignment);
    uint8_t* end = next->data() + next->size;

    if (size <= (size_t)(end - ptr)) {
      _block = next;
      _ptr = ptr + size;
      _end = end;
      return static_cast<void*>(ptr);
    }
  }

  // Calculates the "default" size of a next block - in most cases this would be enough for the allocation. In
  // general we want to gradually increase block size when more and more blocks are allocated until the maximum
  // block size. Since we use shifts (aka log2(size) sizes) we just need block count and minumum/maximum block
  // size shift to calculate the final size.
  size_t defaultBlockSizeShift = blMin<size_t>(_blockCount + _minimumBlockSizeShift, _maximumBlockSizeShift);
  size_t defaultBlockSize = size_t(1) << defaultBlockSizeShift;

  // Allocate a new block. We have to accommodate all possible overheads so after the memory is allocated and then
  // properly aligned there will be size for the requested memory. In 99.9999% cases this is never a problem, but
  // we must be sure that even rare border cases would allocate properly.
  size_t alignmentOverhead = requiredBlockAlignment - blMin<size_t>(requiredBlockAlignment, BL_ALLOC_ALIGNMENT);
  size_t blockSizeOverhead = kBlockSize + BL_ALLOC_OVERHEAD + alignmentOverhead;

  // If the requested size is larger than a default calculated block size -> increase block size so the allocation
  // would be enough to fit the requested size.
  size_t finalBlockSize = defaultBlockSize;

  if (BL_UNLIKELY(size > defaultBlockSize - blockSizeOverhead)) {
    if (BL_UNLIKELY(size > SIZE_MAX - blockSizeOverhead)) {
      // This would probably never happen in practice - however, it needs to be done to stop malicious cases like
      // `alloc(SIZE_MAX)`.
      return nullptr;
    }
    finalBlockSize = size + alignmentOverhead + kBlockSize;
  }
  else {
    finalBlockSize -= BL_ALLOC_OVERHEAD;
  }

  // Allocate new block.
  Block* newBlock = static_cast<Block*>(::malloc(finalBlockSize));

  if (BL_UNLIKELY(!newBlock)) {
    return nullptr;
  }

  // finalBlockSize includes the struct size, which must be avoided when assigning the size to a newly allocated block.
  size_t realBlockSize = finalBlockSize - kBlockSize;

  // Align the pointer to `minimumAlignment` and adjust the size of this block accordingly. It's the same as using
  // `minimumAlignment - Support::alignUpDiff()`, just written differently.
  newBlock->prev = nullptr;
  newBlock->next = nullptr;
  newBlock->size = realBlockSize;

  if (curBlock != &kArenaAllocatorZeroBlock.block) {
    newBlock->prev = curBlock;
    curBlock->next = newBlock;

    // Does only happen if there is a next block, but the requested memory can't fit into it. In this case a new
    // buffer is allocated and inserted between the current block and the next one.
    if (next) {
      newBlock->next = next;
      next->prev = newBlock;
    }
  }

  uint8_t* ptr = IntOps::alignUp(newBlock->data(), requiredBlockAlignment);
  uint8_t* end = newBlock->data() + realBlockSize;

  _ptr = ptr + size;
  _end = end;
  _block = newBlock;
  _blockCount++;

  BL_ASSERT(_ptr <= _end);
  return static_cast<void*>(ptr);
}

void* ArenaAllocator::allocZeroed(size_t size, size_t alignment) noexcept {
  void* p = alloc(size, alignment);
  if (BL_UNLIKELY(!p))
    return p;
  return memset(p, 0, size);
}

} // {bl}
