// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PATH_INTERNAL_P_H_INCLUDED
#define BLEND2D_PATH_INTERNAL_P_H_INCLUDED

#include "../../blend2d-impl.h"

void copyCommandsReversed(
    BLArray<BLPathCmd> &src,
    size_t srcStart,
    BLArray<BLPathCmd> &dest,
    size_t destStart,
    size_t length)
{
  // TODO: Make it work with `size_t`
  int srcEnd = srcStart;
  int srcIdx = srcStart + length - 1;
  int destIdx = destStart;
  bool needsClose = false;

  if (src[srcStart] == BL_PATH_CMD_MOVE)
  {
    dest.replace(destIdx++, BL_PATH_CMD_MOVE);
    srcEnd += 1;
  }

  if (src[srcIdx] == BL_PATH_CMD_CLOSE)
  {
    needsClose = true;
    srcIdx -= 1;
  }

  size_t destEnd = destStart + length;
  if (destEnd > dest.size())
  {
    // `dest` needs to grow
    dest.resize(destEnd, BLPathCmd());
  }

  while (srcIdx >= srcEnd)
  {
    BLPathCmd cmd = src[srcIdx--];

    if (cmd == BL_PATH_CMD_MOVE)
    {
      if (needsClose)
      {
        dest.replace(destIdx++, BL_PATH_CMD_CLOSE);
      }

      dest.replace(destIdx++, BL_PATH_CMD_MOVE);
      needsClose = false;
    }
    else if (cmd == BL_PATH_CMD_CLOSE)
    {
      dest.replace(destIdx++, BL_PATH_CMD_MOVE);
      needsClose = true;
    }
    else
    {
      dest.replace(destIdx++, cmd);
    }
  }

  if (needsClose)
  {
    dest.replace(destIdx++, BL_PATH_CMD_CLOSE);
  }
}

#endif // BLEND2D_PATH_INTERNAL_P_H_INCLUDED
