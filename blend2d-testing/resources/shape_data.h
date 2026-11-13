// This file is part of Blend2D project <https://blend2d.com>
//
// See LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_TESTING_RESOURCES_SHAPE_DATA_H_INCLUDED
#define BLEND2D_TESTING_RESOURCES_SHAPE_DATA_H_INCLUDED

#include <blend2d/blend2d.h>

namespace bl::Resources {

//! Built-in shapes that can be retrieved by `get_shape_data()`.
enum class ShapeKind {
  kButterfly,
  kFish,
  kDragon,
  kWorld,
  kMaxValue = kWorld
};

//! Represents a path data that consists of MOVE, LINE, QUAD, CUBIC, and CLOSE commands.
//!
//! Each shape is normalized to [0, 0, 1, 1] bounding box so it can be scaled to any bounds.
struct ShapeData {
  size_t size;
  const char* commands;
  const BLPoint* vertices;
};

//! Iterates over ShapeData content.
class ShapeIterator {
public:
  size_t remaining;
  const char* commands;
  const BLPoint* vertices;

  explicit inline ShapeIterator(ShapeData data) noexcept {
    remaining = data.size;
    commands = data.commands;
    vertices = data.vertices;
  }

  inline bool has_command() const noexcept {
    return remaining != 0;
  }

  inline void next() noexcept {
    char command = commands[0];

    remaining--;
    commands++;

    switch (command) {
      case 'M': vertices += 1; break;
      case 'L': vertices += 1; break;
      case 'Q': vertices += 2; break;
      case 'C': vertices += 3; break;
      default:
        break;
    }
  }

  inline bool is_close() const noexcept { return commands[0] == 'Z'; }
  inline bool is_move_to() const noexcept { return commands[0] == 'M'; }
  inline bool is_line_to() const noexcept { return commands[0] == 'L'; }
  inline bool is_quad_to() const noexcept { return commands[0] == 'Q'; }
  inline bool is_cubic_to() const noexcept { return commands[0] == 'C'; }

  template<typename Index>
  inline BLPoint vertex(const Index& i) const noexcept { return vertices[i]; }

  template<typename Index>
  inline double x(const Index& i) const noexcept { return vertices[i].x; }

  template<typename Index>
  inline double y(const Index& i) const noexcept { return vertices[i].y; }
};

//! Returns built-in shape data for `shape_kind`.
ShapeData get_shape_data(ShapeKind shape_kind);

//! Converts `shape_data` to a regular `BLPath` instance, scaled to `w` and `h`.
BLPath shape_to_path(const ShapeData& shape_data, double w, double h) noexcept;

} // {bl::Resources}

#endif // BLEND2D_TESTING_RESOURCES_SHAPE_DATA_H_INCLUDED
