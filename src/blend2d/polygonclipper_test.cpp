// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_test_p.h"
#if defined(BL_TEST)

#include "api-impl.h"
#include "blend2d/polygonclipper.h"
#include "blend2d.h"

// PolygonClipper - Tests
// ======================

namespace bl {
namespace Tests {

//#define BL_EXPORT_IMAGE

static void addRectangle(BLPolygonClipper& polygonClipper,
                         double x0, double y0,
                         double width, double height,
                         bool isSubject) noexcept {
    polygonClipper.addEdge(BLPoint(x0, y0), BLPoint(x0 + width, y0), isSubject);
    polygonClipper.addEdge(BLPoint(x0 + width, y0), BLPoint(x0 + width, y0 + height), isSubject);
    polygonClipper.addEdge(BLPoint(x0 + width, y0 + height), BLPoint(x0, y0 + height), isSubject);
    polygonClipper.addEdge(BLPoint(x0, y0 + height), BLPoint(x0, y0), isSubject);
}

static bool comparePaths(const BLPath& path1, const BLPath& path2, const BLPath& boxPath, int eps) {
    if (path1.empty() && path2.empty())
        return true;

    BLPath totalPath = path1;
    totalPath.addPath(path2);
    totalPath.addPath(boxPath);

    BLBox boundingBox;
    BLResult result = totalPath.getControlBox(&boundingBox);
    EXPECT_EQ(result, BL_SUCCESS);

    int w = (int)std::ceil(boundingBox.x1 - boundingBox.x0 + 1.0);
    int h = (int)std::ceil(boundingBox.y1 - boundingBox.y0 + 1.0);

    BLImage image1(w, h, BL_FORMAT_XRGB32);
    BLImage image2(w, h, BL_FORMAT_XRGB32);

    {
        BLContext context1(image1);
        context1.clearAll();
        context1.translate(boundingBox.x0, boundingBox.y0);

        BLContext context2(image2);
        context2.clearAll();
        context2.translate(boundingBox.x0, boundingBox.y0);

        context1.setFillStyle(BLRgba32(0x00, 0xFF, 0x00, 0xFF));
        context2.setFillStyle(BLRgba32(0x00, 0xFF, 0x00, 0xFF));

        context1.fillPath(path1);
        context2.fillPath(path2);
    }

    BLImageData imageData1{};
    BLImageData imageData2{};

    image1.getData(&imageData1);
    image2.getData(&imageData2);

    EXPECT_EQ(imageData1.size, imageData2.size);

    if (imageData1.size != imageData2.size)
        return false;

    const uint8_t* data1 = (const uint8_t*)imageData1.pixelData;
    const uint8_t* data2 = (const uint8_t*)imageData2.pixelData;
    const size_t byteSize = image1.width() * image1.height() * image1.depth() / 8;

    int diffTotal = 0;
    for (int i = 0; i < byteSize; ++i) {
        const int d1 = data1[i];
        const int d2 = data2[i];
        const int diff = blAbs(d2 - d1);
        diffTotal += diff;
    }

#ifdef BL_EXPORT_IMAGE
    if (diffTotal > eps)
    {
        BLImageCodec imageCodec;
        imageCodec.findByName("PNG");
        EXPECT_TRUE(imageCodec.isValid());

        image1.writeToFile("1.png", imageCodec);
        image2.writeToFile("2.png", imageCodec);
    }
#endif

    EXPECT_LE(diffTotal, eps);

    return true;
}

static void testPolygonClipperBasic() noexcept {
    INFO("Testing Polygon Clipper - basic");

    BLPolygonClipper polygonClipper;
    polygonClipper;
}

static void testPolygonClipperSimpleRectNotOverlapped() noexcept {
    INFO("Testing Polygon Clipper - simple not overlapped rects");

    for (BLBooleanOperator op : { BL_BOOLEAN_OPERATOR_UNION,
                                  BL_BOOLEAN_OPERATOR_INTERSECTION,
                                  BL_BOOLEAN_OPERATOR_DIFFERENCE,
                                  BL_BOOLEAN_OPERATOR_SYMMETRIC_DIFFERENCE }) {
        BLPolygonClipper polygonClipper;
        polygonClipper.setOperator(op);

        addRectangle(polygonClipper, 0.0, 0.0, 100.0, 100.0, true);
        addRectangle(polygonClipper, 200.0, 0.0, 100.0, 100.0, false);

        BLResult result = polygonClipper.perform();
        EXPECT_EQ(result, BL_SUCCESS);

        BLPath clippedPath = polygonClipper.getPath();
        EXPECT_EQ(clippedPath.empty(), op == BL_BOOLEAN_OPERATOR_INTERSECTION);

        BLPath expectedPath;

        if (op != BL_BOOLEAN_OPERATOR_INTERSECTION)
            expectedPath.addRect(0.0, 0.0, 100.0, 100.0);

        if (op == BL_BOOLEAN_OPERATOR_UNION || op == BL_BOOLEAN_OPERATOR_SYMMETRIC_DIFFERENCE)
            expectedPath.addRect(200.0, 0.0, 100.0, 100.0);

        EXPECT_TRUE(comparePaths(clippedPath, expectedPath, BLPath(), 32));
    }
}

static void testPolygonClipperSimpleRectWithSharedEdge() noexcept {
    INFO("Testing Polygon Clipper - two rects with shared edge");

    for (BLBooleanOperator op : { BL_BOOLEAN_OPERATOR_UNION,
                                  BL_BOOLEAN_OPERATOR_INTERSECTION,
                                  BL_BOOLEAN_OPERATOR_DIFFERENCE,
                                  BL_BOOLEAN_OPERATOR_SYMMETRIC_DIFFERENCE }) {
        BLPolygonClipper polygonClipper;
        polygonClipper.setOperator(op);

        addRectangle(polygonClipper, 0.0, 0.0, 100.0, 100.0, true);
        addRectangle(polygonClipper, 100.0, 0.0, 100.0, 100.0, false);

        BLResult result = polygonClipper.perform();
        EXPECT_EQ(result, BL_SUCCESS);

        BLPath clippedPath = polygonClipper.getPath();
        EXPECT_EQ(clippedPath.empty(), op == BL_BOOLEAN_OPERATOR_INTERSECTION);

        BLPath expectedPath;

        if (op != BL_BOOLEAN_OPERATOR_INTERSECTION)
            expectedPath.addRect(0.0, 0.0, 100.0, 100.0);

        if (op == BL_BOOLEAN_OPERATOR_UNION || op == BL_BOOLEAN_OPERATOR_SYMMETRIC_DIFFERENCE)
            expectedPath.addRect(100.0, 0.0, 100.0, 100.0);

        EXPECT_TRUE(comparePaths(clippedPath, expectedPath, BLPath(), 32));
    }
}

static void testPolygonClipperSimpleRectOverlap() noexcept {
    INFO("Testing Polygon Clipper - two rects overlapped");

    for (BLBooleanOperator op : { BL_BOOLEAN_OPERATOR_UNION,
                                  BL_BOOLEAN_OPERATOR_INTERSECTION,
                                  BL_BOOLEAN_OPERATOR_DIFFERENCE,
                                  BL_BOOLEAN_OPERATOR_SYMMETRIC_DIFFERENCE }) {
        BLPolygonClipper polygonClipper;
        polygonClipper.setOperator(op);

        addRectangle(polygonClipper, 0.0, 0.0, 100.0, 100.0, true);
        addRectangle(polygonClipper, 50.0, 50.0, 100.0, 100.0, false);

        BLResult result = polygonClipper.perform();
        EXPECT_EQ(result, BL_SUCCESS);

        BLPath expectedPath;
        BLPath clippedPath = polygonClipper.getPath();

        switch (op) {
        case BL_BOOLEAN_OPERATOR_UNION:
            expectedPath.addRect(0.0, 0.0, 100.0, 100.0);
            expectedPath.addRect(50.0, 50.0, 100.0, 100.0);
            break;
        case BL_BOOLEAN_OPERATOR_INTERSECTION:
            expectedPath.addRect(50.0, 50.0, 50.0, 50.0);
            break;
        case BL_BOOLEAN_OPERATOR_DIFFERENCE:
            expectedPath.moveTo(0.0, 0.0);
            expectedPath.lineTo(100.0, 0.0);
            expectedPath.lineTo(100.0, 50.0);
            expectedPath.lineTo(50.0, 50.0);
            expectedPath.lineTo(50.0, 100.0);
            expectedPath.lineTo(0.0, 100.0);
            expectedPath.lineTo(0.0, 0.0);
            break;
        case BL_BOOLEAN_OPERATOR_SYMMETRIC_DIFFERENCE:
            expectedPath.moveTo(0.0, 0.0);
            expectedPath.lineTo(100.0, 0.0);
            expectedPath.lineTo(100.0, 50.0);
            expectedPath.lineTo(50.0, 50.0);
            expectedPath.lineTo(50.0, 100.0);
            expectedPath.lineTo(0.0, 100.0);
            expectedPath.lineTo(0.0, 0.0);

            expectedPath.moveTo(100.0, 50.0);
            expectedPath.lineTo(150.0, 50.0);
            expectedPath.lineTo(150.0, 150.0);
            expectedPath.lineTo( 50.0, 150.0);
            expectedPath.lineTo( 50.0, 100.0);
            expectedPath.lineTo(100.0, 100.0);
            expectedPath.lineTo(100.0, 50.0);
            break;
        }

        EXPECT_TRUE(comparePaths(clippedPath, expectedPath, BLPath(), 32));
    }
}

UNIT(polygon_clipper, BL_TEST_GROUP_POLYGON_CLIPPER) {
    testPolygonClipperBasic();
    testPolygonClipperSimpleRectNotOverlapped();
    testPolygonClipperSimpleRectWithSharedEdge();
    testPolygonClipperSimpleRectOverlap();
}

} // {Tests}
} // {bl}

#endif // BL_TEST
