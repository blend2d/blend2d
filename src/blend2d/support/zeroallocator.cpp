// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../runtime_p.h"
#include "../support/arenaallocator_p.h"
#include "../support/arenalist_p.h"
#include "../support/arenatree_p.h"
#include "../support/bitops_p.h"
#include "../support/intops_p.h"
#include "../support/traits_p.h"
#include "../support/wrap_p.h"
#include "../support/zeroallocator_p.h"
#include "../threading/mutex_p.h"

// Define to always check whether the memory has been zeroed before release.
// #define BL_BUILD_DEBUG_ZERO_ALLOCATOR

namespace bl {

#if defined(BL_BUILD_DEBUG) || defined(BL_BUILD_DEBUG_ZERO_ALLOCATOR)
static void ZeroAllocator_checkReleasedMemory(uint8_t* ptr, size_t size) noexcept {
  // Must be aligned.
  BL_ASSERT(IntOps::isAligned(ptr , sizeof(uintptr_t)));
  BL_ASSERT(IntOps::isAligned(size, sizeof(uintptr_t)));

  uintptr_t* p = reinterpret_cast<uintptr_t*>(ptr);
  bool zeroedMemoryWasNotZero = false;

  for (size_t i = 0; i < size / sizeof(uintptr_t); i++) {
    if (p[i] != 0) {
      zeroedMemoryWasNotZero = true;
      blRuntimeMessageFmt("bl::ZeroAllocator::checkReleasedMemory(): Found non-zero: %p[%zu] == %zu\n", p, i * sizeof(uintptr_t), size_t(p[i]));
    }
  }

  BL_ASSERT(!zeroedMemoryWasNotZero);
}
#endif

//! Calculate the number of elements that would be required if `base` is granularized by `granularity`.
//! This function can be used to calculate the number of BitWords to represent N bits, for example.
template<typename X, typename Y>
static constexpr X ZeroAllocator_numGranularized(const X& base, const Y& granularity) noexcept {
  using U = BLInternal::UIntByType<X>;
  return X((U(base) + U(granularity) - 1) / U(granularity));
}

//! Based on asmjit's JitAllocator, but modified and enhanced for our own purposes.
class ZeroAllocator {
public:
  BL_NONCOPYABLE(ZeroAllocator)
  typedef PrivateBitWordOps BitOps;

  enum : uint32_t {
    kBlockAlignment = 64,
    kBlockGranularity = 1024,

    kMinBlockSize = 1024 * 1024,     // 1MB.
    kMaxBlockSize = 1024 * 1024 * 16 // 16MB.
  };

  static constexpr size_t bitWordCountFromAreaSize(uint32_t areaSize) noexcept {
    return IntOps::alignUp<size_t>(areaSize, IntOps::bitSizeOf<BLBitWord>()) / IntOps::bitSizeOf<BLBitWord>();
  }

  class Block : public ArenaTreeNode<Block>, public ArenaListNode<Block> {
  public:
    BL_NONCOPYABLE(Block)

    enum Flags : uint32_t {
      //! This is a statically allocated block.
      kFlagStatic = 0x00000001u,
      //! Block is dirty (some members need to be updated).
      kFlagDirty  = 0x80000000u
    };

    //! Zeroed buffer managed by this block.
    uint8_t* _buffer;
    //! Aligned `_buffer` to kBlockAlignment.
    uint8_t* _bufferAligned;
    //! Size of `buffer` in bytes.
    size_t _blockSize;

    //! Block flags.
    uint32_t _flags;
    //! Size of the whole block area (bit-vector size).
    uint32_t _areaSize;
    //! Used area (number of bits in bit-vector used).
    uint32_t _areaUsed;
    //! The largest unused continuous area in the bit-vector (or `_areaSize` to initiate rescan).
    uint32_t _largestUnusedArea;
    //! Start of a search range (for unused bits).
    uint32_t _searchStart;
    //! End of a search range (for unused bits).
    uint32_t _searchEnd;
    //! Bit vector representing all used areas (0 = unused, 1 = used).
    BLBitWord _bitVector[1];

    BL_INLINE Block(uint8_t* buffer, size_t blockSize, uint32_t areaSize) noexcept
      : ArenaTreeNode(),
        _buffer(buffer),
        _bufferAligned(IntOps::alignUp(buffer, kBlockAlignment)),
        _blockSize(blockSize),
        _flags(0),
        _areaSize(areaSize),
        _areaUsed(0),
        _largestUnusedArea(areaSize),
        _searchStart(0),
        _searchEnd(areaSize) {}

    BL_INLINE uint8_t* bufferAligned() const noexcept { return _bufferAligned; }
    BL_INLINE size_t blockSize() const noexcept { return _blockSize; }

    BL_INLINE size_t overheadSize() const noexcept {
      return sizeof(Block) - sizeof(BLBitWord) + ZeroAllocator_numGranularized(areaSize(), IntOps::bitSizeOf<BLBitWord>());
    }

    BL_INLINE uint32_t flags() const noexcept { return _flags; }
    BL_INLINE bool hasFlag(uint32_t flag) const noexcept { return (_flags & flag) != 0; }
    BL_INLINE void addFlags(uint32_t flags) noexcept { _flags |= flags; }
    BL_INLINE void clearFlags(uint32_t flags) noexcept { _flags &= ~flags; }

    BL_INLINE uint32_t areaSize() const noexcept { return _areaSize; }
    BL_INLINE uint32_t areaUsed() const noexcept { return _areaUsed; }
    BL_INLINE uint32_t areaAvailable() const noexcept { return areaSize() - areaUsed(); }
    BL_INLINE uint32_t largestUnusedArea() const noexcept { return _largestUnusedArea; }

    BL_INLINE void resetBitVector() noexcept {
      memset(_bitVector, 0, ZeroAllocator_numGranularized(_areaSize, IntOps::bitSizeOf<BLBitWord>()) * sizeof(BLBitWord));
    }

    // RBTree default CMP uses '<' and '>' operators.
    BL_INLINE bool operator<(const Block& other) const noexcept { return bufferAligned() < other.bufferAligned(); }
    BL_INLINE bool operator>(const Block& other) const noexcept { return bufferAligned() > other.bufferAligned(); }

    // Special implementation for querying blocks by `key`, which must be in `[buffer, buffer + blockSize)` range.
    BL_INLINE bool operator<(const uint8_t* key) const noexcept { return bufferAligned() + blockSize() <= key; }
    BL_INLINE bool operator>(const uint8_t* key) const noexcept { return bufferAligned() > key; }
  };

  //! Mutex for thread safety.
  mutable BLMutex _mutex;
  //! Tree that contains all blocks.
  ArenaTree<Block> _tree;
  //! Double linked list of blocks.
  ArenaList<Block> _blocks;
  //! Allocated block count.
  size_t _blockCount;
  //! Area size of base block.
  size_t _baseAreaSize;
  //! Number of bits reserved across all blocks.
  size_t _totalAreaSize;
  //! Number of bits used across all blocks.
  size_t _totalAreaUsed;
  //! A threshold to trigger auto-cleanup.
  size_t _cleanupThreshold;
  //! Memory overhead required to manage blocks.
  size_t _overheadSize;

  //! \name Construction & Destruction
  //! \{

  BL_INLINE ZeroAllocator(Block* baseBlock) noexcept
    : _tree(),
      _blocks(),
      _blockCount(0),
      _baseAreaSize(0),
      _totalAreaSize(0),
      _totalAreaUsed(0),
      _cleanupThreshold(0),
      _overheadSize(0) {

    baseBlock->addFlags(Block::kFlagStatic);
    insertBlock(baseBlock);

    _baseAreaSize = _totalAreaSize;
    _cleanupThreshold = _totalAreaSize;
  }

  BL_INLINE ~ZeroAllocator() noexcept {
    _cleanupInternal();
  }

  //! \}

  //! \name Block Management
  //! \{

  // Allocate a new `ZeroAllocator::Block` for the given `blockSize`.
  Block* newBlock(size_t blockSize) noexcept {
    uint32_t areaSize = uint32_t((blockSize + kBlockGranularity - 1) / kBlockGranularity);
    uint32_t numBitWords = (areaSize + IntOps::bitSizeOf<BLBitWord>() - 1u) / IntOps::bitSizeOf<BLBitWord>();

    size_t blockStructSize = sizeof(Block) + size_t(numBitWords - 1) * sizeof(BLBitWord);
    Block* block = static_cast<Block*>(malloc(blockStructSize));
    uint8_t* buffer = static_cast<uint8_t*>(::calloc(1, blockSize + kBlockAlignment));

    // Out of memory.
    if (BL_UNLIKELY(!block || !buffer)) {
      if (buffer)
        free(buffer);

      if (block)
        free(block);

      return nullptr;
    }

    blCallCtor(*block, buffer, blockSize, areaSize);
    block->resetBitVector();
    return block;
  }

  void deleteBlock(Block* block) noexcept {
    BL_ASSERT(!(block->hasFlag(Block::kFlagStatic)));

    free(block->_buffer);
    free(block);
  }

  void insertBlock(Block* block) noexcept {
    // Add to RBTree and List.
    _tree.insert(block);
    _blocks.append(block);

    // Update statistics.
    _blockCount++;
    _totalAreaSize += block->areaSize();
    _overheadSize += block->overheadSize();
  }

  void removeBlock(Block* block) noexcept {
    // Remove from RBTree and List.
    _tree.remove(block);
    _blocks.unlink(block);

    // Update statistics.
    _blockCount--;
    _totalAreaSize -= block->areaSize();
    _overheadSize -= block->overheadSize();
  }

  BL_INLINE size_t calculateIdealBlockSize(size_t allocationSize) noexcept {
    uint32_t kMaxSizeShift = IntOps::ctzStatic(kMaxBlockSize) -
                             IntOps::ctzStatic(kMinBlockSize) ;

    size_t blockSize = size_t(kMinBlockSize) << blMin<size_t>(_blockCount, kMaxSizeShift);
    if (blockSize < allocationSize)
      blockSize = IntOps::alignUp(allocationSize, blockSize);
    return blockSize;
  }

  BL_INLINE size_t calculateCleanupThreshold() const noexcept {
    if (_blockCount <= 6)
      return 0;

    size_t area = _totalAreaSize - _baseAreaSize;
    size_t threshold = area / 5u;
    return _baseAreaSize + threshold;
  }

  //! \}

  //! \name Cleanup
  //! \{

  void _cleanupInternal(size_t n = SIZE_MAX) noexcept {
    Block* block = _blocks.last();

    while (block && n) {
      Block* prev = block->prev();
      if (block->areaUsed() == 0 && !(block->hasFlag(Block::kFlagStatic))) {
        removeBlock(block);
        deleteBlock(block);
        n--;
      }
      block = prev;
    }

    _cleanupThreshold = calculateCleanupThreshold();
  }

  //! \}

  //! \name Alloc & Release
  //! \{

  void* _allocInternal(size_t size, size_t* allocatedSize) noexcept {
    constexpr uint32_t kNoIndex = Traits::maxValue<uint32_t>();

    // Align to minimum granularity by default.
    size = IntOps::alignUp<size_t>(size, kBlockGranularity);
    *allocatedSize = 0;

    if (BL_UNLIKELY(size == 0 || size > Traits::maxValue<uint32_t>() / 2))
      return nullptr;

    Block* block = _blocks.first();
    uint32_t areaIndex = kNoIndex;
    uint32_t areaSize = uint32_t(ZeroAllocator_numGranularized(size, kBlockGranularity));

    // Try to find the requested memory area in existing blocks.
    if (block) {
      Block* initial = block;
      do {
        Block* next = block->hasNext() ? block->next() : _blocks.first();
        if (block->areaAvailable() >= areaSize) {
          if (block->hasFlag(Block::kFlagDirty) || block->largestUnusedArea() >= areaSize) {
            uint32_t blockAreaSize = block->areaSize();
            uint32_t searchStart = block->_searchStart;
            uint32_t searchEnd = block->_searchEnd;

            BitOps::BitVectorFlipIterator it(
              block->_bitVector, ZeroAllocator_numGranularized(searchEnd, BitOps::kNumBits), searchStart, BitOps::ones());

            // If there is unused area available then there has to be at least one match.
            BL_ASSERT(it.hasNext());

            uint32_t bestArea = blockAreaSize;
            uint32_t largestArea = 0;

            uint32_t holeIndex = uint32_t(it.peekNext());
            uint32_t holeEnd = holeIndex;

            searchStart = holeIndex;
            do {
              holeIndex = uint32_t(it.nextAndFlip());
              if (holeIndex >= searchEnd)
                break;

              holeEnd = it.hasNext() ? blMin(searchEnd, uint32_t(it.nextAndFlip())) : searchEnd;
              uint32_t holeSize = holeEnd - holeIndex;

              if (holeSize >= areaSize && bestArea >= holeSize) {
                largestArea = blMax(largestArea, bestArea);
                bestArea = holeSize;
                areaIndex = holeIndex;
              }
              else {
                largestArea = blMax(largestArea, holeSize);
              }
            } while (it.hasNext());
            searchEnd = holeEnd;

            // Because we have traversed the entire block, we can now mark the
            // largest unused area that can be used to cache the next traversal.
            block->_searchStart = searchStart;
            block->_searchEnd = searchEnd;
            block->_largestUnusedArea = largestArea;
            block->clearFlags(Block::kFlagDirty);

            if (areaIndex != kNoIndex) {
              if (searchStart == areaIndex)
                block->_searchStart += areaSize;
              break;
            }
          }
        }

        block = next;
      } while (block != initial);
    }

    // Allocate a new block if there is no region of a required width.
    if (areaIndex == kNoIndex) {
      size_t blockSize = calculateIdealBlockSize(size);
      block = newBlock(blockSize);

      if (BL_UNLIKELY(!block))
        return nullptr;

      insertBlock(block);
      _cleanupThreshold = calculateCleanupThreshold();

      areaIndex = 0;
      block->_searchStart = areaSize;
      block->_largestUnusedArea = block->areaSize() - areaSize;
    }

    // Update statistics.
    _totalAreaUsed += areaSize;
    block->_areaUsed += areaSize;

    // Handle special cases.
    if (block->areaAvailable() == 0) {
      // The whole block is filled.
      block->_searchStart = block->areaSize();
      block->_searchEnd = 0;
      block->_largestUnusedArea = 0;
      block->clearFlags(Block::kFlagDirty);
    }

    // Mark the newly allocated space as occupied and also the sentinel.
    BitOps::bitArrayFill(block->_bitVector, areaIndex, areaSize);

    // Return a pointer to allocated memory.
    uint8_t* result = block->bufferAligned() + areaIndex * kBlockGranularity;
    BL_ASSERT(result >= block->bufferAligned());
    BL_ASSERT(result <= block->bufferAligned() + block->blockSize() - size);

    *allocatedSize = size;
    return result;
  }

  void _releaseInternal(void* ptr, size_t size) noexcept {
    BL_ASSERT(ptr != nullptr);
    BL_ASSERT(size != 0);

    Block* block = _tree.get(static_cast<uint8_t*>(ptr));
    BL_ASSERT(block != nullptr);

#if defined(BL_BUILD_DEBUG) || defined(BL_BUILD_DEBUG_ZERO_ALLOCATOR)
    ZeroAllocator_checkReleasedMemory(static_cast<uint8_t*>(ptr), size);
#endif

    // Offset relative to the start of the block.
    size_t byteOffset = (size_t)((uint8_t*)ptr - block->bufferAligned());

    // The first bit representing the allocated area and its size.
    uint32_t areaIndex = uint32_t(byteOffset / kBlockGranularity);
    uint32_t areaSize = uint32_t(ZeroAllocator_numGranularized(size, kBlockGranularity));

    // Update the search region and statistics.
    block->_searchStart = blMin(block->_searchStart, areaIndex);
    block->_searchEnd = blMax(block->_searchEnd, areaIndex + areaSize);
    block->addFlags(Block::kFlagDirty);

    block->_areaUsed -= areaSize;
    _totalAreaUsed -= areaSize;

    // Clear bits used to mark this area as occupied.
    BitOps::bitArrayClear(block->_bitVector, areaIndex, areaSize);

    if (_totalAreaUsed < _cleanupThreshold)
      _cleanupInternal(1);
  }

  BL_INLINE void* _resizeInternal(void* prevPtr, size_t prevSize, size_t size, size_t* allocatedSize) noexcept {
    if (prevPtr != nullptr)
      _releaseInternal(prevPtr, prevSize);
    return _allocInternal(size, allocatedSize);
  }

  //! \}

  //! \name API
  //! \{

  BL_INLINE void* alloc(size_t size, size_t* allocatedSize) noexcept {
    return _mutex.protect([&] { return _allocInternal(size, allocatedSize); });
  }

  BL_INLINE void* resize(void* prevPtr, size_t prevSize, size_t size, size_t* allocatedSize) noexcept {
    return _mutex.protect([&] { return _resizeInternal(prevPtr, prevSize, size, allocatedSize); });
  }

  BL_INLINE void release(void* ptr, size_t size) noexcept {
    _mutex.protect([&] { _releaseInternal(ptr, size); });
  }

  BL_INLINE void cleanup() noexcept {
    _mutex.protect([&] { _cleanupInternal(); });
  }

  BL_INLINE void onResourceInfo(BLRuntimeResourceInfo* resourceInfo) noexcept {
    _mutex.protect([&] {
      resourceInfo->zmUsed = _totalAreaUsed * kBlockGranularity;
      resourceInfo->zmReserved = _totalAreaSize * kBlockGranularity;
      resourceInfo->zmOverhead = _overheadSize;
      resourceInfo->zmBlockCount = _blockCount;
    });
  }

  //! \}
};

static Wrap<ZeroAllocator> zeroAllocatorGlobal;

// ZeroAllocator - Static Buffer
// ===============================

// Base memory is a zeroed memory allocated by the linker. By default we use 1MB of memory that we will use
// as a base before obtaining more from the system if that's not enough.

struct ZeroAllocatorStaticBlock {
  enum : uint32_t {
    kBlockSize = 1024u * 1024u,
    kAreaSize = ZeroAllocator_numGranularized(kBlockSize, ZeroAllocator::kBlockGranularity),
    kBitWordCount = ZeroAllocator_numGranularized(kAreaSize, IntOps::bitSizeOf<BLBitWord>())
  };

  Wrap<ZeroAllocator::Block> block;
  BLBitWord bitWords[kBitWordCount];
};

struct alignas(64) ZeroAllocatorStaticBuffer {
  uint8_t buffer[ZeroAllocatorStaticBlock::kBlockSize];
};

static ZeroAllocatorStaticBlock zeroAllocatorStaticBlock;
static ZeroAllocatorStaticBuffer zeroAllocatorStaticBuffer;

} // {bl}

// bl::ZeroAllocator - API
// =======================

void* blZeroAllocatorAlloc(size_t size, size_t* allocatedSize) noexcept {
  return bl::zeroAllocatorGlobal->alloc(size, allocatedSize);
}

void* blZeroAllocatorResize(void* prevPtr, size_t prevSize, size_t size, size_t* allocatedSize) noexcept {
  return bl::zeroAllocatorGlobal->resize(prevPtr, prevSize, size, allocatedSize);
}

void blZeroAllocatorRelease(void* ptr, size_t size) noexcept {
  bl::zeroAllocatorGlobal->release(ptr, size);
}

// bl::ZeroAllocator - Runtime
// ===========================

static void BL_CDECL blZeroAllocatorRtShutdown(BLRuntimeContext* rt) noexcept {
  blUnused(rt);
  bl::zeroAllocatorGlobal.destroy();
}

static void BL_CDECL blZeroAllocatorRtCleanup(BLRuntimeContext* rt, BLRuntimeCleanupFlags cleanupFlags) noexcept {
  blUnused(rt);
  if (cleanupFlags & BL_RUNTIME_CLEANUP_ZEROED_POOL)
    bl::zeroAllocatorGlobal->cleanup();
}

static void BL_CDECL blZeroAllocatorRtResourceInfo(BLRuntimeContext* rt, BLRuntimeResourceInfo* resourceInfo) noexcept {
  blUnused(rt);
  bl::zeroAllocatorGlobal->onResourceInfo(resourceInfo);
}

void blZeroAllocatorRtInit(BLRuntimeContext* rt) noexcept {
  bl::ZeroAllocator::Block* block =
    bl::zeroAllocatorStaticBlock.block.init(
      bl::zeroAllocatorStaticBuffer.buffer,
      bl::ZeroAllocatorStaticBlock::kBlockSize,
      bl::ZeroAllocatorStaticBlock::kAreaSize);

  bl::zeroAllocatorGlobal.init(block);

  rt->shutdownHandlers.add(blZeroAllocatorRtShutdown);
  rt->cleanupHandlers.add(blZeroAllocatorRtCleanup);
  rt->resourceInfoHandlers.add(blZeroAllocatorRtResourceInfo);
}
