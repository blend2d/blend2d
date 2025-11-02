// This file is part of AsmJit project <https://asmjit.com>
//
// See asmjit.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef TESTING_PERFORMANCE_TIMER_H_INCLUDED
#define TESTING_PERFORMANCE_TIMER_H_INCLUDED

#include <chrono>

class PerformanceTimer {
public:
  typedef std::chrono::high_resolution_clock::time_point TimePoint;

  TimePoint _start_time {};
  TimePoint _end_time {};

  inline void start() {
    _start_time = std::chrono::high_resolution_clock::now();
  }

  inline void stop() {
    _end_time = std::chrono::high_resolution_clock::now();
  }

  inline double duration() const {
    std::chrono::duration<double> elapsed = _end_time - _start_time;
    return elapsed.count() * 1000;
  }
};

#endif // TESTING_PERFORMANCE_TIMER_H_INCLUDED
