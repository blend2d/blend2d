// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PATH2_P_H_INCLUDED
#define BLEND2D_PATH2_P_H_INCLUDED

#include <cstdio>

#include "path-internal_p.h"
#include "point_p.h"
#include "util_p.h"

struct BLPath2
{
  BLArray<BLPoint2> points;
  BLArray<BLPathCmd> commands;
  BLArray<double> weights;

  BLPath2() noexcept
      : points(BLArray<BLPoint2>()), commands(BLArray<BLPathCmd>()), weights(BLArray<double>()) {}

  void moveTo(BLPoint2 p0)
  {
    commands.append(BL_PATH_CMD_MOVE);
    points.append(p0);
  }

  void lineTo(BLPoint2 p1)
  {
    commands.append(BL_PATH_CMD_ON);
    points.append(p1);
  }

  void quadTo(BLPoint2 p1, BLPoint2 p2)
  {
    commands.append(BL_PATH_CMD_QUAD);
    points.append(p1);
    points.append(p2);
  }

  void cubicTo(BLPoint2 p1, BLPoint2 p2, BLPoint2 p3)
  {
    commands.append(BL_PATH_CMD_CUBIC);
    points.append(p1);
    points.append(p2);
    points.append(p3);
  }

  void conicTo(BLPoint2 p1, BLPoint2 p2, double w)
  {
    commands.append(BL_PATH_CMD_CONIC);
    points.append(p1);
    points.append(p2);
    weights.append(w);
  }

  void arcTo(BLPoint2 p1, BLPoint2 p2)
  {
    conicTo(p1, p2, 0.70710678118654757);
  }

  void clear()
  {
    commands.clear();
    points.clear();
    weights.clear();
  }

  void close()
  {
    commands.append(BL_PATH_CMD_CLOSE);
  }

  void addPath(BLPath2 &input, bool append = false)
  {
    if (append && input.isValid())
    {
      copyArray(input.commands, 1, commands, commands.size(), input.commands.size() - 1);
      copyArray(input.points, 1, points, points.size(), input.points.size() - 1);
    }
    else
    {
      copyArray(input.commands, 0, commands, commands.size(), input.commands.size());
      copyArray(input.points, 0, points, points.size(), input.points.size());
    }

    copyArray(input.weights, 0, weights, weights.size(), input.weights.size());
  }

  void addPathReversed(BLPath2 &input, bool append = false)
  {
    if (append && input.isValid())
    {
      copyCommandsReversed(input.commands, 1, commands, commands.size(), input.commands.size() - 1);
      copyArrayReversed(input.points, 0, points, points.size(), input.points.size() - 1);
    }
    else
    {
      copyCommandsReversed(input.commands, 0, commands, commands.size(), input.commands.size());
      copyArrayReversed(input.points, 0, points, points.size(), input.points.size());
    }

    copyArrayReversed(input.weights, 0, weights, weights.size(), input.weights.size());
  }

  BLArray<BLPoint2> getPoints()
  {
    return points;
  }

  BLArray<BLPathCmd> getCommands()
  {
    return commands;
  }

  BLArray<double> getWeights()
  {
    return weights;
  }

  Option<BLPathCmd> getFirstCommand()
  {
    return commands.size() > 0 ? Option<BLPathCmd>(commands.first()) : Option<BLPathCmd>();
  }

  Option<BLPoint2> getFirstPoint()
  {
    return points.size() > 0 ? Option<BLPoint2>(points.first()) : Option<BLPoint2>();
  }

  Option<BLPathCmd> getLastCommand()
  {
    return commands.size() > 0 ? Option<BLPathCmd>(commands.last()) : Option<BLPathCmd>();
  }

  Option<BLPoint2> getLastPoint()
  {
    return points.size() > 0 ? Option<BLPoint2>(points.last()) : Option<BLPoint2>();
  }

  bool isValid()
  {
    Option<BLPathCmd> cmd = getFirstCommand();
    return cmd.hasValue == true && cmd.value == BL_PATH_CMD_MOVE;
  }

  BLPath printPath()
  {
    BLPath path = BLPath();

    size_t cIdx = 0;
    size_t pIdx = 0;
    size_t wIdx = 0;

    printf("********* Path **********\n");

    while (cIdx < commands.size())
    {
      BLPathCmd command = commands[cIdx++];
      switch (command)
      {
      case BL_PATH_CMD_MOVE:
      {
        BLPoint2 p0 = points[pIdx++];
        printf("Move: %f %f\n", p0.x, p0.y);
        break;
      }
      case BL_PATH_CMD_ON:
      {
        BLPoint2 p1 = points[pIdx++];
        printf("Line: %f %f\n", p1.x, p1.y);
        break;
      }
      case BL_PATH_CMD_QUAD:
      {
        BLPoint2 p1 = points[pIdx++];
        BLPoint2 p2 = points[pIdx++];
        printf("Quad: %f %f %f %f\n", p1.x, p1.y, p2.x, p2.y);

        break;
      }
      case BL_PATH_CMD_CUBIC:
      {
        BLPoint2 p1 = points[pIdx++];
        BLPoint2 p2 = points[pIdx++];
        BLPoint2 p3 = points[pIdx++];
        printf("Cubic: %f %f %f %f %f %f\n", p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);

        break;
      }
      case BL_PATH_CMD_CONIC:
      {
        BLPoint2 p1 = points[pIdx++];
        BLPoint2 p2 = points[pIdx++];
        double w = weights[wIdx++];
        printf("Conic: %f %f %f %f %f\n", p1.x, p1.y, p2.x, p2.y, w);
        break;
      }
      case BL_PATH_CMD_CLOSE:
      {
        path.close();
        printf("Close\n");
        break;
      }
      }
    }

    return path;
  }

  BLPath getPath()
  {
    BLPath path = BLPath();

    size_t cIdx = 0;
    size_t pIdx = 0;
    size_t wIdx = 0;

    while (cIdx < commands.size())
    {
      BLPathCmd command = commands[cIdx++];
      switch (command)
      {
      case BL_PATH_CMD_MOVE:
      {
        BLPoint2 p0 = points[pIdx++];
        path.moveTo(BLPoint(p0.x, p0.y));
        break;
      }
      case BL_PATH_CMD_ON:
      {
        BLPoint2 p1 = points[pIdx++];
        path.lineTo(BLPoint(p1.x, p1.y));
        break;
      }
      case BL_PATH_CMD_QUAD:
      {
        BLPoint2 p1 = points[pIdx++];
        BLPoint2 p2 = points[pIdx++];
        path.quadTo(BLPoint(p1.x, p1.y), BLPoint(p2.x, p2.y));
        break;
      }
      case BL_PATH_CMD_CUBIC:
      {
        BLPoint2 p1 = points[pIdx++];
        BLPoint2 p2 = points[pIdx++];
        BLPoint2 p3 = points[pIdx++];
        path.cubicTo(BLPoint(p1.x, p1.y), BLPoint(p2.x, p2.y), BLPoint(p3.x, p3.y));
        break;
      }
      case BL_PATH_CMD_CONIC:
      {
        BLPoint2 p1 = points[pIdx++];
        BLPoint2 p2 = points[pIdx++];
        double w = weights[wIdx++];
        path.conicTo(BLPoint(p1.x, p1.y), BLPoint(p2.x, p2.y), w);
        break;
      }
      case BL_PATH_CMD_CLOSE:
      {
        path.close();
        break;
      }
      }
    }

    return path;
  }
};

#endif // BLEND2D_PATH2_P_H_INCLUDED
