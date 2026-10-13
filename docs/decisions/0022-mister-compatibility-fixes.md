# 0022: Games run with the MiSTer core's compatibility fixes

**Status:** accepted

## Context

The reference is the Jaguar_MiSTer core ([0002](0002-the-fpga-core-is-the-reference.md)), and the
simulation and the core model its netlist as translated. The MiSTer core as it ships does not run
only that: its `max_compat` option, on by default (`Jaguar.sv`, `max_compat = !status[30]`),
turns on fixes that are not the netlist's, among them:

- **`vintbugfix`** (`misc.v`): a write of INT1 that enables the vertical interrupt where it was
  disabled clears the interrupt's latch (`vclr`). On the netlist the latch, set while the
  interrupt was disabled, stays set and the interrupt never rises again; the MiSTer source names
  Iron Soldier 2 and Barkley Shut Up and Jam as needing the fix. Native and Barkley wait for that
  interrupt with STOP and hang without it.
- **`patch_checksums`** (`Jaguar.sv`): the BIOS's BEQ that fails a cartridge's checksum (`$19C6` in
  the model M's BIOS, `$136E` in the others', found by the BEQ's opcode) is read as a BRA, so that
  a cartridge whose checksum does not match boots.
- **`olpbugfix`** (`ob.v`): OLP's two halves, written by the 68000 apart, taken as one should the
  object processor start between them.

The games are checked outside the repository ([0015](0015-bios-and-games-checked-outside-the-repository.md));
they are meant to run as they do on the MiSTer.

## Decision

- **A `compat` option** in the core (`RunOptions::compat`, `--compat`, `--no-compat`) and in the
  simulation (`jagsim --compat`, `--no-compat`, `vintbugfix` driven from it) gives the MiSTer
  core's `max_compat`: `vintbugfix` and `patch_checksums`. `olpbugfix` is not modelled; it is taken
  up if a game is found to need it.
- **It is on where a BIOS is given**, as games are run, and off otherwise, as the references run:
  the references stay the netlist's, and a reference that needs the option names it
  (`m68k_vint_latch_compat`, beside `m68k_vint_latch` without it).

## Consequences

- Native and both Barkley Shut Up and Jam run to their titles and play; a cartridge whose checksum
  the BIOS rejects (the red screen) boots past the check.
- A run with a BIOS is the MiSTer core's as it ships, not the bare netlist's; `--no-compat` gives
  the netlist's.
