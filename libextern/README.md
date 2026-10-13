# Vendored libraries

Third-party code carried in the tree rather than fetched when the build is configured, as PGMEmu
carries it: what the core needs to build and test, with no network.

## The rules

- One directory per library, holding upstream's files **unmodified** and a `CMakeLists.txt` of
  ours. A change upstream needs goes upstream; a change we need is a reason to reconsider the
  library, not to fork it silently.
- Upstream's files are not formatted, not tidied, and built without the project's warning set;
  the include directory is `SYSTEM` on the consumer's side.
- The table below is updated **in the same commit** as the files it describes.

## Inventory

| Library | Upstream | Commit | Licence | Local changes | Used by |
|---|---|---|---|---|---|
| `Moira` | https://github.com/dirkwhoffmann/Moira | `ce28239b0507ebfe1bf2ad2b9c9252dc2f2cc031` (v3.0-15, 2026-09-23), PGMEmu's | MIT | none; `MoiraConfig.h` not carried | `jag_core`: the 68000, see [0013](../docs/decisions/0013-the-68000-is-moira.md) |

Of Moira, the contents of upstream's `Moira/` directory are carried, except:

- **`MoiraConfig.h`.** Moira expects its client to supply this file; ours is
  `src/jag_core/moira/MoiraConfig.h`, and `src/jag_core/src/M68000.cpp` fails the build if another
  is found.
- **`CMakeLists.txt`.** It is replaced by ours, which passes the same flags.

## Updating one

1. Take upstream at the new commit (or PGMEmu's copy of it) into a scratch directory, outside this
   tree.
2. Replace the library's files with upstream's, file by file, keeping the subset listed above.
3. Diff upstream's own `CMakeLists.txt` and `MoiraConfig.h` against the previous version, and carry
   over what changed into ours.
4. Update the commit column above, then build and check every reference in the same commit: the
   corrections of [hardware/m68000.md](../docs/hardware/m68000.md) depend on where Moira makes its
   bus cycles.
