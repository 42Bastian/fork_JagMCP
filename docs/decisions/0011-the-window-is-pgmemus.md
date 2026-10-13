# 0011: The window is PGMEmu's; the machine runs in parts on a thread of its own

**Status:** accepted

## Context

The core runs a program to a limit and prints its state, which is what the references need. To
see the frames and hear the sound as a program makes them, a front end has to run the machine
without a limit, at the console's speed, and take the pictures and the samples as they come. The
owner's PGMEmu has such a front end: an SDL3 window rendered through SDL_GPU with Dear ImGui over
it (its decision 0008), the emulator on a thread of its own paced by the host's clock, and the sound
through an SDL audio stream whose resampling is nudged to keep its queue full. The owner chose to
model JagMCP's on it.

## Decision

- **The front end is PGMEmu's** (`src/jag_app`, `jagmcp`): SDL3, SDL_GPU, Dear ImGui with docking,
  at the tags PGMEmu pins, fetched as PGMEmu fetches them (`cmake/Dependencies.cmake`); its
  audio output and screen texture. The core and the command line need none of them, and a build
  without the application (`-DJAG_BUILD_APP=OFF`) fetches nothing.
- **The machine runs in parts.** What the runner did in one call is `Machine`: main RAM, the bus
  and the processors, run up to a cycle and on from there. The processors take their turns
  ([0009](0009-two-processors-by-turns.md)) within a part as within a run, so the parts give the
  run the runner gives in one; the runner's `--part N` runs a reference that way to check it.
- **The emulation thread runs a part of a quarter of a millisecond whenever the machine is that far
  behind the host's clock**, at most a tenth of a second behind: the console's speed, or as fast as
  the core goes where that is slower.
- **The picture is the video output's frame**, as the frame collector of
  [0010](0010-video-by-cycle-frames-as-references.md) completes it, handed over as it completes. It
  is shown a pixel square, as large as fits and by whole multiples of the display's pixels where it
  fits once.
- **The sound is the I2S output**: at each frame the interface generates (its word select rising
  with the serial clock enabled), the transmit latches, at the rate SCLK gives (the DSP's clock over
  64 times SCLK + 1). The runner's `--wav` writes them as the simulation's `--wav` does, and the
  two are the same file.

## Consequences

- What the window shows and plays is what the references check: no second path through the core.
- A program that needs the BIOS's state (the video set up, an object list) shows nothing until the
  68000 runs the BIOS; the boot intros of JaguarDemos are such programs.
- A program keeping both RISCs busy runs slower than the console: `tj_julia` at about 40%,
  the object processor tests at about 20%. The window shows the speed.
- PGMEmu's post-processing shaders, input mapping, debugger windows and control protocol are not
  taken; each would be taken from it when wanted.
