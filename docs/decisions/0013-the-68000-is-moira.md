# 0013: The 68000 is Moira, its bus cycles made on the stepped bus; its references start at reset

**Status:** accepted

## Context

The 68000 runs the BIOS and the games' main loops; it sets the RISCs and the blitter going, and
everything else waits for it. Up to now the references started at the GO of a RISC
([0008](0008-main-bus-by-cycle.md)): the 68000 of the simulation's boot program, its last bus
cycles and STOP, was all of it the core had. To run what a game runs, the 68000 has to be a
processor of its own, whose bus cycles meet Tom's arbiter and memory controller at the cycles the
netlist's 68000 (the nuked 68000 of the MiSTer core, `rtl/nuked-68k/68k.v`) makes them, with the
wait states Tom's DTACK gives them.

PGMEmu's 68000 is Moira (https://github.com/dirkwhoffmann/Moira, MIT), an instruction-level
emulator that calls its client at every bus access at the 68000 clock it makes it, when built with
precise timing. The owner chose to take it as PGMEmu has it.

## Decision

- **Moira is carried unmodified** in `libextern/Moira`, at the commit PGMEmu carries, with PGMEmu's
  configuration (`src/jag_core/moira/MoiraConfig.h`), as PGMEmu's `libextern` rules carry it.
- **Its bus cycles are made on the stepped bus** (`M68000`, `MainBus`): Moira calls for a cycle at
  its S4; the bus is brought up to two clocks before it, the 68000's pins (AS falling at S2, a
  read's strobes with it and a write's two cycles later, the function code) are stepped through
  Tom's external master interface and memory controller, and the cycle ends three cycles after the
  first cycle at the clock's first half, from S4 on, in which Tom gives DTACK: each clock later than
  S4 is a wait state added to Moira's clock. A cycle starts later, a clock at a time, while the bus
  is not the 68000's (`w432`: BR or BGACK seen, BG given), and holds a grant back in its S0 and S1
  (`w395`); the 68000's bus arbitration is the netlist's.
- **Where the netlist's 68000 makes its bus cycles otherwise than Moira, the subclass corrects
  them**, in the times Moira gives them: the order of a long word's halves, of JSR's fetch and push,
  MOVEM's extra read, the dummy reads of DBcc and of the instructions writing CCR or SR, STOP's
  missing fetch, TAS's read-modify-write cycle, the reset's two clocks. Each is in
  [hardware/m68000.md](../hardware/m68000.md), with what the simulation shows. Moira is not
  patched: what it lacks is said there, and goes upstream if the owner wishes.
- **A 68000 program's reference starts at reset** (`--m68k`): the simulation's OS ROM sets MEMCON1,
  MEMCON2, G_END and D_END as the BIOS does and jumps to the program, which the testbench loads into
  main RAM during reset; the core runs the same ROM from reset, MEMCON1 and MEMCON2 being
  registers now. The trace takes the 68000's bus cycles, each as AS falls and as it rises
  ([spec/trace.md](../spec/trace.md)); a 68000 program's state is what it writes to memory.
- **Tom's interrupt** reaches the 68000's IPL (level 2) as the netlist's `intl`; the acknowledge is
  a bus cycle of function code 7, which Tom ends in a cycle and answers with vector 64. Where the
  68000 decides on it is not where Moira polls it: the subclass gives Moira the line where most
  instructions sample it, and the instructions that sample it elsewhere are left as they are
  ([hardware/m68000.md](../hardware/m68000.md)). The owner judged the exact point not worth the
  68000's microcode: the simulation's 68000 is not the chip's to the cycle either.

## Consequences

- References: hand-written programs for every group of instructions, exceptions and Tom's
  interrupts (`tests/m68k`), and generated programs of every instruction that does not trap
  (`random_m68k`), on the program's own data, the ROM, MEMCON1 and MEMCON2 and Jerry's ports.
  Generated programs interrupted at random (`m68k_irq`) agree in about half the cases and are a
  tool, not references.
- The bus is stepped through every cycle of a 68000 bus cycle; between them, and while the 68000
  works without the bus, it rests. The RISCs' programs pay for the 68000's place in the bus: the
  Julia intro takes 3.36 s a second of the console's time against 3.18 s before.
- An address error is not modelled: the simulation's 68000 starts a cycle without strobes, stacks
  its frame in another order and the complement of the address
  ([hardware/m68000.md](../hardware/m68000.md)).
- The 68000 does not yet share the bus with the RISCs: a program that starts the GPU or the DSP, or
  the BIOS, is still to be done.
