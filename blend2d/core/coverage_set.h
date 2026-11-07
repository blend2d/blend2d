// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_COVERAGE_SET_H_INCLUDED
#define BLEND2D_CORE_COVERAGE_SET_H_INCLUDED

#include <blend2d/core/object.h>
#include <blend2d/core/object_layer.h>
#include <blend2d/core/span.h>

//! \addtogroup bl_c_api
//! \{

//! \name BLCoverageSet - C API
//! \{

BL_EXTERN_C_BEGIN

BL_API BLResult BL_CDECL bl_coverage_set_init(BLCoverageSetCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_coverage_set_init_move(BLCoverageSetCore* self, BLCoverageSetCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_coverage_set_init_weak(BLCoverageSetCore* self, const BLCoverageSetCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_coverage_set_init_range(BLCoverageSetCore* self, uint32_t range_start, uint32_t range_end) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_coverage_set_destroy(BLCoverageSetCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_coverage_set_reset(BLCoverageSetCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_coverage_set_assign_move(BLCoverageSetCore* self, BLCoverageSetCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_coverage_set_assign_weak(BLCoverageSetCore* self, const BLCoverageSetCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_coverage_set_assign_deep(BLCoverageSetCore* self, const BLCoverageSetCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_coverage_set_assign_range(BLCoverageSetCore* self, uint32_t range_start, uint32_t range_end) BL_NOEXCEPT_C;
BL_API bool BL_CDECL bl_coverage_set_is_empty(const BLCoverageSetCore* self) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL bl_coverage_set_get_page_count(const BLCoverageSetCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API size_t BL_CDECL bl_coverage_set_get_page_capacity(const BLCoverageSetCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API bool BL_CDECL bl_coverage_set_get_range(const BLCoverageSetCore* self, uint32_t* range_start_out, uint32_t* range_end_out) BL_NOEXCEPT_C;
BL_API uint32_t BL_CDECL bl_coverage_set_get_cardinality(const BLCoverageSetCore* self) BL_NOEXCEPT_C;
BL_API uint32_t BL_CDECL bl_coverage_set_get_cardinality_in_range(const BLCoverageSetCore* self, uint32_t range_start, uint32_t range_end) BL_NOEXCEPT_C;
BL_API bool BL_CDECL bl_coverage_set_contains(const BLCoverageSetCore* self, uint32_t index) BL_NOEXCEPT_C;
BL_API bool BL_CDECL bl_coverage_set_contains_in_range(const BLCoverageSetCore* self, uint32_t range_start, uint32_t range_end) BL_NOEXCEPT_C;
BL_API bool BL_CDECL bl_coverage_set_equals(const BLCoverageSetCore* a, const BLCoverageSetCore* b) BL_NOEXCEPT_C;
BL_API bool BL_CDECL bl_coverage_set_subsumes(const BLCoverageSetCore* a, const BLCoverageSetCore* b) BL_NOEXCEPT_C;
BL_API bool BL_CDECL bl_coverage_set_intersects(const BLCoverageSetCore* a, const BLCoverageSetCore* b) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_coverage_set_clear(BLCoverageSetCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_coverage_set_shrink(BLCoverageSetCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_coverage_set_optimize(BLCoverageSetCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_coverage_set_chop(BLCoverageSetCore* self, uint32_t range_start, uint32_t range_end) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_coverage_set_add(BLCoverageSetCore* self, uint32_t index) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_coverage_set_add_range(BLCoverageSetCore* self, uint32_t range_start, uint32_t range_end) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_coverage_set_add_ranges(BLCoverageSetCore* self, const BLCoverageSetRange* range_data, size_t range_count) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_coverage_set_remove(BLCoverageSetCore* self, uint32_t index) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_coverage_set_remove_range(BLCoverageSetCore* self, uint32_t range_start, uint32_t range_end) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_coverage_set_remove_ranges(BLCoverageSetCore* self, const BLCoverageSetRange* range_data, size_t range_count) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_coverage_set_combine(BLCoverageSetCore* dst, const BLCoverageSetCore* a, const BLCoverageSetCore* b, BLBooleanOp op) BL_NOEXCEPT_C;

BL_EXTERN_C_END

//! Coverage set [C API].
struct BLCoverageSetCore BL_INHERITS_FROM(BLObjectCore) {
  BL_DEFINE_OBJECT(BLCoverageSet, bl::ObjectLayer::ObjectInfo<
    BL_OBJECT_TYPE_COVERAGE_SET,
    bl::ObjectLayer::ObjectMode::kDyn,
    bl::ObjectLayer::ObjectLayout::kUnknown>
  )
};

//! \}

//! \cond INTERNAL
//! \name BLCoverageSet - Internals
//! \{

#ifdef __cplusplus
//! Coverage set [C Impl].
struct BLCoverageSetImpl BL_INHERITS_FROM(BLObjectImpl) {
  //! Indexes to master-plane pages.
  uint16_t mp_offsets[34];
  //! Reserved bytes for padding purposes to make this struct 16-byte aligned.
  uint8_t reserved[12];
};
#endif // __cplusplus

//! \}
//! \endcond

//! \}

//! \addtogroup bl_containers
//! \{

//! \name BLCoverageSet - C/C++ API
//! \{

//! Range to be used with \ref BLCoverageSet container, defined as `[start, end)` (end is exclusive).
struct BLCoverageSetRange {
  uint32_t start;
  uint32_t end;
};

//! \}

//! \name BLCoverageSet - C++ API
//! \{
#ifdef __cplusplus

//! Coverage set [C++ API].
//!
//! Coverage set is a bit-set container designed especially for covering unicode characters and glyphs in fonts. The
//! container can be filled either from unicode text input or can cover glyph indexes. If filled with unicode code
//! points it would contain a set of all distinct code-points added to it. In addition, it can be filled with
//! code-points that a particular font supports. In that case it would represent unicode font coverage, which can be
//! used by font-related queries, see \ref BLFontManager.
//!
//! Memory is managed via pages. Each page is 128 bytes long and describes either a master-plane or sub-plane.
//! A master-plane is a directory, which contains references to sub-planes and their cardinalities. A container that
//! needs a single sub-plane needs to have allocated a corresponding master-plane, that can reference it. However,
//! since each master plane can reference up to 32 sub-planes, it's unlikely to need many master-planes in common
//! workloads. For example if your characters fit into the first unicode plane (BMP) the container would need to
//! allocate at most 2 master-planes to cover all 64 sub-planes.
//!
//! In general, the container allows to store uniform planes without needing a reference - these are called special
//! planes and can have either all zero bits or be completely filled (thus all bits are 1). This is a useful space
//! optimization that pays off when the container is either very sparse (only few bits are set) or has long ranges,
//! where many are spanning across multiple sub-planes. In general, each range needs at most 2 sub-planes to cover
//! the end-points, which don't have to be sub-plane aligned.
class BLCoverageSet final : public BLCoverageSetCore {
public:
  //! \cond INTERNAL
  //! Object layout used by \ref BLCoverageSet.
  struct alignas(BLObjectDetail) Layout {
    //! Number of master-planes used.
    uint16_t mp_count;
    //! Number of pages in use (a single page can be used for either a master-plane header or plane-data itself).
    uint16_t page_count;
    //! Number of allocated pages (capacity of the container in page units).
    uint16_t page_capacity;
    //! A first page that has been freed - this page must be allocated next.
    uint16_t page_free_list;

    //! Pointer to \ref BLCoverageSetImpl data.
    BLObjectDetail::Ptr64<BLCoverageSetImpl> impl;

    //! Reserved, currently set to zero.
    uint16_t reserved[7];
    //! Aliases with `BLObjectDetail::type_byte` (not much reasons to access `type_byte` via this member).
    uint8_t type_byte;
    //! Aliases with `BLObjectDetail::meta_byte` (not much reasons to access `meta_byte` via this member).
    uint8_t meta_byte;
  };

  static_assert(sizeof(Layout) == sizeof(BLObjectDetail));
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  //! Creates a default-initialized coverage set, which uses a shared null data.
  BL_INLINE BLCoverageSet() noexcept {
    bl_coverage_set_init(this);
  }

  //! Moves `other` coverage set into a new instance, and resets `other`.
  BL_INLINE BLCoverageSet(BLCoverageSet&& other) noexcept {
    bl_coverage_set_init_move(this, &other);
  }

  //! Creates a weak-copy of `other` coverage set - the two instances would share the data of `other`.
  BL_INLINE BLCoverageSet(const BLCoverageSet& other) noexcept {
    bl_coverage_set_init_weak(this, &other);
  }

  //! Destroys the coverage set container.
  BL_INLINE ~BLCoverageSet() noexcept {
    if (bl::ObjectLayer::requires_destroy(this)) {
      bl_coverage_set_destroy(this);
    }
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  //! Tests whether the coverage set has a content - this means that it stores at least one non-zero bit.
  //!
  //! \note This is essentially the opposite of `is_empty()`.
  BL_INLINE_NODEBUG explicit operator bool() const noexcept { return !is_empty(); }

  //! Move assignment.
  //!
  //! \note The `other` coverage-set is reset by move assignment, so its state after the move operation is
  //! the same as a default constructed coverage set.
  BL_INLINE BLCoverageSet& operator=(BLCoverageSet&& other) noexcept {
    bl_coverage_set_assign_move(this, &other);
    return *this;
  }

  //! Copy assignment, performs weak copy of the data held by the `other` coverage set.
  BL_INLINE BLCoverageSet& operator=(const BLCoverageSet& other) noexcept {
    bl_coverage_set_assign_weak(this, &other);
    return *this;
  }

  //! Tests whether this coverage set and `other` are equal - which means that both have exactly the same bits set.
  BL_INLINE_NODEBUG bool operator==(const BLCoverageSet& other) const noexcept { return  equals(other); }
  //! Tests whether this coverage set and `other` aren't equal - which means that their bits set doesn't match
  BL_INLINE_NODEBUG bool operator!=(const BLCoverageSet& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  //! Clears the content of the coverage set and releases its data.
  //!
  //! After reset the container content matches a default constructed instance.
  BL_INLINE_NODEBUG BLResult reset() noexcept { return bl_coverage_set_reset(this); }

  //! Swaps the content of this coverage set with `other`.
  BL_INLINE_NODEBUG void swap(BLCoverageSetCore& other) noexcept { _d.swap(other._d); }

  //! \}

  //! \name Accessors
  //! \{

  //! Tests whether the coverage set is empty (has no bits set to 1).
  //!
  //! Returns `true` if the container's cardinality is zero.
  BL_INLINE_NODEBUG bool is_empty() const noexcept {
    return bl_coverage_set_is_empty(this);
  }

  //! Returns the number of pages used by the coverage set.
  //!
  //! \note A non-zero return value doesn't indicate that the cardinality is greater than 0. The container can have
  //! pages allocated, which have no bits set, which could be a result of some operations. So always use \ref is_empty()
  //! to check whether the set is empty.
  BL_INLINE_NODEBUG size_t page_count() const noexcept { return _d.layout<Layout>().page_count; }

  //! Returns the number of pages the coverage set has allocated.
  //!
  //! Each page is 128 bytes of data used for either master-plane or sub-plane.
  BL_INLINE_NODEBUG size_t page_capacity() const noexcept { return _d.layout<Layout>().page_capacity; }

  //! Returns the range of the coverage set as `[start_out, end_out)`.
  //!
  //! Returns true if the query was successful, false if the coverage set is empty.
  BL_INLINE bool get_range(uint32_t* range_start_out, uint32_t* range_end_out) const noexcept {
    return bl_coverage_set_get_range(this, range_start_out, range_end_out);
  }

  //! Returns the number of bits set in the coverage set.
  BL_INLINE uint32_t cardinality() const noexcept {
    return bl_coverage_set_get_cardinality(this);
  }

  //! Returns the number of bits set in `[range_start, range_end)`.
  BL_INLINE uint32_t cardinality_in_range(uint32_t range_start, uint32_t range_end) const noexcept {
    return bl_coverage_set_get_cardinality_in_range(this, range_start, range_end);
  }

  //! \}

  //! \name Equality & Comparison
  //! \{

  //! Returns whether this coverage set and `other` have exactly the same bits set.
  //!
  //! \note Equality means that the cardinality of both is the same and that both containers have exactly the same
  //! bits set. However, both containers can use different representations of various pages. For example if one
  //! container has more master-planes, but they are all zero, and master planes they have in common match each
  //! other they are considered equal.
  BL_INLINE bool equals(const BLCoverageSetCore& other) const noexcept {
    return bl_coverage_set_equals(this, &other);
  }

  //! \}

  //! \name Test Operations
  //! \{

  //! Returns whether the coverage set contains bit at the given `index`.
  BL_INLINE bool contains(uint32_t index) const noexcept {
    return bl_coverage_set_contains(this, index);
  }

  //! Returns whether there is at least one bit set in the given range `[range_start:range_end)`.
  BL_INLINE bool contains_in_range(uint32_t range_start, uint32_t range_end) const noexcept {
    return bl_coverage_set_contains_in_range(this, range_start, range_end);
  }

  //! Returns whether this coverage set subsumes `other`.
  BL_INLINE bool subsumes(const BLCoverageSetCore& other) const noexcept {
    return bl_coverage_set_subsumes(this, &other);
  }

  //! Returns whether this coverage set intersects with `other` (which means they have bits in common).
  BL_INLINE bool intersects(const BLCoverageSetCore& other) const noexcept {
    return bl_coverage_set_intersects(this, &other);
  }

  //! \}

  //! \name Content Manipulation
  //! \{

  //! Move assignment, the same as `operator=`, but returns `BLResult` instead of `this`.
  //!
  //! \remarks This function always succeeds and returns \ref BL_SUCCESS.
  BL_INLINE BLResult assign(BLCoverageSetCore&& other) noexcept {
    return bl_coverage_set_assign_move(this, &other);
  }

  //! Copy assignment, the same as `operator=`, but returns `BLResult` instead of `this`.
  //!
  //! \remarks This function always succeeds and returns \ref BL_SUCCESS.
  BL_INLINE BLResult assign(const BLCoverageSetCore& other) noexcept {
    return bl_coverage_set_assign_weak(this, &other);
  }

  //! Copy assignment, but creates a deep copy of the `other` coverage set instead of weak copy.
  BL_INLINE BLResult assign_deep(const BLCoverageSetCore& other) noexcept {
    return bl_coverage_set_assign_deep(this, &other);
  }

  //! Replaces the content of the coverage set by the given range.
  BL_INLINE BLResult assign_range(uint32_t range_start, uint32_t range_end) noexcept {
    return bl_coverage_set_assign_range(this, range_start, range_end);
  }

  //! Clears the content of the coverage set without releasing its dynamically allocated data, when possible.
  //!
  //! \remarks After this operation, the container is always empty. If the container was shared, a null impl would
  //! get assigned to it. However, if the container held dynamically allocated data and was unique, the data would
  //! be retained and additional operations would not need to allocate if the data is large enough.
  BL_INLINE BLResult clear() noexcept {
    return bl_coverage_set_clear(this);
  }

  //! Shrinks the capacity of the coverage set to match the actual content and optimizes the container to occupy as
  //! small space as possible by using special master-planes and sub-planes where applicable. Shrink always reallocates
  //! the coverage set in case there is data to shrink or keeps it as is if the container already occupies the smallest
  //! space possible.
  //!
  //! \remarks If the coverage set already occupies the smallest space possible, this function would not move any data.
  //! However, if shrink is possible, it would allocate new data, copy everything to this new location, and then free
  //! the previous data. By copying the content from one container to another the pages would be allocated consecutively.
  BL_INLINE BLResult shrink() noexcept {
    return bl_coverage_set_shrink(this);
  }

  //! Optimizes the coverage set in a way that it searches for sub-planes that are either all empty or full, and frees
  //! these sub-planes as they can use a built-in representation for exactly this purpose. In addition to sub-planes,
  //! master-planes are also analyzed and could be optimized the same way.
  //!
  //! \remarks Unlike \ref shrink(), \ref optimize() would not move data around if the container is mutable - it's
  //! designed for optimizing the data representation in case the container was heavily manipulated with the assumption
  //! that it will be manipulated again soon.
  BL_INLINE BLResult optimize() noexcept {
    return bl_coverage_set_optimize(this);
  }

  //! Bounds the coverage set to the given interval `[range_start:range_end)` - bits outside of this range will be removed.
  BL_INLINE BLResult chop(uint32_t range_start, uint32_t range_end) noexcept {
    return bl_coverage_set_chop(this, range_start, range_end);
  }

  //! Truncates the coverage set so it's maximum bit set is less than `range_end`, which removes all bits starting from
  //! `range_end` and onwards.
  //!
  //! \remarks This operation does the same as `chop(0, range_end)`.
  BL_INLINE BLResult truncate(uint32_t range_end) noexcept {
    return bl_coverage_set_chop(this, 0, range_end);
  }

  //! Adds a single bit `index` to the coverage set.
  BL_INLINE BLResult add(uint32_t index) noexcept {
    return bl_coverage_set_add(this, index);
  }

  //! Adds a range of bits in `[range_start:range_end)` range to the coverage set.
  //!
  //! \remarks Invalid or empty range is ignored and valid range is clamped to the container's limit. The function
  //! always succeeds with \ref BL_SUCCESS unless memory allocation failed, in that case \ref BL_ERROR_OUT_OF_MEMORY
  //! is returned.
  BL_INLINE BLResult add_range(uint32_t range_start, uint32_t range_end) noexcept {
    return bl_coverage_set_add_range(this, range_start, range_end);
  }

  //! \overload
  BL_INLINE BLResult add_range(BLCoverageSetRange range) noexcept {
    return bl_coverage_set_add_range(this, range.start, range.end);
  }

  //! Adds multiple ranges of bits set to the coverage set.
  //!
  //! \remarks Invalid or empty ranges are ignored and valid ranges are clamped to the container's limit. The function
  //! always succeeds with \ref BL_SUCCESS unless memory allocation failed, in that case \ref BL_ERROR_OUT_OF_MEMORY
  //! is returned.
  BL_INLINE BLResult add_ranges(BLSpanConvertible<const BLCoverageSetRange> auto&& ranges) noexcept {
    return bl_coverage_set_add_ranges(this, ranges.data(), ranges.size());
  }

  //! \overload
  BL_INLINE BLResult add_ranges(const BLCoverageSetRange* range_data, size_t range_count) noexcept {
    return bl_coverage_set_add_ranges(this, range_data, range_count);
  }

  //! Removes a single bit `index` from the coverage set.
  //!
  //! If `index` doesn't belong to an existing sub-plane, the function does nothing. If `index` is out of range,
  //! \ref BL_ERROR_INVALID_VALUE is returned.
  BL_INLINE BLResult remove(uint32_t index) noexcept {
    return bl_coverage_set_remove(this, index);
  }

  //! Removes a range of bits in `[range_start:range_end)` range from the coverage set.
  //!
  //! \remarks Invalid or empty range is ignored and valid range is clamped to the container's limit. The function
  //! always succeeds with \ref BL_SUCCESS unless memory allocation failed, in that case \ref BL_ERROR_OUT_OF_MEMORY
  //! is returned.
  BL_INLINE BLResult remove_range(uint32_t range_start, uint32_t range_end) noexcept {
    return bl_coverage_set_remove_range(this, range_start, range_end);
  }

  //! \overload
  BL_INLINE BLResult remove_range(BLCoverageSetRange range) noexcept {
    return bl_coverage_set_remove_range(this, range.start, range.end);
  }

  //! Removes multiple ranges from the coverage set.
  //!
  //! \remarks Invalid or empty ranges are ignored and valid ranges are clamped to the container's limit. The function
  //! always succeeds with \ref BL_SUCCESS unless memory allocation failed, in that case \ref BL_ERROR_OUT_OF_MEMORY
  //! is returned.
  BL_INLINE BLResult remove_ranges(BLSpanConvertible<const BLCoverageSetRange> auto&& ranges) noexcept {
    return bl_coverage_set_remove_ranges(this, ranges.data(), ranges.size());
  }

  //! \overload
  BL_INLINE BLResult remove_ranges(const BLCoverageSetRange* range_data, size_t range_count) noexcept {
    return bl_coverage_set_remove_ranges(this, range_data, range_count);
  }

  //! Combines this coverage set and `other` using the given boolean `op`.
  BL_INLINE BLResult combine(const BLCoverageSetCore& other, BLBooleanOp op) noexcept {
    return bl_coverage_set_combine(this, this, &other, op);
  }

  //! Combines `a` and `b` coverage sets using the given boolean `op` and stores the result into `dst`.
  static BL_INLINE BLResult combine(BLCoverageSetCore& dst, const BLCoverageSetCore& a, const BLCoverageSetCore& b, BLBooleanOp op) noexcept {
    return bl_coverage_set_combine(&dst, &a, &b, op);
  }

  //! \}
};

#endif

//! \}
//! \}

#endif // BLEND2D_CORE_COVERAGE_SET_H_INCLUDED
