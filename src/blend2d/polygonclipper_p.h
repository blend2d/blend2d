// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_POLYGONCLIPPER_P_H_INCLUDED
#define BLEND2D_POLYGONCLIPPER_P_H_INCLUDED

#include "api.h"
#include "api-internal_p.h"
#include "support/arenaallocator_p.h"
#include "support/arenalist_p.h"
#include "polygonclipper.h"
#include "geometry.h"
#include "path.h"

#include <vector>
#include <array>
#include <set>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

//! Status flags for sweep events in a polygon clipper.
enum class SweepEventFlags : uint32_t {
    kNoFlags                        = 0u,

    /// Indicates whether the sweep event is the start of a line segment.
    /// If set, the event marks the start; if not, it is the end of the segment.
    kIsLeft                         = 0x00000001u,

    /// Specifies the polygon to which the sweep event belongs. If set,
    /// the event is part of the "subject" polygon; otherwise, it's part of
    /// the "clipping" polygon.
    kIsSubject                      = 0x00000002u,

    /// Determines the relative vertical position of the polygon containing
    /// the edge. If set, the polygon is "below" the edge; otherwise, it is
    /// "above" the edge.
    kIsInOut                        = 0x00000004u,

    /// Indicates that the current edge is inside the other polygon.
    /// For an edge belonging to the "subject" polygon, if set, it signifies
    /// containment within the "clipping" polygon.
    kIsInside                       = 0x00000008u,

    kSegmentNonContributing         = 0x00000010u,
    kSegmentSameTransition          = 0x00000020u,
    kSegmentDifferentTransition     = 0x00000040u,
};

BL_DEFINE_ENUM_FLAGS(SweepEventFlags)

//! Point specified as [x, y] using `int64` as a storage type.
struct BLPointI64 {
    int64_t x;
    int64_t y;

#ifdef __cplusplus
    BL_INLINE_NODEBUG BLPointI64() noexcept = default;
    BL_INLINE_NODEBUG constexpr BLPointI64(const BLPointI64&) noexcept = default;

    BL_INLINE_NODEBUG constexpr BLPointI64(int64_t x, int64_t y) noexcept
        : x(x),
        y(y) {}

    BL_INLINE_NODEBUG BLPointI64& operator=(const BLPointI64& other) noexcept = default;

    BL_NODISCARD
        BL_INLINE_NODEBUG bool operator==(const BLPointI64& other) const noexcept { return  equals(other); }

    BL_NODISCARD
        BL_INLINE_NODEBUG bool operator!=(const BLPointI64& other) const noexcept { return !equals(other); }

    BL_INLINE_NODEBUG void reset() noexcept { reset(0, 0); }
    BL_INLINE_NODEBUG void reset(const BLPointI64& other) noexcept { reset(other.x, other.y); }
    BL_INLINE_NODEBUG void reset(int64_t xValue, int64_t yValue) noexcept {
        x = xValue;
        y = yValue;
    }

    BL_NODISCARD
        BL_INLINE_NODEBUG bool equals(const BLPointI64& other) const noexcept {
        return bool(unsigned(blEquals(x, other.x)) &
                    unsigned(blEquals(y, other.y)));
    }

    BL_NODISCARD
        BL_INLINE_NODEBUG bool operator<(const BLPointI64& other) const {
        return x < other.x || (x == other.x && y < other.y);
    }
#endif
};

static BL_INLINE_NODEBUG constexpr BLPointI64 operator-(const BLPointI64& self) noexcept { return BLPointI64(-self.x, -self.y); }

static BL_INLINE_NODEBUG constexpr BLPointI64 operator+(const BLPointI64& a, int64_t b) noexcept { return BLPointI64(a.x + b, a.y + b); }
static BL_INLINE_NODEBUG constexpr BLPointI64 operator-(const BLPointI64& a, int64_t b) noexcept { return BLPointI64(a.x - b, a.y - b); }
static BL_INLINE_NODEBUG constexpr BLPointI64 operator*(const BLPointI64& a, int64_t b) noexcept { return BLPointI64(a.x * b, a.y * b); }

static BL_INLINE_NODEBUG constexpr BLPointI64 operator+(int64_t a, const BLPointI64& b) noexcept { return BLPointI64(a + b.x, a + b.y); }
static BL_INLINE_NODEBUG constexpr BLPointI64 operator-(int64_t a, const BLPointI64& b) noexcept { return BLPointI64(a - b.x, a - b.y); }
static BL_INLINE_NODEBUG constexpr BLPointI64 operator*(int64_t a, const BLPointI64& b) noexcept { return BLPointI64(a * b.x, a * b.y); }

static BL_INLINE_NODEBUG constexpr BLPointI64 operator+(const BLPointI64& a, const BLPointI64& b) noexcept { return BLPointI64(a.x + b.x, a.y + b.y); }
static BL_INLINE_NODEBUG constexpr BLPointI64 operator-(const BLPointI64& a, const BLPointI64& b) noexcept { return BLPointI64(a.x - b.x, a.y - b.y); }
static BL_INLINE_NODEBUG constexpr BLPointI64 operator*(const BLPointI64& a, const BLPointI64& b) noexcept { return BLPointI64(a.x * b.x, a.y * b.y); }

static BL_INLINE_NODEBUG BLPointI64& operator+=(BLPointI64& a, int64_t b) noexcept { a.reset(a.x + b, a.y + b); return a; }
static BL_INLINE_NODEBUG BLPointI64& operator-=(BLPointI64& a, int64_t b) noexcept { a.reset(a.x - b, a.y - b); return a; }
static BL_INLINE_NODEBUG BLPointI64& operator*=(BLPointI64& a, int64_t b) noexcept { a.reset(a.x * b, a.y * b); return a; }
static BL_INLINE_NODEBUG BLPointI64& operator/=(BLPointI64& a, int64_t b) noexcept { a.reset(a.x / b, a.y / b); return a; }

static BL_INLINE_NODEBUG BLPointI64& operator+=(BLPointI64& a, const BLPointI64& b) noexcept { a.reset(a.x + b.x, a.y + b.y); return a; }
static BL_INLINE_NODEBUG BLPointI64& operator-=(BLPointI64& a, const BLPointI64& b) noexcept { a.reset(a.x - b.x, a.y - b.y); return a; }
static BL_INLINE_NODEBUG BLPointI64& operator*=(BLPointI64& a, const BLPointI64& b) noexcept { a.reset(a.x * b.x, a.y * b.y); return a; }
static BL_INLINE_NODEBUG BLPointI64& operator/=(BLPointI64& a, const BLPointI64& b) noexcept { a.reset(a.x / b.x, a.y / b.y); return a; }

using BLVectorI64 = BLPointI64;

struct Segment
{
    constexpr inline Segment() = default;
    constexpr inline Segment(const BLPointI64& p1, const BLPointI64& p2, bool isSubject) :
        _p1(p1), _p2(p2), _isSubject(isSubject) {

    }

    BLPointI64 _p1;
    BLPointI64 _p2;
    bool _isSubject = false;

    BLVectorI64 tangent() const noexcept;
    int64_t lengthSquared() const noexcept;
    bool isEmpty() const noexcept;

    void checkOrientation();
};

struct SweepEvent {
    BLPointI64 _pt;
    SweepEvent* _opposite{};
    SweepEventFlags _flags{};

    BL_INLINE BL_CONSTEXPR bool isLeft() const noexcept { return blTestFlag(_flags, SweepEventFlags::kIsLeft); }
    BL_INLINE BL_CONSTEXPR bool isRight() const noexcept { return !isRight(); }
    BL_INLINE BL_CONSTEXPR bool isSubject() const noexcept { return blTestFlag(_flags, SweepEventFlags::kIsSubject); }
    BL_INLINE BL_CONSTEXPR bool isClipping() const noexcept { return !isSubject(); }
    BL_INLINE BL_CONSTEXPR bool isInOut() const noexcept { return blTestFlag(_flags, SweepEventFlags::kIsInOut); }
    BL_INLINE BL_CONSTEXPR bool isInside() const noexcept { return blTestFlag(_flags, SweepEventFlags::kIsInside); }

    BL_INLINE BL_CONSTEXPR bool isSegmentNonContributing() const noexcept { return blTestFlag(_flags, SweepEventFlags::kSegmentNonContributing); }
    BL_INLINE BL_CONSTEXPR bool isSegmentSameTransition() const noexcept { return blTestFlag(_flags, SweepEventFlags::kSegmentSameTransition); }
    BL_INLINE BL_CONSTEXPR bool isSegmentDifferentTransition() const noexcept { return blTestFlag(_flags, SweepEventFlags::kSegmentDifferentTransition); }
    BL_INLINE BL_CONSTEXPR bool isSegmentNormal() const noexcept { return !blTestFlag(_flags, SweepEventFlags::kSegmentNonContributing | SweepEventFlags::kSegmentSameTransition | SweepEventFlags::kSegmentDifferentTransition); }

    Segment getSegment() const noexcept;

    /// Determines whether a point (pt) is located above a line segment. The function
    /// yields consistent results regardless of whether it is invoked from the left or
    /// the right endpoint of the segment. If the point is above the segment (considering
    /// the y-axis), it returns true; otherwise, it returns false.
    /// \param pt The point to be tested.
    /// \return A boolean value indicating whether the point is above the segment.
    bool isPointAbove(const BLPointI64& pt) const noexcept;

    /// Determines whether a point (pt) is located below a line segment. The function
    /// yields consistent results regardless of whether it is invoked from the left or
    /// the right endpoint of the segment. If the point is below the segment (considering
    /// the y-axis), it returns true; otherwise, it returns false.
    /// \param pt The point to be tested.
    /// \return A boolean value indicating whether the point is below the segment.
    bool isPointBelow(const BLPointI64& pt) const noexcept;
};

struct LineEquation
{
    int64_t a{};
    int64_t b{};
    int64_t c{};
};

struct SegmentIntersection
{
    int intersect1 = 0;
    int intersect2 = 0;

    std::array<BLPointI64, 2> intersectionPoints1;
    std::array<BLPointI64, 2> intersectionPoints2;
};

class SegmentUtils
{
public:
    static BLVectorI64 getTangent(const Segment& segment) noexcept;
    static BLVectorI64 getNormal(const Segment& segment) noexcept;

    static LineEquation getLineEquation(const Segment& segment) noexcept;
    static SegmentIntersection getSegmentIntersection(const Segment& segment1, const Segment& segment2) noexcept;

    /// Returns true, if the point 'p' which lies on the line defined by the segment,
    /// lies also on this segment. It is assumed, that the point 'p' lies on this line,
    /// if it does not, then behavior of this function is undefined.
    /// \param segment Segment
    /// \param p Point
    /// \returns True, if the point lies in the segment.
    static bool liesOnSegment(const Segment& segment, const BLPointI64& p) noexcept;

    /// Returns true, if the point 'p' is one of end points of the segment.
    static bool isEndpointOfSegment(const Segment& segment, const BLPointI64& p) noexcept;

    /// Returns true, if the point 'p' which lies on the line defined by the segment,
    /// is a interior point of this segment. It is assumed, that the point 'p' lies on this line,
    /// if it does not, then behavior of this function is undefined.
    /// \param segment Segment
    /// \param p Point
    /// \returns True, if the point is interior point of the segment.
    static bool isInteriorPointOfSegment(const Segment& segment, const BLPointI64& p) noexcept;

    /// Returns true, if segments are collinear (i.e. both of them
    /// define exactly the same line).
    static bool isCollinear(const Segment& s1, const Segment& s2) noexcept;

    /// Returns true, if point \p p lies on the line defined by segment \p s.
    static bool isCollinear(const Segment& s, const BLPointI64& p) noexcept;
};

class PolygonConnectorPathItemNode : public ArenaListNode<PolygonConnectorPathItemNode> {
public:
    inline PolygonConnectorPathItemNode() = default;
    inline PolygonConnectorPathItemNode(const BLPointI64& pt) : _pt(pt) { }

    BLPointI64 _pt;
};

class PolygonConnectorPathNode : public ArenaListNode<PolygonConnectorPathNode> {
public:

    bool empty() const noexcept { return _path.empty(); }
    PolygonConnectorPathItemNode* front() const noexcept { return _path.first(); }
    PolygonConnectorPathItemNode* back() const noexcept { return _path.last(); }

    void append(PolygonConnectorPathItemNode* node) noexcept { _path.append(node); }
    void prepend(PolygonConnectorPathItemNode* node) noexcept { _path.prepend(node); }

    PolygonConnectorPathItemNode* popFront() noexcept { return _path.popFirst(); }
    PolygonConnectorPathItemNode* popBack() noexcept { return _path.pop(); }

private:
    ArenaList<PolygonConnectorPathItemNode> _path;
};

class PolygonConnector {
public:
    inline PolygonConnector(ArenaAllocator& allocator) noexcept : _allocator(allocator) {}
    ~PolygonConnector() noexcept;

    void reset() noexcept;
    void addEdge(const BLPointI64& p1, const BLPointI64& p2) noexcept;
    const BLPath& getPath() const noexcept { return _path; }

    double getScaleInverted() const;
    void setScaleInverted(double newScaleInverted);

private:
    struct FindPathResult {
        PolygonConnectorPathNode* _node{};
        bool _isFront{};
    };

    void removePolygonPath(PolygonConnectorPathNode* node) noexcept;

    FindPathResult find(const BLPointI64& point) const noexcept;

    BLPoint getUnscaledPoint(const BLPointI64& p) const noexcept;

    ArenaAllocator& _allocator;
    ArenaList<PolygonConnectorPathNode> _openPolygons;
    ArenaPool<PolygonConnectorPathNode> _poolPaths;
    ArenaPool<PolygonConnectorPathItemNode> _poolItemNodes;
    BLPath _path;
    double _scaleInverted = 0;
};

class PolygonClipperImpl {
public:
    PolygonClipperImpl(size_t blockSize, void* staticData, size_t staticSize) noexcept;

    void setScale(double scale) noexcept;
    void setOperator(BLBooleanOperator newOperator) noexcept;

    void addSegment(const BLPoint& p1, const BLPoint& p2, bool isSubject) noexcept;

    /// Performs polygon clipping
    BLResult perform() noexcept;

    const BLPath& getPath() const noexcept;

private:
    void createProcessedSegments();
    void createSweepEvents(std::vector<SweepEvent*>& queue);

    bool isSelfOverlapping(SweepEvent* ev1, SweepEvent* ev2) const noexcept;
    SweepEvent* getPreviousEvent(std::set<SweepEvent*>& statusLine, SweepEvent* event) const noexcept;
    SweepEvent* getNextEvent(std::set<SweepEvent*>& statusLine, SweepEvent* event) const noexcept;

    BLResult updateFlags(SweepEvent* eventPrevPrev,
                         SweepEvent* eventPrev,
                         SweepEvent* eventCurr,
                         SweepEvent* eventNext) noexcept;

    void updateOverlappedEvents(SweepEvent* event1, SweepEvent* event2);

    void updateResult(BLResult& oldResult, BLResult newResult) const noexcept;

    SweepEvent* allocSweepEvent() noexcept;
    void freeSweepEvent(SweepEvent* event) noexcept;

    /// Based on the operator (intersection, union, difference,
    /// symmetric difference), it is decided whether to add the edge to the resulting
    /// polygon. Event must represent the starting point.
    /// \param edge The edge we want to insert
    void addResultEdge(SweepEvent* edge);


    BLBooleanOperator _operator = BL_BOOLEAN_OPERATOR_UNION;

    ArenaAllocator _memoryAllocator;
    ArenaPool<SweepEvent> _sweepEventPool;

    std::vector<Segment> _originalSegments;
    std::vector<Segment> _processedSegments;

    double _scale = 1000;

    PolygonConnector _connector;
};

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_POLYGONCLIPPER_P_H_INCLUDED
