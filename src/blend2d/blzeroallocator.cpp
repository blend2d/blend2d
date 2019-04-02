// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blbitarray_p.h"
#include "./blrandom_p.h"
#include "./blruntime_p.h"
#include "./blsupport_p.h"
#include "./blthreading_p.h"
#include "./blzeroallocator_p.h"
#include "./blzoneallocator_p.h"
#include "./blzonelist_p.h"
#include "./blzonetree_p.h"

// ============================================================================
// [BLZeroAllocator - Implementation]
// ============================================================================

#ifdef BL_BUILD_DEBUG
static void blZeroAllocatorVerifyIfZeroed(uint8_t* ptr, size_t size) noexcept {
  // Must be aligned.
  BL_ASSERT(blIsAligned(ptr , sizeof(uintptr_t)));
  BL_ASSERT(blIsAligned(size, sizeof(uintptr_t)));

  uintptr_t* p = reinterpret_cast<uintptr_t*>(ptr);
  for (size_t i = 0; i < size / sizeof(uintptr_t); i++)
    BL_ASSERT(p[i] == 0);
}
#endif

//! Calculate the number of elements that would be required if `base` is
//! granularized by `granularity`. This function can be used to calculate
//! the number of BitWords to represent N bits, for example.
template<typename X, typename Y>
constexpr X blZeroAllocatorNumGranularized(X base, Y granularity) noexcept {
  typedef typename BLInternal::StdInt<sizeof(X), 1>::Type U;
  return X((U(base) + U(granularity) - 1) / U(granularity));
}

//! Based on asmjit's JitAllocator, but modified and enhanced for our own purposes.
class BLZeroAllocator {
public:
  BL_NONCOPYABLE(BLZeroAllocator)

  enum : uint32_t {
    kBlockAlignment = 64,
    kBlockGranularity = 1024,

    kMinBlockSize = 1048576, // 1MB.
    kMaxBlockSize = 8388608  // 8MB.
  };

  static constexpr size_t bitWordCountFromAreaSize(uint32_t areaSize) noexcept {
    return blAlignUp<size_t>(areaSize, blBitSizeOf<BLBitWord>()) / blBitSizeOf<BLBitWord>();
  }

  class Block : public BLZoneTreeNode<Block>,
                public BLZoneListNode<Block> {
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
      : BLZoneTreeNode(),
        _buffer(buffer),
        _bufferAligned(blAlignUp(buffer, kBlockAlignment)),
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
      return sizeof(Block) - sizeof(BLBitWord) + blZeroAllocatorNumGranularized(areaSize(), blBitSizeOf<BLBitWord>());
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
      memset(_bitVector, 0, blZeroAllocatorNumGranularized(_areaSize, blBitSizeOf<BLBitWord>()) * sizeof(BLBitWord));
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
  BLZoneTree<Block> _tree;
  //! Double linked list of blocks.
  BLZoneList<Block> _blocks;
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

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  BL_INLINE BLZeroAllocator(Block* baseBlock) noexcept
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

  BL_INLINE ~BLZeroAllocator() noexcept {
    _cleanupInternal();
  }

  // --------------------------------------------------------------------------
  // [Block Management]
  // --------------------------------------------------------------------------

  // Allocate a new `BLZeroAllocator::Block` for the given `blockSize`.
  Block* newBlock(size_t blockSize) noexcept {
    uint32_t areaSize = uint32_t((blockSize + kBlockGranularity - 1) / kBlockGranularity);
    uint32_t numBitWords = (areaSize + blBitSizeOf<BLBitWord>() - 1u) / blBitSizeOf<BLBitWord>();

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

    block = new(block) Block(buffer, blockSize, areaSize);
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
    uint32_t kMaxSizeShift = blBitCtz(kMaxBlockSize) -
                             blBitCtz(kMinBlockSize) ;

    size_t blockSize = size_t(kMinBlockSize) << blMin<size_t>(_blockCount, kMaxSizeShift);
    if (blockSize < allocationSize)
      blockSize = blAlignUp(allocationSize, blockSize);
    return blockSize;
  }

  BL_INLINE size_t calculateCleanupThreshold() const noexcept {
    if (_blockCount <= 6)
      return 0;

    size_t area = _totalAreaSize - _baseAreaSize;
    size_t threshold = area / 5u;
    return _baseAreaSize + threshold;
  }

  // --------------------------------------------------------------------------
  // [Cleanup]
  // --------------------------------------------------------------------------

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

  BL_INLINE void cleanup() noexcept {
    BLMutexGuard guard(_mutex);
    _cleanupInternal();
  }

  // --------------------------------------------------------------------------
  // [Memory Info]
  // --------------------------------------------------------------------------

  BL_INLINE void onMemoryInfo(BLRuntimeMemoryInfo* memoryInfo) noexcept {
    BLMutexGuard guard(_mutex);

    memoryInfo->zmUsed = _totalAreaUsed * kBlockGranularity;
    memoryInfo->zmReserved = _totalAreaSize * kBlockGranularity;
    memoryInfo->zmOverhead = _overheadSize;
    memoryInfo->zmBlockCount = _blockCount;
  }

  // --------------------------------------------------------------------------
  // [Alloc / Release]
  // --------------------------------------------------------------------------

  void* _allocInternal(size_t size, size_t* allocatedSize) noexcept {
    constexpr uint32_t kNoIndex = blMaxValue<uint32_t>();

    // Align to minimum granularity by default.
    size = blAlignUp<size_t>(size, kBlockGranularity);
    *allocatedSize = 0;

    if (BL_UNLIKELY(size == 0 || size > blMaxValue<uint32_t>() / 2))
      return nullptr;

    Block* block = _blocks.first();
    uint32_t areaIndex = kNoIndex;
    uint32_t areaSize = uint32_t(blZeroAllocatorNumGranularized(size, kBlockGranularity));

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

            BLBitVectorFlipIterator<BLBitWord> it(
              block->_bitVector, blZeroAllocatorNumGranularized(searchEnd, blBitSizeOf<BLBitWord>()), searchStart, blBitOnes<BLBitWord>());

            // If there is unused area available then there has to be at least one match.
            BL_ASSERT(it.hasNext());

            uint32_t bestArea = blockAreaSize;
            uint32_t largestArea = 0;

            uint32_t holeIndex = uint32_t(it.peekNext());
            uint32_t holeEnd = holeIndex;

            searchStart = holeIndex;
            do {
              holeIndex = uint32_t(it.nextAndFlip());
              if (holeIndex >= searchEnd) break;

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
    blBitArrayFillInternal(block->_bitVector, areaIndex, areaSize);

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

    #ifdef BL_BUILD_DEBUG
    blZeroAllocatorVerifyIfZeroed(static_cast<uint8_t*>(ptr), size);
    #endif

    // Offset relative to the start of the block.
    size_t byteOffset = (size_t)((uint8_t*)ptr - block->bufferAligned());

    // The first bit representing the allocated area and its size.
    uint32_t areaIndex = uint32_t(byteOffset / kBlockGranularity);
    uint32_t areaSize = uint32_t(blZeroAllocatorNumGranularized(size, kBlockGranularity));

    // Update the search region and statistics.
    block->_searchStart = blMin(block->_searchStart, areaIndex);
    block->_searchEnd = blMax(block->_searchEnd, areaIndex + areaSize);
    block->addFlags(Block::kFlagDirty);

    block->_areaUsed -= areaSize;
    _totalAreaUsed -= areaSize;

    // Clear bits used to mark this area as occupied.
    blBitArrayClearInternal(block->_bitVector, areaIndex, areaSize);

    if (_totalAreaUsed < _cleanupThreshold)
      _cleanupInternal(1);
  }

  BL_INLINE void* alloc(size_t size, size_t* allocatedSize) noexcept {
    BLMutexGuard guard(_mutex);
    return _allocInternal(size, allocatedSize);
  }

  BL_INLINE void* resize(void* prevPtr, size_t prevSize, size_t size, size_t* allocatedSize) noexcept {
    BLMutexGuard guard(_mutex);
    if (prevPtr != nullptr)
      _releaseInternal(prevPtr, prevSize);
    return _allocInternal(size, allocatedSize);
  }

  BL_INLINE void release(void* ptr, size_t size) noexcept {
    BLMutexGuard guard(_mutex);
    _releaseInternal(ptr, size);
  }
};

static BLWrap<BLZeroAllocator> blZeroMemAllocator;

// ============================================================================
// [BLZeroAllocator - Static Buffer]
// ============================================================================

// Base memory is a zeroed memory allocated by the linker. By default we use
// 1MB of memory that we will use as a base before obtaining more from the
// system if that's not enough.

struct BLZeroAllocatorStaticBlock {
  enum : uint32_t {
    kBlockSize = 1024u * 1024u,
    kAreaSize = blZeroAllocatorNumGranularized(kBlockSize, BLZeroAllocator::kBlockGranularity),
    kBitWordCount = blZeroAllocatorNumGranularized(kAreaSize, blBitSizeOf<BLBitWord>())
  };

  BLWrap<BLZeroAllocator::Block> block;
  BLBitWord bitWords[kBitWordCount];
};

struct alignas(64) BLZeroAllocatorStaticBuffer {
  uint8_t buffer[BLZeroAllocatorStaticBlock::kBlockSize];
};

static BLZeroAllocatorStaticBlock blZeroAllocatorStaticBlock;
static BLZeroAllocatorStaticBuffer blZeroAllocatorStaticBuffer;

// ============================================================================
// [BLZeroAllocator - API]
// ============================================================================

void* blZeroAllocatorAlloc(size_t size, size_t* allocatedSize) noexcept {
  return blZeroMemAllocator->alloc(size, allocatedSize);
}

void* blZeroAllocatorResize(void* prevPtr, size_t prevSize, size_t size, size_t* allocatedSize) noexcept {
  return blZeroMemAllocator->resize(prevPtr, prevSize, size, allocatedSize);
}

void blZeroAllocatorRelease(void* ptr, size_t size) noexcept {
  blZeroMemAllocator->release(ptr, size);
}

// ============================================================================
// [BLZeroAllocator - Runtime Init]
// ============================================================================

static void BL_CDECL blZeroAllocatorRtShutdown(BLRuntimeContext* rt) noexcept {
  BL_UNUSED(rt);
  blZeroMemAllocator.destroy();
}

static void BL_CDECL blZeroAllocatorRtCleanup(BLRuntimeContext* rt, uint32_t cleanupFlags) noexcept {
  BL_UNUSED(rt);
  if (cleanupFlags & BL_RUNTIME_CLEANUP_ZEROED_POOL)
    blZeroMemAllocator->cleanup();
}

static void BL_CDECL blZeroAllocatorRtMemoryInfo(BLRuntimeContext* rt, BLRuntimeMemoryInfo* memoryInfo) noexcept {
  BL_UNUSED(rt);
  blZeroMemAllocator->onMemoryInfo(memoryInfo);
}

void blZeroAllocatorRtInit(BLRuntimeContext* rt) noexcept {
  BLZeroAllocator::Block* block =
    blZeroAllocatorStaticBlock.block.init(
      blZeroAllocatorStaticBuffer.buffer,
      BLZeroAllocatorStaticBlock::kBlockSize,
      BLZeroAllocatorStaticBlock::kAreaSize);

  blZeroMemAllocator.init(block);

  rt->shutdownHandlers.add(blZeroAllocatorRtShutdown);
  rt->cleanupHandlers.add(blZeroAllocatorRtCleanup);
  rt->memoryInfoHandlers.add(blZeroAllocatorRtMemoryInfo);
}

// ============================================================================
// [BLZeroAllocator - Unit Tests]
// ============================================================================

#ifdef BL_BUILD_TEST
// A helper class to verify that BLZeroAllocator doesn't return addresses that overlap.
class BLZeroAllocatorWrapper {
public:
  BL_INLINE BLZeroAllocatorWrapper() noexcept {}

  // Address to a memory region of a given size.
  class Range {
  public:
    BL_INLINE Range(uint8_t* addr, size_t size) noexcept
      : addr(addr),
        size(size) {}
    uint8_t* addr;
    size_t size;
  };

  // Based on BLZeroAllocator::Block, serves our purpose well...
  class Record : public BLZoneTreeNode<Record>,
                 public Range {
  public:
    BL_INLINE Record(uint8_t* addr, size_t size)
      : BLZoneTreeNode<Record>(),
        Range(addr, size) {}

    BL_INLINE bool operator<(const Record& other) const noexcept { return addr < other.addr; }
    BL_INLINE bool operator>(const Record& other) const noexcept { return addr > other.addr; }

    BL_INLINE bool operator<(const uint8_t* key) const noexcept { return addr + size <= key; }
    BL_INLINE bool operator>(const uint8_t* key) const noexcept { return addr > key; }
  };

  BLZoneTree<Record> _records;

  void _insert(void* p_, size_t size) noexcept {
    uint8_t* p = static_cast<uint8_t*>(p_);
    uint8_t* pEnd = p + size - 1;

    Record* record = _records.get(p);
    if (record)
      EXPECT(record == nullptr,
             "Address [%p:%p] collides with a newly allocated [%p:%p]\n", record->addr, record->addr + record->size, p, p + size);

    record = _records.get(pEnd);
    if (record)
      EXPECT(record == nullptr,
             "Address [%p:%p] collides with a newly allocated [%p:%p]\n", record->addr, record->addr + record->size, p, p + size);

    record = new(malloc(sizeof(Record))) Record(p, size);
    EXPECT(record != nullptr,
           "Out of memory, cannot allocate 'Record'");

    _records.insert(record);
  }

  void _remove(void* p) noexcept {
    Record* record = _records.get(static_cast<uint8_t*>(p));
    EXPECT(record != nullptr,
           "Address [%p] doesn't exist\n", p);

    _records.remove(record);
    free(record);
  }

  void* alloc(size_t size) noexcept {
    size_t allocatedSize = 0;
    void* p = blZeroAllocatorAlloc(size, &allocatedSize);
    EXPECT(p != nullptr,
           "BLZeroAllocator failed to allocate '%u' bytes\n", unsigned(size));

    for (size_t i = 0; i < allocatedSize; i++) {
      EXPECT(static_cast<const uint8_t*>(p)[i] == 0,
            "The returned pointer doesn't point to a zeroed memory %p[%u]\n", p, int(size));
    }

    _insert(p, allocatedSize);
    return p;
  }

  size_t getSizeOfPtr(void* p) noexcept {
    Record* record = _records.get(static_cast<uint8_t*>(p));
    return record ? record->size : size_t(0);
  }

  void release(void* p) noexcept {
    size_t size = getSizeOfPtr(p);
    _remove(p);
    blZeroAllocatorRelease(p, size);
  }
};

static void blZeroAllocatorTestShuffle(void** ptrArray, size_t count, BLRandom& prng) noexcept {
  for (size_t i = 0; i < count; ++i)
    std::swap(ptrArray[i], ptrArray[size_t(prng.nextUInt32() % count)]);
}

static void blZeroAllocatorTestUsage() noexcept {
  BLRuntimeMemoryInfo mi;
  BLRuntime::queryMemoryInfo(&mi);

  INFO("NumBlocks: %9llu"         , (unsigned long long)(mi.zmBlockCount));
  INFO("UsedSize : %9llu [Bytes]" , (unsigned long long)(mi.zmUsed));
  INFO("Reserved : %9llu [Bytes]" , (unsigned long long)(mi.zmReserved));
  INFO("Overhead : %9llu [Bytes]" , (unsigned long long)(mi.zmOverhead));
}

UNIT(blend2d_zero_allocator) {
  BLZeroAllocatorWrapper wrapper;
  BLRandom prng(0);

  size_t i;
  size_t kCount = 50000;

  INFO("Memory alloc/release test - %d allocations", kCount);

  void** ptrArray = (void**)malloc(sizeof(void*) * size_t(kCount));
  EXPECT(ptrArray != nullptr,
        "Couldn't allocate '%u' bytes for pointer-array", unsigned(sizeof(void*) * size_t(kCount)));

  INFO("Allocating zeroed memory...");
  for (i = 0; i < kCount; i++)
    ptrArray[i] = wrapper.alloc((prng.nextUInt32() % 8000) + 128);
  blZeroAllocatorTestUsage();

  INFO("Releasing zeroed memory...");
  for (i = 0; i < kCount; i++)
    wrapper.release(ptrArray[i]);
  blZeroAllocatorTestUsage();

  INFO("Submitting manual cleanup...");
  BLRuntime::cleanup(BL_RUNTIME_CLEANUP_ZEROED_POOL);
  blZeroAllocatorTestUsage();

  INFO("Allocating zeroed memory...", kCount);
  for (i = 0; i < kCount; i++)
    ptrArray[i] = wrapper.alloc((prng.nextUInt32() % 8000) + 128);
  blZeroAllocatorTestUsage();

  INFO("Shuffling...");
  blZeroAllocatorTestShuffle(ptrArray, unsigned(kCount), prng);

  INFO("Releasing 50% blocks...");
  for (i = 0; i < kCount / 2; i++)
    wrapper.release(ptrArray[i]);
  blZeroAllocatorTestUsage();

  INFO("Allocating 50% blocks again...");
  for (i = 0; i < kCount / 2; i++)
    ptrArray[i] = wrapper.alloc((prng.nextUInt32() % 8000) + 128);
  blZeroAllocatorTestUsage();

  INFO("Releasing zeroed memory...");
  for (i = 0; i < kCount; i++)
    wrapper.release(ptrArray[i]);
  blZeroAllocatorTestUsage();

  free(ptrArray);
}
#endif
