# Docling Blend2D backports

Planned release: `docling-0.21.2-patches.1`.

Base: upstream `6dbc2cefbc996379e07104e34519a440b49b15d7` (Blend2D 0.21.2).
This deliberately retains the existing Docling/AsmJit combination rather than
including the 0.21.3 AsmJit API migration from upstream master.

| Upstream PR | Original commit | Fix |
| --- | --- | --- |
| [#268](https://github.com/blend2d/blend2d/pull/268) | `7ed62afea305fe91e64273635adffded71339828` | Reset stale analytic-fill span length in JIT and reference pipelines |
| [#270](https://github.com/blend2d/blend2d/pull/270) | `9db89f811916fa751895e2551476414e4e892864` | Stop quadratic offset subdivision when a split makes no progress |
| [#271](https://github.com/blend2d/blend2d/pull/271) | `3ed5b6c6a38799e509011a1c4885df7e3b166746` | Handle degenerate offset directions and join curve offsets |
| [#273](https://github.com/blend2d/blend2d/pull/273) | `86c847930adb84bf8c7d213ddb5272a07d7b6cc9` | Preserve flat-cubic cusps using stable quadratic roots |

Original author: Jeff Witt <1848307+wittjeff@users.noreply.github.com>.
The original commits for #270/#271/#273 also credit Claude Opus 5.5
<noreply@anthropic.com>. Upstream regression tests are included with the fixes.
Preserve this provenance when committing the consolidated changes.

The runtime sources match the four backports previously validated in
docling-parse byte for byte. Additional changes are the upstream tests and
their CMake registration. The tested AsmJit revision is
`0bd5787b54b575ed94bf32ac452153b34385c514`.

## Validation

Build with the tested AsmJit checkout; its path is supplied explicitly so that
an unrelated AsmJit update cannot change generated rasterization pipelines:

```bash
cmake -S . -B build-docling \
  -DASMJIT_DIR=/absolute/path/to/asmjit \
  -DBLEND2D_STATIC=ON -DBLEND2D_TEST=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-docling --target bl_test_runner --parallel 4
./build-docling/bl_test_runner
```

The docling-parse integration additionally runs its direct native regression
cases, Python unit tests, real Honda page-7 image check, and rendering evidence.

Validated on 2026-10-30 with AppleClang 17 / macOS ARM64: all upstream unit
tests pass; docling-parse's five native cases and 317 Python cases pass (two
Python skips). Its rebuilt extension is byte-identical to the previously
validated patch-based build, preserving the recorded 3,370-page render results.

## Release policy

Create the annotated release tag after committing and validating these changes.
Never move a published tag; use `docling-0.21.2-patches.2` for a subsequent
release. Consumers can resolve the release tag to a full commit SHA for their
dependency pin. Future upgrades should deliberately select an upstream base,
drop fixes already merged there, and repeat the tests with the matching AsmJit
revision. Return consumers to upstream once it includes the required fixes.
