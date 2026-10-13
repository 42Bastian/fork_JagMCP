#include "jag/Bs94.hpp"

#include <cstring>
#include <stdexcept>

namespace jag
{

Bs94 parseBs94( std::span<std::uint8_t const> file )
{
  if ( file.size() < 12 || std::memcmp( file.data(), "BS94", 4 ) != 0 )
    throw std::runtime_error( "not a BS94 file" );
  auto const be32 = [&]( std::size_t at )
  {
    return static_cast<std::uint32_t>( file[at] ) << 24 | static_cast<std::uint32_t>( file[at + 1] ) << 16 |
           static_cast<std::uint32_t>( file[at + 2] ) << 8 | file[at + 3];
  };
  Bs94 result;
  result.address = be32( 4 );
  result.data.assign( file.begin() + 12, file.end() );
  return result;
}

} // namespace jag
