# 0026: A read no Tom register answers gives MEMCON1, and is reported

**Status:** accepted

## Context

Tom's registers are read through an internal tri-state bus. `iodec.v` selects the register that
drives it. Only some addresses are decoded for reading: MEMCON1 to LPV, OB0 to OB3, TEST2, TEST3,
PIT0, PIT1, INT1, LBRA, the CLUT and the line buffers. INT1 is read only at $F000E0
(`intr`: `a[3:1]` is 0). $F000E2 (INT2) and the other write-only registers are not decoded.

The decoder also keeps the bus from floating. `memc1r = memc1 | (float4 & floaten)`: MEMCON1 drives
the bus whenever no register is read and no internal read is under way. During an internal read of
an address that nothing decodes, nothing drives the bus. On the chip, the bus then keeps its
charge, so the read gives MEMCON1.

The MiSTer core turns the tri-state buses into ORs with a default of 0 (`abus.v`, `misc.v`). The
simulation reads 0 there. JagMCP did the same.

A program depends on it. The boot block of the 3D poly engine (`polygon.j64`, 2026) copies a 68000
handler to $110. The handler reads INT2 at $F000E2 to acknowledge the vertical interrupt, then stops
the 68000. It runs on a real Jaguar: MEMCON1's low byte is $65 after the BIOS, bit 0 set, so the
write back to INT1 clears VI. With 0, VI is never acknowledged. The 68000 takes it again and again,
without RTE, until its stack overflows. The pending request keeps the GPU off the bus (`ihandler`),
and the GPU never loads the program. The read is a bug in the handler: it should read INT1 at
$F000E0. It works by chance.

## Decision

- **A read of Tom's space ($F00000 to $F01FFF) that no register decodes gives MEMCON1**, for every
  master that reads Tom's registers: the 68000, the GPU and the DSP. `Tom::readDecodes` lists the
  decoded reads.
- **This departs from the reference** ([0002](0002-the-fpga-core-is-the-reference.md)). It follows
  the chip, as the netlist describes it, where the simulation cannot.
- **Each such read is recorded**: the processor, the instruction, the address, the value, the
  first cycle, the count (`Machine::undrivenReads`). A program that reads there has a bug that
  works only by what the bus held. The records are shown by `debug.diagnostics` and the
  `diagnostics` field of `emu.status` ([control-protocol.md](../spec/control-protocol.md)), by
  `jagmcp-cli` on stderr after a run, and in the window's Status.

## Consequences

- `polygon.j64` boots and runs as on the console.
- No reference reads such an address. Every reference stays equal. The control transcript
  (`tests/control/lines.jsonl`) has the 68000, the GPU and the DSP read INT2.
- The model is simple: the read always gives MEMCON1. After a read of a register in the cycle
  before, the bus could hold that register's value instead. This is not modelled.
- Jerry's reads of addresses that nothing drives are not reported yet.
