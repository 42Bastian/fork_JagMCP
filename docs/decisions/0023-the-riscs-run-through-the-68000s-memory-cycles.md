# 0023: Through a 68000 bus cycle to memory the RISCs run on to where its next could start

**Status:** accepted

## Context

With the 68000 leading ([0014](0014-three-processors-the-68000-leads.md)), a 68000 bus cycle the
RISCs hold back runs them until the bus can go on: a cycle further each time. Over 15 s of Native
(200M bus cycles) that was 92 million turns from the 68000's bus cycles, 109 million in all, each
RISC's run three to five cycles long; the turns, not the work in them, were a large part of the
cost.

A 68000 bus cycle to memory (the DRAM, the ROM, the cartridge) reaches nothing of the RISCs': not
their local spaces through the slave ports, nor Tom's or Jerry's registers, through which a write
could start, stop or interrupt them, move their timers or the object processor. What it can reach
comes with its next bus cycle, whose AS falls six cycles after this one's DTACK at the earliest, as
the bus already takes it (the 68000's horizon two after DTACK, its next S0 decided then).

## Decision

- **While a 68000 bus cycle to memory waits for DTACK**, not yet taken, the RISCs it holds back run
  on to six cycles past where the bus stands (`M68000::NEXT_START`), not one; the bus decodes the
  cycle as Tom does (`MainBus::m68kAtMemory`: neither Tom's nor Jerry's devices, nor an interrupt's
  acknowledge).
- **Before a bus cycle starts**, the RISCs run on to its start in one turn: the 68000 puts nothing on
  the bus before it.
- A 68000 bus cycle to Tom's or Jerry's devices runs them as before, a cycle at a time.

## Consequences

- Native's turns from the 68000's bus cycles fall from 92 to 41 million, all of them from 109 to 57
  million; 15 s of Native take 32.5 s rather than 33.9 s, Cybermorph's 36.0 s rather than 37.5 s
  (profile-guided: 28.0 s rather than 29.7 s, 31.1 s rather than 33.1 s).
- The run is the same cycle for cycle: every reference equal in both modes, each of the 104
  cartridges' state after 30M bus cycles (registers, main RAM, frames) and 15 s of Native and of
  Cybermorph the same as before, yarc reloaded as the simulation leaves it.
