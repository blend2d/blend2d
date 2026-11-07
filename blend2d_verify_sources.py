#!/usr/bin/env python3
"""Verify that Blend2D source files use a consistent style for the file
header (the license-info comment) and for header guards.

The tool iterates over all source files inside the `blend2d/` package
subdirectory (3rdparty and blend2d-testing are intentionally ignored) and
reports files that deviate from the canonical conventions:

  License header (short form used by every file except the umbrella
  `blend2d.h`, which carries the full Zlib text):

    // This file is part of Blend2D project <https://blend2d.com>
    //
    // See blend2d.h or LICENSE.md for license and copyright information
    // SPDX-License-Identifier: Zlib

  Header guard (header files only):

    #ifndef  BLEND2D_<PATH>_INCLUDED
    #define  BLEND2D_<PATH>_INCLUDED
    ...
    #endif // BLEND2D_<PATH>_INCLUDED

  where <PATH> is the include path uppercased with '/', '.' and '-'
  turned into '_'. For example `blend2d/core/array.h` maps to
  `BLEND2D_CORE_ARRAY_H_INCLUDED`. The umbrella headers that live directly
  in `blend2d/` (e.g. `blend2d.h`, `blend2d-debug.h`) collapse the repeated
  package name and are therefore also accepted as `BLEND2D_H_INCLUDED`
  and `BLEND2D_DEBUG_H_INCLUDED`.

Exit code is 0 when every scanned file is consistent, 1 otherwise.
"""

import argparse
import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# Name of the package subdirectory that holds the actual sources.
PACKAGE_NAME = "blend2d"

# File extensions we treat as C/C++ headers and sources.
HEADER_EXTS = (".h", ".hpp", ".hh", ".hxx")
SOURCE_EXTS = (".cpp", ".cc", ".cxx")

# Canonical license header (short form). This is the exact block, line by
# line, that must appear at the very top of the file.
LICENSE_FIRST_LINE = "// This file is part of Blend2D project <https://blend2d.com>"
SHORT_LICENSE = [
  "// This file is part of Blend2D project <https://blend2d.com>",
  "//",
  "// See blend2d.h or LICENSE.md for license and copyright information",
  "// SPDX-License-Identifier: Zlib",
]

# Marker unique to the full Zlib license text (used by the umbrella header).
FULL_LICENSE_MARKER = "This software is provided 'as-is'"

IFNDEF_RE = re.compile(r"^[ \t]*#[ \t]*ifndef[ \t]+([A-Za-z_][A-Za-z0-9_]*)", re.MULTILINE)
DEFINE_RE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_][A-Za-z0-9_]*)", re.MULTILINE)
PRAGMA_ONCE_RE = re.compile(r"^[ \t]*#[ \t]*pragma[ \t]+once\b", re.MULTILINE)
ENDIF_RE = re.compile(r"^[ \t]*#[ \t]*endif\b(.*)$", re.MULTILINE)


def read_text_file(file_name):
  with open(file_name, "r", encoding="utf-8", errors="replace") as f:
    return f.read()


def resolve_package_dir(root):
  """Return the absolute path to the `blend2d/` package directory.

  `root` may point at the repository root (which contains `blend2d/`), at the
  package directory itself, or be None to auto-detect relative to this script.
  """
  candidates = []
  if root:
    root = os.path.abspath(root)
    candidates += [os.path.join(root, PACKAGE_NAME), root]
  else:
    candidates += [os.path.join(SCRIPT_DIR, PACKAGE_NAME), SCRIPT_DIR]

  for path in candidates:
    if os.path.basename(os.path.normpath(path)) == PACKAGE_NAME and os.path.isdir(path):
      return path
  return None


def iter_source_files(package_dir):
  """Yield source files under `package_dir`, sorted by relative path."""
  result = []
  for dir_path, dir_names, file_names in os.walk(package_dir):
    dir_names.sort()
    for name in file_names:
      if name.endswith(HEADER_EXTS) or name.endswith(SOURCE_EXTS):
        result.append(os.path.join(dir_path, name))
  result.sort()
  return result


def include_path(package_dir, file_path):
  """Return the include-style path, e.g. 'blend2d/core/array.h'."""
  root = os.path.dirname(package_dir)
  return os.path.relpath(file_path, root).replace(os.sep, "/")


def path_guard(rel_path):
  """Guard derived from the full include path."""
  return re.sub(r"[^0-9A-Za-z]", "_", rel_path).upper() + "_INCLUDED"


def stem_guard(rel_path):
  """Guard derived from the file stem only (umbrella top-level headers)."""
  base = os.path.basename(rel_path)
  stem, ext = os.path.splitext(base)
  stem = re.sub(r"[^0-9A-Za-z]", "_", stem).upper()
  return "{}_{}_INCLUDED".format(stem, ext[1:].upper())


def expected_guards(package_dir, file_path, rel_path):
  """Return (primary_expected, accepted_set) of guard names for a header."""
  primary = path_guard(rel_path)
  accepted = {primary}
  # Umbrella headers live directly in the package directory and collapse the
  # repeated 'blend2d' segment, e.g. blend2d/blend2d-debug.h -> BLEND2D_DEBUG_H_INCLUDED.
  if os.path.dirname(file_path) == package_dir:
    accepted.add(stem_guard(rel_path))
  return primary, accepted


def check_license(text):
  """Return (style, issues). `style` is one of short/full/none."""
  issues = []
  lines = text.split("\n")
  head = lines[:8]

  # Locate the canonical first line within the first few lines.
  offset = None
  for i, line in enumerate(head):
    if line == LICENSE_FIRST_LINE:
      offset = i
      break

  if offset is None:
    issues.append("missing or nonstandard license header")
    return "none", issues

  if offset > 0:
    lead = lines[0].strip() or "(blank)"
    issues.append(
      "license header is not at the top of the file: {} line(s) before it "
      "(first line is {!r})".format(offset, lead)
    )

  # The umbrella header carries the full Zlib license text.
  if FULL_LICENSE_MARKER in text[:2048]:
    return "full", issues

  block = lines[offset:offset + len(SHORT_LICENSE)]
  if block == SHORT_LICENSE:
    return "short", issues

  for i, expected in enumerate(SHORT_LICENSE):
    got = block[i] if i < len(block) else "(missing)"
    if got != expected:
      issues.append(
        "license line {} differs: got {!r}, expected {!r}".format(offset + i + 1, got, expected)
      )
  return "nonstandard", issues


def check_header_guard(text, primary, accepted):
  """Return a list of header-guard issues for a header file."""
  issues = []

  ifndef = IFNDEF_RE.search(text)
  if ifndef is None:
    if PRAGMA_ONCE_RE.search(text):
      issues.append("uses '#pragma once' (project convention is '#ifndef' guards)")
    else:
      issues.append("no header guard found")
    return issues

  guard = ifndef.group(1)

  define = DEFINE_RE.search(text, ifndef.end())
  define_name = define.group(1) if define else None
  if define_name != guard:
    issues.append(
      "'#define' does not match '#ifndef' ('#ifndef {}' vs '#define {}')".format(guard, define_name)
    )

  if guard not in accepted:
    if not guard.endswith("_INCLUDED") and guard + "_INCLUDED" in accepted:
      issues.append(
        'guard is missing the "_INCLUDED" suffix: {} (expected {})'.format(guard, primary)
      )
    else:
      issues.append("guard name mismatch: {} (expected {})".format(guard, primary))

  endifs = ENDIF_RE.findall(text)
  if not endifs:
    issues.append("no '#endif' found")
  else:
    tail = endifs[-1].strip()
    if not tail:
      issues.append("closing '#endif' has no comment (expected '#endif // {}')".format(guard))
    elif guard not in tail:
      issues.append(
        "closing '#endif' comment mismatch: '#endif {}' (expected '#endif // {}')".format(tail, guard)
      )

  return issues


def check_source_guard(text):
  """Warn if a .cpp accidentally carries an include guard."""
  ifndef = IFNDEF_RE.search(text)
  if ifndef and ifndef.group(1).endswith("_INCLUDED"):
    define = DEFINE_RE.search(text, ifndef.end())
    if define and define.group(1) == ifndef.group(1):
      return ["source file has an include guard ({}), which is unexpected".format(ifndef.group(1))]
  return []


def main():
  parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument(
    "root", nargs="?", default=None,
    help="repository root or the blend2d/ package directory (auto-detected by default)",
  )
  parser.add_argument("-v", "--verbose", action="store_true", help="also list files that are clean")
  parser.add_argument(
    "--no-empty", action="store_true", help="do not report empty (zero-byte) placeholder files",
  )
  args = parser.parse_args()

  package_dir = resolve_package_dir(args.root)
  if package_dir is None:
    sys.stderr.write("error: could not locate the '{}/' package directory\n".format(PACKAGE_NAME))
    return 2

  files = iter_source_files(package_dir)

  license_problems = []   # (rel, [issues])
  guard_problems = []     # (rel, [issues])
  empty_files = []
  clean = []
  header_count = 0
  source_count = 0

  for file_path in files:
    rel = include_path(package_dir, file_path)
    is_header = file_path.endswith(HEADER_EXTS)
    if is_header:
      header_count += 1
    else:
      source_count += 1

    text = read_text_file(file_path)
    if not text.strip():
      empty_files.append(rel)
      continue

    _, lic_issues = check_license(text)
    if lic_issues:
      license_problems.append((rel, lic_issues))

    if is_header:
      primary, accepted = expected_guards(package_dir, file_path, rel)
      grd_issues = check_header_guard(text, primary, accepted)
    else:
      grd_issues = check_source_guard(text)
    if grd_issues:
      guard_problems.append((rel, grd_issues))

    if not lic_issues and not grd_issues:
      clean.append(rel)

  # ---- Report --------------------------------------------------------------
  print("Blend2D source verification")
  print("  package: {}".format(package_dir))
  print("  scanned: {} files ({} headers, {} sources)".format(
    len(files), header_count, source_count))
  print()

  if license_problems:
    print("LICENSE HEADER ISSUES ({} file(s))".format(len(license_problems)))
    for rel, issues in license_problems:
      print("  {}".format(rel))
      for issue in issues:
        print("    - {}".format(issue))
    print()

  if guard_problems:
    print("HEADER GUARD ISSUES ({} file(s))".format(len(guard_problems)))
    for rel, issues in guard_problems:
      print("  {}".format(rel))
      for issue in issues:
        print("    - {}".format(issue))
    print()

  if empty_files and not args.no_empty:
    print("EMPTY FILES ({}) - skipped".format(len(empty_files)))
    for rel in empty_files:
      print("  {}".format(rel))
    print()

  if args.verbose and clean:
    print("CLEAN ({} file(s))".format(len(clean)))
    for rel in clean:
      print("  {}".format(rel))
    print()

  print("SUMMARY")
  print("  license issues: {} file(s)".format(len(license_problems)))
  print("  guard issues:   {} file(s)".format(len(guard_problems)))
  print("  empty files:    {}".format(len(empty_files)))
  print("  clean:          {} file(s)".format(len(clean)))

  failed = bool(license_problems or guard_problems)
  print("  result:         {}".format("FAIL" if failed else "OK"))
  return 1 if failed else 0


if __name__ == "__main__":
  sys.exit(main())
