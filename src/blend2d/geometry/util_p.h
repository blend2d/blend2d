// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_UTIL_P_H_INCLUDED
#define BLEND2D_UTIL_P_H_INCLUDED

#include "../../blend2d-impl.h"

template <typename T>
struct Option
{
  union
  {
    T value;
    size_t _dummy;
  };
  bool hasValue;

  Option() noexcept
      : hasValue(false), _dummy(0) {}

  Option(const T &value) noexcept
      : hasValue(true), value(value) {}
};

template <typename T>
void copyArray(BLArray<T> &src, size_t srcStart, BLArray<T> &dest, size_t destStart, size_t length)
{
  size_t destEnd = destStart + length;

  if (destEnd > dest.size())
  {
    // `dest` needs to grow
    dest.resize(destEnd, T());
  }

  size_t srcIdx = srcStart;
  size_t destIdx = destStart;

  while (destIdx < destEnd)
  {
    dest.replace(destIdx++, src[srcIdx++]);
  }
}

template <typename T>
void copyArrayReversed(BLArray<T> &src, size_t srcStart, BLArray<T> &dest, size_t destStart, size_t length)
{
  size_t destEnd = destStart + length;

  if (destEnd > dest.size())
  {
    // `dest` needs to grow
    dest.resize(destEnd, T());
  }

  size_t srcIdx = srcStart + length - 1;
  size_t destIdx = destStart;

  while (destIdx < destEnd)
  {
    dest.replace(destIdx++, src[srcIdx--]);
  }
}

#endif // BLEND2D_POINT_P_H_INCLUDED
