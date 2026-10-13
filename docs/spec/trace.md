# Traces

The simulation (`sim/sim_main.cpp`, `--log`) and the core (`jagmcp-cli --log`) write a run as one
event per line, `<cycle> <event>`, the cycle being the cycle of the processor run (the DSP, or the
GPU with `--gpu`) counted from its first clock with its GO bit set (one cycle is four `sys_clk`
cycles of the simulation). The state's `dsp_cycles` counts the same cycles, either processor's.

The state ends with the frames of the video output completed in the run, each as
`frame[n]: <FNV-1a> <width>x<height>`: the pixels (red, green, blue bytes) of the MiSTer core's
video output at its pixel enable, outside hblank and vblank, a line ending as hblank rises and a
frame as vblank rises, or as VC wraps at VP (`vid.v` `nextfield`) after a field in which vblank did
not rise (VBB past VP), the first counted from the first vblank after GO. A line's hash takes its
pixel count first, as two bytes.

The other processor's events are written too, prefixed with its name (`6135 dsp:X f1b000`), in
the same cycles: the GPU and the DSP run on one clock. A GPU program that starts the DSP is so
checked for both; the state then holds the DSP's registers as well (`dsp:b0.r0`).

| Event | Meaning |
|---|---|
| `X f1b000` | An instruction is issued (`execon` `exe`); the address is the prefetcher's `program_count`. |
| `W08":80000000 B` | A register file write: register, bank (`'` bank 0, `"` bank 1), value, port (`A` or `B`, the simulation only). |
| `F NC-` | The flags changed to N, C, Z (`-` for a clear flag). |
| `L(001004) w4` | An external read request of the DSP, as the gateway takes it: address, width in bytes. Ignored by the comparison. |
| `S(001004)=12345678 w4` | An external write request: address, data, width. Ignored by the comparison. |
| `IRQ i2s` | The I2S interrupt request rises (the simulation only). |
| `I 56 r5,r2 s=… d=…` | The core's view of an issued instruction: opcode, the registers it names, their values. Ignored by the comparison. |
| `A(e00008) r fc6` | The 68000 starts a bus cycle (AS falls): address, read or write, function code. |
| `D=4e71` | The 68000's bus cycle ends (AS rises): the data read or written, `--` for a byte its strobes leave out; an interrupt's acknowledge shows its vector, the low byte (`D=--40`). A read-modify-write cycle (TAS) is one, traced as a read whose data is the write's. |
| `I 004000 move.l D0, D1` | The core's view of a 68000 instruction about to execute: its address and Moira's disassembly. Ignored by the comparison. |

A 68000 program's run (`--m68k`) counts the same cycles from the end of reset, the state having no
registers of its own: its `finished` is 0 and `dsp_cycles` the limit, a RISC's registers being in it,
prefixed, only if the program started it.

`--log-from N` (either program) leaves out the events before cycle N, so that a part of a long run
(the BIOS's) can be recorded and checked.

`jagmcp-cli --check` compares the `X`, `W` (without the port) and `F` events of both processors and
the 68000's `A` and `D` events, sorted by cycle and by text within a cycle, and the final state: `finished`, `dsp_cycles`, the
registers, the memory hashes and the frames. The data of the 68000's read of the high half of a
RISC's CTRL or FLAGS ($F02100, $F02114, $F1A100, $F1A114) is left out: it is the RISC's pipeline,
which the core does not have in every cycle ([hardware/m68000.md](../hardware/m68000.md)). The state is taken when the GO bit of the processor run has
fallen (`dsp_cycles`): it holds every write the trace shows, and none planned for that cycle or
after it.

The `X` address is the prefetcher's view: in the delay slot of a JUMP taken right after it, it is
the address after the delay slot instruction (`pipe_jump_taken`).
