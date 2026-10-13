# 0018: One control API, served as PGMEmu serves its own

**Status:** accepted

## Context

The emulator is debugged by agents as much as by a person: a program loaded, run for some frames,
its memory, registers and code read, a screenshot looked at, its sound heard. Until now the only
way in was the runner's arguments: a run from reset to a limit, its state printed at the end. To
look at a running program meant a new run each time, and nothing an agent can call.

PGMEmu, the owner's emulator of another board, answers the same need: one dispatcher of methods
(JSON in, JSON out), reached by several thin transports, JSON-lines on stdio and TCP and the Model
Context Protocol on stdio and HTTP, its tool list made from the method table (PGMEmu's 0005 and
0012). Its agents already know that shape.

## Decision

- **Every capability is a method of one dispatcher** (`jag::control::Dispatcher`, in
  `src/jag_control`), answering a request object with a response object, over a session that owns
  the machine (`jag::control::Session`). Method names, the request and response format and the
  error codes follow PGMEmu's where the two emulators have the same thing to say, so that the same
  scripts and the same habits serve both. The protocol is [spec/control-protocol.md](../spec/control-protocol.md).
- **The transports are PGMEmu's, ported** (`src/jag_server`): JSON-lines on stdio and on TCP, MCP on
  stdio and over HTTP; a tool per method, named with underscores for the dots, a screenshot
  answered as an image. They reach the dispatcher through a request handler, so that the window can
  hand requests to its emulation thread.
- **The libraries are PGMEmu's**: [nlohmann/json](https://github.com/nlohmann/json) for the
  requests and answers, [cpp-httplib](https://github.com/yhirose/cpp-httplib) for MCP's HTTP,
  header-only, fetched by `cmake/Dependencies.cmake` at pinned versions. The TCP lines run on
  sockets of our own. The core needs neither: only `jag_control` and `jag_server` link them.
- **Both network transports listen on 127.0.0.1 only**, and the HTTP one turns away a browser's
  request whose `Origin` is not a local page: whoever connects drives the emulator, and a page
  elsewhere could otherwise reach it through a name that resolves to the loopback address.
- **The core gains what a debugger reads between runs and nothing it runs by**: memory read and
  written without a bus cycle, the processors' registers, a RISC disassembler, the 68000's from
  Moira. A write into a RISC's local RAM drops what the timing automaton knew of the code there.
- **The protocol is tested by transcripts** (`tests/control/*.jsonl`, the answers in `.expect`
  files the emulator writes), run on programs of the repository, and the network transports over
  real sockets.

## Consequences

- An agent drives a running program as it drives PGMEmu: `jagmcp-cli --mcp`, or the line protocol
  through `scripts/jagmcp.py`.
- A transcript holds the emulator's answers, cycle counts among them; a change of the core's timing
  that is right by the references changes them too, and they are written again
  (`jagmcp-cli --batch FILE > FILE.expect`, from `tests/`). Unlike a reference, a transcript is not
  evidence of the hardware.
- Two more fetched libraries, and code of PGMEmu's carried in two places; a fix to the transports
  in one is ported to the other by hand.
- Serving beyond the loopback interface, or TLS, would need this revisited.
