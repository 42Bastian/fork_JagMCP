#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace jag
{

// One instruction of a RISC as text, the GPU's or the DSP's (they differ in a few opcodes): its
// mnemonic and operands, and how many words it takes (MOVEI three). `word(n)` reads the n-th word
// from the instruction's address; `address` is that address, for JR's target.
struct RiscInstruction
{
  std::string text;
  std::uint32_t words = 1;
};

RiscInstruction
disassembleRisc( bool gpu, std::uint32_t address, std::function<std::uint16_t( std::uint32_t )> const& word );

} // namespace jag
