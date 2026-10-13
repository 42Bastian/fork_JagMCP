# 0024: A stopped 68000 lets the RISCs run on to where it could next make a bus cycle

**Status:** accepted

## Context

With the 68000 leading ([0014](0014-three-processors-the-68000-leads.md)), a 68000 stopped by STOP
polls Tom's interrupt line every two of its clocks, four bus cycles, the line ten cycles back
(`IPL_STOPPED`). Each poll ran the RISCs up to the cycle after its sample, and the bus with them: a
turn every four bus cycles. A game that waits for its vertical interrupt with STOP spends most of a
frame so: in 12.5 s of Native's gameplay, 71 million polls, most of the run's work done in turns
four cycles long.

A stopped 68000 puts nothing on the bus until a poll takes an interrupt. Its first bus cycle then is
the push of PC's low word, six clocks after the poll (Moira's `execInterrupt`): its AS 20 cycles
after the poll's sample at the soonest (`M68000::WAKE_START`). Before it the RISCs cannot be
reached by the 68000, as before a bus cycle of a running 68000
([0023](0023-the-riscs-run-through-the-68000s-memory-cycles.md)).

## Decision

- **A poll of a stopped 68000 runs the RISCs on to `WAKE_START` past its sample**, not to the cycle
  after it; the polls within that reach step the bus alone, each still sampling the line where it
  did.

## Consequences

- Native's gameplay (12.5 s of the console, the 68000 stopped most of each frame) takes 20.2 s
  rather than 21.4 s. Where the 68000 is seldom stopped nothing changes: the first 15 s of Native
  30.4 s rather than 30.6 s, of Cybermorph 33.9 s, of Doom 74.8 s, of Kasumi Ninja 70.7 s, as
  before.
- The run is the same cycle for cycle: every reference equal in both modes, each of the 104
  cartridges' state after 30M bus cycles and 15 s of Native and of Cybermorph the same as before,
  yarc reloaded as the simulation leaves it.
- Taking a poll without Moira's `execute()`, and the 68000's horizon moved on to the next poll's
  reach, were tried as well and gained nothing more.
