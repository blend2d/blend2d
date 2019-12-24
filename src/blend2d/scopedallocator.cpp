// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "./api-build_p.h"
#include "./scopedallocator_p.h"
#include "./support_p.h"

// ============================================================================
// [BLScopedAllocator]
// ============================================================================

void* BLScopedAllocator::alloc(size_t size, size_t alignment) noexcept {
  // First try to allocate from the local memory pool.
  uint8_t* p = blAlignUp(poolPtr, alignment);
  size_t remain = size_t(blUSubSaturate((uintptr_t)poolEnd, (uintptr_t)p));

  if (remain >= size) {
    poolPtr = p + size;
    return p;
  }

  // Bail to malloc if local pool was either not provided or didn't have the
  // required capacity.
  size_t sizeWithOverhead = size + sizeof(Link) + (alignment - 1);
  p = static_cast<uint8_t*>(malloc(sizeWithOverhead));

  if (p == nullptr)
    return nullptr;

  reinterpret_cast<Link*>(p)->next = links;
  links = reinterpret_cast<Link*>(p);

  return blAlignUp(p + sizeof(Link), alignment);
}

void BLScopedAllocator::reset() noexcept {
  Link* link = links;
  while (link) {
    Link* next = link->next;
    free(link);
    link = next;
  }

  links = nullptr;
  poolPtr = poolMem;
}
