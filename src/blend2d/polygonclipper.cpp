// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "api-impl.h"
#include "polygonclipper.h"
#include "polygonclipper_p.h"
#include "geometry_p.h"
#include "matrix.h"

#include <set>
#include <map>
#include <algorithm>

BLPolygonClipper::BLPolygonClipper() noexcept :
    _impl(nullptr) {
    _impl = reinterpret_cast<bl::PolygonClipperImpl*>(_buffer);

    void* staticData = _buffer + sizeof(bl::PolygonClipperImpl);
    constexpr size_t staticSize = MEMORY_BLOCK_SIZE - sizeof(bl::PolygonClipperImpl);
    blCallCtor(*_impl, MEMORY_BLOCK_SIZE, staticData, staticSize);
}

BLPolygonClipper::~BLPolygonClipper() noexcept {
    blCallDtor(*_impl);
}

void BLPolygonClipper::setScale(double scale) noexcept {
    _impl->setScale(scale);
}

BLResult BLPolygonClipper::perform() noexcept {
    return _impl->perform();
}

void BLPolygonClipper::addEdge(const BLPoint& p1, const BLPoint& p2, bool isSubject) noexcept {
    _impl->addSegment(p1, p2, isSubject);
}

void BLPolygonClipper::setOperator(BLBooleanOperator booleanOperator) noexcept {
    _impl->setOperator(booleanOperator);
}

const BLPath& BLPolygonClipper::getPath() const noexcept {
    return _impl->getPath();
}

namespace bl {

static BL_INLINE int64_t getSignedAreaTimesTwo(const BLPointI64& p1, const BLPointI64& p2, const BLPointI64& p3) {
    return (p1.x * p2.y - p1.x * p3.y + p2.x * p3.y - p2.x * p1.y + p3.x * p1.y - p3.x * p2.y);
}

static BL_INLINE bool isClockwise(const BLPointI64& p1, const BLPointI64& p2, const BLPointI64& p3) {
    return getSignedAreaTimesTwo(p1, p2, p3) < 0;
}

static BL_INLINE bool isCounterClockwise(const BLPointI64& p1, const BLPointI64& p2, const BLPointI64& p3) {
    return getSignedAreaTimesTwo(p1, p2, p3) > 0;
}

static BL_INLINE bool isCollinear(const SweepEvent* a, const SweepEvent* b) {
    const int64_t area1 = getSignedAreaTimesTwo(a->_pt, a->_opposite->_pt, b->_pt);
    const int64_t area2 = getSignedAreaTimesTwo(a->_pt, a->_opposite->_pt, b->_opposite->_pt);
    const int64_t totalArea = blAbs(area1) + blAbs(area2);
    return totalArea == 0;
}

struct SweepEventComparator {
    int operator()(const SweepEvent* a, const SweepEvent* b) const noexcept {
        if (a == b)
            return 0;

        // Compare x-coordinates first as we are sweeping from left to right.
        if (a->_pt.x < b->_pt.x)
            return -1;
        if (a->_pt.x > b->_pt.x)
            return 1;

        // If we have the same coordinate, we process the point with the lower y-coordinate first.
        if (a->_pt.y < b->_pt.y)
            return -1;
        if (a->_pt.y > b->_pt.y)
            return 1;

        // The point is the same in both events. We process right endpoint events first.
        if (a->isLeft() != b->isLeft())
            return a->isLeft() ? 1 : -1;

        // Otherwise, the order should remain as it is in the status line.
        return a->isPointAbove(b->_opposite->_pt) ? -1 : 1;
    }
};

struct SweepEventLessThan {
    bool operator()(const SweepEvent* a, const SweepEvent* b) const noexcept {
        return SweepEventComparator()(a, b) == -1;
    }
};

struct StatusLineComparator {
    BL_INLINE BL_CONSTEXPR StatusLineComparator() { }

    int operator()(const SweepEvent* a, const SweepEvent* b) const noexcept {
        if (a == b)
            return 0;

        SweepEventComparator sweepEventComparator;

        if (!isCollinear(a, b)) {
            // Segments are not collinear

            // Both segments start at the same point. Determine which line is above the other
            // in the sweep line by considering the second point of each segment. If b's right
            // point is above the line segment defined by points (a->_pt, a->_opposite->_pt),
            // it indicates that segment a precedes segment b in the status line.
            if (a->_pt == b->_pt)
                return a->isPointAbove(b->_opposite->_pt) ? -1 : 1;

            // Test whether event a is inserted before event b in the priority queue.
            // If a < b, then test whether b's start point is above the line segment
            // defined by points (a->_pt, a->_opposite->_pt). If so, a must precede
            // b in the status line. Otherwise, b must precede a in the status line.
            if (sweepEventComparator(a, b) < 0) {
                return a->isPointAbove(b->_pt) ? -1 : 1;
            } else {
                return b->isPointAbove(a->_pt) ? 1 : -1;
            }
        } else {
            // Segments are collinear
            if (a->_pt == b->_pt) {
                if (a->_opposite->_pt == b->_opposite->_pt) {
                    return a->isSubject() ? -1 : 1;
                } else {
                    return sweepEventComparator(a->_opposite, b->_opposite);
                }
            } else {
                return sweepEventComparator(a, b);
            }
        }
    }
};

BLVectorI64 Segment::tangent() const noexcept {
    return _p2 - _p1;
}

int64_t Segment::lengthSquared() const noexcept {
    BLVectorI64 v = tangent();
    return v.x * v.x + v.y * v.y;
}

bool Segment::isEmpty() const noexcept {
    return _p1 == _p2;
}

void Segment::checkOrientation() {
    if (_p2 < _p1) {
        std::swap(_p1, _p2);
    }
}

PolygonClipperImpl::PolygonClipperImpl(size_t blockSize, void* staticData, size_t staticSize) noexcept :
    _memoryAllocator(blockSize, 1, staticData, staticSize),
    _connector(_memoryAllocator){
    _connector.setScaleInverted(1.0 / _scale);
}

void PolygonClipperImpl::setScale(double scale) noexcept {
    _scale = scale;
    _connector.setScaleInverted(1.0 / _scale);
}

void PolygonClipperImpl::setOperator(BLBooleanOperator newOperator) noexcept {
    _operator = newOperator;
}

void PolygonClipperImpl::addSegment(const BLPoint& p1, const BLPoint& p2, bool isSubject) noexcept {
    BLPointI64 scaledP1(static_cast<int64_t>(p1.x * _scale), static_cast<int64_t>(p1.y * _scale));
    BLPointI64 scaledP2(static_cast<int64_t>(p2.x * _scale), static_cast<int64_t>(p2.y * _scale));

    if (scaledP1 != scaledP2)
        _originalSegments.emplace_back(scaledP1, scaledP2, isSubject);
}

BLResult PolygonClipperImpl::perform() noexcept {
    BLResult result = BL_SUCCESS;

    // Create subdivided segments, so no intersections
    // with other segments do exist (with exception of endpoints).
    createProcessedSegments();

    // Create sweep events
    std::vector<SweepEvent*> sweepEvents;
    createSweepEvents(sweepEvents);

    std::set<SweepEvent*> statusLine;

    for (SweepEvent* event : sweepEvents) {
        if (event->isLeft()) {
            // Start point of the segment. We must insert current
            // event into the status line. Only sweep events of left (starting)
            // points of the segments are in the status line.
            statusLine.insert(event);

            SweepEvent* evPrev = getPreviousEvent(statusLine, event);
            SweepEvent* evPrevPrev = getPreviousEvent(statusLine, evPrev);
            SweepEvent* evNext = getNextEvent(statusLine, event);
            updateResult(result, updateFlags(evPrevPrev, evPrev, event, evNext));
        } else {
            // Add edge to polygon connector
            addResultEdge(event->_opposite);

            // Clear sweep events
            statusLine.erase(event->_opposite);
            freeSweepEvent(event->_opposite);
            freeSweepEvent(event);
        }

        if (result != BL_SUCCESS)
            break;
    }

    return result;
}

const BLPath& PolygonClipperImpl::getPath() const noexcept {
    return _connector.getPath();
}

struct BentleyOttmanEvent {
    BLPointI64 point;
    size_t segmentIndex{};
    bool isStart{};

    bool operator<(const BentleyOttmanEvent& other) const {
        return (point < other.point) || (point == other.point && isStart && !other.isStart);
    }
};

void PolygonClipperImpl::createProcessedSegments() {
    std::vector<BentleyOttmanEvent> events;
    for (size_t i = 0; i < _originalSegments.size(); ++i) {
        Segment segment = _originalSegments[i];
        segment.checkOrientation();

        events.push_back({segment._p1, i, true});
        events.push_back({segment._p2, i, false});
    }

    std::stable_sort(events.begin(), events.end());

    std::set<size_t> activeSegments;
    std::map<std::size_t, std::vector<BLPointI64>> segmentIntersections;

    // Find all intersections between the segments
    for (const auto& event : events) {
        size_t currentSegmentIndex = event.segmentIndex;

        if (event.isStart) {
            for (const auto& activeSegmentIndex : activeSegments) {
                SegmentIntersection intersections = SegmentUtils::getSegmentIntersection(_originalSegments[currentSegmentIndex], _originalSegments[activeSegmentIndex]);

                for (int i = 0; i < intersections.intersect1; ++i)
                    segmentIntersections[currentSegmentIndex].push_back(intersections.intersectionPoints1[i]);

                for (int i = 0; i < intersections.intersect2; ++i)
                    segmentIntersections[activeSegmentIndex].push_back(intersections.intersectionPoints2[i]);
            }
            activeSegments.insert(currentSegmentIndex);
        } else {
            activeSegments.erase(currentSegmentIndex);
        }
    }

    _processedSegments.clear();

    // Create subdivided, processed elements
    for (size_t i = 0; i < _originalSegments.size(); ++i) {
        Segment originalSegment = _originalSegments[i];
        originalSegment.checkOrientation();

        auto it = segmentIntersections.find(i);
        if (it == segmentIntersections.cend()) {
            _processedSegments.push_back(originalSegment);
        } else {
            std::vector<BLPointI64> points = std::move(it->second);
            points.push_back(originalSegment._p1);
            points.push_back(originalSegment._p2);
            std::sort(points.begin(), points.end());
            points.erase(std::unique(points.begin(), points.end()), points.end());

            for (size_t pi = 1; pi < points.size(); ++pi) {
                Segment segment;
                segment._p1 = points[pi - 1];
                segment._p2 = points[pi];
                segment._isSubject = originalSegment._isSubject;
                segment.checkOrientation();

                _processedSegments.push_back(segment);
            }
        }
    }
}

void PolygonClipperImpl::createSweepEvents(std::vector<SweepEvent*>& queue) {
    for (const Segment& segment : _processedSegments) {
        SweepEvent* ev1 = allocSweepEvent();
        SweepEvent* ev2 = allocSweepEvent();

        ev1->_pt = segment._p1;
        ev2->_pt = segment._p2;

        ev1->_opposite = ev2;
        ev2->_opposite = ev1;

        if (segment._isSubject) {
            ev1->_flags |= SweepEventFlags::kIsSubject;
            ev2->_flags |= SweepEventFlags::kIsSubject;
        }

        if (SweepEventComparator()(ev1, ev2) < 0)
            ev1->_flags |= SweepEventFlags::kIsLeft;
        else
            ev2->_flags |= SweepEventFlags::kIsLeft;

        queue.push_back(ev1);
        queue.push_back(ev2);
    }

    std::sort(queue.begin(), queue.end(), SweepEventLessThan());
}

LineEquation SegmentUtils::getLineEquation(const Segment& segment) noexcept {
    LineEquation result;

    BLVectorI64 normal = getNormal(segment);

    result.a = normal.x;
    result.b = normal.y;
    result.c = -(result.a * segment._p1.x + result.b * segment._p1.y);

    return result;
}

SegmentIntersection SegmentUtils::getSegmentIntersection(const Segment& segment1, const Segment& segment2) noexcept {
    SegmentIntersection segmentIntersection;

    segmentIntersection.intersect1 = 0;
    segmentIntersection.intersect2 = 0;

    LineEquation eq1 = getLineEquation(segment1);
    LineEquation eq2 = getLineEquation(segment2);

    int64_t D = eq1.a * eq2.b - eq2.a * eq1.b;

    if (D == 0) {
        // Parallel segments. Check, if any endpoint of the segments
        // lies in the interior of the other segment. We must also check,
        // if they are collinear.
        if (isCollinear(segment1, segment2)) {
            if (isInteriorPointOfSegment(segment1, segment2._p1)) {
                segmentIntersection.intersectionPoints1[segmentIntersection.intersect1++] = segment2._p1;
            }
            if (isInteriorPointOfSegment(segment1, segment2._p2)) {
                segmentIntersection.intersectionPoints1[segmentIntersection.intersect1++] = segment2._p2;
            }
            if (isInteriorPointOfSegment(segment2, segment1._p1)) {
                segmentIntersection.intersectionPoints2[segmentIntersection.intersect2++] = segment1._p1;
            }
            if (isInteriorPointOfSegment(segment2, segment1._p2)) {
                segmentIntersection.intersectionPoints2[segmentIntersection.intersect2++] = segment1._p2;
            }
        }
    } else {
        // Non-collinear segments. We will calculate subdeterminants
        // using cramers rule and solve equations.
        int64_t Dx = eq1.b * eq2.c - eq2.b * eq1.c;
        int64_t Dy = eq2.a * eq1.c - eq1.a * eq2.c;

        int64_t x = Dx / D;
        int64_t y = Dy / D;

        BLPointI64 p(x, y);

        const bool liesOnSegment1 = liesOnSegment(segment1, p);
        const bool liesOnSegment2 = liesOnSegment(segment2, p);

        if (liesOnSegment1 && liesOnSegment2) {
            // Now, we have guaranteed that p lies on the both segments.
            // We insert the intersection point only, if it is an interior
            // point of the segment.

            if (!isEndpointOfSegment(segment1, p)) {
                segmentIntersection.intersect1 = 1;
                segmentIntersection.intersectionPoints1[0] = p;
            }

            if (!isEndpointOfSegment(segment2, p)) {
                segmentIntersection.intersect2 = 1;
                segmentIntersection.intersectionPoints2[0] = p;
            }
        }
    }

    return segmentIntersection;
}

bool SegmentUtils::liesOnSegment(const Segment& segment, const BLPointI64& p) noexcept {
    int64_t minX = blMin(segment._p1.x, segment._p2.x);
    int64_t maxX = blMax(segment._p1.x, segment._p2.x);
    int64_t minY = blMin(segment._p1.y, segment._p2.y);
    int64_t maxY = blMax(segment._p1.y, segment._p2.y);

    return (minX <= p.x && p.x <= maxX) && (minY <= p.y && p.y <= maxY);
}

bool SegmentUtils::isEndpointOfSegment(const Segment& segment, const BLPointI64& p) noexcept {
    return p == segment._p1 || p == segment._p2;
}

bool SegmentUtils::isInteriorPointOfSegment(const Segment& segment, const BLPointI64& p) noexcept {
    return liesOnSegment(segment, p) && !isEndpointOfSegment(segment, p);
}

bool SegmentUtils::isCollinear(const Segment& s1, const Segment& s2) noexcept {
    return isCollinear(s1, s2._p1) && isCollinear(s1, s2._p2);
}

bool SegmentUtils::isCollinear(const Segment& s, const BLPointI64& p) noexcept {
    BLVectorI64 v1 = s.tangent();
    BLVectorI64 v2 = p - s._p1;

    int64_t d = v1.x * v2.y - v1.y * v2.x;
    return d == 0;
}

BLVectorI64 SegmentUtils::getTangent(const Segment& segment) noexcept {
    return segment.tangent();
}

BLVectorI64 SegmentUtils::getNormal(const Segment& segment) noexcept {
    BLVectorI64 tangent = getTangent(segment);
    BLVectorI64 normal(tangent.y, -tangent.x);
    return normal;
}

bool PolygonClipperImpl::isSelfOverlapping(SweepEvent* ev1, SweepEvent* ev2) const noexcept {
    return ev1->isSubject() == ev2->isSubject();
}

SweepEvent* PolygonClipperImpl::getPreviousEvent(std::set<SweepEvent*>& statusLine, SweepEvent* event) const noexcept {
    SweepEvent* previousEvent = nullptr;

    if (!event)
        return previousEvent;

    StatusLineComparator comparator;
    for (SweepEvent* currentEvent : statusLine) {
        if (comparator(currentEvent, event) >= 0 || currentEvent == event)
            continue;

        if (!previousEvent || comparator(previousEvent, currentEvent) < 0)
            previousEvent = currentEvent;
    }

    return previousEvent;
}

SweepEvent* PolygonClipperImpl::getNextEvent(std::set<SweepEvent*>& statusLine, SweepEvent* event) const noexcept {
    SweepEvent* nextEvent = nullptr;

    if (!event)
        return nextEvent;

    StatusLineComparator comparator;
    for (SweepEvent* currentEvent : statusLine) {
        if (comparator(currentEvent, event) <= 0 || currentEvent == event)
            continue;

        if (!nextEvent || comparator(nextEvent, currentEvent) < 0)
            nextEvent = currentEvent;
    }

    return nextEvent;
}

BLResult PolygonClipperImpl::updateFlags(SweepEvent* eventPrevPrev, SweepEvent* eventPrev, SweepEvent* eventCurr, SweepEvent* eventNext) noexcept {
    if (!eventPrev) {
        // This is an outer edge; simply clear the flags.
        eventCurr->_flags &= ~(SweepEventFlags::kIsInOut | SweepEventFlags::kIsInside);
    } else if (!eventPrev->isSegmentNormal()) {
        if (isSelfOverlapping(eventPrev, eventCurr))
            return blTraceError(BL_ERROR_POLYGON_POLYGON_SELF_OVERLAPS);

        // At this point, things get more complex. The segments overlap with another segment.
        // Two edges of the same polygon overlapping implies an error; hence, overlapping
        // edges must belong to different polygons (subject or clipping). If there's no edge
        // before the previous one, set the flag kIsInside (since the previous edge belongs to another
        // polygon, indicating we are inside a polygon), and unset the flag kIsInOut (because the current polygon
        // is above the current edge represented by sNode).
        if (!eventPrevPrev) {
            blSetFlag(eventCurr->_flags, SweepEventFlags::kIsInside, true);
            blSetFlag(eventCurr->_flags, SweepEventFlags::kIsInOut, false);
        }

        if (isSelfOverlapping(eventPrev, eventPrevPrev))
            return blTraceError(BL_ERROR_POLYGON_POLYGON_SELF_OVERLAPS);

        if (eventPrev->isSubject() == eventCurr->isSubject()) {
            blSetFlag(eventCurr->_flags, SweepEventFlags::kIsInside, !eventPrevPrev->isInOut());
            blSetFlag(eventCurr->_flags, SweepEventFlags::kIsInOut, !eventPrev->isInOut());
        } else {
            blSetFlag(eventCurr->_flags, SweepEventFlags::kIsInside, !eventPrev->isInOut());
            blSetFlag(eventCurr->_flags, SweepEventFlags::kIsInOut, !eventPrevPrev->isInOut());
        }
    } else if (eventPrev->isSubject() == eventCurr->isSubject()) {
        // Both edges belong to the same polygon. Thus, if the previous edge is inside
        // another polygon, then the current edge is also inside that polygon.
        blSetFlag(eventCurr->_flags, SweepEventFlags::kIsInside, eventPrev->isInside());

        // Since the edges belong to the same polygon, one edge implies the polygon
        // is below, and the other implies the polygon is above. This can be determined
        // using the isInOut flag.
        blSetFlag(eventCurr->_flags, SweepEventFlags::kIsInOut, !eventPrev->isInOut());
    } else {
        // The previous edge is below the current edge. This means if the polygon is above the previous edge,
        // the current edge is inside the other polygon, necessitating setting the flag kIsInside.
        blSetFlag(eventCurr->_flags, SweepEventFlags::kIsInside, !eventPrev->isInOut());

        // If the current polygon is below the current edge and the previous edge is inside the current polygon,
        // it signifies our polygon ends at the current edge.
        blSetFlag(eventCurr->_flags, SweepEventFlags::kIsInOut, eventPrev->isInside());
    }

    updateOverlappedEvents(eventPrev, eventCurr);
    updateOverlappedEvents(eventCurr, eventNext);

    return BL_SUCCESS;
}

void PolygonClipperImpl::updateOverlappedEvents(SweepEvent* event1, SweepEvent* event2) {
    if (!event1 || !event2)
        return;

    if ((event1->_pt == event2->_pt && event1->_opposite->_pt == event2->_opposite->_pt) ||
        (event1->_pt == event2->_opposite->_pt && event1->_opposite->_pt == event2->_pt)) {
        // Overlapping segments

        blSetFlag(event1->_flags, SweepEventFlags::kSegmentNonContributing, true);
        blSetFlag(event1->_opposite->_flags, SweepEventFlags::kSegmentNonContributing, true);

        blSetFlag(event2->_flags, SweepEventFlags::kSegmentSameTransition, event1->isInOut() == event2->isInOut());
        blSetFlag(event2->_flags, SweepEventFlags::kSegmentDifferentTransition, event1->isInOut() != event2->isInOut());
    }
}

void PolygonClipperImpl::updateResult(BLResult& oldResult, BLResult newResult) const noexcept {
    if (oldResult == BL_SUCCESS)
        oldResult = newResult;
}

SweepEvent* PolygonClipperImpl::allocSweepEvent() noexcept {
    SweepEvent* event = _sweepEventPool.alloc(_memoryAllocator);
    blCallCtor(*event);
    return event;
}

void PolygonClipperImpl::freeSweepEvent(SweepEvent* event) noexcept {
    blCallDtor(*event);
    _sweepEventPool.free(event);
}

bool SweepEvent::isPointAbove(const BLPointI64& pt) const noexcept {
    if (isLeft())
        return isCounterClockwise(_pt, _opposite->_pt, pt);
    else
        return isCounterClockwise(_opposite->_pt, _pt, pt);
}

bool SweepEvent::isPointBelow(const BLPointI64& pt) const noexcept {
    if (isLeft())
        return isClockwise(_pt, _opposite->_pt, pt);
    else
        return isClockwise(_opposite->_pt, _pt, pt);
}

PolygonConnector::~PolygonConnector() noexcept {
    reset();
}

void PolygonConnector::reset() noexcept {
    while (!_openPolygons.empty())
        removePolygonPath(_openPolygons.first());
}

void PolygonConnector::addEdge(const BLPointI64& p1, const BLPointI64& p2) noexcept {
    if (p1 == p2)
        return;

    FindPathResult f1 = find(p1);
    FindPathResult f2 = find(p2);

    if (!f1._node && !f2._node) {
        // It is a completely new edge, we will create a new polygon and
        // add edge points to the polygon.
        PolygonConnectorPathNode* pathNode = _poolPaths.alloc(_allocator);
        blCallCtor(*pathNode);

        PolygonConnectorPathItemNode* startItemNode = _poolItemNodes.alloc(_allocator);
        PolygonConnectorPathItemNode* endItemNode = _poolItemNodes.alloc(_allocator);

        blCallCtor(*startItemNode, p1);
        blCallCtor(*endItemNode, p2);

        pathNode->append(startItemNode);
        pathNode->append(endItemNode);

        _openPolygons.append(pathNode);
    } else if (f1._node && f1._node == f2._node) {
        // We have closed the path. Now, just add polygon
        // to the final path. Then, remove polygon.
        PolygonConnectorPathNode* pathNode = f1._node;

        _path.moveTo(getUnscaledPoint(pathNode->front()->_pt));
        for (PolygonConnectorPathItemNode* itemNode = pathNode->front()->next(); itemNode; itemNode = itemNode->next()) {
            _path.lineTo(getUnscaledPoint(itemNode->_pt));
        }
        _path.lineTo(getUnscaledPoint(pathNode->front()->_pt));

        removePolygonPath(pathNode);
    } else if (!f1._node) {
        PolygonConnectorPathItemNode* newItemNode = _poolItemNodes.alloc(_allocator);
        blCallCtor(*newItemNode, p1);

        if (f2._isFront)
            f2._node->prepend(newItemNode);
        else
            f2._node->append(newItemNode);
    } else if (!f2._node) {
        PolygonConnectorPathItemNode* newItemNode = _poolItemNodes.alloc(_allocator);
        blCallCtor(*newItemNode, p2);

        if (f1._isFront)
            f1._node->prepend(newItemNode);
        else
            f1._node->append(newItemNode);
    } else {
        BL_ASSERT(f1._node && f2._node && f1._node != f2._node);

        // Connect two paths
        while (!f2._node->empty()) {
            PolygonConnectorPathItemNode* itemNode = f2._isFront ? f2._node->popFront() : f2._node->popBack();

            if (f1._isFront)
                f1._node->prepend(itemNode);
            else
                f1._node->append(itemNode);
        }

        removePolygonPath(f2._node);
    }
}

void PolygonConnector::removePolygonPath(PolygonConnectorPathNode* node) noexcept {
    _openPolygons.unlink(node);

    while (!node->empty()) {
        PolygonConnectorPathItemNode* itemNode = node->popBack();
        blCallDtor(*itemNode);
        _poolItemNodes.free(itemNode);
    }

    blCallDtor(*node);
    _poolPaths.free(node);
}

BLPoint PolygonConnector::getUnscaledPoint(const BLPointI64& p) const noexcept {
    const double x = p.x * _scaleInverted;
    const double y = p.y * _scaleInverted;
    return BLPoint(x, y);
}

double PolygonConnector::getScaleInverted() const {
    return _scaleInverted;
}

void PolygonConnector::setScaleInverted(double newScaleInverted) {
    _scaleInverted = newScaleInverted;
}

PolygonConnector::FindPathResult PolygonConnector::find(const BLPointI64& point) const noexcept {
    for (PolygonConnectorPathNode* pathNode = _openPolygons.first(); pathNode; pathNode = pathNode->next()) {
        BL_ASSERT(!pathNode->empty());

        if (pathNode->front()->_pt == point)
            return FindPathResult{pathNode, true};

        if (pathNode->back()->_pt == point)
            return FindPathResult{pathNode, false};
    }

    return FindPathResult{};
}

void PolygonClipperImpl::addResultEdge(SweepEvent* edge)
{
    BL_ASSERT(edge->isLeft());

    if (edge->isSegmentNormal()) {
        switch (_operator) {
        case BL_BOOLEAN_OPERATOR_UNION:
            // The only situation where adding an edge is not necessary
            // is when the edge is located inside another polygon.
            if (!edge->isInside())
                _connector.addEdge(edge->_pt, edge->_opposite->_pt);
            break;

        case BL_BOOLEAN_OPERATOR_INTERSECTION:
            // If two polygons intersect, an edge is added only if
            // it is located inside the other polygon.
            if (edge->isInside())
                _connector.addEdge(edge->_pt, edge->_opposite->_pt);
            break;

        case BL_BOOLEAN_OPERATOR_DIFFERENCE:
            // Here, we have two cases. If the edge belongs to the 'subject' polygon
            // and is outside the 'clipping' polygon, we add this edge to the result.
            // The second case is straightforward: if an edge of the 'clipping' polygon
            // is located inside the 'subject' polygon, we add this edge to the resulting polygon.
            if ((edge->isSubject() && !edge->isInside()) || (edge->isClipping() && edge->isInside()))
                _connector.addEdge(edge->_pt, edge->_opposite->_pt);
            break;

        case BL_BOOLEAN_OPERATOR_SYMMETRIC_DIFFERENCE:
            // When performing the symmetric difference of polygons,
            // we add all edges, as they will all be part of the resulting polygon.
            _connector.addEdge(edge->_pt, edge->_opposite->_pt);
            break;
        }
    } else if (edge->isSegmentSameTransition()) {
        // If we have overlapping edges and both polygons are on
        // the same side of these edges, we add the edge to the result
        // when the operator is 'union' or 'intersection,'
        // as both polygons will be preserved in the resulting polygon.
        // However, for 'difference' or 'symmetric difference,'
        // both parts of the polygons sharing this edge will be removed
        // from the resulting polygon.

        if (_operator == BL_BOOLEAN_OPERATOR_UNION || _operator == BL_BOOLEAN_OPERATOR_INTERSECTION)
            _connector.addEdge(edge->_pt, edge->_opposite->_pt);
    } else if (edge->isSegmentDifferentTransition()) {
        // In this case, two polygons share the same edge but are located
        // on opposite sides of it. For the 'union' operator,
        // this edge will not appear in the resulting polygon,
        // as the polygons merge into one larger polygon.
        // When performing the 'intersection' operation,
        // neither polygon will be included in the result, so no edge is added.
        // However, the edge is added when performing the 'difference' operator,
        // because the original part of the 'subject' polygon sharing this edge
        // remains in the result.
        // Finally, the 'symmetric difference' operator behaves similarly
        // to the 'union' operator in this case.

        if (_operator == BL_BOOLEAN_OPERATOR_DIFFERENCE)
            _connector.addEdge(edge->_pt, edge->_opposite->_pt);
    }
}

} // {bl}
