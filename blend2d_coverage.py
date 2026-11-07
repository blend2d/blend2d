#!/usr/bin/env python3
"""Measure Blend2D C++ test coverage using Clang and LLVM (Python 3.9+).

Examples, run from the repository root:
  python3 blend2d_coverage.py gradient.cpp --html
  python3 blend2d_coverage.py gradient_test.cpp
  python3 blend2d_coverage.py gradient.cpp gradient.h --test-file gradient_test.cpp
  python3 blend2d_coverage.py gradient.cpp --run 'core_gradient*'
  python3 blend2d_coverage.py gradient.cpp --all-tests
  python3 blend2d_coverage.py --html

With source files, select TEST_CASE/TEST_CASE_T registrations from matching
*_test.cpp files by default. With no files, run the entire unit test runner and
report all instrumented production sources and headers. --run and --all-tests
override automatic test selection. Multiple files share the selected test pool.

The first invocation creates a separate instrumented build based on the debug
CMake preset, using -O0 -g and BL_BUILD_RELEASE to disable Blend2D assertions.
Subsequent invocations rebuild incrementally. Every report
uses fresh profiles in its own directory under build/coverage/reports, including
report.txt, summary.json, coverage.json (the LLVM export), and execution logs.
--html also writes html/index.html, with annotated lines and branch outcomes.

Coverage counts execution, including runtime startup/shutdown, not assertion
quality. Only code compiled for the current platform/configuration is measured;
uninstantiated templates and JIT-generated machine code are outside this report.
Runtime-unexecuted constexpr helpers may appear as uncovered functions.

Requires CMake (with support for this checkout's presets), Clang, llvm-profdata,
and llvm-cov from the same LLVM major version. No Python packages are required.
Exit codes: 0 success, 1 operational failure, 2 command-line syntax errors.
"""

import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent
PACKAGE = ROOT / "blend2d"
INSTRUMENTATION = "-fprofile-instr-generate -fcoverage-mapping -fprofile-update=atomic"
BUILD_DEFINES = ["BL_BUILD_RELEASE"]
TEST_CASE_RE = re.compile(r"\bTEST_CASE(?:_T)?\s*\(\s*([A-Za-z_]\w*)\s*[,)]")
# Match comments and literals together so comment markers inside strings don't
# consume real code. Raw string delimiters must match their closing delimiter.
CPP_NON_CODE_RE = re.compile(
  r'//[^\n]*|/\*.*?\*/|R"(?P<delim>[^ ()\\\t\r\n]{0,16})\(.*?\)(?P=delim)"'
  r'|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.DOTALL)


class CoverageError(Exception):
  pass


def run(command, *, cwd=ROOT, env=None, log=None, input_text=None):
  """Execute without a shell; logs include diagnostics on successful commands."""
  command = [str(arg) for arg in command]
  result = subprocess.run(command, cwd=cwd, env=env, input=input_text,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          text=True, encoding="utf-8", errors="replace")
  if log is not None:
    Path(log).write_text("$ " + shlex.join(command) + "\n\n" + result.stdout + result.stderr,
                        encoding="utf-8")
  if result.returncode:
    raise CoverageError("Command failed ({}): {}\n{}{}".format(
      result.returncode, shlex.join(command), result.stdout[-6000:], result.stderr[-6000:]))
  if result.stderr and log is None:
    print(result.stderr.rstrip(), file=sys.stderr)
  return result.stdout


def executable(name):
  path = shutil.which(str(name))
  if not path:
    raise CoverageError("Executable not found: {}".format(name))
  # Keep the driver's name: resolving clang++'s symlink to clang changes its
  # default language/link behavior (in particular linking the C++ runtime).
  return str(Path(path).absolute())


def tool_major(path):
  version = run([path, "--version"])
  match = re.search(r"(?:clang|LLVM) version\s+(\d+)", version, re.IGNORECASE)
  if not match:
    raise CoverageError("Cannot identify LLVM version of {}:\n{}".format(path, version))
  return int(match.group(1))


def find_llvm_tool(name, compiler, major, override=None, required=True):
  if override:
    candidates = [override]
  else:
    candidates = [str(Path(compiler).parent / name),
                  str(Path(compiler).resolve().parent / name), name + "-" + str(major), name]
  for candidate in candidates:
    path = shutil.which(candidate)
    if path and tool_major(path) == major:
      return str(Path(path).resolve())
  if required:
    raise CoverageError("{} from LLVM {} is required; use --{} /path/to/{}.".format(
      name, major, name, name))
  return None


def resolve_file(value):
  """Accept a repository path or an unambiguous basename, e.g. gradient.cpp."""
  path = Path(value).expanduser()
  candidates = [path.resolve(), (ROOT / path).resolve(), (PACKAGE / path).resolve()]
  for candidate in candidates:
    if candidate.is_file() and candidate.is_relative_to(PACKAGE):
      return candidate
  if len(path.parts) == 1:
    names = [path.name] if path.suffix else [path.name + ".cpp"]
    matches = sorted({p.resolve() for name in names for p in PACKAGE.rglob(name)})
    if len(matches) == 1:
      return matches[0]
    if matches:
      raise CoverageError("Ambiguous file {}: {}".format(
        value, ", ".join(str(p.relative_to(ROOT)) for p in matches)))
  raise CoverageError("Cannot find a Blend2D source file: {}".format(value))


def test_names(path):
  # Line splicing happens before comments are recognized by the preprocessor.
  code = re.sub(r"\\\r?\n", "", path.read_text(encoding="utf-8"))
  code = CPP_NON_CODE_RE.sub(" ", code)
  return list(dict.fromkeys(TEST_CASE_RE.findall(code)))


def select_files(args):
  sources = []
  tests = [resolve_file(value) for value in (args.test_file or [])]
  for value in args.files:
    path = resolve_file(value)
    if path.name.endswith("_test.cpp"):
      tests.append(path)
      candidates = [path.with_name(path.name[:-9] + suffix) for suffix in (".cpp", ".h", "_p.h")]
      source = next((candidate for candidate in candidates if candidate.is_file()), None)
      if source is None:
        raise CoverageError("No matching source for {}; specify a source and --test-file.".format(path))
      sources.append(source)
    else:
      sources.append(path)
  sources = list(dict.fromkeys(sources))
  if not sources and args.test_file:
    raise CoverageError("Specify at least one source to report with --test-file.")
  if (args.run or args.all_tests) and tests:
    raise CoverageError("Test files cannot be combined with --run or --all-tests.")
  if sources and not (tests or args.run or args.all_tests):
    for source in sources:
      test = source.with_name(source.stem + "_test.cpp")
      if not test.is_file() and source.stem.endswith("_p"):
        test = source.with_name(source.stem[:-2] + "_test.cpp")
      if not test.is_file():
        raise CoverageError("No matching test file for {}; use --test-file, --run, or --all-tests.".format(
          source.relative_to(ROOT)))
      tests.append(test)
  tests = list(dict.fromkeys(tests))
  names = list(dict.fromkeys(name for test in tests for name in test_names(test)))
  if tests and not names:
    raise CoverageError("No TEST_CASE or TEST_CASE_T registrations found in the selected test files.")
  return sources, tests, names


def configure_and_build(args, compiler, clang, output):
  build = args.build_dir
  # Never convert an ordinary developer build into a coverage build.
  marker = build / ".blend2d-coverage.json"
  if (build / "CMakeCache.txt").exists() and not marker.is_file():
    raise CoverageError("{} is an existing build not created by this script; choose another --build-dir.".format(build))
  configuration = {"source": str(ROOT), "clang": clang, "clang++": compiler,
                   "instrumentation": INSTRUMENTATION, "build_defines": BUILD_DEFINES}
  if marker.is_file():
    previous = json.loads(marker.read_text(encoding="utf-8"))
    if any(previous.get(key) != configuration[key] for key in ("source", "clang", "clang++")):
      raise CoverageError("Coverage source or compiler changed; choose a new --build-dir.")
  # CMake rebuilds affected objects when flags change, including existing
  # coverage builds created before BL_BUILD_RELEASE was enabled.
  compile_flags = INSTRUMENTATION + " " + " ".join("-D" + define for define in BUILD_DEFINES)
  build.mkdir(parents=True, exist_ok=True)
  # Retain ownership even if configuration fails, so a fixed dependency or
  # source checkout can be retried in the same build directory.
  marker.write_text(json.dumps(configuration, indent=2) + "\n", encoding="utf-8")
  print("Configuring coverage build (-O0 -g, BL_BUILD_RELEASE): {}".format(build), flush=True)
  run([executable("cmake"), "--preset", "debug", "-B", build,
       "-DCMAKE_C_COMPILER=" + clang, "-DCMAKE_CXX_COMPILER=" + compiler,
       "-DCMAKE_C_FLAGS=" + compile_flags, "-DCMAKE_CXX_FLAGS=" + compile_flags,
       "-DCMAKE_C_FLAGS_DEBUG=-O0 -g", "-DCMAKE_CXX_FLAGS_DEBUG=-O0 -g",
       "-DCMAKE_EXE_LINKER_FLAGS=-fprofile-instr-generate",
       # The shared configuration compiles all production sources directly into
       # bl_test_runner, including objects an archive linker could leave out.
       "-DBLEND2D_STATIC=OFF", "-DBLEND2D_TEST=ON", "-DBLEND2D_DEMOS=OFF"],
      log=output / "configure.log")
  print("Building bl_test_runner (first run compiles the coverage build)...", flush=True)
  run([executable("cmake"), "--build", build, "--config", "Debug", "--target", "bl_test_runner",
       "--parallel", str(args.jobs)], log=output / "build.log")
  for binary in [build / "bl_test_runner", build / "bl_test_runner.exe",
                 build / "Debug/bl_test_runner", build / "Debug/bl_test_runner.exe"]:
    if binary.is_file():
      return binary
  raise CoverageError("Build completed, but bl_test_runner could not be found in {}.".format(build))


def run_tests(binary, args, names, output):
  env = os.environ.copy()
  # Listing tests also runs static initialization; keep that profile out of the
  # measured selection. A unique output directory prevents merging stale data.
  env["LLVM_PROFILE_FILE"] = str(output / "listing-%p.profraw")
  listing_args = [binary, "--no-color", "--list"]
  if args.run:
    listing_args += ["--run", args.run]
  listing = run(listing_args, env=env, log=output / "list-tests.log")
  available = re.findall(r"^- ([A-Za-z_]\w*)\s*$", listing, re.MULTILINE)
  if not available:
    raise CoverageError("No tests matched the selection. See {}.".format(output / "list-tests.log"))
  if names:
    # Conditional registrations can be absent on this platform. Select only
    # compiled registrations, and explicitly report any that were excluded.
    missing = sorted(set(names) - set(available))
    if missing:
      print("Registrations absent from this build: " + ", ".join(missing), file=sys.stderr)
    selected = [name for name in names if name in available]
    if not selected:
      raise CoverageError("None of the selected file's test registrations are present in this build.")
    filters = selected
  else:
    selected = available
    filters = [args.run]  # None runs the entire runner in one invocation.
  print("Running {} test(s)...".format(len(selected)), flush=True)
  profiles = []
  for index, test_filter in enumerate(filters):
    env["LLVM_PROFILE_FILE"] = str(output / "tests-{}-%p.profraw".format(index))
    command = [binary, "--no-color"]
    if test_filter:
      command += ["--run", test_filter]
    result = run(command, env=env, log=output / "tests-{}.log".format(index))
    passed = re.search(r"^PASSED \[(\d+) tests?(?:, \d+ skipped)?\]", result, re.MULTILINE)
    expected = 1 if names else len(selected)
    if not passed or int(passed.group(1)) != expected:
      raise CoverageError("Expected {} passing test(s); see {}.".format(expected, output / "tests-{}.log".format(index)))
    fresh = sorted(output.glob("tests-{}-*.profraw".format(index)))
    if not fresh:
      raise CoverageError("No LLVM profile was written by {}.".format(binary))
    profiles.extend(fresh)
  return selected, profiles


def production_file(path):
  return (path.is_relative_to(PACKAGE) and path.suffix in (".cpp", ".h")
          and not re.search(r"_test(?:_p)?\.(cpp|h)$", path.name))


def summarize(exported, sources):
  """Use LLVM's own per-file denominators; group template instantiations."""
  files = {}
  for data in exported["data"]:
    for entry in data["files"]:
      path = Path(entry["filename"]).resolve()
      if (path in sources if sources else production_file(path)):
        files[path] = {"file": str(path.relative_to(ROOT)), "summary": entry["summary"],
                       "uncovered_functions": []}
  missing = set(sources) - set(files)
  if missing:
    raise CoverageError("No coverage mapping for: {}. It may not be compiled on this platform.".format(
      ", ".join(str(p.relative_to(ROOT)) for p in sorted(missing))))
  if not files:
    raise CoverageError("No Blend2D production files found in the coverage export.")
  groups = {}
  for data in exported["data"]:
    for function in data.get("functions", []):
      regions = function.get("regions", [])
      if not regions:
        continue
      # The first code region identifies the definition. Other file IDs can
      # refer to macro expansions, which are not additional function definitions.
      region = next((r for r in regions if r[7] == 0), None)
      if region is None:
        continue
      path = Path(function["filenames"][region[5]]).resolve()
      if path not in files:
        continue
      key = (path, *region[:4])
      group = groups.setdefault(key, {"line": region[0], "end_line": region[2],
                                      "execution_count": 0, "mangled_names": set()})
      group["execution_count"] += function["count"]
      group["mangled_names"].add(function["name"])
  for key, group in sorted(groups.items()):
    if group["execution_count"] == 0:
      group["mangled_names"] = sorted(group["mangled_names"])
      files[key[0]]["uncovered_functions"].append(group)
  return [files[path] for path in sorted(files)]


def demangle(files, tool):
  names = sorted({name for entry in files for f in entry["uncovered_functions"] for name in f["mangled_names"]})
  # LLVM prefixes internal-linkage symbols with the translation unit's name.
  symbols = [name.split(":", 1)[-1] if ":_Z" in name else name for name in names]
  decoded = run([tool], input_text="\n".join(symbols) + "\n").splitlines() if tool and names else symbols
  mapping = dict(zip(names, decoded))
  for entry in files:
    for function in entry["uncovered_functions"]:
      function["names"] = sorted({mapping[name] for name in function["mangled_names"]})


def metric(summary, name):
  value = summary.get(name, {"covered": 0, "count": 0})
  total, covered = value["count"], value["covered"]
  return "{}/{} ({:.1f}%)".format(covered, total, 100.0 * covered / total) if total else "n/a"


def format_report(report):
  lines = ["Blend2D test execution coverage (-O0 -g, BL_BUILD_RELEASE; assertions disabled)",
           "Tests: {} selected; includes runtime startup/shutdown.".format(len(report["tests"])),
           "Test files: " + (", ".join(report["test_files"]) or "runner selection"), ""]
  if report["test_filter"]:
    lines.append("Runner filter: " + report["test_filter"])
  rows = [[entry["file"], metric(entry["summary"], "lines"),
           metric(entry["summary"], "branches"), metric(entry["summary"], "functions")]
          for entry in report["files"]]
  totals = {}
  for name in ("lines", "branches", "functions"):
    totals[name] = {key: sum(entry["summary"].get(name, {}).get(key, 0) for entry in report["files"])
                    for key in ("count", "covered")}
  if len(rows) > 1:
    rows.append(["TOTAL", *(metric(totals, name) for name in ("lines", "branches", "functions"))])
  rows.insert(0, ["File", "Executable lines", "Branch outcomes", "Functions"])
  widths = [max(len(row[column]) for row in rows) for column in range(4)]
  lines.extend("  ".join(value.ljust(width) for value, width in zip(row, widths)).rstrip() for row in rows)
  lines += ["", "Functions never executed (all mapped instantiations have zero calls):"]
  found = False
  for entry in report["files"]:
    for function in entry["uncovered_functions"]:
      found = True
      extra = " (+{} instantiations)".format(len(function["names"]) - 1) if len(function["names"]) > 1 else ""
      lines.append("  {}:{}  {}{}".format(entry["file"], function["line"], function["names"][0], extra))
  if not found:
    lines.append("  None.")
  lines += ["", "Coverage measures execution, not correctness of assertions.",
            "Only compiled code is counted; constexpr-only helpers can appear uncovered."]
  return "\n".join(lines) + "\n"


def positive_int(value):
  number = int(value)
  if number < 1:
    raise argparse.ArgumentTypeError("must be at least 1")
  return number


def parse_args():
  parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument("files", nargs="*", help="source files or *_test.cpp files; basenames are accepted")
  selection = parser.add_mutually_exclusive_group()
  selection.add_argument("--test-file", action="append", help="select registrations declared in this file (repeatable)")
  selection.add_argument("--run", help="ZeroTest filter (quote '*' wildcards); overrides matching test files")
  selection.add_argument("--all-tests", action="store_true", help="run the entire unit test runner")
  parser.add_argument("--html", action="store_true", help="also generate an annotated HTML report")
  parser.add_argument("--build-dir", type=Path, default=ROOT / "build/coverage", help="dedicated coverage build directory")
  parser.add_argument("--output-dir", type=Path, help="new or empty report directory (default: unique directory in build/coverage/reports)")
  parser.add_argument("-j", "--jobs", type=positive_int, default=min(os.cpu_count() or 1, 4), help="parallel build jobs (default: up to 4)")
  parser.add_argument("--clangxx", default="clang++", help="Clang C++ compiler executable")
  parser.add_argument("--clang", help="matching Clang C compiler executable")
  parser.add_argument("--llvm-cov", help="matching llvm-cov executable")
  parser.add_argument("--llvm-profdata", help="matching llvm-profdata executable")
  args = parser.parse_args()
  if args.run is not None and not args.run.strip():
    parser.error("--run must not be empty")
  args.build_dir = args.build_dir.expanduser().resolve()
  return args


def main():
  args = parse_args()
  sources, tests, names = select_files(args)
  compiler = executable(args.clangxx)
  major = tool_major(compiler)
  clang = find_llvm_tool("clang", compiler, major, args.clang)
  cov = find_llvm_tool("llvm-cov", compiler, major, args.llvm_cov)
  profdata = find_llvm_tool("llvm-profdata", compiler, major, args.llvm_profdata)
  cxxfilt = find_llvm_tool("llvm-cxxfilt", compiler, major, required=False)
  if args.output_dir:
    output = args.output_dir.expanduser().resolve()
    if output.exists() and any(output.iterdir()):
      raise CoverageError("Report directory must be new or empty: {}".format(output))
    output.mkdir(parents=True, exist_ok=True)
  else:
    reports = args.build_dir / "reports"
    reports.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="run-", dir=reports))
  print("Report directory: {}".format(output), flush=True)
  binary = configure_and_build(args, compiler, clang, output)
  selected, profiles = run_tests(binary, args, names, output)
  profile = output / "tests.profdata"
  run([profdata, "merge", "-sparse", *profiles, "-o", profile], log=output / "merge.log")
  print("Exporting coverage...", flush=True)
  coverage_args = [binary, "-instr-profile=" + str(profile)]
  exported = run([cov, "export", *coverage_args, *sources])
  (output / "coverage.json").write_text(exported, encoding="utf-8")
  files = summarize(json.loads(exported), sources)
  demangle(files, cxxfilt)
  report = {"build_type": "Debug", "build_defines": BUILD_DEFINES, "assertions_enabled": False,
            "llvm_major": major, "binary": str(binary),
            "test_files": [str(path.relative_to(ROOT)) for path in tests],
            "test_filter": args.run, "tests": selected, "files": files}
  (output / "summary.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
  formatted = format_report(report)
  (output / "report.txt").write_text(formatted, encoding="utf-8")
  if args.html:
    print("Generating HTML...", flush=True)
    run([cov, "show", *coverage_args, "-format=html", "-output-dir=" + str(output / "html"),
         "-show-line-counts-or-regions", "-show-branches=count", "-show-expansions",
         "-show-instantiations", *(["-Xdemangler=" + cxxfilt] if cxxfilt else []),
         *(ROOT / entry["file"] for entry in files)], log=output / "html.log")
  print("\n" + formatted, end="")
  print("\nText: {}\nJSON: {}".format(output / "report.txt", output / "summary.json"))
  if args.html:
    print("HTML: {}".format(output / "html/index.html"))
  return 0


if __name__ == "__main__":
  try:
    sys.exit(main())
  except (CoverageError, OSError, ValueError, KeyError) as error:
    print("error: {}".format(error), file=sys.stderr)
    sys.exit(1)
  except KeyboardInterrupt:
    print("\nInterrupted.", file=sys.stderr)
    sys.exit(130)
