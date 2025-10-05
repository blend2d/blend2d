// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// This file provides utility classes and functions shared between some tests.

#ifndef TESTING_CMDLINE_H_INCLUDED
#define TESTING_CMDLINE_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

class CmdLine {
public:
  int _argc;
  const char* const* _argv;

  CmdLine(int argc, const char* const* argv)
    : _argc(argc),
      _argv(argv) {}

  inline int count() const { return _argc; }
  inline const char* const* args() const { return _argv; }

  int find_arg(const char* key) const {
    for (int i = 1; i < _argc; i++) {
      if (strcmp(key, _argv[i]) == 0)
        return i;
    }

    return -1;
  }

  bool has_arg(const char* key) const {
    return find_arg(key) > 0;
  }

  const char* value_of(const char* key, const char* default_value) const {
    size_t key_size = strlen(key);
    for (int i = 1; i < _argc; i++) {
      const char* val = _argv[i];
      if (strlen(val) >= key_size + 1 && val[key_size] == '=' && memcmp(val, key, key_size) == 0)
        return val + key_size + 1;
    }

    return default_value;
  }

  int value_as_int(const char* key, int default_value) const {
    const char* val = value_of(key, nullptr);
    if (val == nullptr || val[0] == '\0')
      return default_value;

    return atoi(val);
  }

  unsigned value_as_uint(const char* key, unsigned default_value) const {
    const char* val = value_of(key, nullptr);
    if (val == nullptr || val[0] == '\0')
      return default_value;

    int v = atoi(val);
    if (v < 0)
      return default_value;
    else
      return unsigned(v);
  }
};

#endif // TESTING_CMDLINE_H_INCLUDED
