# 0015: The BIOS and games are checked outside the repository

**Status:** accepted

## Context

What M9 is for is the console booting: Atari's BIOS checking a cartridge and starting a game. Both
are copyrighted, and so is what a run of them records: a trace's `D` events are the 68000's bus
cycles, the BIOS's and the game's code among them, and its frames are their pictures. The
repository is GPL-2.0 ([0005](0005-licence-gpl-2.md)) and carries only what it may give away. A
run of the BIOS is also long: it shows its logo for seconds of console time, which the simulation
takes most of an hour to reach.

## Decision

- **No BIOS, game image, or recording of them is part of the repository.** `jagsim` and the core
  take them by path (`--bios`, `--cart`), and a check of them is a reference recorded outside the
  repository, from images the owner keeps beside it, with `jagmcp-cli --check`.
- **A part of such a run** is recorded with `--log-from`: the events from a cycle on, so that the
  window where the core first differs is recorded without the hours before it.
- **What a cartridge exercises is covered in the repository by programs of our own** (`tests/m68k`
  with a section `.cart`): its ROM cycles at each width and speed, the RISCs and the blitter
  reading it, its EEPROM.

## Consequences

- `ctest` does not run the BIOS. A change to the core that the references in the repository do not
  catch can break the BIOS's boot unseen until a local check runs.
- A difference found in a run of the BIOS is turned into a program of our own that shows it before
  it is fixed (`pipe_jr_store_index`), so that the repository's references keep it fixed.
