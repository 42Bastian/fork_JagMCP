# 0010: The video output and the object processor are stepped by cycle; frames are references

**Status:** accepted

## Context

What the RISCs see of Tom's video is its time base, and the object processor taking the bus at the
start of each displayed line: it reads its list and the pixel data a phrase at a time, writes each
object's header back, and starts a refresh at its stop object. How long a RISC's access to main RAM
takes depends on all of that, so the object processor's bus cycles must be exact; what it draws must
be checked too.

PGMEmu draws a line whole when its RTL starts fetching it (its decision 0011): there the drawing
takes no bus from the CPU but for the sprite list's copy. On the Jaguar the drawing is the bus
traffic.

## Decision

- **The object processor is a master of Tom's bus**, ported signal for signal (`ob.v`, `wbk.v`,
  `obdata.v`, the writing side of `lbuf.v`) and stepped with the bus in every cycle while the
  time base runs or it has work under way. Its pixels go into the line buffers as the netlist
  writes them, the CLUT read as `obdata.v` reads it.
- **The video output is stepped by cycle** (`vid.v`'s blanking and display, `pix.v`, `cryrgb.v`
  with the colour tables extracted from the ROM models), brought forward to the cycle before
  every write of Tom's registers and every bus cycle of the object processor, so that a change in
  the middle of a line lands on the pixel it lands on in the netlist. While the time base is
  stopped it is moved by arithmetic.
- **Frames are part of a reference.** The simulation collects the pixels of the MiSTer core's
  video output at its pixel enable, outside its blanking, a line ending as hblank rises and a frame
  as vblank rises, and prints each frame's hash in the state; the core collects the same and
  compares it. `--frames DIR` writes them as images.
- **The simulation's DRAM is addressed by row and column**, as the MiSTer core's SDRAM controller
  addresses it ([0009](0009-two-processors-by-turns.md)).

## Consequences

- A reference with the time base running checks the frames pixel for pixel, the border, the
  blanking and the pixel width with them.
- The bus is stepped in every cycle while the time base runs: it is never at rest then. Programs
  without video are as fast as before (`mandel` 0.28 s, a second of LSP 0.17 s).
- Pixels the video output takes from Tom's data bus rather than from a line buffer are not
  modelled ([hardware/tom.md](../hardware/tom.md)); the generated object lists are not references
  until they are.
