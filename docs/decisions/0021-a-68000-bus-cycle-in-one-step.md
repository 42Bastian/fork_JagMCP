# 0021: A 68000 bus cycle on a quiet bus is replayed in one step

**Status:** accepted

## Context

The bus's automaton ([0020](0020-the-bus-replays-its-control.md)) took the state machines' logic
out of a cycle, but a cycle still gathered its inputs, found its transition and moved its data one
at a time. In Cybermorph more than half of the bus's cycles have nothing but the 68000 and the
refresh at work: the 68000 fetching from the cartridge, polling main RAM, copying, while the GPU
works in its local RAM and the object processor waits for a line. Such a cycle is the same work
every time the 68000 makes the same access from the same state.

## Decision

- **A span** is a 68000 bus cycle from the cycle it is asked in through DTACK, on a bus where
  nothing else is at work: no RISC's request, no interrupt passed on, no data of another master's
  under way, the blitter settled, the object processor waiting for a line with its inputs settled,
  Tom's interrupt handler not changing (`Tom::ihandlerSteady`). It is recorded the first time it
  is stepped, as the data moves its cycles make (the 68000's address and DTACK, the data bus's
  latches, its read or write, the rows and the address Tom holds, a cycle's start, the object
  processor's clock and the line buffers' bus where they change) and the node it ends in.
- **It is replayed in one step** when the access is asked from the same node with the same inputs:
  the access, where its address and the one still on the bus decode to and the rows they meet, the
  address Tom holds, Tom's interrupt handler, the object processor at work or not, and the refresh
  counters as far as they act within the span (where the prescaler ends, the period counter
  then). The refresh counters are then moved by arithmetic.
- **The RISCs run up to the span's end first**, the bus held: the 68000 leads the turns
  ([0014](0014-three-processors-the-68000-leads.md)), so the RISCs are usually a few cycles behind
  it and the bus could not go on. A RISC that needs the bus stops, one that hands it a request or
  an interrupt shows it, and the cycles are then stepped as before.
- `--step` steps them; built with `JAG_BUS_CHECK` defined, every replay is also done without what
  reaches outside the bus, the cycles stepped, and the two compared.

## Consequences

- Cybermorph's first 15 s of the console take 45 s, where they took 49 s: three quarters of the
  68000's bus cycles are replayed, some 45% of the bus's cycles. The rest of a cycle's work is
  where other masters are at work, and outside the bus.
- The RISCs are run ahead of the bus more often than before; that they run by turns and the bus
  steps only what all have handed over is what keeps it exact.
