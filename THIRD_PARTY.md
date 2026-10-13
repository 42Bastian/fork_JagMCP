# Credits and third-party licences

JagMCP is released under the GNU General Public License, version 2 only ([LICENSE](LICENSE),
[0005](docs/decisions/0005-licence-gpl-2.md)). This file names the work it is derived from.

## The hardware: the Jaguar_MiSTer core

The emulated hardware is ported from the Atari Jaguar core for MiSTer,
https://github.com/MiSTer-devel/Jaguar_MiSTer, released under the GNU General Public License,
version 2. Its Tom and Jerry are **Gregory Estrade (Torlus)**'s translation of the original Atari
netlists to Verilog (JagNetlists, 2013); the port to MiSTer is by **ElectronAsh** and **Kitrinx**,
with updates by **GreyRogue**. Each module's header names the netlist files it ports and the commit
it was read at.

`src/jag_core/src/WaveRom.inc`, the contents of Jerry's wave table ROM, is extracted from the
core's `rtl/Jerry/raa016a.v` by `scripts/extract-wave-rom.py`; `src/jag_core/src/CryRom.inc`, Tom's
CRY colour tables, from `rtl/Tom/ra8008a.v`, `ra8008b.v` and `ra8008c.v` by
`scripts/extract-cry-rom.py`.

## The 68000: Moira

`libextern/Moira` is Moira (https://github.com/dirkwhoffmann/Moira), a Motorola 68000 emulator by
**Dirk W. Hoffmann**, released under the MIT licence (`libextern/Moira/LICENSE`), carried unmodified
as PGMEmu carries it ([libextern/README.md](libextern/README.md)).

## The window

`src/jag_app` is modelled on the front end of PGMEmu (https://github.com/laoo/PGMEmu), an emulator
of the IGS PolyGame Master by **Waldemar Pawlaszek**, released under the GNU General Public
License, version 2; its audio output and screen texture are taken from it. Fetched when the build is configured
([cmake/Dependencies.cmake](cmake/Dependencies.cmake)), and linked into the window:

| Library | Copyright | Licence |
|---|---|---|
| [SDL 3](https://github.com/libsdl-org/SDL) | Sam Lantinga | zlib |
| [Dear ImGui](https://github.com/ocornut/imgui) | Omar Cornut | MIT |

## The control API

`src/jag_server`, the control protocol's transports, is ported from PGMEmu's, and the protocol
follows its own ([0018](docs/decisions/0018-one-control-api.md)). Fetched when the build is
configured, and linked into `jagmcp-cli`:

| Library | Copyright | Licence |
|---|---|---|
| [JSON for Modern C++](https://github.com/nlohmann/json) | Niels Lohmann | MIT |
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | Yuji Hirose | MIT |

## Test programs

The programs in `tests/asm/` come from TomNJerry (https://github.com/laoo/TomNJerry), by
**Waldemar Pawlaszek** and **Bastian Schick (42Bastian)**, public domain. `jaghard.inc` holds
Atari's Jaguar hardware equates in the form lyxass (https://github.com/42Bastian/lyxass) reads.
They are assembled for the tests only.

`tests/programs/tj_julia/` and `tests/programs/tj_starpath/` are TomJerryJulia and tj_starpath,
256 byte intros of JaguarDemos by **Bastian Schick (42Bastian)**, released under the Unlicense,
assembled from its sources with tests/asm's include files.
