// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_SOLVE_P_H_INCLUDED
#define BLEND2D_SOLVE_P_H_INCLUDED

enum QuadraticSolveType : uint8_t
{
  kQuadraticSolveTypeZero,
  kQuadraticSolveTypeTwo,
};

struct QuadraticSolveResult
{
  QuadraticSolveType type;
  double x[2];
};

/**
 * Solves `a * x + b = 0` for `x`.
 */
double solveLinear(double a, double b)
{
  return -b / a;
}

/**
 * Solves `a * x^2 + 2 * b * x + c = 0` for real `x`.
 *
 * References:
 * - James F. Blinn.
 *   *How to Solve a Quadratic Equation (Part 1-2)*.
 *   IEEE Computer Graphics and Applications.
 */
QuadraticSolveResult solveQuadratic(double a, double b, double c)
{
  double d = b * b - a * c;

  if (d < 0)
  {
    // No roots (ignore complex pair)
    return QuadraticSolveResult{.type = kQuadraticSolveTypeZero, .x = {0, 0}};
  }
  else
  {
    // Two roots
    double x1, x2;

    d = sqrt(d);

    if (b > 0)
    {
      d = -b - d;
      x1 = c / d;
      x2 = d / a;
    }
    else if (b < 0)
    {
      d = -b + d;
      x1 = d / a;
      x2 = c / d;
    }
    else if (a * a >= c * c)
    {
      x1 = d / a;
      x2 = -d / a;
    }
    else
    {
      x1 = c / -d;
      x2 = c / d;
    }

    return QuadraticSolveResult{.type = kQuadraticSolveTypeTwo, .x = {x1, x2}};
  }
}

#endif // BLEND2D_SOLVE_P_H_INCLUDED
