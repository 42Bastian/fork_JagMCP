# 0017: The window shows the picture at 4:3

**Status:** accepted

## Context

The window showed the frame a pixel square ([0011](0011-the-window-is-pgmemus.md)). A frame is what
the video output shows outside its blanking: in PAL some 52 µs of each of about 283 lines, nearly
the whole picture of a television, which shows it at 4:3. At the pixel widths games use its pixels
are far from square: yarc reloaded's frame is 690 by 283, shown square at about 2.4:1, twice as wide
as it is seen on a television.

## Decision

- **The frame is shown 4:3**, as large as fits; its lines by whole multiples of the display's
  pixels where it fits at least once, its width four thirds of its height, sampled nearest.
- The frame itself, and the frames the runner writes and hashes, are unchanged: the core gives
  pixels as the MiSTer core's output does.

## Consequences

- Pixels of the frame are not all the same width on the display where it is not a whole multiple
  of them.
- A frame not of the whole picture (a smaller one set by the blanking registers) is stretched to
  4:3 all the same.
