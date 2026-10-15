# 0030: A single step runs the whole machine; no processor is held

**Status:** accepted

## Context

A debugger wants to execute one instruction of the processor it looks at (`debug.singlestep`
with `cpu`), and to run to the next frame, as Virtual Jaguar's debugger does. On the console the
68000, the GPU and the DSP share one bus, cycle by cycle. A step of one of them could hold the
other two, or let them run.

## Decision

- **A step runs the whole machine until the stepped processor is about to execute its next
  instruction.** The others run the same cycles, as on the console. None is held.
  - The 68000 leads the turns ([0014](0014-three-processors-the-68000-leads.md)). A RISC cannot
    run past it, so the 68000 cannot be held while a RISC runs.
  - A held processor keeps its bus request and its part of the bus's state. The others would then
    run on a machine that does not exist, and the debugger would show what the program never does.
  - The run stays the run the program makes without a debugger. This was checked on a cartridge:
    300 frames, with 1,500 steps of the three processors in the middle, give the same frames, the
    same cycle at each frame's end, and the same RAM as without the steps.
- **A step is a breakpoint at the next instruction** ([0019](0019-breakpoints-step-the-risc.md)):
  the RISC runs on its stepped core, the 68000 in steps of 32 cycles, and a RISC the 68000 waits on
  stops where the 68000's instruction ends, not exact. The step executes the instruction at the
  `pc` the debugger shows. A RISC not stopped by a probe may not have issued it yet, so the first
  instruction it issues is passed.
- **A step to the next frame stops where Tom completes a frame**, the frame `emu.run_frames`
  counts. Each processor then stops before its next instruction. With the 68000 the machine runs
  in steps of 32 cycles here too: a 68000 in a STOP that nothing can end skips to the end of the
  run, and would be far past the frame.

## Consequences

- A step of a RISC is as slow as a run with a breakpoint, for the few cycles it takes.
- 300 steps of a frame take 3 % longer than `emu.run_frames` of 300 frames (12.8 s and 12.4 s,
  on a cartridge).
- A processor that cannot execute (GO clear, a halted 68000, a STOP that nothing can end) is not
  stepped: the answer is `halted` at once, not a run to the timeout.
- Holding the other processors, as a hardware single step of a RISC does through its control
  register, is not offered.
