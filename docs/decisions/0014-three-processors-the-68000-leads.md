# 0014: With the 68000, three processors take turns; the 68000 leads

**Status:** accepted

## Context

A 68000 program sets the GPU and the DSP going: it writes their programs into their RAM through
their slave ports, then G_PC or D_PC and GO, and works on beside them; the BIOS does nothing else.
From then on three masters meet at Tom's arbiter. The RISCs take their turns as they did
([0009](0009-two-processors-by-turns.md)): each hands the bus its cycles and waits where it needs
the bus's outputs and the other has not handed it the cycle. The 68000 cannot wait like that: Moira
makes a bus cycle inside an instruction it runs to its end
([0013](0013-the-68000-is-moira.md)), so once it has asked for a cycle the bus has to be stepped
through it before Moira goes on.

## Decision

- **The 68000 leads.** A part of the run is the 68000's, up to the cycle asked for; the RISCs it
  has started then run up to it by turns, as before.
- **A 68000 bus cycle the RISCs hold back runs them.** While the 68000 waits for DTACK the bus
  steps its pins itself; where it cannot step because a RISC has not handed it the cycle, the
  machine runs the RISCs, the one behind first, until the bus can go on. The 68000's interrupt is
  sampled from Tom's line only once the bus has stepped the cycle sampled, the RISCs run as far as
  that needs. A RISC that cannot get on for the 68000 leaves its turn.
- **The slave ports.** The 68000's writes into the GPU's space go through Tom's internal I/O cycles
  to `gpu_cpu.v`: a long word's high half latched, its low half served by the GPU's memory
  controller in the next cycle, G_CTRL's GO starting the GPU a cycle later; the blitter's
  registers are written there as the GPU's local bus writes them. A read gives the long word's high
  half and latches its low half for the read of it. The DSP's space is the same through Jerry's
  slave, as for the GPU's writes ([0009](0009-two-processors-by-turns.md)).
- **The GPU's interrupt to the 68000** (G_CTRL's CPUINT) is a pulse of the cycle after the write,
  taken into INT1 as the object processor's is.

## Consequences

- References: hand-written programs that start the GPU and the DSP, drive the blitter, share the
  bus with the GPU and take its interrupt (`tests/m68k`), and generated ones: the 68000 starting a
  generated GPU or DSP program (`random_m68k_gpu`, `random_m68k_dsp`) and running generated
  instructions beside it.
- A generated program tells the 68000 it is ending by a word in main RAM. What CTRL's and FLAGS'
  high half shows while the processor runs (its pipeline's state) is not modelled for another
  master's read ([hardware/m68000.md](../hardware/m68000.md)).
- After reset the board's `xbgl` says the bus is granted until BGACK has been asserted once, so Tom
  may serve a RISC while the 68000's AS is low; the 68000 then ends its cycle on that master's
  DTACK and reads what is on the bus, as the simulation does.
