# 0009: The GPU and the DSP run by turns, the bus stepped as both hand it their cycles

**Status:** accepted

## Context

A GPU program may start the DSP: it writes the DSP's program into its RAM over the bus, sets D_PC
and DSPGO, and from then on both work on main RAM. JaguarDemos' small intros do this (one of them
draws a Julia set, the GPU one pixel in two, the DSP the others). Their requests meet at Tom's
arbiter, so one bus state serves both; the cycle the DSP starts in, and every access from then on,
depends on the other's.

Each processor's core runs ahead on its own in its automaton
([0007](0007-timing-automaton.md)) and only hands the bus a cycle while a request of its own is
under way ([0008](0008-main-bus-by-cycle.md)). Stepping the two together cycle by cycle would keep
them right but give up the automaton for both.

## Decision

- **One bus, a port for each master** (`MainBus`): the DSP's gateway behind Jerry's bus master, the
  GPU's on Tom's own bus. Each processor hands the bus the requests of its cycles in order, and
  how far it has got (its horizon); the bus steps a cycle once both horizons are past it. A
  stopped processor holds nothing back.
- **A processor waits at the start of a cycle** whose bus outputs it needs and the bus cannot step
  yet: its run returns there, and resumes where it left off. Nowhere else: while the bus works for
  a processor and the other runs too, that processor is stepped, not replayed, so that no node of
  the automaton is cut by a wait.
- **The runner runs the one behind**, the GPU first when both are at the same cycle: a cycle of the
  DSP latches its timers' requests after the bus has stepped it, and that needs the GPU's request of
  the cycle. The GPU runs until it waits; the DSP up to the GPU.
- **The GPU reaches the DSP through Jerry's slave**, as the netlist does: Tom's I/O cycle, a half at
  a time, Jerry's slave states (`j_jmem.v` `qs0` to `qs4`), the DSP's slave port (`gpu_cpu.v`:
  the high half latched, the low half asking for the DSP's local bus, `ioreq`), served by the DSP's
  memory controller in the next cycle (`ioserv`). A GO written this way starts the DSP in the cycle
  after. While the GPU has an access to Jerry's space under way, the DSP steps the bus with it.
- **The trace and the state are both processors'**, the one the program did not start prefixed
  with its name ([spec/trace.md](../spec/trace.md)), so that a reference checks both.
- **The simulation boots the GPU as the BIOS does**: the 68000 writes D_END as well as G_END, so
  that Jerry is big endian when the GPU starts the DSP. Its DRAM is addressed as the MiSTer core's
  SDRAM controller addresses it, by the row Tom puts out as RAS falls and the column as CAS falls,
  rather than by the address bus at CAS: a write whose master has lost the bus by then went to the
  address of the master that took it.

## Consequences

- Every reference was re-recorded: the GPU's with main RAM traffic changed with the new boot,
  `sine`'s state lost the wave ROM's printouts (now patched out of the build), nothing else
  changed. Two intros of JaguarDemos are references; the Julia one is equal for 4.5
  million events, both processors', over two million cycles.
- With one processor nothing waits: `mandel` takes 0.27 s against 0.25 s, a second of LSP 0.16 s
  against 0.15 s. With two, every cycle in which either works with the bus is stepped by turns: the
  Julia intro runs at about half real time while both draw, and slower while both poll main RAM
  for the next frame. Both are for the native code milestone.
- A processor running ahead may miss what the other does to it that the bus does not carry in
  order: what is left out is in [hardware/main-bus.md](../hardware/main-bus.md).
