# 0007: Timing is replayed from an automaton the stepped core builds

**Status:** accepted. The prototype missed its speed criterion (below); the owner kept the record
as the ground for native code generation, planned as a milestone of its own

## Context

Stepping the front end, the memory controller, the flags, the divider and the interrupt controller
every cycle ([0006](0006-front-end-stepped-by-cycle.md) and the two milestones after it) matches the
FPGA core on every reference, and costs about 26 ns a DSP cycle: 40 million cycles a second,
1.5 times the DSP's 26.6 MHz, against 2.9 for TomNJerry and 17 for the prototype of
[0003](0003-risc-by-instruction.md). The cost has no hot spot. The compiled `cycle()` is 881
instructions with about 100 conditional branches, `execute()` 796, and the processor retires about
five an ns: the price is the number of instructions a cycle, spread over the whole step. Decoding
once, computing only the units whose output is taken, stepping the I2S interface and the divider
only while they have work, and counting out quiet stalls in one step gained a tenth. The system
will hold a second RISC, the 68000, the object processor and the blitter; a RISC at 1.5 times real
time leaves no room for them.

What the stepped core computes, however, repeats. Over the whole of `mandel` (16.45 million
instructions issued) the pipeline's control state at the cycle an instruction issues, taken in
full (the queue with its words, `execon`, the jump latches, the cycles every register and the
flags become ready, the write calendar, the memory controller, the divider, the accumulator, the
interrupt controller, the bank) takes 362 distinct values; over 60 million issues of a loop with an
interrupt every 830 cycles, 156. In steady code an instruction is issued in one and the same
pipeline state every time, interrupts included.

## Decision

The stepped core stays the only source of timing, and records what it computes into an automaton
the core then replays:

- A **node** is a pipeline control state at an issue cycle, times held relative to that cycle.
  It is created the first time the stepped core issues an instruction in that state.
- A node's **variant** is what followed in the stepped core for one **outcome** of the data the
  timing depends on: a jump's condition and target, a load or store's address class (local RAM,
  I/O register and the bits of it that steer the pipeline, external), the cycle the I2S interrupt
  line next changes. The variant holds the cycles to the next issue, the data actions of those
  cycles (the access, a store's data read, a MOVEI's words, a load's or the divider's write-back,
  an accumulation) at their offsets, and the node that follows.
- **Replay** of a variant runs the datapath of the instruction and the actions at their recorded
  offsets; the register file, its calendar, the flags, the divider's and the I2S interface's data
  keep being stepped as before, the control does not. An outcome without a variant, an I/O read, an
  I2S control write or a stall longer than 32 cycles puts the live state back from the node and
  steps the core, recording.
- A write into local RAM that holds words any node fetched discards the automaton.
- The replay must write the same trace as stepping, event for event; every reference checks both.

## What the prototype measured

Every reference equal under replay and under stepping. On `mandel`, 0.10 to 0.13 s for its 21
million cycles against 0.50 s stepping: 160 to 210 million cycles a second, 6 to 8 times real
time, 2.7 times TomNJerry. A loop of 200 instructions with an I2S interrupt every 830 cycles: 4.5
times real time. The datapath, the register file writes and the flags no longer decide the speed;
the host's branch predictor does. The same work costs 2 ns an instruction when a loop of thirty
nodes repeats and 14 ns when a loop of two thousand does, whatever the data: the one indirect jump
an instruction and the branches the data takes are mispredicted once the sequence is longer than
the predictor's history. Calling the datapath directly from the chain, arenas with stable
addresses, hot fields first, the outcome computed by the node's kind and a branch-free register
write gained a tenth on the interrupt loop and nothing on `mandel`. The criterion of ten times is
not met; the way past it is compiling chains of nodes to native code, which has no dispatch to
mispredict, or the stepped core on its own thread beside the other chips.

## Consequences

- The cost moves from the cycle to the instruction: a datapath run, a few actions, a handful of
  stores a cycle for the data that keeps stepping.
- The criterion set for the prototype was all references equal under replay, and `mandel` at ten
  times real time or more. The first was met, the second was not; the owner decided to build the
  rest of the system on the automaton and to take the remaining speed from native code generated
  from its chains, once the bus model fixes what the
  outcome of an external access is.
- The automaton is bounded by the program's distinct pipeline states, a few hundred for a program
  of a few hundred instructions; it is not a cache that thrashes.
