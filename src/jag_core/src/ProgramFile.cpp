#include "jag/ProgramFile.hpp"

#include <cstring>
#include <stdexcept>

namespace jag
{

namespace
{

constexpr std::uint16_t COFF_MAGIC = 0x0150;
constexpr std::size_t COFF_HEADER = 20;
constexpr std::size_t AOUT_HEADER = 28; // the a.out header, with the entry point at 16
constexpr std::size_t SECTION_HEADER = 40;
constexpr std::uint32_t STYP_BSS = 0x80;

std::uint32_t be16( std::span<std::uint8_t const> file, std::size_t at )
{
  return static_cast<std::uint32_t>( file[at] ) << 8 | file[at + 1];
}

std::uint32_t be32( std::span<std::uint8_t const> file, std::size_t at )
{
  return be16( file, at ) << 16 | be16( file, at + 2 );
}

ProgramFile parseBs94( std::span<std::uint8_t const> file )
{
  ProgramFile result;
  result.entry = be32( file, 4 );
  result.sections.push_back( { .address = result.entry, .data = { file.begin() + 12, file.end() } } );
  return result;
}

ProgramFile parseCoff( std::span<std::uint8_t const> file )
{
  std::size_t const count = be16( file, 2 );
  std::size_t const optional = be16( file, 16 );
  std::size_t const sections = COFF_HEADER + optional;
  if ( file.size() < sections + ( count * SECTION_HEADER ) )
    throw std::runtime_error( "a COFF file cut short in its headers" );
  ProgramFile result;
  for ( std::size_t i = 0; i < count; ++i )
  {
    std::size_t const at = sections + ( i * SECTION_HEADER );
    std::uint32_t const address = be32( file, at + 8 ); // s_paddr
    std::uint32_t const size = be32( file, at + 16 );
    std::uint32_t const offset = be32( file, at + 20 );
    std::uint32_t const flags = be32( file, at + 36 );
    if ( ( flags & STYP_BSS ) != 0 || size == 0 )
      continue;
    if ( offset > file.size() || size > file.size() - offset )
      throw std::runtime_error( "a COFF file cut short in its sections" );
    result.sections.push_back(
        { .address = address, .data = { file.begin() + offset, file.begin() + offset + size } } );
  }
  if ( result.sections.empty() )
    throw std::runtime_error( "a COFF file with nothing to load" );
  result.coff = true;
  result.entry = optional >= AOUT_HEADER ? be32( file, COFF_HEADER + 16 ) : result.sections.front().address;
  return result;
}

} // namespace

bool isCoff( std::span<std::uint8_t const> file )
{
  return file.size() >= COFF_HEADER && be16( file, 0 ) == COFF_MAGIC;
}

ProgramFile parseProgramFile( std::span<std::uint8_t const> file )
{
  if ( file.size() >= 12 && std::memcmp( file.data(), "BS94", 4 ) == 0 )
    return parseBs94( file );
  if ( isCoff( file ) )
    return parseCoff( file );
  throw std::runtime_error( "not a BS94 or COFF file" );
}

} // namespace jag
