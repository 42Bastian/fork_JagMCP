# 0025: CI builds three platforms, and a tag makes a release

**Status:** accepted

## Context

JagMCP was built only on the owner's Mac, with Apple clang. PGMEmu, whose window, control API and
build it follows ([0011](0011-the-window-is-pgmemus.md), [0018](0018-one-control-api.md)), already
builds on three platforms from GitHub Actions and publishes releases from tags
(PGMEmu's decision 0022); the same layout is taken. What is not the same: the tests here are the
recorded references of the FPGA simulation, which are text and must reach a Windows runner byte for
byte.

## Decision

- `.github/workflows/ci.yml` builds and tests on every push and pull request:
  - macOS arm64 (Apple clang, macOS 13 or later), Linux x64 (GCC 14, Ubuntu 24.04) and Windows x64
    (MSVC, its runtime linked in);
  - with warnings as errors (`JAG_WERROR`), so that what one compiler warns of is fixed before it
    is merged;
  - `scripts/format.sh --check` with clang-format 23, the version the code is formatted with.
- What CI runs is `ctest`: the recorded references (twice, fast and stepped), the control
  protocol's transcripts and, off Windows, the network servers. The BIOS and the games are checked
  outside the repository ([0015](0015-bios-and-games-checked-outside-the-repository.md)); a change
  to the core that only a game shows is found where the images are, before a release.
- `.gitattributes` keeps text LF in every checkout and leaves `tests/reference` and
  `tests/programs` as they are, so that a runner's `autocrlf` does not change the traces the core
  is compared with.
- `src/jag_core/src/PortabilityChecks.cpp` asserts the C++23 library features the code uses, so that one that
  is missing on a platform fails the build there and not in a later change.
- Each platform's package is what `cmake --install <build> --component jagmcp` holds
  (`cmake/Install.cmake`): the window, `jagmcp-cli`, the licence, the credits and the licence of
  every library linked in. On macOS the window is `JagMCP.app`, assembled at install time so that
  the build tree keeps its executable where the scripts find it.
- A tag `v*` publishes the three packages as a GitHub release; the version is the tag's, as
  `cmake/Version.cmake` reads it. The macOS application is signed ad hoc, not by a developer.
- The profile-guided build (`scripts/build-pgo.sh`, [0016](0016-profile-guided-release.md)) is not
  part of CI: it is Clang-only and takes a run of games to train on. Release packages are the
  plain `release` preset, which is slower than the author's PGO build.

## Consequences

- A change that builds on one platform and not another fails CI before it is merged.
- The first CI run on Linux and Windows is also the first build there: what GCC and MSVC refuse is
  fixed as it is found.
- The published binaries are slower than a local PGO build; a user who wants the speed builds it
  with Clang.
- Moving to a notarized application needs a Developer ID and its certificate as the repository's
  secrets.
