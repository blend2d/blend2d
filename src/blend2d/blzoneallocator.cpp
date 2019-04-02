// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blsupport_p.h"
#include "./blzoneallocator_p.h"

// ============================================================================
// [BLZoneAllocator - Statics]
// ============================================================================

// Zero size block used by `BLZoneAllocator` that doesn't have any memory allocated.
// Should be allocated in read-only memory and should never be modified.
const BLZoneAllocator::Block BLZoneAllocator::_zeroBlock = { nullptr, nullptr, 0 };

// ============================================================================
// [BLZoneAllocator - Init / Reset]
// ============================================================================

void BLZoneAllocator::_init(size_t blockSize, size_t blockAlignment, void* staticData, size_t staticSize) noexcept {
  BL_ASSERT(blockSize >= kMinBlockSize);
  BL_ASSERT(blockSize <= kMaxBlockSize);
  BL_ASSERT(blockAlignment <= 64);

  _assignZeroBlock();
  _blockSize = blockSize & blTrailingBitMask<size_t>(blBitSizeOf<size_t>() - 4);
  _hasStaticBlock = staticData != nullptr;
  _blockAlignmentShift = blBitCtz(blockAlignment) & 0x7;

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

void BLZoneAllocator::reset() noexcept {
  // Can't be altered.
  Block* cur = _block;
  if (cur == &_zeroBlock)
    return;

  Block* initial = const_cast<BLZoneAllocator::Block*>(&_zeroBlock);
  _ptr = initial->data();
  _end = initial->data();
  _block = initial;

  // Since cur can be in the middle of the double-linked list, we have to
  // traverse both directions (`prev` and `next`) separately to visit all.
  Block* next = cur->next;
  do {
    Block* prev = cur->prev;

    // If this is the first block and this BLZoneAllocatorTmp is temporary then
    // the first block is statically allocated. We cannot free it and it makes
    // sense to keep it even when this is hard reset.
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

// ============================================================================
// [BLZoneAllocator - Alloc]
// ============================================================================

void* BLZoneAllocator::_alloc(size_t size, size_t alignment) noexcept {
  Block* curBlock = _block;
  Block* next = curBlock->next;

  size_t rawBlockAlignment = blockAlignment();
  size_t minimumAlignment = blMax<size_t>(alignment, rawBlockAlignment);

  // If the `BLZoneAllocator` has been cleared the current block doesn't have to be the
  // last one. Check if there is a block that can be used instead of allocating
  // a new one. If there is a `next` block it's completely unused, we don't have
  // to check for remaining bytes in that case.
  if (next) {
    uint8_t* ptr = blAlignUp(next->data(), minimumAlignment);
    uint8_t* end = blAlignDown(next->data() + next->size, rawBlockAlignment);

    if (size <= (size_t)(end - ptr)) {
      _block = next;
      _ptr = ptr + size;
      _end = blAlignDown(next->data() + next->size, rawBlockAlignment);
      return static_cast<void*>(ptr);
    }
  }

  size_t blockAlignmentOverhead = alignment - blMin<size_t>(alignment, BL_ALLOC_ALIGNMENT);
  size_t newSize = blMax(blockSize(), size);

  // Prevent arithmetic overflow.
  if (BL_UNLIKELY(newSize > SIZE_MAX - kBlockSize - blockAlignmentOverhead))
    return nullptr;

  // Allocate new block - we add alignment overhead to `newSize`, which becomes the
  // new block size, and we also add `kBlockOverhead` to the allocator as it includes
  // members of `BLZoneAllocator::Block` structure.
  newSize += blockAlignmentOverhead;
  Block* newBlock = static_cast<Block*>(malloc(newSize + kBlockSize));

  if (BL_UNLIKELY(!newBlock))
    return nullptr;

  // Align the pointer to `minimumAlignment` and adjust the size of this block
  // accordingly. It's the same as using `minimumAlignment - blAlignUpDiff()`,
  // just written differently.
  {
    newBlock->prev = nullptr;
    newBlock->next = nullptr;
    newBlock->size = newSize;

    if (curBlock != &_zeroBlock) {
      newBlock->prev = curBlock;
      curBlock->next = newBlock;

      // Does only happen if there is a next block, but the requested memory
      // can't fit into it. In this case a new buffer is allocated and inserted
      // between the current block and the next one.
      if (next) {
        newBlock->next = next;
        next->prev = newBlock;
      }
    }

    uint8_t* ptr = blAlignUp(newBlock->data(), minimumAlignment);
    uint8_t* end = blAlignDown(newBlock->data() + newSize, rawBlockAlignment);

    _ptr = ptr + size;
    _end = end;
    _block = newBlock;

    BL_ASSERT(_ptr <= _end);
    return static_cast<void*>(ptr);
  }
}

void* BLZoneAllocator::allocZeroed(size_t size, size_t alignment) noexcept {
  void* p = alloc(size, alignment);
  if (BL_UNLIKELY(!p))
    return p;
  return memset(p, 0, size);
}
