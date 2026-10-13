#!/usr/bin/env python3
"""Records the reference traces the core is tested against: every program runs in the Verilator
simulation of the FPGA core (sim/obj_dir/jagsim), and its trace and final state are written to
tests/reference/.

Programs come from four places:
  tests/asm/*.js       assembled with lyxass; only the RUN directive is read
  tests/m68k/*.s       68000 programs (--m68k), assembled with m68k-elf-as and linked at $4000,
                       where the simulation's boot program jumps; the RUN directive is "| RUN:".
                       A section .cart is linked at $800000 and becomes the cartridge,
                       <name>.cart (--cart)
  tests/programs/*.run binaries loaded at D_RAM, or BS94 files at their address (PROGRAM:), with
                       the runner's arguments (RUN:); --load paths are relative to tests/programs
  random:<class>:<seed> generated here, straight to machine code (see RANDOM_CLASSES)

For each program <name> the directory gets <name>.o (BS94), <name>.args (the runner's arguments
after the file), <name>.trace and <name>.state (jagsim's stdout).

usage: make-references.py [--sim sim/obj_dir/jagsim] [--lyxass ../lyxass/lyxass]
                          [--binutils m68k-elf-] [--random CLASS:FIRST:COUNT ...] [--no-asm]
                          [--out DIR] [names...]

--out records elsewhere than tests/reference: a wider run of a class, checked with
jagmcp-cli --check-all DIR, need not become references.
"""

import argparse
import random
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REF = ROOT / "tests" / "reference"
ASM = ROOT / "tests" / "asm"
M68K = ROOT / "tests" / "m68k"
M68K_ORG = 0x4000
CART_ORG = 0x800000
BINUTILS = "m68k-elf-"
PROGRAMS_DIR = ROOT / "tests" / "programs"
D_RAM = 0xF1B000
D_CTRL = 0xF1A114
CYCLE_CAP = 20000  # bus cycles; references stay small, long programs are checked on a prefix

# --- encoding -------------------------------------------------------------------------------

def op(code, src, dst):
    return [(code << 10) | ((src & 31) << 5) | (dst & 31)]


def movei(value, dst):
    return op(38, 0, dst) + [value & 0xFFFF, (value >> 16) & 0xFFFF]


STOP_FLAG = None  # where a program started by the 68000 says it ends, before it stops


def stop_dsp():
    # [movei #STOP_FLAG,r30 / store r30,(r30)] / movei #D_CTRL,r30 / moveq #0,r31 / .w: store r31,(r30) /
    # jr .w / nop
    flag = movei(STOP_FLAG, 30) + op(47, 30, 30) if STOP_FLAG is not None else []
    return flag + movei(D_CTRL, 30) + op(35, 0, 31) + op(47, 30, 31) + op(53, 0x1E, 0) + op(57, 0, 0)


def bs94(address, words):
    if len(words) % 2:
        words = words + op(57, 0, 0)
    data = b"".join(struct.pack(">H", w) for w in words)
    return b"BS94" + struct.pack(">II", address, len(data)) + data


# --- random programs ------------------------------------------------------------------------

# opcode -> operand kind: rr two registers, r one register (dst), q quick 1..32, b bit 0..31,
# s signed -16..15, m moveq 0..31, l shlq (encoded 32 - n), i movei, n none
ALU = {0: "rr", 1: "rr", 2: "q", 3: "q", 4: "rr", 5: "rr", 6: "q", 7: "q", 8: "r", 9: "rr",
       10: "rr", 11: "rr", 12: "r", 13: "b", 14: "b", 15: "b", 16: "rr", 17: "rr", 22: "r",
       23: "rr", 24: "l", 25: "q", 26: "rr", 27: "q", 28: "rr", 29: "q", 30: "rr", 31: "s",
       33: "r", 34: "rr", 35: "m", 36: "rr", 37: "rr", 38: "i", 42: "r", 48: "r", 51: "r",
       56: "rr", 57: "n"}

ALU_DSP = dict(ALU)
# one word instructions for delay slots: everything in ALU but MOVEI
SLOT = {k: v for k, v in ALU.items() if v != "i"}
CONDITIONS = [0, 0, 1, 2, 4, 8, 20, 24]  # always (twice as likely), ne, eq, cc, cs, pl, mi
POOL = list(range(8))  # a few registers, so that instructions depend on each other


def random_insn(rnd, ops):
    code = rnd.choice(list(ops))
    kind = ops[code]
    s, d = rnd.choice(POOL), rnd.choice(POOL)
    if kind == "rr":
        return op(code, s, d)
    if kind == "r":
        return op(code, 0, d)
    if kind in ("q", "b", "s", "m", "l"):
        return op(code, rnd.randrange(32), d)
    if kind == "pk":  # PACK, or UNPACK with the source field odd
        return op(code, rnd.randrange(2), d)
    if kind == "i":
        return movei(rnd.getrandbits(32), d)
    return op(code, 0, 0)


def alu_body(rnd, base):
    words = []
    for _ in range(30):
        words += random_insn(rnd, ALU)
    return words


def jump_body(rnd, base):
    """Blocks of ALU instructions with forward JR and JUMP, taken or not by their condition."""
    words = []
    pending = []  # (index of the word to patch, kind) for jumps whose target is not placed yet
    for _ in range(12):
        for _ in range(rnd.randrange(0, 4)):
            words += random_insn(rnd, ALU)
        cc = rnd.choice(CONDITIONS)
        if rnd.random() < 0.5:
            pending.append((len(words), "jr", cc))
            words += op(53, 0, cc)
        else:
            pending.append((len(words), "jump", cc))
            words += movei(0, 20) + op(52, 20, cc)
        words += random_insn(rnd, SLOT)
        # the target: a few instructions on, so that the skipped ones are seen not to run
        skip = rnd.randrange(0, 4)
        for _ in range(skip):
            words += random_insn(rnd, SLOT)
        at, kind, cc = pending.pop()
        target = base + 2 * len(words)
        if kind == "jr":
            offset = (target - (base + 2 * at + 2)) // 2
            words[at] = (53 << 10) | ((offset & 31) << 5) | cc
        else:
            words[at + 1] = target & 0xFFFF
            words[at + 2] = target >> 16
    return words


# local RAM the load and store class works in: pointers r8 to r11, bases r14 and r15, indices r12
# and r13 (multiples of 4), all outside POOL so that the ALU instructions leave them alone
DATA = D_RAM + 0x1000
LONG_LOADS = {41: "p", 43: "n", 44: "n", 58: "x", 59: "x"}  # load, (r14+n), (r15+n), (r14+rx), (r15+rx)
LONG_STORES = {47: "p", 49: "n", 50: "n", 60: "x", 61: "x"}


def memory_insn(rnd):
    loads, stores = dict(LONG_LOADS), dict(LONG_STORES)
    if D_RAM == 0xF03000:  # the GPU's LOADP and STOREP
        loads[42], stores[48] = "p", "p"
    code = rnd.choice(list(loads) + list(stores))
    kind = loads.get(code) or stores[code]
    d = rnd.choice(POOL)
    if kind == "p":
        return op(code, rnd.choice([8, 9, 10, 11]), d)
    if kind == "n":
        # 1..7 names a register of POOL as well: the scoreboard compares it as one
        return op(code, rnd.choice([1, 2, 3, 4, 5, 6, 7, rnd.randrange(1, 32)]), d)
    return op(code, rnd.choice([12, 13]), d)


def load_store_body(rnd, base):
    """Long loads and stores to local RAM, every addressing mode, among ALU instructions."""
    words = []
    for r, offset in zip([8, 9, 10, 11], [0, 4, 0x40, 0x7c]):
        words += movei(DATA + offset, r)
    words += movei(DATA, 14) + movei(DATA + 0x80, 15) + movei(8, 12) + movei(0x20, 13)
    for _ in range(30):
        words += memory_insn(rnd) if rnd.random() < 0.5 else random_insn(rnd, ALU)
    return words


NARROW = {39: "loadb", 40: "loadw", 45: "storeb", 46: "storew", 41: "load", 47: "store"}


def narrow_body(rnd, base):
    """Byte and word loads and stores to local RAM at every alignment, long ones among them."""
    words = []
    for r, offset in zip([8, 9, 10, 11], [0, 1, 2, 3]):
        words += movei(DATA + 4 * rnd.randrange(8) + offset, r)
    for _ in range(30):
        if rnd.random() < 0.5:
            code = rnd.choice(list(NARROW))
            words += op(code, rnd.choice([8, 9, 10, 11]), rnd.choice(POOL))
        else:
            words += random_insn(rnd, ALU)
    return words


D_FLAGS, D_REMAIN, D_MACHI, D_MOD = 0xF1A100, 0xF1A11C, 0xF1A120, 0xF1A118
# opcode -> kind for the multiply-accumulate and modulo instructions the io class adds
MAC = {18: "rr", 20: "rr", 19: "r", 32: "q", 63: "q"}  # imultn, imacn, resmac, subqmod, addqmod
MAC_DSP = dict(MAC)


def io_insn(rnd):
    """One access to the DSP's I/O registers, through pointers r20 to r24 of both banks."""
    d = rnd.choice(POOL)
    kind = rnd.randrange(9)
    if kind == 0:
        return op(21, rnd.choice(POOL), d)  # div
    if kind == 1:
        return movei(rnd.randrange(2), 25) + op(47, 21, 25)  # D_DIVCTRL
    if kind == 2:
        return op(41, 21, d)  # D_REMAIN
    if kind == 3:
        return op(41, 20, d)  # D_FLAGS, with the pipeline's bits
    if kind == 4:
        return op(41, 24, d)  # D_CTRL, likewise
    if kind == 5:
        return op(41, 22, d)  # D_MACHI
    if kind == 6:
        return movei(rnd.choice([0, 0xF, 0xFF, 0x3FF, rnd.getrandbits(32)]), 25) + op(47, 23, 25)  # D_MOD
    # D_FLAGS: the flags and the register bank, no interrupt enabled
    return movei(rnd.randrange(8) | rnd.randrange(2) << 14, 25) + op(47, 20, 25)


def io_body(rnd, base):
    """The divider and the DSP's registers among ALU and multiply-accumulate instructions."""
    words = []
    for r, a in zip([20, 21, 22, 23, 24], [D_FLAGS, D_REMAIN, D_MACHI, D_MOD, D_CTRL]):
        words += movei(a, r) + op(36, r, r)  # and moveta, the same in bank 1
    ops = {**ALU, **MAC}
    for _ in range(30):
        words += io_insn(rnd) if rnd.random() < 0.4 else random_insn(rnd, ops)
    return words


SCLK, SMODE, D_CTRL_ = 0xF1A150, 0xF1A154, D_CTRL
STACK = D_RAM + 0x1F00


def irq_program(rnd, jumps=False):
    """The I2S interrupt at a random serial clock and mode, taken in a loop of ALU, MOVEI,
    multiply-accumulate and local load and store instructions. Starts at D_RAM; the interrupt
    service routine is at its vector, D_RAM + $10, in bank 0 (r27 to r31)."""
    base = D_RAM
    words = movei(0, 20) + op(52, 20, 0) + op(57, 0, 0)  # movei #start,r20 / jump (r20) / nop
    start_at = 0  # the movei
    words += op(57, 0, 0) * (8 - len(words))
    # the service routine: count, clear the I2S latch and IMASK, return
    isr = movei(D_FLAGS, 30) + op(41, 30, 29) + op(15, 3, 29) + op(14, 10, 29) + op(2, 1, 27)
    for _ in range(rnd.randrange(3)):
        isr += op(rnd.choice([0, 4, 9, 10, 11, 26]), rnd.choice([21, 22, 23]), rnd.choice([21, 22, 23]))
    isr += op(41, 31, 28) + op(2, 2, 28) + op(2, 4, 31) + op(52, 28, 0) + op(47, 30, 29)
    words += isr
    start = base + 2 * len(words)
    words[start_at + 1], words[start_at + 2] = start & 0xFFFF, start >> 16
    for r in POOL:
        words += movei(rnd.getrandbits(32), r)
    for r, offset in zip([8, 9, 10, 11], [0, 4, 0x40, 0x7c]):
        words += movei(DATA + offset, r)
    words += movei(DATA, 14) + movei(DATA + 0x80, 15) + movei(8, 12) + movei(0x20, 13)
    words += movei(STACK, 20) + op(36, 20, 31) + op(35, 0, 20) + op(36, 20, 27)  # bank 0 r31, r27
    smode = rnd.choice([0b001101, 0b010101, 0b011101, 0b100101, 0b001111])
    sclk = rnd.randrange(3, 6) if smode & 32 else rnd.randrange(6)  # the routine must leave time
    words += movei(SCLK, 20) + movei(sclk, 21) + op(47, 20, 21)
    words += op(2, 4, 20) + movei(smode, 21) + op(47, 20, 21)
    words += movei(D_FLAGS, 20) + movei(0x4020, 21) + op(47, 20, 21)  # I2S enabled, bank 1
    words += movei(rnd.randrange(3, 7), 16)
    loop_at = len(words)
    words += movei(0, 17)
    loop = len(words)
    words[loop_at + 1], words[loop_at + 2] = (base + 2 * loop) & 0xFFFF, (base + 2 * loop) >> 16
    if jumps:
        words += jump_body(rnd, base + 2 * len(words))
    ops = {**ALU, **MAC}
    last = None
    for _ in range(rnd.randrange(10, 30) if not jumps else 0):
        insn = memory_insn(rnd) if rnd.random() < 0.25 else random_insn(rnd, ops)
        # an indexed access right after an IMACN takes the accumulated sum as its address
        # (macop_pp), most likely outside local RAM
        while last == 20 and insn[0] >> 10 in (43, 44, 49, 50, 58, 59, 60, 61):
            insn = random_insn(rnd, ops)
        words += insn
        last = insn[0] >> 10
    words += op(6, 1, 16)  # subq #1,r16
    words += op(52, 17, 1)  # jump ne,(r17)
    words += op(57, 0, 0)
    words += op(57, 0, 0) * 2 + stop_dsp()
    return words


# main RAM the external class works in: rows 2 to 5 and 7 of bank 0 (a row is 2 KB with 64 bit
# wide DRAM; bank 1, $400000, is not fitted), and Jerry's joystick ports, read only
EXT_POINTERS = [0x1000, 0x1010, 0x1800, 0x3FF8, 0x2000, 0x2804]
EXT_BASES = [0x1000, 0x1800, 0x2800]
JOYSTICK = 0xF14000
LOADS_ALL = {39: "b", 40: "w", 41: "l"}  # loadb, loadw, load (Rn)
STORES_ALL = {45: "b", 46: "w", 47: "l"}  # storeb, storew, store
EXT_ARGS = ["--mem", "$1000:$1000", "--mem", "$2000:$1000", "--mem", "$3000:$1000",
            "--poke", "$1000=$12345678", "--poke", "$1010=$9abcdef0", "--poke", "$1800=$cafef00d",
            "--poke", "$3ff8=$0badc0de", "--poke", "$2000=$76543210", "--poke", "$2804=$55aa33cc"]


def external_insn(rnd):
    """A load or store outside the local space: main RAM through r8 (a long word boundary), r9 (two
    past one), r10 (an odd address, bytes only), r14 and r15 (bases), r12 and r13 (indices),
    Jerry's joystick ports through r11 (loads only); a local one through r19 now and then. A word
    at an odd address is left out: its high byte is what the bus held before."""
    d = rnd.choice(POOL)
    kind = rnd.randrange(10)
    gpu = D_RAM == 0xF03000
    if gpu and kind == 9:
        # the GPU's phrases: LOADP, STOREP, G_HIDATA read and written (r18)
        # (a LOADP not on a phrase boundary leaves G_HIDATA what the bus held: from r8 only)
        return rnd.choice([op(42, 8, d), op(48, rnd.choice([8, 9]), d),
                           op(41, 18, d), op(47, 18, d)])
    if kind < 4:
        code = rnd.choice(list(LOADS_ALL))
        return op(code, rnd.choice([8, 9, 10, 11] if code == 39 else [8, 9, 11]), d)
    if kind < 7:
        code = rnd.choice(list(STORES_ALL))
        return op(code, rnd.choice([8, 9, 10] if code == 45 else [8, 9]), d)
    if kind < 9:
        code = rnd.choice([43, 44, 49, 50, 58, 59, 60, 61])  # (r14+n), (r15+n), (r14+rx), (r15+rx)
        if code in (43, 44, 49, 50):
            return op(code, rnd.randrange(1, 8), d)
        return op(code, rnd.choice([12, 13]), d)
    return op(rnd.choice([41, 47]), 19, d)  # local


def hidata_kept(rnd, body):
    """The GPU's G_HIDATA takes the high half of the bus with every external load, what an
    earlier cycle left there where a load narrower than a phrase does not drive it (not
    modelled): G_HIDATA is read, and STOREP writes it, only where a LOADP or a write of G_HIDATA
    set it with no other external load since, around the loop."""
    def kind(insn):
        code, src = insn[0] >> 10, (insn[0] >> 5) & 31
        if code == 42 or (code == 47 and src == 18):
            return "set"
        if code == 48 or (code == 41 and src == 18):
            return "use"
        external = (code in (39, 40, 41) and src != 19) or code in (43, 44, 58, 59)
        return "spoil" if external else None
    # a LOADP's G_HIDATA is there once the load is: its register is read first (xlddwait)
    pending = None
    for _ in range(2):  # twice: what the end of the loop leaves is what its start finds
        clean = False if _ == 0 else clean
        for i, insn in enumerate(body):
            k = kind(insn)
            if k == "set":
                clean = True
                pending = insn[0] & 31 if insn[0] >> 10 == 42 else None
            elif k == "spoil":
                clean = False
            elif k == "use" and not clean:
                body[i] = random_insn(rnd, ALU)
            elif k == "use" and pending is not None and _ == 1:
                body[i] = op(10, pending, pending) + insn  # or rn,rn
                pending = None
    return body


def external_program(rnd):
    """Loads and stores to main RAM and Jerry's ports, all widths and addressing modes, among
    local ones, ALU instructions, DIV and MOVEI, looped long enough for the refresh to come.
    One program in four starts with an external access, while the 68000 is still on the bus."""
    base = D_RAM
    words = []
    if rnd.random() < 0.25:
        words += movei(rnd.choice(EXT_POINTERS + [JOYSTICK + (0 if D_RAM == 0xF03000 else 2)]), 8)
        words += op(rnd.choice([39, 40, 41, 45, 46, 47]) if rnd.random() < 0.5 else 41, 8, 0)
    for r in POOL:
        words += movei(rnd.getrandbits(32), r)
    pointers = rnd.sample(EXT_POINTERS, 3)
    gpu = D_RAM == 0xF03000
    for r, a in zip([8, 9, 10], pointers):
        words += movei(a + {8: 0, 9: 4 if gpu else 2, 10: rnd.choice([1, 3])}[r], r)
    if gpu:
        words += movei(D_MOD, 18)  # G_HIDATA
    words += movei(JOYSTICK + (0 if gpu else rnd.choice([0, 2])), 11)  # the GPU's long word at 2: not modelled
    bases = rnd.sample(EXT_BASES, 2)
    words += movei(bases[0], 14) + movei(bases[1], 15) + movei(8, 12) + movei(0x20, 13)
    words += movei(DATA, 19)
    words += movei(rnd.randrange(6, 24), 16)  # past the first refresh, at cycle 1906, mostly
    loop_at = len(words)
    words += movei(0, 17)
    loop = len(words)
    words[loop_at + 1], words[loop_at + 2] = (base + 2 * loop) & 0xFFFF, (base + 2 * loop) >> 16
    body = []
    for _ in range(rnd.randrange(15, 40)):
        x = rnd.random()
        if x < 0.45:
            body.append(external_insn(rnd))
        elif x < 0.55:
            body.append(op(21, rnd.choice(POOL), rnd.choice(POOL)))  # div
        elif x < 0.62:
            body.append(movei(rnd.getrandbits(32), rnd.choice(POOL)))
        else:
            body.append(random_insn(rnd, ALU))
    if gpu:
        body = hidata_kept(rnd, body)
    for insn in body:
        words += insn
    words += op(6, 1, 16)  # subq #1,r16
    words += op(52, 17, 1)  # jump ne,(r17)
    words += op(57, 0, 0)
    words += op(57, 0, 0) * 2 + stop_dsp()
    return words


# the timers: Jerry's two (JPIT1 to JPIT4, read at JPIT_READ, the second's at JPIT_READ2) or Tom's
# PIT (PIT0 and PIT1, read where they are written), for the GPU
JPIT1, JPIT_READ, JPIT_READ2, TIMER_COUNT = 0xF10000, 0xF10036, 0xF1003A, 2


def timer_program(rnd):
    """Jerry's timers written through the bus, by long word and by word, read as they count, and
    their interrupts taken (timer 1 at D_RAM + $20, timer 2 at D_RAM + $30), among ALU
    instructions and loads and stores to main RAM; for the GPU, Tom's PIT (at G_RAM + $20)."""
    base = D_RAM
    words = movei(0, 20) + op(52, 20, 0) + op(57, 0, 0)  # movei #start,r20 / jump (r20) / nop
    start_at = 0
    words += op(57, 0, 0) * (16 - len(words))
    vectors = []
    for _ in range(2):  # the vectors: movei #isr,r28 / jump (r28) / nop
        vectors.append(len(words))
        words += movei(0, 28) + op(52, 28, 0) + op(57, 0, 0)
        words += op(57, 0, 0) * (8 - 5)
    for n, vector in enumerate(vectors):
        isr = base + 2 * len(words)
        words[vector + 1], words[vector + 2] = isr & 0xFFFF, isr >> 16
        # count, clear the timer's latch and IMASK, return
        words += movei(D_FLAGS, 30) + op(41, 30, 29) + op(15, 3, 29) + op(14, 11 + n, 29)
        words += op(2, 1, 26 + n)
        for _ in range(rnd.randrange(3)):
            words += op(rnd.choice([0, 4, 9, 10, 11, 26]), rnd.choice([21, 22, 23]), rnd.choice([21, 22, 23]))
        words += op(41, 31, 28) + op(2, 2, 28) + op(2, 4, 31) + op(52, 28, 0) + op(47, 30, 29)
    start = base + 2 * len(words)
    words[start_at + 1], words[start_at + 2] = start & 0xFFFF, start >> 16
    for r in POOL:
        words += movei(rnd.getrandbits(32), r)
    words += movei(STACK, 20) + op(36, 20, 31)  # bank 0 r31
    words += movei(JPIT1, 21) + movei(JPIT1 + 4, 22) + movei(JPIT_READ, 23) + movei(JPIT_READ2, 24)
    words += movei(0x1000, 8) + movei(0x1808 if TIMER_COUNT == 1 else 0x1806, 9)

    def setting():
        # a period of 120 cycles at least, so that the routines keep up
        while True:
            pre, div = rnd.randrange(0, 60), rnd.randrange(0, 12)
            if (pre + 1) * (div + 1) >= 120:
                return pre, div

    def program_timer(n):
        pre, div = setting()
        if rnd.random() < 0.5:  # one long word: prescaler, then divider
            return movei(pre << 16 | div, 25) + op(47, 21 + n, 25)
        out = movei(div, 25) + op(2, 2, 21 + n) + op(46, 21 + n, 25) + op(6, 2, 21 + n)  # JPIT2 first
        return out + movei(pre, 25) + op(46, 21 + n, 25)

    timers = rnd.choice([[0], [1], [0, 1]]) if TIMER_COUNT == 2 else [0]
    for n in timers:
        words += program_timer(n)
    enable = sum(1 << (6 + n) for n in timers)
    words += movei(D_FLAGS, 20) + movei(0x4000 | enable, 25) + op(47, 20, 25)
    words += movei(rnd.randrange(4, 12), 16)
    loop_at = len(words)
    words += movei(0, 17)
    loop = len(words)
    words[loop_at + 1], words[loop_at + 2] = (base + 2 * loop) & 0xFFFF, (base + 2 * loop) >> 16
    for _ in range(rnd.randrange(10, 30)):
        x = rnd.random()
        if x < 0.15:  # read a timer; the GPU's PIT1 by word (a long word two past a boundary is not modelled)
            code, reg = rnd.choice([40, 41]), rnd.choice([23, 24])
            words += op(40 if reg == 24 and TIMER_COUNT == 1 else code, reg, rnd.choice(POOL))
        elif x < 0.2:
            words += program_timer(rnd.choice(timers))
        elif x < 0.35:
            words += op(rnd.choice([41, 47]), rnd.choice([8, 9]), rnd.choice(POOL))  # main RAM
        else:
            words += random_insn(rnd, ALU)
    words += op(6, 1, 16)  # subq #1,r16
    words += op(52, 17, 1)  # jump ne,(r17)
    words += op(57, 0, 0)
    words += movei(D_FLAGS, 20) + movei(0x4000, 25) + op(47, 20, 25)  # interrupts off
    words += op(57, 0, 0) * 2 + stop_dsp()
    return words


FLAG = DATA + 0x100  # the count the interrupt service routine increments


HC, VC, VMODE, HP, HDE, VP, VDB, VDE, VI, PIT0, INT1 = (0xF00004, 0xF00006, 0xF00028, 0xF0002E, 0xF0003C,
                                                       0xF0003E, 0xF00046, 0xF00048, 0xF0004E, 0xF00050,
                                                       0xF000E0)
BORD1, BORD2, HBB, HBE, VBB, VBE = 0xF0002A, 0xF0002C, 0xF00030, 0xF00032, 0xF00040, 0xF00042


def video_program(rnd):
    """Tom's video time base started with short half lines and fields, the object processor idle
    (VDB and VDE out of the field), HC, VC and INT1 read and INT1's requests (the vertical
    interrupt, the PIT) enabled and acknowledged, the registers now and then written again, the
    border colour changed, among ALU instructions and loads and stores to main RAM: frames of the
    border and the blanking."""
    words = []
    for r in POOL:
        words += movei(rnd.getrandbits(32), r)

    def put(address, value, size=1):  # movei #address,r20 / movei #value,r25 / storew (store) r25,(r20)
        return movei(address, 20) + movei(value, 25) + op(46 if size == 1 else 47, 20, 25)

    hp = rnd.randrange(60, 400)
    vp = rnd.randrange(6, 40)
    words += put(HP, hp) + put(VP, vp)
    words += put(HDE, rnd.randrange(hp) | rnd.choice([0, 0x400])) + put(VI, rnd.randrange(vp + 1))
    words += put(VDB, 0x7FF) + put(VDE, 0x7FF)
    words += put(HBB, rnd.randrange(hp) | 0x400) + put(HBE, rnd.randrange(hp))
    words += put(VBB, rnd.randrange(vp + 1)) + put(VBE, rnd.randrange(vp + 1))
    words += put(BORD1, rnd.getrandbits(16)) + put(BORD2, rnd.getrandbits(8))
    if rnd.random() < 0.5:  # the PIT, for INT1's bit 3
        words += put(PIT0, rnd.randrange(20, 200) << 16 | rnd.randrange(0, 8), 2)
    words += put(INT1, rnd.choice([1, 8, 9]))
    words += put(VMODE, rnd.choice([0x6C1, 0xC1, 0x6C7, 0x4C1, 0x2C1, 0x8C3, 0xEC1]))
    words += movei(HC, 21) + movei(VC, 22) + movei(INT1, 23) + movei(0x1000, 8) + movei(0x1808 if TIMER_COUNT == 1 else 0x1806, 9)
    words += movei(rnd.randrange(4, 12), 16)
    loop_at = len(words)
    words += movei(0, 17)
    loop = len(words)
    words[loop_at + 1], words[loop_at + 2] = (D_RAM + 2 * loop) & 0xFFFF, (D_RAM + 2 * loop) >> 16
    for _ in range(rnd.randrange(10, 30)):
        x = rnd.random()
        if x < 0.25:  # a counter or INT1, by word, or HC and VC by long word
            if rnd.random() < 0.3:
                words += op(41, 21, rnd.choice(POOL))
            else:
                words += op(40, rnd.choice([21, 22, 23]), rnd.choice(POOL))
        elif x < 0.32:  # acknowledge INT1's requests, enable some
            words += movei(rnd.choice([0x100, 0x800, 0x900]) | rnd.choice([1, 8, 9]), 25) + op(46, 23, 25)
        elif x < 0.35:  # a register written again
            words += rnd.choice([put(VI, rnd.randrange(vp + 1)), put(HDE, rnd.randrange(hp)),
                                 put(VC, rnd.randrange(vp)), put(HC, rnd.randrange(hp))])
        elif x < 0.42:  # the border colour
            words += put(BORD1, rnd.getrandbits(16)) if rnd.random() < 0.7 else put(BORD2, rnd.getrandbits(8))
        elif x < 0.45:
            words += op(rnd.choice([41, 47]), rnd.choice([8, 9]), rnd.choice(POOL))  # main RAM
        else:
            words += random_insn(rnd, ALU)
    words += op(6, 1, 16)  # subq #1,r16
    words += op(52, 17, 1)  # jump ne,(r17)
    words += op(57, 0, 0)
    words += op(57, 0, 0) * 2 + stop_dsp()
    return words


OLP, OBF, HDB1, HDB2, HS, VS, BG = 0xF00020, 0xF00026, 0xF00038, 0xF0003A, 0xF00034, 0xF00044, 0xF00058
CLUT = 0xF00400
OBJECTS, PIXELS = 0x10000, 0x20000  # the object list, rebuilt every field; the pixels


def object_list(rnd, vdb, vde):
    """A list of up to four objects, 32 bytes apart from OBJECTS: bitmaps of every depth with their
    flags, scaled bitmaps and branches, the last a stop. Returns the phrases, by object."""
    count = rnd.randrange(1, 5)
    objects = []
    for n in range(count):
        here = OBJECTS + 32 * n
        nxt = OBJECTS + 32 * (n + 1)
        kind = rnd.choice(["bitmap", "bitmap", "scaled", "branch"])
        ypos = rnd.randrange(vdb, vde - 2)
        if kind == "branch":
            link = OBJECTS + 32 * rnd.randrange(n + 1, count + 1)
            cc = rnd.randrange(5)
            objects.append([3 | ypos << 3 | cc << 14 | (link >> 3) << 24])
            continue
        depth = rnd.randrange(6)
        dwidth = rnd.randrange(1, 4)
        iwidth = rnd.randrange(1, 4)
        data = PIXELS + 8 * rnd.randrange(0, 200)
        p0 = (1 if kind == "scaled" else 0) | ypos << 3 | rnd.randrange(2, 9) << 14 | (nxt >> 3) << 24 | (data >> 3) << 43
        xpos = rnd.randrange(-24, 100) & 0xFFF
        flags = rnd.choice([0, 0, 1, 2, 4, 8, 5, 9, 15])  # reflect, rmw, trans, release
        firstpix = rnd.randrange(64) if depth < 4 and rnd.random() < 0.3 else 0
        p1 = xpos | depth << 12 | rnd.randrange(1, 3) << 15 | dwidth << 18 | iwidth << 28 | rnd.getrandbits(7) << 38 | \
            flags << 45 | firstpix << 49
        phrases = [p0, p1]
        if kind == "scaled":
            phrases.append(rnd.randrange(0x10, 0x48) | rnd.randrange(0x10, 0x48) << 8 | rnd.getrandbits(8) << 16)
        objects.append(phrases)
    objects.append([4])
    return objects


def objects_program(rnd):
    """The object processor: Tom's time base with a display window, the CLUT and pixels filled with
    a pseudo-random sequence, a list of bitmaps (every depth, reflected, RMW, transparent, scaled),
    branches and a stop copied to main RAM in every field, the pixels it draws seen in the frames;
    ALU instructions and loads from main RAM meanwhile."""
    words = []
    for r in POOL:
        words += movei(rnd.getrandbits(32), r)

    def put(address, value):
        return movei(address, 20) + movei(value, 25) + op(46, 20, 25)

    def jr(cc, target):  # a JR to the word index `target`, from the next one (words up to it in place)
        offset = target - (len(words) + 1)
        assert -16 <= offset < 16
        return op(53, offset & 31, cc)

    hp = rnd.randrange(80, 160)
    vp = rnd.randrange(20, 40)
    vdb, vde = rnd.randrange(2, 8), rnd.randrange(12, vp - 2)
    # the display begins within the horizontal blank: the first pixels the output takes from Tom's
    # data bus (before the line buffer is read) are not modelled
    hdb, hbe = rnd.randrange(4, 12), rnd.randrange(14, 30)
    regs = [(HP, hp), (HBB, 0x400 | (hp - rnd.randrange(4, 16))), (HBE, hbe), (HS, 0x400 | (hp - 2)),
            (HDB1, hdb), (HDB2, hdb if rnd.random() < 0.8 else 0x400 | hdb), (HDE, 0x400 | (hp - rnd.randrange(16, 30))),
            (VP, vp), (VBB, vp - 1), (VBE, 1), (VS, vp - 1), (VDB, vdb), (VDE, vde), (VI, 0x7FF),
            (BORD1, rnd.getrandbits(16)), (BORD2, rnd.getrandbits(8)), (BG, rnd.getrandbits(16)),
            (OLP, OBJECTS & 0xFFFF), (OLP + 2, OBJECTS >> 16)]
    for a, v in regs:
        words += put(a, v)
    # the CLUT and the pixels: r1 a rotated sum
    words += movei(rnd.getrandbits(32), 1) + movei(rnd.getrandbits(32) | 1, 3)
    for base, count, size in ((CLUT, 256, 46), (PIXELS, 512, 47)):  # storew, store
        words += movei(base, 14) + movei(count, 2)
        loop = len(words)
        words += op(34, 1, 4) + op(29, 7, 4) + op(11, 4, 1) + op(0, 3, 1)  # move r1,r4 / rorq #7,r4 / xor / add
        words += op(size, 14, 1) + op(2, 2 if size == 46 else 4, 14) + op(6, 1, 2)  # store, addq, subq #1,r2
        words += jr(1, loop) + op(57, 0, 0)
    words += put(VMODE, rnd.choice([0x6C1, 0x6C7, 0x4C1, 0x2C1, 0xEC1, 0x0C1, 0x6C3]))  # not direct RGB16
    # the frame loop: the list copied, then the next field waited for
    # each object at its 32 byte slot, padded with zero phrases
    slots = [obj + [0] * (4 - len(obj)) for obj in object_list(rnd, vdb, vde)]
    longs = [v for phrases4 in slots for p in phrases4 for v in ((p >> 32) & 0xFFFFFFFF, p & 0xFFFFFFFF)]
    words += movei(VC, 22) + movei(0x7FF, 21) + movei(0x1000, 8)
    frame = len(words)
    words += movei(0, 15) + movei(OBJECTS, 14) + movei(len(longs), 2)  # r15: the template, patched below
    template_at = frame
    copy = len(words)
    words += op(41, 15, 4) + op(2, 4, 15) + op(47, 14, 4) + op(2, 4, 14) + op(6, 1, 2)
    words += jr(1, copy) + op(57, 0, 0)
    for _ in range(rnd.randrange(3, 8)):
        words += random_insn(rnd, ALU) if rnd.random() < 0.8 else op(41, 8, rnd.choice(POOL))
    w1 = len(words)  # wait for VC 0, then for it to leave 0
    words += op(40, 22, 0) + op(9, 21, 0) + op(31, 0, 0)
    words += jr(1, w1) + op(57, 0, 0)
    w2 = len(words)
    words += op(40, 22, 0) + op(9, 21, 0) + op(31, 0, 0)
    words += jr(2, w2) + op(57, 0, 0)
    words += movei(D_RAM + 2 * frame, 23) + op(52, 23, 0) + op(57, 0, 0)  # jump (r23)
    if len(words) % 2:
        words += op(57, 0, 0)
    template = D_RAM + 2 * len(words)
    words[template_at + 1], words[template_at + 2] = template & 0xFFFF, template >> 16
    for v in longs:
        words += [v >> 16, v & 0xFFFF]
    assert 2 * len(words) < 0xF00
    return words


def wait_program(rnd):
    """Loops that wait for an interrupt (I2S or timer 1) to change a count in local RAM: their
    data the same every iteration, or not (an I/O read, a store of a changing value), with ALU
    instructions, stores of the same value, JR or JUMP; the waits the core skips."""
    base = D_RAM
    timer = rnd.random() < 0.4
    words = movei(0, 20) + op(52, 20, 0) + op(57, 0, 0)  # movei #start,r20 / jump (r20) / nop
    words += op(57, 0, 0) * (8 - len(words))
    vector = len(words)  # I2S at D_RAM + $10; timer 1 at + $20, reached from here
    if timer:
        words += op(57, 0, 0) * 8
        vector = len(words)
    isr = movei(0, 28) + op(52, 28, 0) + op(57, 0, 0)  # movei #routine,r28 / jump (r28) / nop
    words += isr
    words += op(57, 0, 0) * (8 - len(isr))
    routine = base + 2 * len(words)
    words[vector + 1], words[vector + 2] = routine & 0xFFFF, routine >> 16
    # count, clear the latch and IMASK, return
    words += movei(D_FLAGS, 30) + op(41, 30, 29) + op(15, 3, 29) + op(14, 11 if timer else 10, 29)
    words += movei(FLAG, 28) + op(41, 28, 27) + op(2, 1, 27) + op(47, 28, 27)
    words += op(41, 31, 28) + op(2, 2, 28) + op(2, 4, 31) + op(52, 28, 0) + op(47, 30, 29)
    start = base + 2 * len(words)
    words[1], words[2] = start & 0xFFFF, start >> 16
    for r in POOL:
        words += movei(rnd.getrandbits(32), r)
    words += movei(STACK, 20) + op(36, 20, 31)  # bank 0 r31
    words += movei(FLAG, 10) + movei(0, 12) + movei(DATA, 13) + movei(D_FLAGS, 14)
    if timer:
        pre, div = rnd.randrange(10, 60), rnd.randrange(5, 30)
        words += movei(JPIT1, 21) + movei(pre << 16 | div, 25) + op(47, 21, 25)
        words += movei(D_FLAGS, 20) + movei(0x4040, 21) + op(47, 20, 21)
    else:
        smode = rnd.choice([0b001101, 0b010101, 0b011101, 0b100101])
        sclk = rnd.randrange(3, 8)
        words += movei(SCLK, 20) + movei(sclk, 21) + op(47, 20, 21)
        words += op(2, 4, 20) + movei(smode, 21) + op(47, 20, 21)
        words += movei(D_FLAGS, 20) + movei(0x4020, 21) + op(47, 20, 21)
    words += movei(rnd.randrange(2, 5), 16)
    outer = len(words)
    # the wait: constants, ALU among r1 to r5, perhaps a store or an I/O read, the count compared
    wait = len(words)
    body = []
    use_jump = rnd.random() < 0.5
    for r in range(1, 6):
        if rnd.random() < 0.6:
            body += op(35, rnd.randrange(32), r)  # moveq
    for _ in range(rnd.randrange(0, 4)):
        body += op(rnd.choice([0, 4, 9, 10, 11, 26, 34]), rnd.randrange(1, 6), rnd.randrange(1, 6))
    kind = rnd.random()
    if kind < 0.2:
        body += op(47, 13, rnd.randrange(1, 6))  # a store of the same value, or of one that changes
    elif kind < 0.3:
        body += op(41, 14, 6)  # D_FLAGS: no repeat
    body += op(41, 10, 0) + op(30, 0, 12)  # load (r10),r0 / cmp r0,r12
    if use_jump:
        words += movei(0, 17)
        wait = len(words)
        words[wait - 2], words[wait - 1] = (base + 2 * wait) & 0xFFFF, (base + 2 * wait) >> 16
        words += body + op(52, 17, 2) + op(57, 0, 0)  # jump eq,(r17) / nop
    else:
        words += body
        offset = (2 * wait - (2 * len(words) + 2)) // 2
        if offset < -16:
            words = words[:wait]
            words += op(41, 10, 0) + op(30, 0, 12)
            offset = (2 * wait - (2 * len(words) + 2)) // 2
        words += op(53, offset & 31, 2) + op(57, 0, 0)  # jr eq,wait / nop
    words += op(34, 0, 12)  # move r0,r12
    for _ in range(rnd.randrange(0, 4)):
        words += random_insn(rnd, ALU)
    words += op(6, 1, 16) + movei(base + 2 * outer, 17) + op(52, 17, 1) + op(57, 0, 0)  # loop
    words += movei(D_FLAGS, 20) + movei(0x4000, 21) + op(47, 20, 21)
    words += op(57, 0, 0) * 2 + stop_dsp()
    return words


def mmult_program(rnd):
    """MMULT: a matrix of 64 long words in local RAM, the vector in bank 1, widths 2 to 15 by row
    and by column, D_MTXA moved between them, among ALU instructions, multiply-accumulates,
    local loads and uses of the result."""
    base = D_RAM
    io = D_FLAGS
    mtx = DATA + 0x200
    words = []
    for r in POOL:
        words += movei(rnd.getrandbits(32), r)
    # the matrix: an arithmetic run of random step (r1 value, r3 step, r2 pointer, r4 count)
    words += movei(rnd.getrandbits(32), 1) + movei(rnd.getrandbits(32) | 0x10001, 3)
    words += movei(mtx, 2) + movei(64, 4)
    fill = len(words)
    words += op(47, 2, 1) + op(0, 3, 1) + op(2, 4, 2) + op(6, 1, 4)  # store, add, addq #4, subq #1
    words += op(53, (fill - (len(words) + 1)) & 31, 1) + op(57, 0, 0)  # jr ne,fill / nop
    # the vector: bank 1's r0 to r15 (moveta from bank 0's ... here the current bank is 1, REGPAGE)
    for r in range(16):
        words += movei(rnd.getrandbits(32), r) if rnd.random() < 0.5 else []
    words += movei(io + 4, 20) + movei(io + 8, 21) + movei(mtx, 22) + movei(DATA, 19)
    for _ in range(rnd.randrange(4, 10)):
        width = rnd.randrange(2, 16)
        column = rnd.random() < 0.3
        words += movei(width | (16 if column else 0), 23) + op(47, 20, 23)  # D_MTXC
        words += movei(mtx + 4 * rnd.randrange(0, 64 - width * (width if column else 1) // 1 if not column else 8), 23)
        words += op(47, 21, 23)  # D_MTXA
        for _ in range(rnd.randrange(0, 3)):
            words += random_insn(rnd, ALU)
        rs = rnd.randrange(0, 16 - (width + 1) // 2)
        rd = rnd.choice(POOL)
        words += op(54, rs, rd)  # mmult
        for _ in range(rnd.randrange(0, 4)):
            x = rnd.random()
            if x < 0.3:
                words += op(rnd.choice([0, 10, 11, 4]), rd, rnd.choice(POOL))  # use the result
            elif x < 0.5:
                words += op(41, 19, rnd.choice(POOL))  # a local load
            elif x < 0.6:
                words += op(rnd.choice([18, 20]), rnd.choice(POOL), rnd.choice(POOL))
            else:
                words += random_insn(rnd, ALU)
    words += op(57, 0, 0) * 2 + stop_dsp()
    return words


BLIT = 0xF02200
BLIT_ARGS = ["--mem", "$40000:$40000", "--mem", "$80000:$40000"]


def blit_program(rnd):
    """The blitter: main RAM at $40000 and $80000 filled, then a few blits, each with the registers
    drawn at random (A1 at $40000, A2 at $80000, the pixel sizes, pitches, widths, the pointers and
    their steps and increments, the counts, the data registers, the command's every mode but the
    collision stop), started by B_CMD and waited for on its idle bit, the pointers read back after.
    Now and then the collision stop is enabled, the blit resumed or aborted as the wait finds it
    stopped, and the blitter's interrupt taken (G_RAM + $40) as it ends. The pointers start far
    enough into each window, and move little enough, that every pixel lands within it."""
    base = D_RAM
    words = movei(0, 20) + op(52, 20, 0) + op(57, 0, 0)  # movei #start,r20 / jump (r20) / nop
    words += op(57, 0, 0) * (32 - len(words))
    vector = len(words)  # interrupt 4: movei #isr,r28 / jump (r28) / nop
    words += movei(0, 28) + op(52, 28, 0) + op(57, 0, 0)
    isr = base + 2 * len(words)
    words[vector + 1], words[vector + 2] = isr & 0xFFFF, isr >> 16
    # count, clear the blitter's latch and IMASK, return
    words += movei(D_FLAGS, 30) + op(41, 30, 29) + op(15, 3, 29) + op(14, 13, 29) + op(2, 1, 24)
    words += op(41, 31, 28) + op(2, 2, 28) + op(2, 4, 31) + op(52, 28, 0) + op(47, 30, 29)
    start = base + 2 * len(words)
    words[1], words[2] = start & 0xFFFF, start >> 16
    for r in POOL:
        words += movei(rnd.getrandbits(32), r)
    words += movei(STACK, 20) + op(36, 20, 31)  # bank 0 r31
    if rnd.random() < 0.3:  # the blitter's interrupt
        words += movei(D_FLAGS, 20) + movei(0x4000 | 1 << 8, 25) + op(47, 20, 25)
    for base in (0x40000, 0x80000):  # store r3,(r1) / addq #4,r1 / add r4,r3 / rorq #5,r3 / subq #1,r2
        words += movei(base, 1) + movei(256, 2) + movei(rnd.getrandbits(32), 3) + movei(0x9E3779B9, 4)
        loop = op(47, 1, 3) + op(2, 4, 1) + op(0, 4, 3) + op(29, 5, 3) + op(6, 1, 2)
        words += loop + op(53, -(len(loop) + 1) & 31, 1) + op(57, 0, 0)

    def put(reg, value):  # movei #reg,r20 / movei #value,r25 / store r25,(r20)
        return movei(BLIT + reg, 20) + movei(value & 0xFFFFFFFF, 25) + op(47, 20, 25)

    def signed(lo, hi):
        return rnd.randint(lo, hi) & 0xFFFF

    def flags(mask):
        return (rnd.choice([0, 0, 1, 2, 3]) | rnd.randrange(6) << 3 | rnd.randrange(8) << 6 |
                rnd.randrange(4, 28) << 9 | (0x8000 if mask and rnd.random() < 0.2 else 0) |
                rnd.randrange(4) << 16 | (rnd.random() < 0.5) << 18 | (rnd.random() < 0.15) << 19 |
                (rnd.random() < 0.15) << 20)

    for _ in range(rnd.randrange(1, 4)):
        words += put(0x00, 0x40000) + put(0x04, flags(False))
        words += put(0x08, rnd.randrange(1, 3000) | rnd.randrange(1, 64) << 16)  # A1_CLIP
        words += put(0x0C, rnd.randrange(1500, 1600) | rnd.randrange(20, 30) << 16)  # A1_PIXEL
        words += put(0x10, signed(-48, 48) | rnd.randrange(3) << 16)  # A1_STEP
        words += put(0x14, rnd.getrandbits(32)) + put(0x18, rnd.getrandbits(32))  # A1_FSTEP, A1_FPIXEL
        words += put(0x1C, signed(-8, 8) | signed(-1, 1) << 16)  # A1_INC
        words += put(0x20, rnd.getrandbits(32))  # A1_FINC
        words += put(0x24, 0x80000) + put(0x28, flags(True))
        words += put(0x2C, rnd.getrandbits(32) | 0xF000F000)  # A2_MASK
        words += put(0x30, rnd.randrange(1500, 1600) | rnd.randrange(20, 30) << 16)  # A2_PIXEL
        words += put(0x34, signed(-48, 48) | rnd.randrange(3) << 16)  # A2_STEP
        for reg in range(0x40, 0x78, 4):  # SRCD, DSTD, DSTZ, SRCZ1, SRCZ2, PATD, IINC, ZINC
            if rnd.random() < 0.7:
                words += put(reg, rnd.getrandbits(32))
        for reg in range(0x7C, 0x9C, 4):  # I3 to I0, Z3 to Z0
            if rnd.random() < 0.3:
                words += put(reg, rnd.getrandbits(32))
        words += put(0x3C, rnd.randrange(1, 25) | rnd.randrange(1, 5) << 16)  # B_COUNT
        stop = rnd.random() < 0.3
        words += put(0x78, 4 if stop else 0)  # B_STOP: the collision stop enabled
        cmd = 0
        for b, p in ((0, 0.6), (1, 0.2), (2, 0.2), (3, 0.5), (4, 0.2), (5, 0.2), (6, 0.2), (8, 0.3),
                     (9, 0.5), (10, 0.5), (11, 0.3), (12, 0.2), (13, 0.2), (14, 0.2), (15, 0.2),
                     (16, 0.2), (17, 0.1), (25, 0.2), (26, 0.2), (27, 0.2), (28, 0.2), (29, 0.3), (30, 0.1)):
            cmd |= (rnd.random() < p) << b
        if rnd.random() < 0.3:
            cmd |= rnd.randrange(8) << 18  # ZMODE
        cmd |= rnd.randrange(16) << 21  # LFU
        words += put(0x38, cmd)
        words += movei(BLIT + 0x38, 21) + movei(BLIT + 0x78, 22) + movei(rnd.choice([1, 1, 2]), 27)
        # .wait: load (r21),r26 / btst #1,r26 / jr eq,.run / nop / store r27,(r22) (resume or abort)
        # .run: btst #0,r26 / jr eq,.wait / nop
        wait = op(41, 21, 26) + op(13, 1, 26) + op(53, 2, 2) + op(57, 0, 0) + op(47, 22, 27)
        wait += op(13, 0, 26)
        words += wait + op(53, -(len(wait) + 1) & 31, 2) + op(57, 0, 0)
        for reg in (0x04, 0x18, 0x2C):  # the pointers, at their read addresses
            if rnd.random() < 0.5:
                words += movei(BLIT + reg, 21) + op(41, 21, rnd.choice(POOL))
    words += movei(D_FLAGS, 20) + movei(0x4000, 25) + op(47, 20, 25)  # interrupts off
    words += op(57, 0, 0) * 2 + stop_dsp()
    return words


# classes that write the whole program
PROGRAMS = {
    # MMULT
    "mmult": mmult_program,
    # loops waiting for an interrupt
    "wait": wait_program,
    # Jerry's timers and their interrupts
    "timer": timer_program,
    # Tom's video time base and INT1
    "video": video_program,
    # the object processor and the frames it draws
    "objects": objects_program,
    # the blitter
    "blit": blit_program,
    # loads and stores outside the local space
    "external": external_program,
    # the I2S interrupt
    "irq": irq_program,
    # the I2S interrupt in a loop of forward JR and JUMP
    "irq_jump": lambda rnd: irq_program(rnd, jumps=True),
}


RANDOM_CLASSES = {
    # register to register instructions, no memory, no jumps, no multiply-accumulate or divide
    "alu": alu_body,
    # forward JR and JUMP among them
    "jump": jump_body,
    # long loads and stores to local RAM among them
    "load_store": load_store_body,
    # byte and word ones
    "narrow": narrow_body,
    # the divider and the DSP's I/O registers
    "io": io_body,
}


# --- 68000 programs (--m68k) --------------------------------------------------------------------

M68K_ARGS = ["--m68k", "--mem", "$3f00:$1100"]  # the stack, the program, its data
M68K_BASES = {0: 0x80, 1: 0xc0, 2: 0x100, 3: 0x140}  # a0 to a3 point into the program's data block
M68K_DATA_REGS = ["%d0", "%d1", "%d2", "%d3", "%d4", "%d5"]  # d6 counts loops, d7 indexes


def m68k_ea(rnd, size, writable=True):
    """A memory operand of `size` (b, w, l) through a0 to a3: no odd word, no postincrement or
    predecrement of a byte (the base would go odd); an index is d7, set just before."""
    a = f"%a{rnd.randrange(4)}"
    even = size != "b"
    disp = rnd.randrange(-0x40, 0x40) * (2 if even else 1)
    kinds = ["ind", "disp", "index", "absl", "absw"] + (["post", "pre"] if even else [])
    if not writable:
        kinds += ["pc"]
    k = rnd.choice(kinds)
    pre = []
    if k == "ind":
        ea = f"({a})"
    elif k == "post":
        ea = f"({a})+"
    elif k == "pre":
        ea = f"-({a})"
    elif k == "disp":
        ea = f"{disp}({a})"
    elif k == "index":
        pre = [f"moveq #{rnd.randrange(-32, 32) * 2},%d7"]
        ea = f"{disp // 2 * 2 if even else disp // 2}({a},%d7.{rnd.choice('wl')})"
    elif k == "absl":
        ea = f"(block+{rnd.randrange(0x100) * (2 if even else 1):#x}).l"
    elif k == "absw":
        ea = f"(block+{rnd.randrange(0x100) * (2 if even else 1):#x}).w"
    else:
        ea = f"block+{rnd.randrange(0x40) * 2:#x}(%pc)"
    return pre, ea


def m68k_insn(rnd, depth=0):
    """One instruction, or a few (an index set first, a loop, a branch over one), as source lines."""
    d = lambda: rnd.choice(M68K_DATA_REGS)
    sz = lambda: rnd.choice("bwl")
    imm = lambda size: rnd.getrandbits({"b": 8, "w": 16, "l": 32}[size])
    k = rnd.randrange(30)
    if k < 4:  # MOVE
        size = sz()
        src_kind, dst_kind = rnd.randrange(3), rnd.randrange(2)
        pre = []
        if src_kind == 0:
            src = d()
        elif src_kind == 1:
            src = f"#{imm(size):#x}"
        else:
            pre, src = m68k_ea(rnd, size, writable=False)
        if dst_kind == 0:
            return pre + [f"move.{size} {src},{d()}"]
        pre2, dst = m68k_ea(rnd, size)
        if pre and pre2:
            pre2 = []  # both share d7
            dst = f"(block+{rnd.randrange(0x100) * 2:#x}).l"
        return pre + pre2 + [f"move.{size} {src},{dst}"]
    if k == 4:
        return [f"moveq #{rnd.randrange(-128, 128)},{d()}"]
    if k < 8:  # dyadic, register or memory
        size, opn = sz(), rnd.choice(["add", "sub", "and", "or", "cmp", "eor"])
        if opn in ("cmp",) or (opn != "eor" and rnd.random() < 0.5):
            pre, ea = m68k_ea(rnd, size, writable=False) if rnd.random() < 0.6 else ([], d())
            return pre + [f"{opn}.{size} {ea},{d()}"]
        pre, ea = m68k_ea(rnd, size) if rnd.random() < 0.7 else ([], d())
        return pre + [f"{opn}.{size} {d()},{ea}"]
    if k == 8:  # immediate
        size, opn = sz(), rnd.choice(["addi", "subi", "andi", "ori", "eori", "cmpi"])
        pre, ea = m68k_ea(rnd, size) if rnd.random() < 0.5 else ([], d())
        return pre + [f"{opn}.{size} #{imm(size):#x},{ea}"]
    if k == 9:  # quick
        size, opn = sz(), rnd.choice(["addq", "subq"])
        pre, ea = m68k_ea(rnd, size) if rnd.random() < 0.5 else ([], d())
        return pre + [f"{opn}.{size} #{rnd.randrange(1, 9)},{ea}"]
    if k == 10:  # extended and BCD, between data registers or memory predecremented
        opn = rnd.choice(["addx", "subx", "abcd", "sbcd"])
        name = opn if opn in ("abcd", "sbcd") else f"{opn}.{rnd.choice('wl')}"
        if name != opn and rnd.random() < 0.3:
            a, b = rnd.sample(range(4), 2)
            return [f"{name} -(%a{a}),-(%a{b})"]
        return [f"{name} {d()},{d()}"]
    if k == 11:  # single operand
        size, opn = sz(), rnd.choice(["neg", "negx", "not", "clr", "tst"])
        pre, ea = m68k_ea(rnd, size) if rnd.random() < 0.5 else ([], d())
        return pre + [f"{opn}.{size} {ea}"]
    if k == 12:
        return [rnd.choice([f"ext.w {d()}", f"ext.l {d()}", f"swap {d()}", f"exg {d()},{d()}",
                            f"exg {d()},%a{rnd.randrange(4, 6)}", "nop"])]
    if k < 15:  # multiply and divide
        opn = rnd.choice(["mulu", "muls", "divu", "divs"])
        if opn.startswith("div"):
            if rnd.random() < 0.5:
                return [f"{opn}.w #{rnd.randrange(1, 0x10000):#x},{d()}"]
            return ["ori.w #1,%d5", f"{opn}.w %d5,{d()}"] if rnd.random() < 0.5 else \
                ["move.w #0x8001,%d6", f"{opn}.w %d6,{d()}"]
        r = rnd.random()
        if r < 0.3:
            return [f"{opn}.w #{imm('w'):#x},{d()}"]
        if r < 0.6:
            pre, ea = m68k_ea(rnd, "w", writable=False)
            return pre + [f"{opn}.w {ea},{d()}"]
        return [f"{opn}.w {d()},{d()}"]
    if k < 17:  # shifts and rotates
        opn = rnd.choice(["lsl", "lsr", "asl", "asr", "rol", "ror", "roxl", "roxr"])
        r = rnd.random()
        if r < 0.4:
            return [f"{opn}.{sz()} #{rnd.randrange(1, 9)},{d()}"]
        if r < 0.8:
            return [f"{opn}.{sz()} {d()},{d()}"]
        pre, ea = m68k_ea(rnd, "w")
        return pre + [f"{opn}.w {ea}"]
    if k == 17:  # bit operations
        opn = rnd.choice(["btst", "bset", "bclr", "bchg"])
        src = f"#{rnd.randrange(32)}" if rnd.random() < 0.5 else d()
        if rnd.random() < 0.5:
            return [f"{opn} {src},{d()}"]
        pre, ea = m68k_ea(rnd, "b", writable=opn != "btst")
        if src.startswith("#"):
            src = f"#{rnd.randrange(8)}"
        return pre + [f"{opn} {src},{ea}"]
    if k == 18:  # Scc
        cc = rnd.choice(["t", "f", "hi", "ls", "cc", "cs", "ne", "eq", "vc", "vs", "pl", "mi", "ge", "lt", "gt", "le"])
        pre, ea = m68k_ea(rnd, "b") if rnd.random() < 0.5 else ([], d())
        return pre + [f"s{cc} {ea}"]
    if k == 19:  # LEA, PEA
        pre, ea = m68k_ea(rnd, "l", writable=False)
        if "(%a" not in ea or ")+" in ea or "-(" in ea:
            ea = f"{rnd.randrange(-100, 100)}(%a{rnd.randrange(4)})"
            pre = []
        if rnd.random() < 0.5:
            return pre + [f"lea {ea},%a{rnd.randrange(4, 6)}"]
        return pre + [f"pea {ea}", f"move.l (%sp)+,{d()}"]
    if k == 20:  # MOVEM
        regs = sorted(rnd.sample(range(6), rnd.randrange(1, 5)))
        lst = "/".join(f"%d{r}" for r in regs)
        size = rnd.choice("wl")
        if rnd.random() < 0.5:
            return [f"movem.{size} {lst},-(%sp)", f"movem.{size} (%sp)+,{lst}"]
        a = rnd.randrange(4)
        return [f"movem.{size} {lst},{rnd.randrange(-8, 8) * 4}(%a{a})",
                f"movem.{size} {rnd.randrange(-8, 8) * 4}(%a{a}),{lst}"]
    if k == 21:  # MOVEP
        a, disp = rnd.randrange(4), rnd.randrange(-16, 16) * 2 + 1
        size = rnd.choice("wl")
        return [f"movep.{size} {d()},{disp}(%a{a})"] if rnd.random() < 0.5 else [f"movep.{size} {disp}(%a{a}),{d()}"]
    if k == 22:  # CMPM
        a, b = rnd.sample(range(4), 2)
        return [f"cmpm.{rnd.choice('wl')} (%a{a})+,(%a{b})+"]
    if k == 23:  # the status register
        return [rnd.choice([f"move.w %sr,{d()}", f"move.w {d()},%ccr", f"andi.b #{rnd.getrandbits(5)},%ccr",
                            f"ori.b #{rnd.getrandbits(5)},%ccr", f"eori.b #{rnd.getrandbits(5)},%ccr",
                            f"move.w %sr,{m68k_ea(rnd, 'w')[1] if False else '(%a0)'}"])]
    if k == 24:  # the other memories: the ROM, MEMCON1 and MEMCON2, Jerry's joystick ports
        src = rnd.choice([f"({0xe00000 + 2 * rnd.randrange(64):#x}).l", "(0xf00000).l", "(0xf00002).l",
                          "(0xf14000).l", "(0xf14002).l"])
        return [f"move.{rnd.choice('wl')} {src},{d()}"]
    if k < 27 and depth == 0:  # a branch over one instruction, taken or not
        cc = rnd.choice(["ra", "hi", "ls", "cc", "cs", "ne", "eq", "pl", "mi", "ge", "lt", "gt", "le"])
        return [f"b{cc}.{rnd.choice('sw')} 9f"] + m68k_insn(rnd, 1) + ["9:"]
    if k == 27 and depth == 0:  # a loop
        cc = rnd.choice(["ra", "eq", "ne", "mi", "pl"])
        body = sum((m68k_insn(rnd, 1) for _ in range(rnd.randrange(1, 3))), [])
        return [f"moveq #{rnd.randrange(0, 4)},%d6", "8:"] + body + [f"db{cc} %d6,8b"]
    if k == 28 and depth == 0:
        return [rnd.choice(["bsr.w sub", "jsr sub", "jsr (sub).l", "lea sub,%a4", "jsr (%a4)"][:3])]
    if k == 29:
        pre, ea = m68k_ea(rnd, "b")
        return pre + [f"tas {ea}"]
    return ["nop"]


def m68k_source(rnd, irq=False):
    """A random 68000 program: the registers drawn, then instructions of every kind but those that
    trap, on the program's own block of random data; the registers and SR stored after it. With
    `irq`, Tom's PIT interrupts it all along, at a period drawn: the handler acknowledges in INT1
    and INT2 and counts after the stored registers."""
    out = ["\t.text"]
    if irq:
        out += ["\tmove.l #handler,0x100", "\tmove.w #1,0xf00050",
                f"\tmove.w #{rnd.randrange(100, 400)},0xf00052", "\tmove.w #8,0xf000e0", "\tmove.w #0x2000,%sr"]
    for r in range(8):
        out.append(f"\tmove.l #{rnd.getrandbits(32):#x},%d{r}")
    for a, base in M68K_BASES.items():
        out.append(f"\tlea block+{base:#x},%a{a}")
    for _ in range(rnd.randrange(40, 80)):
        out += ["\t" + line if not line.endswith(":") else line for line in m68k_insn(rnd)]
    out += ["\tmovem.l %d0-%d7/%a0-%a6,dump", "\tmove.w %sr,dump+60", "\tstop #0x2700",
            "sub:"] + ["\t" + line for line in m68k_insn(rnd, 1) if not line.endswith(":")] + ["\trts", "\t.even"]
    if irq:
        out += ["handler:\tmove.w #0x0808,0xf000e0", "\tmove.w #0,0xf000e2", "\taddq.l #1,dump+64", "\trte"]
    words = [f"{rnd.getrandbits(32):#x}" for _ in range(128)]
    out += [f"block:\t.long {','.join(words[i:i + 8])}" if i == 0 else f"\t.long {','.join(words[i:i + 8])}"
            for i in range(0, 128, 8)]
    out += ["dump:\t.space 68"]
    return "\n".join(out) + "\n"


M68K_GPU_CLASSES = ["alu", "jump", "load_store", "narrow", "mmult", "blit"]
M68K_GPU_FLAG = 0x8000
M68K_DSP_CLASSES = ["alu", "jump", "load_store", "narrow", "io", "timer"]


def m68k_gpu_source(rnd, seed, dsp=False):
    """The 68000 sets the GPU going on a generated program of one of M68K_GPU_CLASSES (copied into the
    GPU's RAM through its slave port, G_PC and G_CTRL written), runs generated instructions of its
    own meanwhile, then waits for the GPU's word in main RAM that it is stopping and reads G_CTRL
    until GO is clear (what G_CTRL's high half shows while the GPU runs is not modelled)."""
    global STOP_FLAG
    cls = rnd.choice(M68K_DSP_CLASSES if dsp else M68K_GPU_CLASSES)
    STOP_FLAG = M68K_GPU_FLAG
    try:
        image = random_program(cls if dsp else "gpu_" + cls, seed)
    finally:
        STOP_FLAG = None
    io = 0xF1A100 if dsp else 0xF02100
    address, size = struct.unpack(">II", image[4:12])
    data = image[12:12 + size]
    data += b"\0" * (-len(data) % 4)
    longs = [int.from_bytes(data[i:i + 4], "big") for i in range(0, len(data), 4)]
    out = ["\t.text", "\tlea gpucode,%a0", f"\tlea {address:#x},%a1", f"\tmove.w #{len(longs) - 1},%d0",
           "0:\tmove.l (%a0)+,(%a1)+", "\tdbra %d0,0b", f"\tmove.l #0x4000,{io:#x}", f"\tmove.l #{address:#x},{io + 0x10:#x}",
           f"\tmove.l #1,{io + 0x14:#x}"]  # FLAGS REGPAGE, as the simulation's boot program sets it
    for r in range(8):
        out.append(f"\tmove.l #{rnd.getrandbits(32):#x},%d{r}")
    for a, base in M68K_BASES.items():
        out.append(f"\tlea block+{base:#x},%a{a}")
    for _ in range(rnd.randrange(20, 50)):
        out += ["\t" + line if not line.endswith(":") else line for line in m68k_insn(rnd)]
    out += [f"1:\ttst.l ({M68K_GPU_FLAG:#x}).l", "\tbeq.s 1b", "\tmoveq #7,%d0", "2:\tdbra %d0,2b",
            ] + ([] if dsp else ["3:\tmove.l 0xf02114,%d0", "\tbtst #0,%d0", "\tbne.s 3b"]) + [
            "\tmovem.l %d0-%d7/%a0-%a6,dump", "\tmove.w %sr,dump+60", "\tstop #0x2700",
            "sub:"] + ["\t" + line for line in m68k_insn(rnd, 1) if not line.endswith(":")] + ["\trts", "\t.even"]
    out += [f"gpucode:\t.long {','.join(f'{v:#x}' for v in longs[i:i + 8])}" if i == 0 else
            f"\t.long {','.join(f'{v:#x}' for v in longs[i:i + 8])}" for i in range(0, len(longs), 8)]
    words = [f"{rnd.getrandbits(32):#x}" for _ in range(128)]
    out += [f"block:\t.long {','.join(words[i:i + 8])}" if i == 0 else f"\t.long {','.join(words[i:i + 8])}"
            for i in range(0, 128, 8)]
    out += ["dump:\t.space 68"]
    return "\n".join(out) + "\n", cls


# The GPU's classes (gpu_<class>) are the DSP's moved to the GPU's local space and registers, with
# the GPU's opcodes where the DSP has others: SAT8, SAT16, SAT24 and PACK/UNPACK for SUBQMOD,
# SAT16S, ADDQMOD and MIRROR, no LOADP or STOREP (SAT32S, MIRROR) among the ALU instructions.
GPU_CLASSES = {"alu", "jump", "load_store", "narrow", "io", "external", "mmult", "timer", "video", "objects",
               "blit"}


def use_chip(gpu):
    """Points the generator at the GPU's addresses and opcodes, or back at the DSP's."""
    global D_RAM, D_CTRL, DATA, D_FLAGS, D_REMAIN, D_MACHI, D_MOD, STACK, ALU, SLOT, MAC
    global JPIT1, JPIT_READ, JPIT_READ2, TIMER_COUNT
    JPIT1, JPIT_READ, JPIT_READ2, TIMER_COUNT = (0xF00050, 0xF00050, 0xF00052, 1) if gpu else \
        (0xF10000, 0xF10036, 0xF1003A, 2)
    base, io = (0xF03000, 0xF02100) if gpu else (0xF1B000, 0xF1A100)
    D_RAM, D_CTRL, D_FLAGS, D_REMAIN, D_MOD = base, io + 0x14, io, io + 0x1C, io + 0x18
    D_MACHI = io + 0x18 if gpu else io + 0x20  # the GPU reads G_HIDATA back instead
    DATA = base + (0x800 if gpu else 0x1000)
    STACK = base + (0xF00 if gpu else 0x1F00)
    ALU = dict(ALU_DSP)
    MAC = dict(MAC_DSP)
    if gpu:
        del ALU[42], ALU[48]
        ALU.update({32: "r", 62: "r", 63: "pk"})
        del MAC[32], MAC[63]
    SLOT = {k: v for k, v in ALU.items() if v != "i"}


def random_program(cls, seed):
    rnd = random.Random(f"{cls}:{seed}")
    if cls in ("m68k", "m68k_irq", "m68k_gpu", "m68k_dsp"):
        with tempfile.TemporaryDirectory() as work:
            src = Path(work) / f"random_{cls}_{seed:04d}.s"
            if cls in ("m68k_gpu", "m68k_dsp"):
                src.write_text(m68k_gpu_source(rnd, seed, dsp=cls == "m68k_dsp")[0])
            else:
                src.write_text(m68k_source(rnd, irq=cls == "m68k_irq"))
            return assemble_m68k(BINUTILS, src, Path(work))[0]
    gpu = cls.startswith("gpu_")
    use_chip(gpu)
    name = cls[4:] if gpu else cls
    try:
        if name in PROGRAMS:
            return bs94(D_RAM, PROGRAMS[name](rnd))
        words = []
        for r in POOL:
            words += movei(rnd.getrandbits(32), r)
        words += RANDOM_CLASSES[name](rnd, D_RAM + 2 * len(words))
        words += op(57, 0, 0) * 2 + stop_dsp()
        return bs94(D_RAM, words)
    finally:
        use_chip(False)


# --- asm programs ---------------------------------------------------------------------------

def run_args(source):
    args = []
    for line in source.read_text(errors="replace").splitlines():
        m = re.match(r"^\s*[;|]\s*RUN\s*:\s*(.*)$", line)
        if m:
            args += m.group(1).split(";")[0].split()
    if "--wav" in args:  # the FPGA's I2S runs without it; the core has no wave output
        i = args.index("--wav")
        del args[i:i + (2 if i + 1 < len(args) and not args[i + 1].startswith("--") else 1)]
    if "--cycles" in args:
        i = args.index("--cycles")
        args[i + 1] = str(min(int(args[i + 1], 0), CYCLE_CAP))
    else:
        args += ["--cycles", str(CYCLE_CAP)]
    return args


def program_args(run):
    """A tests/programs/*.run: the binary and the arguments, --load paths made relative to
    tests/reference, where the runner and jagsim resolve them."""
    program, args = None, []
    for line in run.read_text().splitlines():
        m = re.match(r"^\s*;\s*(PROGRAM|RUN)\s*:\s*(.*)$", line)
        if m and m.group(1) == "PROGRAM":
            program = PROGRAMS_DIR / m.group(2).strip()
        elif m:
            args += m.group(2).split()
    for i, a in enumerate(args):
        if i > 0 and args[i - 1] == "--load":
            path, at = a.rsplit("@", 1)
            args[i] = f"../programs/{path}@{at}"
    return program, args


def assemble_m68k(binutils, src, work):
    """A tests/m68k program as BS94 at M68K_ORG, and its cartridge (its section .cart) or None."""
    elf, bin_, cart = work / f"{src.stem}.elf", work / f"{src.stem}.bin", work / f"{src.stem}.cart"
    for cmd in ([f"{binutils}as", "-m68000", "-o", str(work / f"{src.stem}.obj"), str(src)],
                [f"{binutils}ld", f"-Ttext={M68K_ORG:#x}", f"--section-start=.cart={CART_ORG:#x}",
                 "-o", str(elf), str(work / f"{src.stem}.obj")],
                [f"{binutils}objcopy", "-O", "binary", "-R", ".cart", str(elf), str(bin_)],
                [f"{binutils}objcopy", "-O", "binary", "-j", ".cart", str(elf), str(cart)]):
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode != 0:
            sys.exit(f"{src.name}: {cmd[0]} failed\n{res.stdout}{res.stderr}")
    data = bin_.read_bytes()
    data += b"\0" * (-len(data) % 4)
    cart_data = cart.read_bytes() if cart.exists() and cart.stat().st_size else None
    return b"BS94" + struct.pack(">II", M68K_ORG, len(data)) + data, cart_data


def record(sim, name, binary, args):
    obj = REF / f"{name}.o"
    obj.write_bytes(binary)
    trace = REF / f"{name}.trace"
    res = subprocess.run([str(sim), str(obj)] + args + ["--log", str(trace)],
                         capture_output=True, text=True, cwd=REF)
    if res.returncode != 0:
        sys.exit(f"{name}: jagsim failed\n{res.stdout}{res.stderr}")
    (REF / f"{name}.args").write_text(" ".join(args) + "\n")
    (REF / f"{name}.state").write_text(res.stdout)
    finished = "finished: 1" in res.stdout
    print(f"{name}: {'finished' if finished else 'limit'}, {sum(1 for _ in open(trace))} trace lines")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sim", default=str(ROOT / "sim" / "obj_dir" / "jagsim"))
    ap.add_argument("--lyxass", default=str(ROOT.parent / "lyxass" / "lyxass"))
    ap.add_argument("--binutils", default="m68k-elf-", help="the prefix of the 68000 binutils")
    ap.add_argument("--random", action="append", default=[], help="CLASS:FIRST:COUNT")
    ap.add_argument("--no-asm", action="store_true")
    ap.add_argument("--out", help="the directory recorded to (default: tests/reference)")
    ap.add_argument("names", nargs="*", help="asm tests to record (default: all)")
    args = ap.parse_args()
    global REF, BINUTILS
    BINUTILS = args.binutils
    if args.out:
        REF = Path(args.out).resolve()
    REF.mkdir(parents=True, exist_ok=True)

    if not args.no_asm:
        sources = [ASM / f"{n}.js" for n in args.names if (ASM / f"{n}.js").exists()] if args.names else sorted(ASM.glob("*.js"))
        for src in sources:
            obj = REF / f"{src.stem}.o"
            asm = subprocess.run([args.lyxass, "-w", "-o", str(obj), src.name], cwd=ASM,
                                 capture_output=True, text=True)
            if asm.returncode != 0 or not obj.exists():
                sys.exit(f"{src.name}: lyxass failed\n{asm.stdout}{asm.stderr}")
            record(args.sim, src.stem, obj.read_bytes(), run_args(src))

    if not args.no_asm:
        sources = [M68K / f"{n}.s" for n in args.names if (M68K / f"{n}.s").exists()] if args.names else sorted(M68K.glob("*.s"))
        with tempfile.TemporaryDirectory() as work:
            for src in sources:
                binary, cart = assemble_m68k(args.binutils, src, Path(work))
                extra = []
                if cart is not None:
                    (REF / f"{src.stem}.cart").write_bytes(cart)
                    extra = ["--cart", f"{src.stem}.cart"]
                record(args.sim, src.stem, binary, run_args(src) + extra)

    if not args.no_asm:
        runs = [PROGRAMS_DIR / f"{n}.run" for n in args.names] if args.names else sorted(PROGRAMS_DIR.glob("*.run"))
        for run in runs:
            if not run.exists():
                continue
            program, run_list = program_args(run)
            raw = program.read_bytes()
            if raw[:4] == b"BS94":  # loaded where it says
                record(args.sim, run.stem, raw, run_list)
                continue
            words = [int.from_bytes(raw[i:i + 2].ljust(2, b"\0"), "big") for i in range(0, len(raw), 2)]
            record(args.sim, run.stem, bs94(D_RAM, words), run_list)

    for spec in args.random:
        cls, first, count = spec.split(":")
        for seed in range(int(first), int(first) + int(count)):
            extra = ["--gpu"] if cls.startswith("gpu_") else []
            if cls.endswith("external"):
                extra += EXT_ARGS
            if cls.endswith("blit"):
                extra += BLIT_ARGS
            if cls.startswith("m68k"):
                extra += M68K_ARGS
            if cls == "m68k_gpu":
                extra += BLIT_ARGS
            record(args.sim, f"random_{cls}_{seed:04d}", random_program(cls, seed),
                   ["--cycles", str(CYCLE_CAP)] + extra)


if __name__ == "__main__":
    main()
