# 0005: Licence is GPL-2.0

**Status:** accepted

## Context

The owner prefers GPL-2.0, as PGMEmu has, if what the emulator is derived from allows it. Per
[0002](0002-the-fpga-core-is-the-reference.md) the emulated hardware is ported from the
Jaguar_MiSTer core: the microcode table, the datapath of `arith.v` and the scoreboard's rules are
taken from its netlists. The core ships the GPL-2 text (`LICENSE`, since 2020) and states no "or
later". Its `rtl/README.txt`, Gregory Estrade's from 2013, still says "All rights reserved (until I
sort out the licensing stuff)"; the core has been published under the GPL-2 since.

## Decision

JagMCP is released under GPL-2.0 only, the licence the core is published under.

- The simulation harness in `sim/` is ours and under the same licence. It does not carry the core's
  RTL; it builds against a checkout of it.
- The test programs in `tests/asm/` come from TomNJerry, public domain; `jaghard.inc` is Atari's
  hardware equates in lyxass form. They are assembled for the tests and are not part of the
  emulator.

## Consequences

- No GPL-3 code may be copied in (Gearlynx stays a design reference only, as in PGMEmu).
- If the core's authors confirm "GPL-2.0 or later", or the 2013 notice is withdrawn in a way that
  changes the terms, a new record revisits this one.
- [THIRD_PARTY.md](../../THIRD_PARTY.md) names the work JagMCP is derived from.
