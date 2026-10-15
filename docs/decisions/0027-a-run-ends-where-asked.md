# 0027: A run ends where it was asked to, whatever the program does

**Status:** accepted

## Context

The window and the control API run a program in parts and answer between them
([0011](0011-the-window-is-pgmemus.md), [0018](0018-one-control-api.md)). Their runs have no limit.
Two cases froze them: a part never ended, and the window and the server stopped answering.

**A 68000 bus cycle that does not get the bus.** Moira makes a bus cycle in one call, and the bus
is stepped inside it until the cycle ends ([0013](0013-the-68000-is-moira.md)). Moira cannot leave
an instruction in the middle and go on with it later. So the call returned only when the cycle
ended, or at the run's limit, which a part is not. Other masters come before the 68000. When one
keeps the bus, the call does not return:

- An object list that never ends keeps the bus for ever. On the console this is a locked machine,
  its picture still shown. It was met in a 68000 running wild.
- A GPU looping on a register of Jerry kept the bus for 12.3 million cycles (0.46 s) in the boot
  block of `polygon.j64`. The program is correct, and the 68000 is stopped by then anyway.

A limit on the wait would end the run of the second. Any limit is a guess.

**A RISC loop that nothing ends.** The timing automaton skips the iterations of a loop that waits
for an event, up to the next event: the I2S line, a timer, the object processor's interrupt, or the
end of the run ([0007](0007-timing-automaton.md)). With the 68000, it also stopped at the part's
end. Without the 68000 and with no limit, a GPU in `jr` to itself skipped to the end of time.

## Decision

- **The 68000 runs on a thread of its own, by turns with its caller** (`Fiber`). The caller waits
  while it runs. A bus cycle still waiting for the bus where the part ends hands the turn back, and
  the part ends. The next part goes on with the wait, from where it was. The run is the same as in
  one piece; only where the parts end moves.
- **The cycle left waiting is reported**: the instruction, the address, since when, and the master
  the bus is granted to (`Machine::busWait`, the field `bus_wait` of `emu.status`). The window shows
  it once the wait is a second long.
- **A skipped loop stops at the part's end**, with or without the 68000.

## Consequences

- The window and the server answer whatever the program does. A locked console runs on, as the
  real one does, and says what the 68000 waits for.
- Every reference is equal. The 104 cartridges and the 68 demos end in the same state, in one run
  and in parts of 20,000 cycles. 10 s of Native in parts take 22.9 s rather than 22.4 s.
- A hand-over between the threads takes about 2 µs when the other thread sleeps. A debugger's run
  with a RISC breakpoint hands over every 32 cycles. The threads look at the turn for some tens of
  microseconds before they sleep: that run of Native takes 1.07 times as long as before, not 1.7.
- Skipping a loop in parts gives the same run: the DSP intros end the same after 5 s in parts, as
  fast as before.
