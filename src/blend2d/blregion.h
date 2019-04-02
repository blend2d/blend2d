// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLREGION_H
#define BLEND2D_BLREGION_H

#include "./blarray.h"
#include "./blgeometry.h"
#include "./blvariant.h"

//! \addtogroup blend2d_api_geometry
//! \{

// ============================================================================
// [Constants]
// ============================================================================

//! Region type.
BL_DEFINE_ENUM(BLRegionType) {
  //! Region is empty (has no rectangles).
  BL_REGION_TYPE_EMPTY = 0,
  //! Region has one rectangle (rectangular).
  BL_REGION_TYPE_RECT = 1,
  //! Region has more YX sorted rectangles.
  BL_REGION_TYPE_COMPLEX = 2,
  //! Count of region types.
  BL_REGION_TYPE_COUNT = 3
};

// ============================================================================
// [BLRegion - Core]
// ============================================================================

//! 2D region [C Interface - Impl].
struct BLRegionImpl {
  //! Union of either raw `data` & `size` members or their `view`.
  union {
    struct {
      //! Region data (Y/X sorted rectangles).
      BLBoxI* data;
      //! Region size (count of rectangles in the region).
      size_t size;
    };
    //! Region data and size as `BLRegionView`.
    BLRegionView view;
  };

  //! Region capacity (rectangles).
  size_t capacity;

  //! Reference count.
  volatile size_t refCount;
  //! Impl type.
  uint8_t implType;
  //! Impl traits.
  uint8_t implTraits;
  //! Memory pool data.
  uint16_t memPoolData;

  //! Reserved, must be zero.
  uint8_t reserved[4];
  //! Bounding box, empty regions have [0, 0, 0, 0].
  BLBoxI boundingBox;
};

//! 2D region [C Interface - Core].
struct BLRegionCore {
  BLRegionImpl* impl;
};

// ============================================================================
// [BLRegion - C++]
// ============================================================================

#ifdef __cplusplus
//! 2D region [C++ API].
//!
//! Region is a set of rectangles sorted and coalesced by their Y/X coordinates.
class BLRegion : public BLRegionCore {
public:
  //! \cond INTERNAL
  static constexpr const uint32_t kImplType = BL_IMPL_TYPE_REGION;
  //! \endcond

  BL_INLINE BLRegion() noexcept { this->impl = none().impl; }
  BL_INLINE BLRegion(BLRegion&& other) noexcept { blVariantInitMove(this, &other); }
  BL_INLINE BLRegion(const BLRegion& other) noexcept { blVariantInitWeak(this, &other); }
  BL_INLINE explicit BLRegion(BLRegionImpl* impl) noexcept { this->impl = impl; }

  BL_INLINE ~BLRegion() noexcept { blRegionReset(this); }

  BL_INLINE BLRegion& operator=(BLRegion&& other) noexcept { blRegionAssignMove(this, &other); return *this; }
  BL_INLINE BLRegion& operator=(const BLRegion& other) noexcept { blRegionAssignWeak(this, &other); return *this; }

  BL_INLINE bool operator==(const BLRegion& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLRegion& other) const noexcept { return !equals(other); }

  BL_INLINE explicit operator bool() const noexcept { return impl->size != 0; }

  //! Get whether the region is a built-in null instance.
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }
  //! Get type of the region, see `BLRegionType`.
  //!
  //! This inline method has also a C API equivalent `blRegionGetType()`.
  BL_INLINE uint32_t type() const noexcept { return uint32_t(blMin<size_t>(impl->size, BL_REGION_TYPE_COMPLEX)); }

  //! Get whether the region is empty.
  BL_INLINE bool empty() const noexcept { return impl->size == 0; }
  //! Get whether the region is one rectangle.
  BL_INLINE bool isRect() const noexcept { return impl->size == 1; }
  //! Get whether the region is complex.
  BL_INLINE bool isComplex() const noexcept { return impl->size > 1; }

  //! Get region size.
  BL_INLINE size_t size() const noexcept { return impl->size; }
  //! Get region capacity.
  BL_INLINE size_t capacity() const noexcept { return impl->capacity; }
  //! Get const pointer to the region data.
  BL_INLINE const BLBoxI* data() const noexcept { return impl->data; }
  //! Get a bounding box.
  BL_INLINE const BLBoxI& boundingBox() const noexcept { return impl->boundingBox; }

  //! Get the region data and size as `BLRegionView`.
  BL_INLINE const BLRegionView& view() const noexcept { return impl->view; }

  BL_INLINE BLResult reset() noexcept { return blRegionReset(this); }
  BL_INLINE BLResult clear() noexcept { return blRegionClear(this); }

  //! Reserve at least `n` boxes in this region.
  BL_INLINE BLResult reserve(size_t n) noexcept { return blRegionReserve(this, n); }
  //! Shrink the region data so it consumes only memory it requires.
  BL_INLINE BLResult shrink() noexcept { return blRegionShrink(this); }

  BL_INLINE BLResult assign(BLRegion&& other) noexcept { return blRegionAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLRegion& other) noexcept { return blRegionAssignWeak(this, &other); }
  BL_INLINE BLResult assignDeep(const BLRegion& other) noexcept { return blRegionAssignDeep(this, &other); }

  BL_INLINE BLResult assign(const BLBoxI& box) noexcept { return blRegionAssignBoxI(this, &box); }
  BL_INLINE BLResult assign(const BLBoxI* data, size_t n) noexcept { return blRegionAssignBoxIArray(this, data, n); }
  BL_INLINE BLResult assign(const BLRectI& rect) noexcept { return blRegionAssignRectI(this, &rect); }
  BL_INLINE BLResult assign(const BLRectI* data, size_t n) noexcept { return blRegionAssignRectIArray(this, data, n); }

  BL_INLINE BLResult combine(const BLRegion& region, uint32_t op) noexcept { return blRegionCombine(this, this, &region, op); }
  BL_INLINE BLResult combine(const BLBoxI& box, uint32_t op) noexcept { return blRegionCombineRB(this, this, &box, op); }

  //! Translate the region by the given point `pt`.
  //!
  //! Possible overflow will be handled by clipping to a maximum region boundary,
  //! so the final region could be smaller than the region before translation.
  BL_INLINE BLResult translate(const BLPointI& pt) noexcept { return blRegionTranslate(this, this, &pt); }
  //! Translate the region by the given point `pt` and clip it to the given `clipBox`.
  BL_INLINE BLResult translateAndClip(const BLPointI& pt, const BLBoxI& clipBox) noexcept { return blRegionTranslateAndClip(this, this, &pt, &clipBox); }
  //! Translate the region with `r` and clip it to the given `clipBox`.
  BL_INLINE BLResult intersectAndClip(const BLRegion& r, const BLBoxI& clipBox) noexcept { return blRegionIntersectAndClip(this, this, &r, &clipBox); }

  //! Get whether the region is equal to the `other` region.
  BL_INLINE bool equals(const BLRegion& other) const noexcept { return blRegionEquals(this, &other); }

  //! Tests if a given point `pt` is in region, returns `BLHitTest`.
  BL_INLINE uint32_t hitTest(const BLPointI& pt) const noexcept { return blRegionHitTest(this, &pt); }
  //! Tests if a given `box` is in region, returns `BLHitTest`.
  BL_INLINE uint32_t hitTest(const BLBoxI& box) const noexcept { return blRegionHitTestBoxI(this, &box); }

  static BL_INLINE const BLRegion& none() noexcept { return reinterpret_cast<const BLRegion*>(blNone)[kImplType]; }

  static BL_INLINE BLResult combine(BLRegion& dst, const BLRegion& a, const BLRegion& b, uint32_t op) noexcept { return blRegionCombine(&dst, &a, &b, op); }
  static BL_INLINE BLResult combine(BLRegion& dst, const BLRegion& a, const BLBoxI& b, uint32_t op) noexcept { return blRegionCombineRB(&dst, &a, &b, op); }
  static BL_INLINE BLResult combine(BLRegion& dst, const BLBoxI& a, const BLRegion& b, uint32_t op) noexcept { return blRegionCombineBR(&dst, &a, &b, op); }
  static BL_INLINE BLResult combine(BLRegion& dst, const BLBoxI& a, const BLBoxI& b, uint32_t op) noexcept { return blRegionCombineBB(&dst, &a, &b, op); }

  static BL_INLINE BLResult translate(BLRegion& dst, const BLRegion& r, const BLPointI& pt) noexcept { return blRegionTranslate(&dst, &r, &pt); }
  static BL_INLINE BLResult translateAndClip(BLRegion& dst, const BLRegion& r, const BLPointI& pt, const BLBoxI& clipBox) noexcept { return blRegionTranslateAndClip(&dst, &r, &pt, &clipBox); }
  static BL_INLINE BLResult intersectAndClip(BLRegion& dst, const BLRegion& a, const BLRegion& b, const BLBoxI& clipBox) noexcept { return blRegionIntersectAndClip(&dst, &a, &b, &clipBox); }
};
#endif

//! \}

#endif // BLEND2D_BLREGION_H
