#!/usr/bin/env python3
"""Writes src/jag_core/src/WaveRom.inc, the 1024 words of Jerry's wave table ROM, from the netlist's
ROM model (Jaguar_MiSTer rtl/Jerry/raa016a.v).

usage: extract-wave-rom.py [path/to/raa016a.v]
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
source = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT.parent / "Jaguar_MiSTer/rtl/Jerry/raa016a.v"
words = {}
for m in re.finditer(r"rom_blk\['h([0-9A-Fa-f]+)\]\s*<=\s*16'h([0-9A-Fa-f]+);", source.read_text()):
    words[int(m.group(1), 16)] = int(m.group(2), 16)
if sorted(words) != list(range(1024)):
    sys.exit(f"{source}: expected 1024 words, found {len(words)}")
lines = ["// Jerry's wave table ROM, from Jaguar_MiSTer rtl/Jerry/raa016a.v (scripts/extract-wave-rom.py)."]
for i in range(0, 1024, 8):
    lines.append(" ".join(f"0x{words[i + j]:04x}," for j in range(8)))
(ROOT / "src/jag_core/src/WaveRom.inc").write_text("\n".join(lines) + "\n")
