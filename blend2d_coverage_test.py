#!/usr/bin/env python3
"""Regression checks for the coverage reporter; requires only Python's stdlib.

Run with: python3 -m unittest blend2d_coverage_test
These tests use LLVM-shaped exports and mock runner output, so they require no
C++ rebuild or installed LLVM tools.
"""

import contextlib
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import blend2d_coverage as coverage


class CoverageTestCase(unittest.TestCase):
  def setUp(self):
    temporary = tempfile.TemporaryDirectory(prefix="blend2d-coverage-test-")
    self.addCleanup(temporary.cleanup)
    self.root = Path(temporary.name)
    self.package = self.root / "blend2d"
    self.package.mkdir()
    self.output = self.root / "report"
    self.output.mkdir()
    for name, value in (("ROOT", self.root), ("PACKAGE", self.package)):
      patcher = patch.object(coverage, name, value)
      patcher.start()
      self.addCleanup(patcher.stop)
    self.stdout = io.StringIO()
    self.stderr = io.StringIO()
    for stream, capture in (("stdout", self.stdout), ("stderr", self.stderr)):
      redirect = getattr(contextlib, "redirect_" + stream)(capture)
      redirect.__enter__()
      self.addCleanup(redirect.__exit__, None, None, None)

  def write_source(self, name, text):
    path = self.package / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


class TestRegistrations(CoverageTestCase):
  def test_line_splicing_precedes_comment_and_macro_recognition(self):
    path = self.write_source("spliced_test.cpp",
      "// Disabled test on a continued comment line \\\n"
      "TEST_CASE(disabled_by_splice) {}\n"
      "// A second continued comment with CRLF \\\r\n"
      "TEST_CASE(also_disabled_by_splice) {}\n"
      "TEST_CA\\\nSE(actual_spliced_case) {}\n"
      "TEST_CASE(actual_case) {}\n")
    self.assertEqual(coverage.test_names(path), ["actual_spliced_case", "actual_case"])

  def test_comments_and_literals_do_not_register_tests(self):
    path = self.write_source("example_test.cpp", r'''
// TEST_CASE(commented_out) {}
/* TEST_CASE_T(also_commented_out, Types) {} */
const char* text = "TEST_CASE(in_a_string) // not a comment";
const char* escaped = "escaped \" quote TEST_CASE(in_escaped_string)";
const char quote = '\'';
const char* raw = R"tag(TEST_CASE(in_raw_string) /* not a comment */)tag";
const char* multiline = u8R"other(
  TEST_CASE(in_multiline_raw_string)
  )tag" // a different delimiter cannot end this raw string
)other";
TEST_CASE /* comment between tokens */ (actual_test) {}
TEST_CASE_T(
  actual_typed_test, ZeroTest::types<int, double>) {}
TEST_CASE(actual_test) {} // Extract each registration name only once.
''')
    self.assertEqual(coverage.test_names(path), ["actual_test", "actual_typed_test"])

  def test_test_names_need_not_match_file_names(self):
    source = self.write_source("core/example.cpp", "")
    test = self.write_source("core/example_test.cpp", "TEST_CASE(unrelated_name) {}")
    args = SimpleNamespace(files=["example.cpp"], test_file=None, run=None, all_tests=False)
    self.assertEqual(coverage.select_files(args), ([source], [test], ["unrelated_name"]))

  def test_explicit_test_file_can_cover_several_sources(self):
    first = self.write_source("core/first.cpp", "")
    second = self.write_source("core/second.h", "")
    test = self.write_source("core/shared_test.cpp", "TEST_CASE_T(shared_case, Types) {}")
    args = SimpleNamespace(files=["first.cpp", "second.h"], test_file=["shared_test.cpp"],
                           run=None, all_tests=False)
    self.assertEqual(coverage.select_files(args), ([first, second], [test], ["shared_case"]))


class TestCoverageSummaries(CoverageTestCase):
  def setUp(self):
    super().setUp()
    self.source = self.package / "core/example.cpp"
    self.header = self.package / "core/macros_p.h"

  @staticmethod
  def summary(functions, covered):
    return {"functions": {"count": functions, "covered": covered,
                          "percent": 100.0 * covered / functions if functions else 0.0},
            "lines": {"count": 720, "covered": 487, "percent": 67.63888888888889},
            "branches": {"count": 387, "covered": 193, "notcovered": 194,
                         "percent": 49.87080103359173}}

  def function(self, name, count, region, *, filenames=None, other_regions=()):
    return {"name": name, "count": count,
            "filenames": [str(path) for path in (filenames or [self.source])],
            "regions": [region, *other_regions], "branches": []}

  def exported(self, functions, *, files=None):
    if files is None:
      files = [(self.source, self.summary(1, 0))]
    return {"type": "llvm.coverage.json.export", "version": "2.0.1",
            "data": [{"files": [{"filename": str(path), "summary": summary,
                                   "segments": [], "branches": [], "expansions": []}
                                  for path, summary in files],
                      "functions": functions}]}

  def test_templates_are_uncovered_only_when_all_instances_have_zero_calls(self):
    functions = [
      self.function("_Z7coveredIiEvT_", 0, [10, 1, 15, 2, 0, 0, 0, 0]),
      self.function("_Z7coveredIdEvT_", 4, [10, 1, 15, 2, 4, 0, 0, 0]),
      self.function("_Z7missingIiEvT_", 0, [20, 1, 25, 2, 0, 0, 0, 0]),
      self.function("_Z7missingIdEvT_", 0, [20, 1, 25, 2, 0, 0, 0, 0])]
    llvm_summary = self.summary(2, 1)
    result = coverage.summarize(self.exported(functions, files=[(self.source, llvm_summary)]),
                                [self.source])
    self.assertEqual(result[0]["summary"], llvm_summary)
    missing = result[0]["uncovered_functions"]
    self.assertEqual(len(missing), 1)
    self.assertEqual((missing[0]["line"], missing[0]["end_line"]), (20, 25))
    self.assertEqual(missing[0]["execution_count"], 0)
    self.assertEqual(set(missing[0]["mangled_names"]),
                     {"_Z7missingIiEvT_", "_Z7missingIdEvT_"})

  def test_definition_file_id_does_not_attribute_macro_expansion_as_function(self):
    functions = [self.function(
      "_Z7missingv", 0,
      # Expansion regions precede the definition's first code region.
      [20, 1, 20, 5, 0, 1, 0, 1],
      filenames=[self.header, self.source],
      other_regions=([20, 1, 25, 2, 0, 1, 0, 0],
                     [4, 1, 4, 9, 0, 0, 0, 0])),
      self.function("_Z13header_helperv", 0, [40, 1, 42, 2, 0, 0, 0, 0],
                    filenames=[self.header])]
    result = coverage.summarize(self.exported(functions, files=[
      (self.source, self.summary(1, 0)), (self.header, self.summary(1, 0))]),
      [self.source, self.header])
    by_name = {entry["file"]: entry["uncovered_functions"] for entry in result}
    source_missing = by_name["blend2d/core/example.cpp"]
    header_missing = by_name["blend2d/core/macros_p.h"]
    self.assertEqual([function["line"] for function in source_missing], [20])
    self.assertEqual([function["line"] for function in header_missing], [40])

  def test_same_line_functions_with_different_columns_remain_distinct(self):
    functions = [self.function("_ZL6firstv", 5, [10, 1, 10, 12, 5, 0, 0, 0]),
                 self.function("_ZL6secondv", 0, [10, 15, 10, 28, 0, 0, 0, 0])]
    missing = coverage.summarize(self.exported(functions), [self.source])[0]["uncovered_functions"]
    self.assertEqual([function["mangled_names"] for function in missing], [["_ZL6secondv"]])

  def test_global_function_array_is_filtered_to_selected_source(self):
    functions = [self.function("_Z8selectedv", 0, [10, 1, 15, 2, 0, 0, 0, 0]),
                 self.function("_Z9unrelatedv", 0, [20, 1, 25, 2, 0, 0, 0, 0],
                               filenames=[self.header])]
    exported = self.exported(functions, files=[(self.source, self.summary(1, 0)),
                                             (self.header, self.summary(1, 0))])
    result = coverage.summarize(exported, [self.source])
    self.assertEqual(len(result), 1)
    self.assertEqual(result[0]["uncovered_functions"][0]["mangled_names"], ["_Z8selectedv"])

  def test_missing_source_mapping_is_rejected(self):
    with self.assertRaisesRegex(coverage.CoverageError, "No coverage mapping.*macros_p.h"):
      coverage.summarize(self.exported([]), [self.source, self.header])

  def test_default_report_excludes_test_and_external_files(self):
    paths = [self.source, self.header, self.package / "core/example_test.cpp",
             self.package / "core/api_build_test_p.h", self.root / "3rdparty/asmjit.cpp"]
    exported = self.exported([], files=[(path, self.summary(0, 0)) for path in paths])
    result = coverage.summarize(exported, [])
    self.assertEqual([entry["file"] for entry in result],
                     ["blend2d/core/example.cpp", "blend2d/core/macros_p.h"])


class TestRunnerProfiles(CoverageTestCase):
  def fake_runner(self, listing, *, passed=None, write_profiles=True):
    calls = []

    def fake_run(command, *, env, log):
      command = [str(value) for value in command]
      calls.append((command, dict(env)))
      is_listing = "--list" in command
      if is_listing or write_profiles:
        profile = Path(env["LLVM_PROFILE_FILE"].replace("%p", str(1000 + len(calls))))
        profile.write_bytes(b"fake-profile")
      if is_listing:
        return listing
      return passed if passed is not None else "PASSED [1 test, 10 skipped]\n"

    return fake_run, calls

  def test_exact_file_selections_merge_only_their_test_profiles(self):
    listing = "Tests:\n- first_case\n- second_case\n- unrelated_case\n"
    fake, calls = self.fake_runner(listing)
    with patch.object(coverage, "run", side_effect=fake):
      selected, profiles = coverage.run_tests(
        self.root / "runner", SimpleNamespace(run=None),
        ["second_case", "first_case"], self.output)
    self.assertEqual(selected, ["second_case", "first_case"])
    self.assertEqual([command[-2:] for command, _ in calls[1:]],
                     [["--run", "second_case"], ["--run", "first_case"]])
    self.assertEqual(len(profiles), 2)
    self.assertTrue(all(path.name.startswith("tests-") and path.is_file() for path in profiles))
    self.assertEqual(len(list(self.output.glob("listing-*.profraw"))), 1)
    self.assertEqual(len({env["LLVM_PROFILE_FILE"] for _, env in calls}), 3)

  def test_runner_exit_success_with_no_matches_is_rejected(self):
    listing = ('No tests matched --run "missing*"\n'
               '  - use --run xxx to run a test matching xxx exactly\n')
    fake, calls = self.fake_runner(listing)
    with patch.object(coverage, "run", side_effect=fake):
      with self.assertRaisesRegex(coverage.CoverageError, "No tests matched"):
        coverage.run_tests(self.root / "runner", SimpleNamespace(run="missing*"), [], self.output)
    self.assertEqual(len(calls), 1)

  def test_success_exit_without_expected_pass_count_is_rejected(self):
    for result in ("NO TESTS MATCHED [--run first_case] filter!\n", "PASSED [2 tests]\n"):
      with self.subTest(result=result):
        fake, _ = self.fake_runner("Tests:\n- first_case\n", passed=result)
        with patch.object(coverage, "run", side_effect=fake):
          with self.assertRaisesRegex(coverage.CoverageError, "Expected 1 passing test"):
            coverage.run_tests(self.root / "runner", SimpleNamespace(run=None),
                               ["first_case"], self.output)

  def test_successful_test_must_write_a_profile(self):
    fake, _ = self.fake_runner("Tests:\n- first_case\n", write_profiles=False)
    with patch.object(coverage, "run", side_effect=fake):
      with self.assertRaisesRegex(coverage.CoverageError, "No LLVM profile"):
        coverage.run_tests(self.root / "runner", SimpleNamespace(run=None),
                           ["first_case"], self.output)

  def test_platform_conditional_tests_are_reported_and_excluded(self):
    fake, _ = self.fake_runner("Tests:\n- active_case\n")
    with patch.object(coverage, "run", side_effect=fake):
      selected, _ = coverage.run_tests(self.root / "runner", SimpleNamespace(run=None),
                                       ["inactive_case", "active_case"], self.output)
    self.assertEqual(selected, ["active_case"])
    self.assertIn("Registrations absent from this build: inactive_case", self.stderr.getvalue())

  def test_selection_with_no_compiled_registrations_is_rejected(self):
    fake, calls = self.fake_runner("Tests:\n- unrelated_case\n")
    with patch.object(coverage, "run", side_effect=fake):
      with self.assertRaisesRegex(coverage.CoverageError, "None of the selected file"):
        coverage.run_tests(self.root / "runner", SimpleNamespace(run=None),
                           ["inactive_case"], self.output)
    self.assertEqual(len(calls), 1)

  def test_whole_runner_and_wildcard_selections_run_once(self):
    for selection in (None, "core_example*"):
      with self.subTest(selection=selection):
        fake, calls = self.fake_runner("Tests:\n- first_case\n- second_case\n",
                                       passed="PASSED [2 tests]\n")
        with patch.object(coverage, "run", side_effect=fake):
          selected, profiles = coverage.run_tests(
            self.root / "runner", SimpleNamespace(run=selection), [], self.output)
        self.assertEqual(selected, ["first_case", "second_case"])
        self.assertEqual(len(calls), 2)
        self.assertEqual(len(profiles), 1)
        if selection:
          self.assertEqual(calls[0][0][-2:], ["--run", selection])
          self.assertEqual(calls[1][0][-2:], ["--run", selection])
        else:
          self.assertNotIn("--run", calls[1][0])


class TestBuildIsolation(CoverageTestCase):
  def fake_cmake(self, build):
    calls = []

    def execute(command, *, log):
      calls.append([str(argument) for argument in command])
      if "--build" in calls[-1]:
        (build / "bl_test_runner").write_bytes(b"fake binary")
      return ""

    return execute, calls

  def test_coverage_build_disables_assertions_without_optimization(self):
    build = self.root / "build/coverage"
    fake, calls = self.fake_cmake(build)
    with patch.object(coverage, "run", side_effect=fake), \
         patch.object(coverage, "executable", return_value="/tools/cmake"):
      binary = coverage.configure_and_build(SimpleNamespace(build_dir=build, jobs=2),
                                            "/tools/clang++", "/tools/clang", self.output)
    self.assertEqual(binary, build / "bl_test_runner")
    configuration = calls[0]
    for language in ("C", "CXX"):
      prefix = "-DCMAKE_{}_FLAGS=".format(language)
      flags = next(argument[len(prefix):] for argument in configuration
                   if argument.startswith(prefix)).split()
      self.assertIn("-DBL_BUILD_RELEASE", flags)
      self.assertIn("-fprofile-instr-generate", flags)
      self.assertIn("-fcoverage-mapping", flags)
      self.assertIn("-DCMAKE_{}_FLAGS_DEBUG=-O0 -g".format(language), configuration)

  def test_original_coverage_build_is_reconfigured_with_assertions_disabled(self):
    build = self.root / "build/coverage"
    build.mkdir(parents=True)
    (build / "CMakeCache.txt").write_text("# Original coverage configuration\n", encoding="utf-8")
    marker = build / ".blend2d-coverage.json"
    original = {"source": str(self.root), "clang": "/tools/clang", "clang++": "/tools/clang++",
                "instrumentation": coverage.INSTRUMENTATION}
    marker.write_text(json.dumps(original), encoding="utf-8")
    fake, calls = self.fake_cmake(build)
    with patch.object(coverage, "run", side_effect=fake), \
         patch.object(coverage, "executable", return_value="/tools/cmake"):
      coverage.configure_and_build(SimpleNamespace(build_dir=build, jobs=2),
                                   "/tools/clang++", "/tools/clang", self.output)
    self.assertEqual(len(calls), 2)
    self.assertIn("--preset", calls[0])
    self.assertIn("--build", calls[1])
    self.assertEqual(json.loads(marker.read_text(encoding="utf-8")),
                     dict(original, build_defines=["BL_BUILD_RELEASE"]))
    self.assertTrue(any(argument.startswith("-DCMAKE_CXX_FLAGS=")
                        and "-DBL_BUILD_RELEASE" in argument.split("=", 1)[1].split()
                        for argument in calls[0]))

  def test_owned_coverage_build_rejects_changed_compilers(self):
    build = self.root / "build/coverage"
    build.mkdir(parents=True)
    marker = build / ".blend2d-coverage.json"
    original = {"source": str(self.root), "clang": "/tools/clang", "clang++": "/tools/clang++",
                "instrumentation": coverage.INSTRUMENTATION}
    marker.write_text(json.dumps(original), encoding="utf-8")
    for compiler, clang in (("/other/clang++", "/tools/clang"),
                            ("/tools/clang++", "/other/clang")):
      with self.subTest(compiler=compiler, clang=clang), patch.object(coverage, "run") as execute:
        with self.assertRaisesRegex(coverage.CoverageError, "compiler changed"):
          coverage.configure_and_build(SimpleNamespace(build_dir=build, jobs=2),
                                       compiler, clang, self.output)
        execute.assert_not_called()
      self.assertEqual(json.loads(marker.read_text(encoding="utf-8")), original)

  def test_cpp_driver_symlink_is_preserved(self):
    tools = self.root / "tools"
    tools.mkdir()
    target = tools / "clang"
    target.write_text("#!/bin/sh\n", encoding="utf-8")
    target.chmod(0o755)
    driver = tools / "clang++"
    driver.symlink_to("clang")
    self.assertEqual(coverage.executable(str(driver)), str(driver))
    self.assertNotEqual(coverage.executable(str(driver)), str(target))

  def test_existing_developer_build_is_not_reconfigured(self):
    build = self.root / "build/debug"
    build.mkdir(parents=True)
    cache = build / "CMakeCache.txt"
    cache.write_text("CMAKE_BUILD_TYPE:STRING=Debug\n", encoding="utf-8")
    with patch.object(coverage, "run") as execute:
      with self.assertRaisesRegex(coverage.CoverageError, "existing build not created by this script"):
        coverage.configure_and_build(SimpleNamespace(build_dir=build),
                                     "/tools/clang++", "/tools/clang", self.output)
    execute.assert_not_called()
    self.assertEqual(cache.read_text(encoding="utf-8"), "CMAKE_BUILD_TYPE:STRING=Debug\n")

  def test_failed_configuration_can_retry_its_owned_build(self):
    build = self.root / "build/coverage"
    marker = build / ".blend2d-coverage.json"
    args = SimpleNamespace(build_dir=build, jobs=2)
    calls = []

    def fake_cmake(command, *, log):
      calls.append([str(argument) for argument in command])
      # A failing CMake configure commonly still writes CMakeCache.txt.
      self.assertTrue(marker.is_file(), "ownership must be recorded before invoking CMake")
      if len(calls) == 1:
        (build / "CMakeCache.txt").write_text("# Partial CMake configuration\n", encoding="utf-8")
        raise coverage.CoverageError("simulated configure failure")
      if "--build" in calls[-1]:
        (build / "bl_test_runner").write_bytes(b"fake binary")
      return ""

    with patch.object(coverage, "run", side_effect=fake_cmake), \
         patch.object(coverage, "executable", return_value="/tools/cmake"):
      with self.assertRaisesRegex(coverage.CoverageError, "simulated configure failure"):
        coverage.configure_and_build(args, "/tools/clang++", "/tools/clang", self.output)
      self.assertTrue((build / "CMakeCache.txt").is_file())
      binary = coverage.configure_and_build(args, "/tools/clang++", "/tools/clang", self.output)
    self.assertEqual(binary, build / "bl_test_runner")
    self.assertEqual(len(calls), 3)
    self.assertTrue(all("--preset" in command for command in calls[:2]))
    self.assertIn("--build", calls[-1])


if __name__ == "__main__":
  unittest.main()
