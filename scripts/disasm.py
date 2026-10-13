#!/usr/bin/env python3
"""Lists a BS94 DSP program: address, words, instruction.

usage: disasm.py <file.o> [first-address [last-address]]
"""

import struct
import sys

NAMES = ["add", "addc", "addq", "addqt", "sub", "subc", "subq", "subqt", "neg", "and", "or", "xor",
         "not", "btst", "bset", "bclr", "mult", "imult", "imultn", "resmac", "imacn", "div", "abs",
         "sh", "shlq", "shrq", "sha", "sharq", "ror", "rorq", "cmp", "cmpq", "subqmod", "sat16s",
         "move", "moveq", "moveta", "movefa", "movei", "loadb", "loadw", "load", "sat32s",
         "load14n", "load15n", "storeb", "storew", "store", "mirror", "store14n", "store15n",
         "movepc", "jump", "jr", "mmult", "mtoi", "normi", "nop", "load14r", "load15r", "store14r",
         "store15r", "illegal", "addqmod"]
RR = {0, 1, 4, 5, 9, 10, 11, 16, 17, 18, 20, 21, 23, 26, 28, 30, 34, 36, 37, 54, 55, 56}
R = {8, 12, 19, 22, 33, 42, 48, 51}
Q = {2, 3, 6, 7, 25, 27, 29, 32, 63}
CC = {0: "", 1: "ne,", 2: "eq,", 4: "cc,", 8: "cs,", 20: "pl,", 24: "mi,", 5: "hi,", 10: "cseq,"}


def text(words, i, addr):
    w = words[i]
    op, s, d = w >> 10, (w >> 5) & 31, w & 31
    n = NAMES[op]
    if op in RR:
        return n, f"r{s},r{d}", 1
    if op in R:
        return n, f"r{d}", 1
    if op in Q:
        return n, f"#{s or 32},r{d}", 1
    if op == 24:
        return n, f"#{32 - s},r{d}", 1
    if op in (13, 14, 15, 35):
        return n, f"#{s},r{d}", 1
    if op == 31:
        return n, f"#{(s ^ 16) - 16},r{d}", 1
    if op == 38:
        v = words[i + 1] | words[i + 2] << 16 if i + 2 < len(words) else 0
        return n, f"#${v:08x},r{d}", 3
    if op in (39, 40, 41):
        return n, f"(r{s}),r{d}", 1
    if op in (43, 44):
        return "load", f"(r{14 + op - 43}+{(s or 32) * 4}),r{d}", 1
    if op in (45, 46, 47):
        return n, f"r{d},(r{s})", 1
    if op in (49, 50):
        return "store", f"r{d},(r{14 + op - 49}+{(s or 32) * 4})", 1
    if op in (58, 59):
        return "load", f"(r{14 + op - 58}+r{s}),r{d}", 1
    if op in (60, 61):
        return "store", f"r{d},(r{14 + op - 60}+r{s})", 1
    if op == 52:
        return n, f"{CC.get(d, f'{d}?,')}(r{s})", 1
    if op == 53:
        return n, f"{CC.get(d, f'{d}?,')}${addr + 2 + ((s ^ 16) - 16) * 2:06x}", 1
    return n, "", 1


def main():
    data = open(sys.argv[1], "rb").read()
    base = struct.unpack(">I", data[4:8])[0]
    words = [struct.unpack(">H", data[12 + k:14 + k])[0] for k in range(0, len(data) - 13, 2)]
    lo = int(sys.argv[2], 16) if len(sys.argv) > 2 else base
    hi = int(sys.argv[3], 16) if len(sys.argv) > 3 else base + 2 * len(words)
    i = 0
    while i < len(words):
        addr = base + 2 * i
        n, ops, size = text(words, i, addr)
        if lo <= addr <= hi:
            print(f"{addr:06x}  {' '.join(f'{w:04x}' for w in words[i:i + size]):<15} {n:<8}{ops}")
        i += size


if __name__ == "__main__":
    main()
