#include "jag/RiscDisassembler.hpp"

#include <array>
#include <cstdio>
#include <string_view>

namespace jag
{

namespace
{

// The mnemonics by opcode, the DSP's (scripts/disasm.py) and the GPU's where they differ
constexpr std::array<std::string_view, 64> DSP_NAMES{
  "add",    "addc",   "addq",   "addqt",  "sub",    "subc",  "subq",  "subqt",   "neg",    "and",    "or",
  "xor",    "not",    "btst",   "bset",   "bclr",   "mult",  "imult", "imultn",  "resmac", "imacn",  "div",
  "abs",    "sh",     "shlq",   "shrq",   "sha",    "sharq", "ror",   "rorq",    "cmp",    "cmpq",   "subqmod",
  "sat16s", "move",   "moveq",  "moveta", "movefa", "movei", "loadb", "loadw",   "load",   "sat32s", "load",
  "load",   "storeb", "storew", "store",  "mirror", "store", "store", "move",    "jump",   "jr",     "mmult",
  "mtoi",   "normi",  "nop",    "load",   "load",   "store", "store", "illegal", "addqmod"
};

std::string_view name( bool gpu, std::uint32_t op )
{
  if ( gpu )
    switch ( op )
    {
    case 32:
      return "sat8";
    case 33:
      return "sat16";
    case 42:
      return "loadp";
    case 48:
      return "storep";
    case 62:
      return "sat24";
    case 63:
      return "pack";
    default:
      break;
    }
  return DSP_NAMES[op];
}

std::string condition( std::uint32_t cc )
{
  switch ( cc )
  {
  case 0:
    return "";
  case 1:
    return "ne,";
  case 2:
    return "eq,";
  case 4:
    return "cc,";
  case 8:
    return "cs,";
  case 20:
    return "pl,";
  case 24:
    return "mi,";
  case 5:
    return "hi,";
  case 10:
    return "cseq,";
  default:
    return std::to_string( cc ) + "?,";
  }
}

std::string format( char const* pattern, auto... values )
{
  char text[64];
  std::snprintf( text, sizeof text, pattern, values... );
  return text;
}

} // namespace

RiscInstruction
disassembleRisc( bool gpu, std::uint32_t address, std::function<std::uint16_t( std::uint32_t )> const& word )
{
  std::uint16_t const w = word( 0 );
  std::uint32_t const op = w >> 10, s = ( w >> 5 ) & 31u, d = w & 31u;
  std::string const n{ name( gpu, op ) };
  auto const pad = [&n]( std::string const& operands ) { return operands.empty() ? n : n + ' ' + operands; };
  int const sq = static_cast<int>( s ^ 16u ) - 16; // s as a signed five bit number
  switch ( op )
  {
  case 0:
  case 1:
  case 4:
  case 5:
  case 9:
  case 10:
  case 11:
  case 16:
  case 17:
  case 18:
  case 20:
  case 21:
  case 23:
  case 26:
  case 28:
  case 30:
  case 34:
  case 36:
  case 37:
  case 54:
  case 55:
  case 56:
    return { pad( format( "r%u,r%u", s, d ) ) };
  case 8:
  case 12:
  case 19:
  case 22:
    return { pad( format( "r%u", d ) ) };
  case 51:
    return { format( "move pc,r%u", d ) };
  case 33:
  case 42:
  case 48:
    if ( gpu && op == 42 )
      return { pad( format( "(r%u),r%u", s, d ) ) };
    if ( gpu && op == 48 )
      return { pad( format( "r%u,(r%u)", d, s ) ) };
    return { pad( format( "r%u", d ) ) };
  case 32:
  case 62:
  case 63:
    if ( gpu )
      return { op == 63 ? std::string( s & 1u ? "unpack" : "pack" ) + format( " r%u", d ) : pad( format( "r%u", d ) ) };
    if ( op == 62 )
      return { n };
    return { pad( format( "#%u,r%u", s != 0 ? s : 32u, d ) ) };
  case 2:
  case 3:
  case 6:
  case 7:
  case 25:
  case 27:
  case 29:
    return { pad( format( "#%u,r%u", s != 0 ? s : 32u, d ) ) };
  case 24:
    return { pad( format( "#%u,r%u", 32u - s, d ) ) };
  case 13:
  case 14:
  case 15:
  case 35:
    return { pad( format( "#%u,r%u", s, d ) ) };
  case 31:
    return { pad( format( "#%d,r%u", sq, d ) ) };
  case 38:
    return { pad( format( "#$%08x,r%u", static_cast<unsigned>( word( 1 ) | word( 2 ) << 16 ), d ) ), 3 };
  case 39:
  case 40:
  case 41:
    return { pad( format( "(r%u),r%u", s, d ) ) };
  case 43:
  case 44:
    return { pad( format( "(r%u+%u),r%u", 14 + op - 43, ( s != 0 ? s : 32u ) * 4, d ) ) };
  case 45:
  case 46:
  case 47:
    return { pad( format( "r%u,(r%u)", d, s ) ) };
  case 49:
  case 50:
    return { pad( format( "r%u,(r%u+%u)", d, 14 + op - 49, ( s != 0 ? s : 32u ) * 4 ) ) };
  case 58:
  case 59:
    return { pad( format( "(r%u+r%u),r%u", 14 + op - 58, s, d ) ) };
  case 60:
  case 61:
    return { pad( format( "r%u,(r%u+r%u)", d, 14 + op - 60, s ) ) };
  case 52:
    return { pad( condition( d ) + format( "(r%u)", s ) ) };
  case 53:
    return { pad( condition( d ) +
                  format( "$%06x", ( address + 2 + static_cast<std::uint32_t>( sq * 2 ) ) & 0xffffffu ) ) };
  default:
    return { n };
  }
}

} // namespace jag
