# 0031: Video is recorded by ffmpeg, run beside the emulator

**Status:** proposed

## Context

A client wants to record what a run shows and plays into a video file, an MP4 for example. This
must work on macOS, Linux and Windows, and be driven by the control protocol
([0018](0018-one-control-api.md)). The emulator already writes PNG and WAV files with no library.
An MP4 needs a video encoder and an audio encoder.

- The encoders that every player reads are H.264 and AAC. Their good encoders (x264, FFmpeg's) do
  not build with CMake, and are hard to build on Windows.
- AV1 (SVT-AV1) and Opus build with CMake. QuickTime on Macs without an M3 cannot play AV1.
- Each system has its own encoder (VideoToolbox, Media Foundation). Linux has no common one. That
  would be three ways to record, not one.
- ffmpeg runs on all three systems and has all these encoders.

Encoding costs little next to the emulator: x264 at `veryfast`, on one thread, encodes 20 s of a
cartridge in 0.3 s; the emulator takes 40 s to run them.

## Decision

- **ffmpeg, run as a separate program, encodes the record.** The emulator links no encoder. It
  pipes the frames to ffmpeg as raw RGB, and writes the sound to a file beside the record. When the
  record ends, a second run of ffmpeg puts the two together into the file.
  - The process is started with `posix_spawnp`, or `CreateProcess` on Windows. A thread of the
    recorder writes to the pipe, so the emulation does not wait on the encoder.
  - The program is the one `JAGMCP_FFMPEG` names, or `ffmpeg` on the PATH. A request cannot name
    it: anyone who can reach the port would then run any program.
- **Frames and sound are placed by the DSP cycle they were made in.** Tom gives the cycle that
  completed a frame. The sound takes the DSP's cycle at each I2S frame. With a whole program
  deterministic, the record of a run is the same each time.
  - The video is made in parts, each at the rate the program's frames come at. Where the rate
    changes for three frames running, the part ends and another begins: Native's BIOS runs at
    50.08 Hz and the game at 59.99 Hz. One frame off the rate (the video was off a while) does not
    begin a part. Where no frame was completed in time, the frame before is repeated.
  - The parts are timed in DSP cycles (a time base of 1/26,593,900 s, in NUT files), and so is the
    MP4. They join where the cycles say, and each frame lasts exactly the cycles it took.
  - The sound is resampled to 48 kHz, linearly, as AAC does not take the I2S rates (20,776 Hz in
    Defender 2000). Where the I2S made no frame, it is silent.
- **The pixels are not resampled to 4:3.** The file says they are shown 4:3
  ([0017](0017-the-picture-is-shown-4-3.md)). H.264 in 4:2:0 by default; lossless H.264 of the RGB
  pixels when asked. That one gives exactly the frames the core made.
  - H.264 in 4:2:0 keeps colour for blocks of 2 by 2 pixels, and blurs single pixels. A record
    can make each pixel a block of 2 by 2 (`crisp`), nearest-neighbour, before ffmpeg encodes it.
    A pixel is then a whole block of colour: on Atari Karts' title, 44 dB of PSNR against the
    lossless record, where single pixels give 35 dB, for 1.5 times the size.
  - Blocks of 3 by 3 were tried and are not offered: 41.5 dB, as they do not fall on the blocks of
    colour, and x264 at `veryfast` encodes them slower than the emulation runs (80 s against 54 s
    for 30 s of Atari Karts).
- **No B-frames.** NUT keeps no decoding times. With frames out of order, the MP4 made from it
  starts at the wrong frame and loses one. Without them the file is somewhat larger.

## Consequences

- The record needs ffmpeg installed, with libx264. Without it, `video.record_start` answers
  `encoder_missing`, and nothing else changes.
- A record costs the emulation nothing measurable, `crisp` or not: the encoder runs on another
  core.
- A video whose rate changes has a variable frame rate. Players handle it. A frame or two may be
  dropped where the rate changes.
- Frame times in an MKV record are rounded to milliseconds, as Matroska keeps them.
- A record that is never stopped (the emulator ends) leaves its temporary files, and no video.
