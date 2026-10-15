#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace jag
{

// A program file, as the sections to load and the address it starts at. Two formats are read, told
// apart by their first bytes:
// - lyxass's BS94: "BS94", the load address and the length, big endian, then the bytes. It starts
//   at its load address.
// - COFF, as Atari's linkers (aln, rln) write it: magic $0150. Each section with bytes in the file
//   (.text, .data) is loaded at its physical address. .bss has no bytes and is not loaded. It
//   starts at the entry point of its a.out header, or at its first section without one.
struct ProgramFile
{
  struct Section
  {
    std::uint32_t address = 0;
    std::vector<std::uint8_t> data;
  };

  std::vector<Section> sections;
  std::uint32_t entry = 0;
  bool coff = false;
};

// Whether `file` starts as a COFF file does.
bool isCoff( std::span<std::uint8_t const> file );

// Throws std::runtime_error when `file` is neither format, or is cut short.
ProgramFile parseProgramFile( std::span<std::uint8_t const> file );

} // namespace jag
