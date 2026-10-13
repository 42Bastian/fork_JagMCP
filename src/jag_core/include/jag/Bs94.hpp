#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace jag
{

// lyxass's output: "BS94", the load address and the length, big endian, then the bytes.
struct Bs94
{
  std::uint32_t address = 0;
  std::vector<std::uint8_t> data;
};

// Throws std::runtime_error when `file` is not a BS94 image.
Bs94 parseBs94( std::span<std::uint8_t const> file );

} // namespace jag
