// Moira's compile-time configuration, supplied by its client as Moira intends; upstream's own
// MoiraConfig.h is not carried in libextern/Moira (libextern/README.md). These are PGMEmu's
// settings, whose 68000 this is (docs/decisions/0013-the-68000-is-moira.md).

#pragma once

// Proves which MoiraConfig.h was found: src/jag_core/src/M68000.cpp fails the build without it.
#define JAG_MOIRA_CONFIG 1

// sync() is called around every bus access, so that the machine is stepped up to each access
// and its wait states charged where the 68000 makes it.
#define MOIRA_PRECISE_TIMING true

// The client interface is a set of virtual functions.
#define MOIRA_VIRTUAL_API true

// A word or long access at an odd address raises an address error, as on the 68000.
#define MOIRA_EMULATE_ADDRESS_ERROR true

// Function codes: the 68000 puts them on its pins (the trace shows them) and stacks them in an
// address error's frame.
#define MOIRA_EMULATE_FC true

// The 68020 instruction cache does not exist on a 68000.
#define MOIRA_EMULATE_ICACHE false

// The disassembler, for the core's view of the instructions.
#define MOIRA_ENABLE_DASM true

// Musashi compatibility is for Moira's own test runner; the 68000 is wanted.
#define MOIRA_MIMIC_MUSASHI false

// The instructions whose execution the machine is told about: those whose bus cycles the 68000
// makes otherwise than Moira (src/jag_core/src/M68000.cpp), and upstream's.
#define MOIRA_WILL_EXECUTE                                                                          \
  I == Instr::STOP || I == Instr::TAS || I == Instr::BKPT || ( I >= Instr::DBCC && I <= Instr::DBT ) ||  \
      I == Instr::ANDICCR || I == Instr::ANDISR || I == Instr::ORICCR || I == Instr::ORISR ||             \
      I == Instr::EORICCR || I == Instr::EORISR || I == Instr::MOVETCCR || I == Instr::MOVETSR ||        \
      I == Instr::ADD || I == Instr::SUB || I == Instr::ADDQ || I == Instr::SUBQ || I == Instr::ADDI ||   \
      I == Instr::SUBI || I == Instr::ADDX || I == Instr::SUBX || I == Instr::MOVEM || I == Instr::JSR ||  \
      I == Instr::ROXL || I == Instr::ROXR

#define MOIRA_DID_EXECUTE I == Instr::RESET || I == Instr::TAS || I == Instr::ROXL || I == Instr::ROXR
