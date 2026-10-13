# 0002: The FPGA core is the reference

**Status:** accepted

## Context

Three descriptions of the Jaguar's RISCs are at hand: the Software Reference Manual, TomNJerry
(the owner's earlier DSP simulator) and the Jaguar_MiSTer core. The core's Tom and Jerry are the
original netlists translated to Verilog, so their behaviour is the chips' to the gate, while the
manual leaves the pipeline's timing out and TomNJerry differs from the core in ways listed in its
`FPGA_DIFFERENCES.md`.

## Decision

The netlist is the reference. It runs in Verilator (`sim/`, `jagsim`): the whole `jaguar` module,
Tom, Jerry and the 68000, with an ideal DRAM and a 68000 boot program that starts the DSP the way
game code does. The simulation's own departures from the Quartus build are in `sim/README.md`.

## Consequences

- Behaviour is settled by running a program in the simulation, not by reading the manual.
- Where the core is known to depart from the chips (the MiSTer SDRAM and its latency, the boot
  program instead of the BIOS), JagMCP follows the simulation and records the gap in
  `docs/hardware/` when it matters.
- The simulation runs at about 150,000 DSP cycles a second, so references are kept short.
