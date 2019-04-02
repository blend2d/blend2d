// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLARRAYOPS_P_H
#define BLEND2D_BLARRAYOPS_P_H

#include "./blsupport_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLArrayOps - BinarySearch]
// ============================================================================

template<typename T, typename V>
static BL_INLINE size_t blBinarySearch(const T* array, size_t size, const V& value) noexcept {
  if (!size)
    return SIZE_MAX;

  const T* lower = array;
  while (size_t half = size / 2u) {
    const T* middle = lower + half;
    size -= half;
    if (middle[0] <= value)
      lower = middle;
  }

  size_t index = size_t(lower - array);
  BL_ASSUME(index != SIZE_MAX);

  return lower[0] == value ? index : SIZE_MAX;
}

template<typename T, typename V>
static BL_INLINE size_t blBinarySearchClosestFirst(const T* array, size_t size, const V& value) noexcept {
  if (!size)
    return 0;

  const T* lower = array;
  while (size_t half = size / 2u) {
    const T* middle = lower + half;
    size -= half;
    if (middle[0] < value)
      lower = middle;
  }

  if (lower[0] < value)
    lower++;

  return size_t(lower - array);
}

template<typename T, typename V>
static BL_INLINE size_t blBinarySearchClosestLast(const T* array, size_t size, const V& value) noexcept {
  if (!size)
    return 0;

  const T* lower = array;
  while (size_t half = size / 2u) {
    const T* middle = lower + half;
    size -= half;
    if (middle[0] <= value)
      lower = middle;
  }

  return size_t(lower - array);
}

// ============================================================================
// [BLArrayOps - InsertionSort | QuickSort]
// ============================================================================

enum BLSortOrder : uint32_t {
  BL_SORT_ORDER_ASCENDING  = 0,
  BL_SORT_ORDER_DESCENDING = 1
};

//! A helper class that provides comparison of any user-defined type that
//! implements `<` and `>` operators (primitive types are supported as well).
template<uint32_t Order = BL_SORT_ORDER_ASCENDING>
struct BLCompare {
  template<typename A, typename B>
  BL_INLINE int operator()(const A& a, const B& b) const noexcept {
    return (Order == BL_SORT_ORDER_ASCENDING) ? (a < b ? -1 : a > b ?  1 : 0)
                                              : (a < b ?  1 : a > b ? -1 : 0);
  }
};

//! Insertion sort.
template<typename T, typename Compare = BLCompare<BL_SORT_ORDER_ASCENDING>>
static BL_INLINE void blInsertionSort(T* base, size_t size, const Compare& cmp = Compare()) noexcept {
  for (T* pm = base + 1; pm < base + size; pm++)
    for (T* pl = pm; pl > base && cmp(pl[-1], pl[0]) > 0; pl--)
      std::swap(pl[-1], pl[0]);
}

//! Quick-sort implementation.
template<typename T, class Compare>
struct BLQuickSortImpl {
  enum : size_t {
    kStackSize = 64 * 2,
    kISortThreshold = 7
  };

  // Based on "PDCLib - Public Domain C Library" and rewritten to C++.
  static void sort(T* base, size_t size, const Compare& cmp) noexcept {
    T* end = base + size;
    T* stack[kStackSize];
    T** stackptr = stack;

    for (;;) {
      if ((size_t)(end - base) > kISortThreshold) {
        // We work from second to last - first will be pivot element.
        T* pi = base + 1;
        T* pj = end - 1;
        std::swap(base[(size_t)(end - base) / 2], base[0]);

        if (cmp(*pi  , *pj  ) > 0) std::swap(*pi  , *pj  );
        if (cmp(*base, *pj  ) > 0) std::swap(*base, *pj  );
        if (cmp(*pi  , *base) > 0) std::swap(*pi  , *base);

        // Now we have the median for pivot element, entering main loop.
        for (;;) {
          while (pi < pj   && cmp(*++pi, *base) < 0) continue; // Move `i` right until `*i >= pivot`.
          while (pj > base && cmp(*--pj, *base) > 0) continue; // Move `j` left  until `*j <= pivot`.

          if (pi > pj) break;
          std::swap(*pi, *pj);
        }

        // Move pivot into correct place.
        std::swap(*base, *pj);

        // Larger subfile base / end to stack, sort smaller.
        if (pj - base > end - pi) {
          // Left is larger.
          *stackptr++ = base;
          *stackptr++ = pj;
          base = pi;
        }
        else {
          // Right is larger.
          *stackptr++ = pi;
          *stackptr++ = end;
          end = pj;
        }
        BL_ASSERT(stackptr <= stack + kStackSize);
      }
      else {
        blInsertionSort(base, (size_t)(end - base), cmp);
        if (stackptr == stack)
          break;
        end = *--stackptr;
        base = *--stackptr;
      }
    }
  }
};

//! Quick sort.
template<typename T, class Compare = BLCompare<BL_SORT_ORDER_ASCENDING>>
static BL_INLINE void blQuickSort(T* base, size_t size, const Compare& cmp = Compare()) noexcept {
  BLQuickSortImpl<T, Compare>::sort(base, size, cmp);
}

//! \}
//! \endcond

#endif // BLEND2D_BLARRAYOPS_P_H
