#pragma once

// What the RISC's translation units share: the opcodes, the addresses of the local spaces, the
// scoreboard's write-back priorities.

#include <array>
#include <cstdint>

namespace jag::risc
{

// Opcodes of the DSP (SoftRef, "DSP Instruction Set"); the GPU's six that differ are below.
enum : std::uint8_t
{
  ADD = 0,
  ADDC,
  ADDQ,
  ADDQT,
  SUB,
  SUBC,
  SUBQ,
  SUBQT,
  NEG,
  AND,
  OR,
  XOR,
  NOT,
  BTST,
  BSET,
  BCLR,
  MULT,
  IMULT,
  IMULTN,
  RESMAC,
  IMACN,
  DIV,
  ABS,
  SH,
  SHLQ,
  SHRQ,
  SHA,
  SHARQ,
  ROR,
  RORQ,
  CMP,
  CMPQ,
  SUBQMOD,
  SAT16S,
  MOVE,
  MOVEQ,
  MOVETA,
  MOVEFA,
  MOVEI,
  LOADB,
  LOADW,
  LOAD,
  SAT32S,
  LOAD14N,
  LOAD15N,
  STOREB,
  STOREW,
  STORE,
  MIRROR,
  STORE14N,
  STORE15N,
  MOVEPC,
  JUMP,
  JR,
  MMULT,
  MTOI,
  NORMI,
  NOP,
  LOAD14R,
  LOAD15R,
  STORE14R,
  STORE15R,
  ILLEGAL62,
  ADDQMOD
};

// The registers, as offsets from the chip's: D_FLAGS $F1A100 (the DSP), G_FLAGS $F02100 (the GPU).
constexpr std::uint32_t REG_FLAGS = 0x00;
constexpr std::uint32_t REG_MTXC = 0x04;
constexpr std::uint32_t REG_MTXA = 0x08;
constexpr std::uint32_t REG_PC = 0x10;
constexpr std::uint32_t REG_CTRL = 0x14;
constexpr std::uint32_t REG_MOD = 0x18;    // D_MOD; the GPU's G_HIDATA
constexpr std::uint32_t REG_REMAIN = 0x1c; // read: remainder, write: DIVCTRL
constexpr std::uint32_t REG_MACHI = 0x20;  // the DSP's
constexpr std::uint32_t REG_LTXD = 0x48;   // the DSP's I2S interface
constexpr std::uint32_t REG_RTXD = 0x4c;
constexpr std::uint32_t REG_SCLK = 0x50;
constexpr std::uint32_t REG_SMODE = 0x54;

// The DSP's local space: f18000 to f1ffff (gpu_mem.v localaddr with JERRY), its RAM, its
// registers, the wave table ROM, 1024 words (j_sinerom.v).
constexpr std::uint32_t DSP_RAM = 0xf1b000;
constexpr std::uint32_t DSP_RAM_SIZE = 0x2000;
constexpr std::uint32_t DSP_IO = 0xf1a100;
constexpr std::uint32_t DSP_IO_END = 0xf1a160;
constexpr std::uint32_t ROM_BASE = 0xf1d000;

// The blitter's registers in the GPU's local space (gpu_mem.v bliten)
constexpr std::uint32_t BLITTER_BASE = 0xf02200;
constexpr std::array<std::uint16_t, 1024> WAVE_ROM = {
#include "WaveRom.inc"
};
// The GPU's: f02000 to f07fff, its RAM, its registers.
constexpr std::uint32_t GPU_RAM = 0xf03000;
constexpr std::uint32_t GPU_RAM_SIZE = 0x1000;
constexpr std::uint32_t GPU_IO = 0xf02100;
constexpr std::uint32_t GPU_IO_END = 0xf02120;

constexpr std::uint32_t REGPAGE = 1u << 14;
constexpr std::uint32_t IMASK = 1u << 3;

// The GPU's opcodes where the DSP has others (ra6032a.v with JERRY 0).
enum : std::uint8_t
{
  SAT8 = SUBQMOD,
  SAT16 = SAT16S,
  LOADP = SAT32S,
  STOREP = MIRROR,
  SAT24 = ILLEGAL62,
  PACK = ADDQMOD // UNPACK with the source field odd
};

// Write-back sources in the scoreboard's order of priority (sboard.v, *_wbackh): the first one
// present takes the destination port, the second the source port.
constexpr bool isMemory( std::uint32_t op, bool gpu )
{
  return ( op >= LOADB && op <= LOAD ) || op == LOAD14N || op == LOAD15N || ( op >= STOREB && op <= STORE ) ||
         op == STORE14N || op == STORE15N || ( op >= LOAD14R && op <= STORE15R ) ||
         ( gpu && ( op == LOADP || op == STOREP ) );
}

constexpr bool isStore( std::uint32_t op, bool gpu )
{
  return op == STOREB || op == STOREW || op == STORE || op == STORE14N || op == STORE15N || op == STORE14R ||
         op == STORE15R || ( gpu && op == STOREP );
}

constexpr std::uint8_t ALU_PRIORITY = 0;
constexpr std::uint8_t MOV_PRIORITY = 1;
constexpr std::uint8_t IMM_PRIORITY = 2;
constexpr std::uint8_t ILD_PRIORITY = 3;
constexpr std::uint8_t XLD_PRIORITY = 4;
constexpr std::uint8_t DIV_PRIORITY = 5;

} // namespace jag::risc
