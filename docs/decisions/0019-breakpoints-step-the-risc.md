# 0019: A debugger's breakpoints run a RISC on its stepped core; with the 68000 they stop it where the turns allow

**Status:** accepted

## Context

The control API ([0018](0018-one-control-api.md)) reads a program between runs. To debug one, a
run must also stop where the program does something: before an instruction (a breakpoint), at a
read or write of an address (a watchpoint). PGMEmu's 68000 stops at either; so can Moira here.

The RISCs are harder. The timing automaton ([0007](0007-timing-automaton.md)) replays whole chains
of instructions at once, and nothing in a chain can stop between two of them. And the processors
run by turns ([0009](0009-two-processors-by-turns.md), [0014](0014-three-processors-the-68000-leads.md)):
with the 68000, it leads, and in the middle of one of its instructions the bus runs the RISCs as
far as it needs them. A RISC that stopped there would leave the bus waiting for cycles it does not
hand over. On the console a RISC stopped by its single-step bit does not stop the 68000 either.

## Decision

- **While a RISC has a breakpoint or a watchpoint, it runs on its stepped core alone**, as the
  automaton already hands the run to it where it cannot replay: the run is slower, and cycle for
  cycle the same (every reference is checked in both modes). With none left, the automaton takes
  over again.
- A RISC's breakpoint stops a run before the instruction issues; its watchpoint once the memory
  controller has served the load or store. The 68000's breakpoint stops before the instruction,
  its watchpoint after it. The run that goes on executes the instruction stopped at.
- **With the 68000, a stop is made where the turns allow.** A RISC run outside the 68000's
  instruction stops exactly, the 68000 at most a step of 32 cycles further on: while a RISC has
  probes the session runs the machine in steps that short. A RISC the 68000 waits on in the middle
  of an instruction cannot stop: it notes the stop with its registers as they were, runs on as far
  as the 68000's instruction needs it, and the machine stops at that instruction's end. The stop
  says it is not exact, and carries those registers.
- The probes, and the record of the last 1024 instructions each processor executed, are made only
  for a debugger (the control session); the runner, and every reference, runs without them.

## Consequences

- A breakpoint in GPU or DSP code costs the automaton's speed while it is set.
- A RISC's stop in code that shares the bus with the 68000 may come some cycles after the
  breakpoint, a whole loop of the RISC's at worst, and later breakpoints within it are passed.
- The record of instructions costs a store each, in the automaton's replay too, with a debugger
  only.
