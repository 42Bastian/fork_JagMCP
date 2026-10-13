# 0008: The main bus is stepped by DSP cycle, from a fixed state at DSPGO

**Status:** accepted

## Context

A load or store of the DSP outside its local space goes through the DSP's gateway, Jerry's bus
master, Tom's arbiter and memory controller, and back. In the FPGA simulation a long word read from
main RAM takes 24 cycles from the request to the register write, a store holds the next load or
store about 21 cycles, a read of Jerry's own JOYBUTS 36. How long depends on more than the access:

- **The 68000.** It owns the bus whenever nobody else asks for it; Tom has to ask it (BR, BG, the
  board's flip-flop on BG) for every grant, at the 68000's half rate clock. Right after DSPGO it is
  still on the bus: it ends its write of D_CTRL and prefetches one more word from the slow ROM
  before STOP.
- **The refresh.** Tom counts refresh requests whatever the bus does and, once eight are due, takes
  the bus for eight refresh cycles: in the simulation every 3072 cycles. A refresh closes the open
  DRAM rows.
- **The DRAM row** left open by the last access: a long word in the same 2 KB row takes three cycles
  less.
- **The netlist's own races.** The DSP's grant is withdrawn for a cycle when a request follows the
  end of the one before (`dbrls` latched on Jerry's acknowledge); a store started in that cycle
  takes its data from the bus instead of the gateway. A prescaler written while a timer is stopped
  makes a request in the cycle of the write.

The simulation's boot program copied the DSP program with the 68000, so the time from reset to
DSPGO, and with it the refresh counters at DSPGO, changed with the program's length.

## Decision

- The parts on the DSP's way to memory are ported as the netlist's flip-flops and gates, stepped
  once a DSP cycle (`MainBus`): the gateway (`gateway.v`), Jerry's bus master (`j_jmem.v`,
  `j_jbus.v`), Tom's arbiter, external master interface, memory controller with its wait states,
  memory width sequencer, address decoder with the open rows, and refresh counter (`arb.v`,
  `cpu.v`, `mem.v`, `memwidth.v`, `abus.v`, `misc.v`), and the 68000's bus arbitration
  (`68k.v`'s `w268` to `w270`, the board's `xbgl`). The external load write-back path of
  `sboard.v` is added to the DSP (`oneld`, `twold`, `xlddst`, `mbusywait`, `xlddwait`). Jerry's
  timers (`j_jmisc.v`) are written through the bus and request the DSP's interrupts.
- **Data is moved, not stepped**, at the cycle the netlist moves it: a load's when Jerry starts its
  bus cycle, a store's into Jerry's data latch the cycle after, into DRAM as Tom's cycle strobes
  CAS, a timer's as Tom's I/O cycle latches it.
- **The 68000 is the boot program's**, not a processor: the end of its write of DSPGO, its last
  prefetch, then STOP, answering bus requests. It is replaced when the 68000 is emulated.
- **The simulation's boot is fixed**: the testbench writes the DSP program into the DSP RAM during a
  reset of fixed length, and the 68000 only configures and starts the DSP. The state of the
  console at DSPGO is the same in every reference, and the core starts from it.
- While nothing of the DSP's is under way the bus is not stepped; it is brought forward when the
  DSP next hands it a request, its refresh counters by arithmetic while it is at rest.

## Consequences

- Every reference was re-recorded; only `mandel`, whose run crosses refresh, changed.
- The timing automaton ([0007](0007-timing-automaton.md)) records no bus state: a variant with an
  external access is only ever stepped, and while the bus works for the DSP only nodes whose
  successor is no load or store are replayed. While a timer runs or an external load is to be
  written back the core is stepped. `mandel`, a byte store a pixel, takes 0.23 s against 0.13 s
  without the bus; LSP, with its timers running, runs at real time. Both are for the native code
  milestone.
- What the port leaves out is in [hardware/main-bus.md](../hardware/main-bus.md).
