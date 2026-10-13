# 0006: The front end is stepped every cycle

**Status:** accepted; refines [0003](0003-risc-by-instruction.md)

## Context

[0003](0003-risc-by-instruction.md) accounts for the whole pipeline with times. For the scoreboard
that holds: its waits are a few comparisons of the cycle with the times registers and flags are
ready and with the writes of the cycle. For jumps it does not. Over 200 generated programs with
forward JR and JUMP, the cycles between a jump's delay slot and the next instruction issued were 1,
2, 3 or 4 for the same kind of jump, depending on how full the instruction queue was, whether a
fetch was in flight, and whether the delay slot instruction had been fetched when the jump issued.
A rule per case would be a guess at the queue's state; the queue is the state.

The queue's control is small: three bits of size, the fetch address, the jump latches of
`prefetch.v`, the issue and MOVEI states of `execon.v`, and the fetch request of `gpu_mem.v`.

## Decision

The front end is the netlist's, stepped once a DSP cycle: the queue (with the words as they were
read when fetched), `pc.v`, `execon.v` and the program side of `gpu_mem.v`. The instruction in the
decode stage issues in the cycle in which `exe` is set, the scoreboard's waits for that cycle still
being computed from times. The instruction then executes as before, at once, with its writes put in
the calendar.

## Consequences

- Every generated program with jumps matches the simulation; so do the cases no rule would have
  covered: MOVEI in a JR's delay slot taking its data from the jump's target, a JR in a JR's delay
  slot, the address `program_count` shows in a JUMP's delay slot. Self-modifying code sees what the
  queue holds, as the hardware does.
- The core does work every cycle. On `mandel` it runs at about 3 times real time, against 17 for
  the prototype that had no front end; a fast path for the steady queue is planned.
