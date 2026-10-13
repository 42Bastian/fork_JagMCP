#!/usr/bin/env python3
"""Writes src/jag_core/src/CryRom.inc, Tom's CRY colour tables (the red, green and blue level of
each of the 256 colours), from the netlist's ROM models (Jaguar_MiSTer rtl/Tom/ra8008a.v, b and c).

usage: extract-cry-rom.py [path/to/Jaguar_MiSTer/rtl/Tom]
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
tom = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT.parent / "Jaguar_MiSTer/rtl/Tom"
lines = ["// Tom's CRY colour tables, red, green and blue, from Jaguar_MiSTer rtl/Tom/ra8008a.v, ra8008b.v",
         "// and ra8008c.v (scripts/extract-cry-rom.py)."]
for colour, name in (("red", "ra8008a.v"), ("green", "ra8008b.v"), ("blue", "ra8008c.v")):
    source = tom / name
    table = {}
    for m in re.finditer(r"rom_blk\['h\s*([0-9A-Fa-f]+)\]\s*<=\s*8'h([0-9A-Fa-f]+);", source.read_text()):
        table[int(m.group(1), 16)] = int(m.group(2), 16)
    if sorted(table) != list(range(256)):
        sys.exit(f"{source}: expected 256 entries, found {len(table)}")
    lines.append(f"{{ // {colour}")
    for i in range(0, 256, 16):
        lines.append(" ".join(f"0x{table[i + j]:02x}," for j in range(16)))
    lines.append("},")
(ROOT / "src/jag_core/src/CryRom.inc").write_text("\n".join(lines) + "\n")
