# 0012: The blitter is stepped by cycle; Tom's data bus is its byte lanes

**Status:** accepted

## Context

The blitter is Tom's other bus master, set going by the GPU in nearly every program: the GPU
writes its registers on its local bus and waits on B_CMD's idle bit, so how long a blit takes is
the GPU's timing, and what it writes is main RAM. Its inner loop reads source and destination data,
pixel by pixel or a phrase at a time, and computes what it writes with a pipeline of registers that
latch in the middle of a cycle as well as at its end.

What the blitter reads is not only memory. Tom's internal data bus (`dbus.v`) keeps, lane by lane,
the last byte each read or write put on it; a read narrower than a phrase replaces only its own
lanes. In font mode the blitter reads a byte of source and uses the whole phrase, the other seven
lanes being whatever the bus held. The same bus is what the line buffers' write path and the
CLUT's bus carry while nothing else drives them, which is where the video output's "pixels taken
from Tom's data bus" ([0010](0010-video-by-cycle-frames-as-references.md)) came from.

## Decision

- **The blitter is ported signal for signal** (`blit.v` and the modules under it), a cycle in two
  halves: the latches the GPU writes, and those the loop loads from the bus, take their values at
  tlw; the flip-flops at the edge. The width and justification it gives the bus are seen after
  tlw. It is stepped with the bus while it works, and until a cycle changes nothing.
- **Its registers are on the GPU's local bus.** The GPU's write is queued for the cycle it is
  served in and taken as the bus steps that cycle; a read sees the blitter as the bus has left it.
  While the blitter works, has a write to take or holds the bus, the GPU steps the bus with itself,
  so that what it reads is the blitter of that cycle; its interrupt (`gpu_irq[4]`) is taken as the
  object processor's is.
- **Tom's data bus is eight byte lanes.** A DRAM read latches the lanes of its byte mask as the
  cycle ends (`q3b`); a write's data takes all of them, moved up as `up.v` moves it, in the cycle
  after Tom takes the write (the GPU's `wden`, the blitter's `data_ena`, the object processor's
  header); a read of Tom's registers takes the lanes of its word. What reads it reads the lanes:
  the blitter, moved down for a justified read (`down.v`); the object processor's phrases; the DRAM
  as CAS falls, for the blitter's and the object processor's writes. The line buffer the video
  output shows carries, while it does not read it, what the object processor writes or else the
  bus's low long word; the CLUT's buses carry the bus's low word while the pixel path does not
  read them.
- **The board's address lines** carry Tom's address while Tom holds the bus and are pulled up
  otherwise, which decides how the bus's words reach the line buffers. DRAM bank 1 is empty.

## Consequences

- Generated blits (`gpu_blit`: every mode of the command but a DSP or 68000 writing the registers,
  the collision stop resumed or aborted, the interrupt) and generated object lists (`gpu_objects`)
  are references: memory hashes, frames and the GPU's waiting on B_CMD.
- The GPU is stepped while the blitter works, so that a program spending its time polling B_CMD
  runs at the stepped core's speed.
- The lanes are what the data bus holds between cycles; a value driven and replaced within a
  cycle is not seen. Reads of ROM and of Jerry's ports, and Jerry's own writes, do not reach the
  lanes ([hardware/tom.md](../hardware/tom.md)).
