// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_RASTER_RASTERDEBUG_P_H
#define BLEND2D_RASTER_RASTERDEBUG_P_H

#include "../api-internal_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rasterdefs_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

namespace BLRasterDebug {

static void debugEdges(BLEdgeStorage<int>* edgeStorage) noexcept {
  BLEdgeVector<int>** edges = edgeStorage->bandEdges();
  size_t count = edgeStorage->bandCount();
  uint32_t bandHeight = edgeStorage->bandHeight();

  int minX = blMaxValue<int>();
  int minY = blMaxValue<int>();
  int maxX = blMinValue<int>();
  int maxY = blMinValue<int>();

  const BLBoxI& bb = edgeStorage->boundingBox();
  printf("EDGE STORAGE [%d.%d %d.%d %d.%d %d.%d]:\n",
         bb.x0 >> 8, bb.x0 & 0xFF,
         bb.y0 >> 8, bb.y0 & 0xFF,
         bb.x1 >> 8, bb.x1 & 0xFF,
         bb.y1 >> 8, bb.y1 & 0xFF);

  for (size_t bandId = 0; bandId < count; bandId++) {
    BLEdgeVector<int>* edge = edges[bandId];
    if (edge) {
      printf("BAND #%d y={%d:%d}\n", int(bandId), int(bandId * bandHeight), int((bandId + 1) * bandHeight - 1));
      while (edge) {
        printf("  EDGES {sign=%u count=%u}", unsigned(edge->signBit), unsigned(edge->count));

        if (edge->count <= 1)
          printf("{WRONG COUNT!}");

        BLEdgePoint<int>* ptr = edge->pts;
        BLEdgePoint<int>* ptrStart = ptr;
        BLEdgePoint<int>* ptrEnd = edge->pts + edge->count;

        while (ptr != ptrEnd) {
          minX = blMin(minX, ptr[0].x);
          minY = blMin(minY, ptr[0].y);
          maxX = blMax(maxX, ptr[0].x);
          maxY = blMax(maxY, ptr[0].y);

          printf(" [%d.%d, %d.%d]", ptr[0].x >> 8, ptr[0].x & 0xFF, ptr[0].y >> 8, ptr[0].y & 0xFF);

          if (ptr != ptrStart && ptr[-1].y > ptr[0].y)
            printf(" !INVALID! ");

          ptr++;
        }

        printf("\n");
        edge = edge->next;
      }
    }
  }

  printf("EDGE STORAGE BBOX [%d.%d, %d.%d] -> [%d.%d, %d.%d]\n\n",
    minX >> 8, minX & 0xFF,
    minY >> 8, minY & 0xFF,
    maxX >> 8, maxX & 0xFF,
    maxY >> 8, maxY & 0xFF);
}

} // {BLRasterDebug}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERDEBUG_P_H
