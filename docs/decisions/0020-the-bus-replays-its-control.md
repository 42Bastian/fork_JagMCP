# 0020: The main bus replays its control from transitions it records

**Status:** accepted

## Context

With the 68000 running (a cartridge through the BIOS) the main bus is stepped in every cycle: the
68000 is on it nearly all the time, polling, fetching from ROM, copying, while the GPU, the
blitter and the object processor work beside it. A rest ([0008](0008-main-bus-by-cycle.md)) never
starts. Cybermorph ran about 3.5 times slower than the console, and stepping the bus was half of
that time. Measured, the logic of the bus's own state machines (arbiter, memory controller, CPU
interface, Jerry's master and slave, the gateways, the 68000's arbitration, the width sequencer,
the refresh) was some four fifths of a cycle's cost, whoever else was at work; the object
processor's and the blitter's own logic the rest.

The RISCs met the same problem in their pipeline and answered it with an automaton
([0007](0007-timing-automaton.md)): the control state as a node, what the stepped core did from it
recorded and replayed, the data kept live.

## Decision

- **The bus's control state is one value** (`MainBus::Control`): every flip-flop that steers the
  bus, without the addresses and data it moves and without absolute times. Bytes, without
  padding, so that two states compare and hash as their bytes.
- **A cycle is worked out in three parts**: what the control reads besides its own state is
  gathered first (the RISCs' requests and DMAEN, the 68000's cycle under way and where the cycle
  is in it, the object processor's and the blitter's requests, Tom's interrupt handler, the
  refresh counters at their ends, and what the address on the bus decodes to); the next state and
  what it tells the data side, from those two alone; the data moved as they say.
- **The middle part is replayed**: the state is a node, the gathered inputs a 64-bit key, a
  transition the next node and the signals. A key not seen from a node is worked out and recorded.
  Writing MEMCON1 or MEMCON2, on which the transitions depend, drops the automaton; so does growing
  past a million nodes.
- `--step` works every cycle out, as it steps the RISCs' cores alone; every reference is checked in
  both modes. Built with `JAG_BUS_CHECK` defined, every replayed transition is also worked out and
  compared, which tells a key that leaves out something the control reads.

## Consequences

- The bus's cycle no longer pays for the state machines' logic, only for gathering its inputs,
  finding the transition and moving the data. The gain is modest so far (Cybermorph 54 s to 50 s
  for 15 s of the console, yarc reloaded 17.5 s to 16 s): the gathering and the data side, and
  the memory the transitions are scattered over, cost about as much again. Replaying several
  cycles at once, where the inputs over them are known, is what remains.
- Some 300,000 nodes and a million transitions for a game's first minute, about 100 MB.
- Code that changes the control state outside a cycle (a rest, the constructor) takes it out of
  its node first.
