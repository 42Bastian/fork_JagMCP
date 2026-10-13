# 0003: The RISC is executed by instruction, its pipeline accounted for

**Status:** accepted

## Context

A Jaguar has two RISCs at 26.6 MHz, and with the 68000, the blitter and the object processor
they must run comfortably faster than real time. Three ways to emulate them were measured on an
Apple M4 with the same program (`tests/asm/mandel.js`):

| | DSP cycles a second | real time |
|---|---|---|
| The netlist in Verilator (`jagsim`, the whole machine) | 0.15 million | 0.006x |
| TomNJerry: every pipeline stage stepped every half cycle | about 75 million | 2.8x |
| A prototype that executes an instruction at a time and keeps the time each register is ready | 454 million | 17x |

Translating the netlist is exact by construction but two orders of magnitude too slow; Verilator
is already the best general translator, and hand-optimising gates gives up the exactness. Stepping
the stages costs the same small work in every stage every cycle (a profile of TomNJerry shows it
spread evenly over ten stage functions) and has little to gain from tuning. Accounting for the
pipeline with times costs almost nothing over plain interpretation in the prototype.

## Decision

The RISC executes one instruction per step. The pipeline is reproduced by the times at which the
scoreboard would let an instruction issue and at which each write reaches the register file
([architecture.md](../architecture.md)). The rules are taken from the netlist (`sboard.v`,
`execon.v`, `prefetch.v`, the microcode) and confirmed by recorded traces.

## Consequences

- A rule that is not exactly the netlist's shows up as a trace that differs from the simulation's
  in some cycle; the generated programs make that likely to be found.
- Waiting loops can later be skipped whole when their state repeats exactly (in the LSP player 74
  percent of the DSP's cycles are a loop polling two words of local RAM).
- Interactions inside the pipeline that the times do not capture need a rule each; the ones found
  are commented where they are applied.
