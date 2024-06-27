// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_test_p.h"
#if defined(BL_TEST)

#include "../random_p.h"
#include "../runtime_p.h"
#include "../support/arenaallocator_p.h"
#include "../support/arenalist_p.h"
#include "../support/arenatree_p.h"
#include "../support/traits_p.h"
#include "../support/zeroallocator_p.h"

// bl::ZeroAllocator - Tests
// =========================

namespace bl {
namespace Tests {

// A helper class to verify that BLZeroAllocator doesn't return addresses that overlap.
class ZeroAllocatorWrapper {
public:
  BL_INLINE ZeroAllocatorWrapper() noexcept {}

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
  class Record : public ArenaTreeNode<Record>,
                 public Range {
  public:
    BL_INLINE Record(uint8_t* addr, size_t size)
      : ArenaTreeNode<Record>(),
        Range(addr, size) {}

    BL_INLINE bool operator<(const Record& other) const noexcept { return addr < other.addr; }
    BL_INLINE bool operator>(const Record& other) const noexcept { return addr > other.addr; }

    BL_INLINE bool operator<(const uint8_t* key) const noexcept { return addr + size <= key; }
    BL_INLINE bool operator>(const uint8_t* key) const noexcept { return addr > key; }
  };

  ArenaTree<Record> _records;

  void _insert(void* p_, size_t size) noexcept {
    uint8_t* p = static_cast<uint8_t*>(p_);
    uint8_t* pEnd = p + size - 1;

    Record* record = _records.get(p);
    if (record)
      EXPECT_EQ(record, nullptr)
        .message("Address [%p:%p] collides with a newly allocated [%p:%p]\n", record->addr, record->addr + record->size, p, p + size);

    record = _records.get(pEnd);
    if (record)
      EXPECT_EQ(record, nullptr)
        .message("Address [%p:%p] collides with a newly allocated [%p:%p]\n", record->addr, record->addr + record->size, p, p + size);

    void* rPtr = malloc(sizeof(Record));
    EXPECT_NE(rPtr, nullptr).message("Out of memory, cannot allocate 'Record'");
    _records.insert(new(BLInternal::PlacementNew{rPtr}) Record(p, size));
  }

  void _remove(void* p) noexcept {
    Record* record = _records.get(static_cast<uint8_t*>(p));
    EXPECT_NE(record, nullptr).message("Address [%p] doesn't exist\n", p);

    _records.remove(record);
    free(record);
  }

  void* alloc(size_t size) noexcept {
    size_t allocatedSize = 0;
    void* p = blZeroAllocatorAlloc(size, &allocatedSize);
    EXPECT_NE(p, nullptr).message("BLZeroAllocator failed to allocate '%u' bytes\n", unsigned(size));

    for (size_t i = 0; i < allocatedSize; i++) {
      EXPECT_EQ(static_cast<const uint8_t*>(p)[i], 0)
        .message("The returned pointer doesn't point to a zeroed memory %p[%u]\n", p, int(size));
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
    BLInternal::swap(ptrArray[i], ptrArray[size_t(prng.nextUInt32() % count)]);
}

static void blZeroAllocatorTestUsage() noexcept {
  BLRuntimeResourceInfo info;
  BLRuntime::queryResourceInfo(&info);

  INFO("  NumBlocks: %9llu"         , (unsigned long long)(info.zmBlockCount));
  INFO("  UsedSize : %9llu [Bytes]" , (unsigned long long)(info.zmUsed));
  INFO("  Reserved : %9llu [Bytes]" , (unsigned long long)(info.zmReserved));
  INFO("  Overhead : %9llu [Bytes]" , (unsigned long long)(info.zmOverhead));
}

UNIT(zero_allocator, BL_TEST_GROUP_CORE_UTILITIES) {
  ZeroAllocatorWrapper wrapper;
  BLRandom prng(0);

  size_t i;
  size_t kCount = 50000;

  INFO("Memory alloc/release test - %d allocations", kCount);

  void** ptrArray = (void**)malloc(sizeof(void*) * size_t(kCount));
  EXPECT_NE(ptrArray, nullptr)
    .message("Couldn't allocate '%u' bytes for pointer-array", unsigned(sizeof(void*) * size_t(kCount)));

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

  INFO("Releasing 50%% blocks...");
  for (i = 0; i < kCount / 2; i++)
    wrapper.release(ptrArray[i]);
  blZeroAllocatorTestUsage();

  INFO("Allocating 50%% blocks again...");
  for (i = 0; i < kCount / 2; i++)
    ptrArray[i] = wrapper.alloc((prng.nextUInt32() % 8000) + 128);
  blZeroAllocatorTestUsage();

  INFO("Releasing zeroed memory...");
  for (i = 0; i < kCount; i++)
    wrapper.release(ptrArray[i]);
  blZeroAllocatorTestUsage();

  free(ptrArray);
}

} // {Tests}
} // {bl}

#endif // BL_TEST
