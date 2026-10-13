# 0016: A release may be built profile-guided

**Status:** accepted

## Context

The core is a port of netlists stepped by cycle: the bus, the object processor and the blitter work
out hundreds of signals in each cycle they run, through branches whose outcomes depend on the
program. A game should run in real time; the costs are spread over many functions and no single
change of the code removes a large part of them. Clang can lay the code out and choose its branches
from a profile of runs (`-fprofile-instr-generate`, `-fprofile-instr-use`): on yarc reloaded run
through the BIOS that took 13 % off with a profile of the repository's own runs, 19 % with one of
yarc itself.

## Decision

- **Two presets, `pgo-gen` and `pgo`, and `scripts/build-pgo.sh`**, which builds the instrumented
  `pgo-gen`, runs every reference and the programs of `tests/programs` at length with it, merges the
  profile and builds `pgo` from it. Clang only.
- **Further runs to profile are the caller's**, given to the script: a run of the BIOS and a game
  kept outside the repository ([0015](0015-bios-and-games-checked-outside-the-repository.md)) makes
  the profile closer to what is played. The profile is not kept in the repository.
- **`release` stays the build of development and of the checks.** A profile changes how the code is
  laid out, not what it does; `pgo` passes the same references.

## Consequences

- A profile-guided build takes two builds and the runs between them, a few minutes.
- The profile grows stale as the code changes: the script is run again, not the old profile reused.
- How fast `pgo` is depends on what it was trained on; timings compare builds of the same profile.
