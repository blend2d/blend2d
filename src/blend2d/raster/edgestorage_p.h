// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_EDGESTORAGE_P_H_INCLUDED
#define BLEND2D_RASTER_EDGESTORAGE_P_H_INCLUDED

#include "../math_p.h"
#include "../support/intops_p.h"
#include "../support/traits_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace BLRasterEngine {

//! Parametrized point used by edge builder that should represent either 16-bit
//! or 32-bit fixed point.
template<typename T>
struct EdgePoint {
  T x, y;

  BL_INLINE void reset(T x_, T y_) noexcept {
    this->x = x_;
    this->y = y_;
  }
};

template<typename CoordT>
struct alignas(8) EdgeVector {
  EdgeVector<CoordT>* next;
  size_t signBit : 1;
  size_t count : BLIntOps::bitSizeOf<size_t>() - 1;
  EdgePoint<CoordT> pts[1];

  static constexpr uint32_t minSizeOf() noexcept {
    return uint32_t(sizeof(EdgeVector<CoordT>) + sizeof(EdgePoint<CoordT>));
  }
};

template<typename CoordT>
struct EdgeList {
  EdgeVector<CoordT>* _first;
  EdgeVector<CoordT>* _last;

  BL_INLINE void reset() noexcept {
    _first = nullptr;
    _last = nullptr;
  }

  BL_INLINE bool empty() const noexcept { return _last == nullptr; }

  BL_INLINE EdgeVector<CoordT>* first() const noexcept { return _first; }
  BL_INLINE EdgeVector<CoordT>* last() const noexcept { return _last; }

  BL_INLINE void append(EdgeVector<CoordT>* item) noexcept {
    item->next = nullptr;
    if (empty()) {
      _first = item;
      _last = item;
    }
    else {
      _last->next = item;
      _last = item;
    }
  }
};

template<typename CoordT>
class EdgeStorage {
public:
  //! Start edge vectors of each band.
  EdgeList<CoordT>* _bandEdges;
  //! Length of `_bandEdges` array.
  uint32_t _bandCount;
  //! Capacity of `_bandEdges` array.
  uint32_t _bandCapacity;
  //! Height of a single band (in pixels).
  uint32_t _bandHeight;
  //! Shift to get a bandId from a fixed-point y coordinate.
  uint32_t _fixedBandHeightShift;
  //! Bounding box in fixed-point.
  BLBoxI _boundingBox;

  BL_INLINE EdgeStorage() noexcept { reset(); }
  BL_INLINE EdgeStorage(const EdgeStorage& other) noexcept = default;

  BL_INLINE void reset() noexcept {
    _bandEdges = nullptr;
    _bandCount = 0;
    _bandCapacity = 0;
    _bandHeight = 0;
    _fixedBandHeightShift = 0;
    resetBoundingBox();
  }

  BL_INLINE void clear() noexcept {
    if (!empty()) {
      size_t bandStart = bandStartFromBBox();
      size_t bandEnd = bandEndFromBBox();

      for (size_t i = bandStart; i < bandEnd; i++)
        _bandEdges[i].reset();

      resetBoundingBox();
    }
  }

  BL_INLINE bool empty() const noexcept { return _boundingBox.y0 == BLTraits::maxValue<int>(); }

  BL_INLINE EdgeList<CoordT>* bandEdges() const noexcept { return _bandEdges; }
  BL_INLINE uint32_t bandCount() const noexcept { return _bandCount; }
  BL_INLINE uint32_t bandCapacity() const noexcept { return _bandCapacity; }
  BL_INLINE uint32_t bandHeight() const noexcept { return _bandHeight; }
  BL_INLINE uint32_t fixedBandHeightShift() const noexcept { return _fixedBandHeightShift; }
  BL_INLINE const BLBoxI& boundingBox() const noexcept { return _boundingBox; }

  BL_INLINE void initData(EdgeList<CoordT>* bandEdges, uint32_t bandCount, uint32_t bandCapacity, uint32_t bandHeight) noexcept {
    _bandEdges = bandEdges;
    _bandCount = bandCount;
    _bandCapacity = bandCapacity;
    _bandHeight = bandHeight;
    _fixedBandHeightShift = BLIntOps::ctz(bandHeight) + BLPipeline::A8Info::kShift;
  }

  BL_INLINE void resetBoundingBox() noexcept {
    _boundingBox.reset(BLTraits::maxValue<int>(), BLTraits::maxValue<int>(), BLTraits::minValue<int>(), BLTraits::minValue<int>());
  }

  BL_INLINE uint32_t bandStartFromBBox() const noexcept {
    return unsigned(boundingBox().y0) >> fixedBandHeightShift();
  }

  BL_INLINE uint32_t bandEndFromBBox() const noexcept {
    // NOTE: Calculating `bandEnd` is tricky, because in some rare cases
    // the bounding box can end exactly at some band's initial coordinate.
    // In such case we don't know whether the band has data there or not,
    // so we must consider it initially.
    return blMin((unsigned(boundingBox().y1) >> fixedBandHeightShift()) + 1, bandCount());
  }

  BL_INLINE EdgeVector<CoordT>* flattenEdgeLinks() noexcept {
    EdgeList<int>* bandEdges = this->bandEdges();

    size_t bandId = bandStartFromBBox();
    size_t bandEnd = bandEndFromBBox();

    EdgeVector<CoordT>* first = bandEdges[bandId].first();
    EdgeVector<CoordT>* current = bandEdges[bandId].last();

    // The first band must always be non-null as it starts the edges.
    BL_ASSERT(first != nullptr);
    BL_ASSERT(current != nullptr);

    bandEdges[bandId].reset();
    while (++bandId < bandEnd) {
      EdgeVector<int>* bandFirst = bandEdges[bandId].first();
      if (!bandFirst)
        continue;
      current->next = bandFirst;
      current = bandEdges[bandId].last();
      bandEdges[bandId].reset();
    }

    return first;
  }
};

} // {BLRasterEngine}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_EDGESTORAGE_P_H_INCLUDED

