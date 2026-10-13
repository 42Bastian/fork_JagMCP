# 0001: Documentation regime

**Status:** accepted

## Context

JagMCP is written largely by AI agent sessions, as PGMEmu is. Left alone, such sessions append to
whatever document looks plausible, restate signatures that rot, and leave status files that
duplicate git. PGMEmu adopted NGA's regime against that, and it has held.

## Decision

PGMEmu's regime is adopted whole, as written in [docs/README.md](../README.md): every document
answers one question and has a prohibition, documentation carries why and contracts, one fact one
place, no status files, documentation changes with the code. `docs/hardware/` records where the
emulator knowingly departs from the netlist, since that cannot be read off either.

## Consequences

- A plan in `docs/plans/` is a queue that only shrinks.
- Facts about the hardware are not copied from the netlist or SoftRef; documents and code comments
  name the netlist file and signal.
