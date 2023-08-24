// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PATH_P_H_INCLUDED
#define BLEND2D_PATH_P_H_INCLUDED

#include "api-internal_p.h"
#include "math_p.h"
#include "object_p.h"
#include "path.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! \name BLPath - Private Structs
//! \{

//! Private implementation that extends \ref BLPathImpl.
struct BLPathPrivateImpl : public BLPathImpl {
  BLBox controlBox;
  BLBox boundingBox;
};

//! \}

namespace BLPathPrivate {

using BLObjectPrivate::RCMode;

//! \name BLPath - Internals - Common Functionality (Container)
//! \{

static BL_INLINE constexpr size_t capacityFromImplSize(BLObjectImplSize implSize) noexcept {
  return (implSize.value() - sizeof(BLPathPrivateImpl)) / (sizeof(BLPoint) + 1);
}

static BL_INLINE constexpr BLObjectImplSize implSizeFromCapacity(size_t capacity) noexcept {
  return BLObjectImplSize(sizeof(BLPathPrivateImpl) + capacity * (sizeof(BLPoint) + 1));
}

//! \}

//! \name BLPath - Internals - Common Functionality (Impl)
//! \{

static BL_INLINE bool isImplMutable(BLPathImpl* impl) noexcept {
  return BLObjectPrivate::isImplMutable(impl);
}

static BL_INLINE BLResult freeImpl(BLPathPrivateImpl* impl) noexcept {
  return BLObjectPrivate::freeImpl(impl);
}

template<RCMode kRCMode>
static BL_INLINE BLResult releaseImpl(BLPathPrivateImpl* impl) noexcept {
  return BLObjectPrivate::derefImplAndTest<kRCMode>(impl) ? freeImpl(impl) : BLResult(BL_SUCCESS);
}

//! \}

//! \name BLPath - Internals - Common Functionality (Instance)
//! \{

static BL_INLINE BLPathPrivateImpl* getImpl(const BLPathCore* self) noexcept {
  return static_cast<BLPathPrivateImpl*>(self->_d.impl);
}

static BL_INLINE BLResult retainInstance(const BLPathCore* self, size_t n = 1) noexcept {
  return BLObjectPrivate::retainInstance(self, n);
}

static BL_INLINE BLResult releaseInstance(BLPathCore* self) noexcept {
  return releaseImpl<RCMode::kMaybe>(getImpl(self));
}

static BL_INLINE BLResult replaceInstance(BLPathCore* self, const BLPathCore* other) noexcept {
  BLPathPrivateImpl* impl = getImpl(self);
  self->_d = other->_d;
  return releaseImpl<RCMode::kMaybe>(impl);
}

//! \}

//! \name BLPath - Internals - Other
//! \{

static BL_INLINE constexpr BLApproximationOptions makeDefaultApproximationOptions() noexcept {
  return BLApproximationOptions {
    BL_FLATTEN_MODE_DEFAULT, // Default flattening mode.
    BL_OFFSET_MODE_DEFAULT,  // Default offset mode.
    { 0, 0, 0, 0, 0, 0 },    // Reserved.
    0.20,                    // Default curve flattening tolerance.
    0.05,                    // Default curve simplification tolerance.
    0.414213562              // Default offset parameter.
  };
}

//! \}

} // {BLPathPrivate}

//! \name BLPath - Private Iterator
//! \{

//! Path iterator that can iterate over RAW data.
struct BLPathIterator {
  const uint8_t* cmd;
  const uint8_t* end;
  const BLPoint* vtx;

  BL_INLINE BLPathIterator() noexcept = default;
  BL_INLINE BLPathIterator(const BLPathView& view) noexcept { reset(view); }
  BL_INLINE BLPathIterator(const uint8_t* cmd_, const BLPoint* vtx_, size_t n) noexcept { reset(cmd_, vtx_, n); }

  BL_INLINE BLPathIterator operator++(int) noexcept { BLPathIterator out(*this); cmd++; vtx++; return out; }
  BL_INLINE BLPathIterator operator--(int) noexcept { BLPathIterator out(*this); cmd--; vtx--; return out; }

  BL_INLINE BLPathIterator& operator++() noexcept { cmd++; vtx++; return *this; }
  BL_INLINE BLPathIterator& operator--() noexcept { cmd--; vtx--; return *this; }

  BL_INLINE BLPathIterator& operator+=(size_t n) noexcept { cmd += n; vtx += n; return *this; }
  BL_INLINE BLPathIterator& operator-=(size_t n) noexcept { cmd -= n; vtx -= n; return *this; }

  BL_INLINE bool atEnd() const noexcept { return cmd == end; }
  BL_INLINE bool afterEnd() const noexcept { return cmd > end; }
  BL_INLINE bool beforeEnd() const noexcept { return cmd < end; }

  BL_INLINE size_t remainingForward() const noexcept { return (size_t)(end - cmd); }
  BL_INLINE size_t remainingBackward() const noexcept { return (size_t)(cmd - end); }

  BL_INLINE void reset(const BLPathView& view) noexcept {
    reset(view.commandData, view.vertexData, view.size);
  }

  BL_INLINE void reset(const uint8_t* cmd_, const BLPoint* vtx_, size_t n) noexcept {
    cmd = cmd_;
    end = cmd_ + n;
    vtx = vtx_;
  }

  BL_INLINE void reverse() noexcept {
    intptr_t n = intptr_t(remainingForward()) - 1;

    end = cmd - 1;
    cmd += n;
    vtx += n;
  }
};

//! \}

//! \name BLPath - Private Appender
//! \{

//! Low-level interface that can be used to append vertices & commands to an existing path fast. The interface is
//! designed in a way that the user using it must reserve enough space and then call `append...()` functions that
//! can only be called when there is enough storage left for that command. The storage requirements are specified
//! by `begin()` or by `ensure()`. The latter is mostly used to reallocate the array in case more vertices are needed
//! than initially passed to `begin()`.
//!
//! When designing a loop the appender can be used in the following way, where `SourceT` is an iterable object that
//! can provide us some path vertices and commands:
//!
//! ```
//! template<typename Iterator>
//! BLResult appendToPath(BLPath& dst, const Iterator& iter) noexcept {
//!   BLPathPrivate::BLPathAppender appender;
//!   BL_PROPAGATE(appender.beginAssign(dst, 32));
//!
//!   while (!iter.end()) {
//!     // Maximum number of vertices required by moveTo/lineTo/quadTo/cubicTo.
//!     BL_PROPAGATE(appender.ensure(dst, 3));
//!
//!     switch (iter.command()) {
//!       case BL_PATH_CMD_MOVE : appender.moveTo(iter[0]); break;
//!       case BL_PATH_CMD_ON   : appender.lineTo(iter[0]); break;
//!       case BL_PATH_CMD_QUAD : appender.quadTo(iter[0], iter[1]); break;
//!       case BL_PATH_CMD_CUBIC: appender.cubicTo(iter[0], iter[1], iter[2]); break;
//!       case BL_PATH_CMD_CLOSE: appender.close(); break;
//!     }
//!
//!     iter.advance();
//!   }
//!
//!   appender.done(dst);
//!   return BL_SUCCESS;
//! }
//! ```
class BLPathAppender {
public:
  // Internal struct that represents a single command and prevents compile thinking that it may alias.
  struct Cmd { uint8_t value; };

  Cmd* cmd;
  Cmd* end;
  BLPoint* vtx;

  BL_INLINE BLPathAppender() noexcept
    : cmd(nullptr) {}

  BL_INLINE void reset() noexcept { cmd = nullptr; }
  BL_INLINE bool empty() const noexcept { return cmd == nullptr; }
  BL_INLINE size_t remainingSize() const noexcept { return (size_t)(end - cmd); }

  BL_INLINE size_t currentIndex(const BLPath& dst) const noexcept {
    return (size_t)(cmd - reinterpret_cast<Cmd*>(BLPathPrivate::getImpl(&dst)->commandData));
  }

  BL_INLINE void _advance(size_t n) noexcept {
    BL_ASSERT(remainingSize() >= n);

    cmd += n;
    vtx += n;
  }

  BL_INLINE BLResult begin(BLPathCore* dst, BLModifyOp op, size_t n) noexcept {
    BLPoint* vtxPtrLocal;
    uint8_t* cmdPtrLocal;
    BL_PROPAGATE(blPathModifyOp(dst, op, n, &cmdPtrLocal, &vtxPtrLocal));

    BLPathImpl* dstI = BLPathPrivate::getImpl(dst);
    vtx = vtxPtrLocal;
    cmd = reinterpret_cast<Cmd*>(cmdPtrLocal);
    end = reinterpret_cast<Cmd*>(dstI->commandData + dstI->capacity);

    BL_ASSERT(remainingSize() >= n);
    return BL_SUCCESS;
  }

  BL_INLINE BLResult beginAssign(BLPathCore* dst, size_t n) noexcept { return begin(dst, BL_MODIFY_OP_ASSIGN_GROW, n); }
  BL_INLINE BLResult beginAppend(BLPathCore* dst, size_t n) noexcept { return begin(dst, BL_MODIFY_OP_APPEND_GROW, n); }

  BL_INLINE BLResult ensure(BLPathCore* dst, size_t n) noexcept {
    if (BL_LIKELY(remainingSize() >= n))
      return BL_SUCCESS;

    BLPathImpl* dstI = BLPathPrivate::getImpl(dst);

    dstI->size = (size_t)(reinterpret_cast<uint8_t*>(cmd) - dstI->commandData);
    BL_ASSERT(dstI->size <= dstI->capacity);

    uint8_t* cmdPtrLocal;
    BLPoint* vtxPtrLocal;
    BL_PROPAGATE(blPathModifyOp(dst, BL_MODIFY_OP_APPEND_GROW, n, &cmdPtrLocal, &vtxPtrLocal));

    dstI = BLPathPrivate::getImpl(dst);
    vtx = vtxPtrLocal;
    cmd = reinterpret_cast<Cmd*>(cmdPtrLocal);
    end = reinterpret_cast<Cmd*>(dstI->commandData + dstI->capacity);

    BL_ASSERT(remainingSize() >= n);
    return BL_SUCCESS;
  }

  BL_INLINE void back(size_t n = 1) noexcept {
    cmd -= n;
    vtx -= n;
  }

  BL_INLINE void sync(BLPathCore* dst) noexcept {
    BLPathImpl* dstI = BLPathPrivate::getImpl(dst);
    size_t newSize = (size_t)(reinterpret_cast<uint8_t*>(cmd) - dstI->commandData);

    BL_ASSERT(!empty());
    BL_ASSERT(newSize <= dstI->capacity);

    dstI->size = newSize;
  }

  BL_INLINE void done(BLPathCore* dst) noexcept {
    sync(dst);
    reset();
  }

  BL_INLINE void moveTo(const BLPoint& p0) noexcept { moveTo(p0.x, p0.y); }
  BL_INLINE void moveTo(const BLPointI& p0) noexcept { moveTo(p0.x, p0.y); }

  BL_INLINE void moveTo(double x0, double y0) noexcept {
    BL_ASSERT(remainingSize() >= 1);

    cmd[0].value = BL_PATH_CMD_MOVE;
    vtx[0].reset(x0, y0);

    cmd++;
    vtx++;
  }

  BL_INLINE void lineTo(const BLPoint& p1) noexcept { lineTo(p1.x, p1.y); }
  BL_INLINE void lineTo(const BLPointI& p1) noexcept { lineTo(p1.x, p1.y); }

  BL_INLINE void lineTo(double x1, double y1) noexcept {
    BL_ASSERT(remainingSize() >= 1);

    cmd[0].value = BL_PATH_CMD_ON;
    vtx[0].reset(x1, y1);

    cmd++;
    vtx++;
  }

  BL_INLINE void quadTo(const BLPoint& p1, const BLPoint& p2) noexcept {
    quadTo(p1.x, p1.y, p2.x, p2.y);
  }

  BL_INLINE void quadTo(double x1, double y1, double x2, double y2) noexcept {
    BL_ASSERT(remainingSize() >= 2);

    cmd[0].value = BL_PATH_CMD_QUAD;
    cmd[1].value = BL_PATH_CMD_ON;
    vtx[0].reset(x1, y1);
    vtx[1].reset(x2, y2);

    cmd += 2;
    vtx += 2;
  }

  BL_INLINE void conicTo(const BLPoint& p1, const BLPoint& p2, double w) noexcept {
    BL_ASSERT(remainingSize() >= 3);
    double k = 4.0 * w / (3.0 * (1.0 + w));

    cmd[0].value = BL_PATH_CMD_CUBIC;
    cmd[1].value = BL_PATH_CMD_CUBIC;
    cmd[2].value = BL_PATH_CMD_ON;

    BLPoint p0 = vtx[-1];
    vtx[0] = p0 + (p1 - p0) * k;
    vtx[1] = p2 + (p1 - p2) * k;
    vtx[2] = p2;

    cmd += 3;
    vtx += 3;
  }

/*
  // TODO [Conic]: Use this instead of cubic approximation
  BL_INLINE void conicTo(const BLPoint& p1, const BLPoint& p2, double w) noexcept {
    quadTo(p1.x, p1.y, p2.x, p2.y);
  }

  BL_INLINE void conicTo(double x1, double y1, double x2, double y2, double w) noexcept {
    BL_ASSERT(remainingSize() >= 3);

    cmd[0].value = BL_PATH_CMD_CONIC;
    cmd[1].value = BL_PATH_CMD_WEIGHT;
    cmd[2].value = BL_PATH_CMD_ON;
    vtx[0].reset(x1, y1);
    vtx[1].reset(w, blNaN<double>());
    vtx[2].reset(x2, y2);

    cmd += 3;
    vtx += 3;
  }
*/
  BL_INLINE void cubicTo(const BLPoint& p1, const BLPoint& p2, const BLPoint& p3) noexcept {
    return cubicTo(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
  }

  BL_INLINE void cubicTo(double x1, double y1, double x2, double y2, double x3, double y3) noexcept {
    BL_ASSERT(remainingSize() >= 3);

    cmd[0].value = BL_PATH_CMD_CUBIC;
    cmd[1].value = BL_PATH_CMD_CUBIC;
    cmd[2].value = BL_PATH_CMD_ON;
    vtx[0].reset(x1, y1);
    vtx[1].reset(x2, y2);
    vtx[2].reset(x3, y3);

    cmd += 3;
    vtx += 3;
  }

  BL_INLINE void arcQuadrantTo(const BLPoint& p1, const BLPoint& p2) noexcept {
    BL_ASSERT(remainingSize() >= 3);

    cmd[0].value = BL_PATH_CMD_CUBIC;
    cmd[1].value = BL_PATH_CMD_CUBIC;
    cmd[2].value = BL_PATH_CMD_ON;

    BLPoint p0 = vtx[-1];
    vtx[0] = p0 + (p1 - p0) * BL_M_KAPPA;
    vtx[1] = p2 + (p1 - p2) * BL_M_KAPPA;
    vtx[2] = p2;

    cmd += 3;
    vtx += 3;
  }

  BL_INLINE void addVertex(uint8_t cmd_, const BLPoint& p) noexcept {
    BL_ASSERT(remainingSize() >= 1);

    cmd[0].value = cmd_;
    vtx[0] = p;

    cmd++;
    vtx++;
  }

  BL_INLINE void addVertex(uint8_t cmd_, double x, double y) noexcept {
    BL_ASSERT(remainingSize() >= 1);

    cmd[0].value = cmd_;
    vtx[0].reset(x, y);

    cmd++;
    vtx++;
  }

  BL_INLINE void close() noexcept {
    BL_ASSERT(remainingSize() >= 1);

    cmd[0].value = BL_PATH_CMD_CLOSE;
    vtx[0].reset(blNaN<double>(), blNaN<double>());

    cmd++;
    vtx++;
  }

  BL_INLINE void addBox(double x0, double y0, double x1, double y1, BLGeometryDirection dir) noexcept {
    BL_ASSERT(remainingSize() >= 5);

    cmd[0].value = BL_PATH_CMD_MOVE;
    cmd[1].value = BL_PATH_CMD_ON;
    cmd[2].value = BL_PATH_CMD_ON;
    cmd[3].value = BL_PATH_CMD_ON;
    cmd[4].value = BL_PATH_CMD_CLOSE;

    vtx[0].reset(x0, y0);
    vtx[1].reset(x1, y0);
    vtx[2].reset(x1, y1);
    vtx[3].reset(x0, y1);
    vtx[4].reset(blNaN<double>(), blNaN<double>());

    if (dir != BL_GEOMETRY_DIRECTION_CW) {
      vtx[1].reset(x0, y1);
      vtx[3].reset(x1, y0);
    }

    cmd += 5;
    vtx += 5;
  }

  BL_INLINE void addBoxCW(double x0, double y0, double x1, double y1) noexcept {
    addBox(x0, y0, x1, y1, BL_GEOMETRY_DIRECTION_CW);
  }

  BL_INLINE void addBoxCCW(double x0, double y0, double x1, double y1) noexcept {
    addBox(x0, y0, x1, y1, BL_GEOMETRY_DIRECTION_CCW);
  }
};

//! \}

//! \}
//! \endcond

#endif // BLEND2D_PATH_P_H_INCLUDED
