#include "Methods.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <string>

namespace jag::control
{

namespace
{

// PNG's CRC-32 (ISO 3309), and zlib's Adler-32.
std::uint32_t crc32( std::string_view data, std::uint32_t crc = 0 )
{
  static auto const TABLE = []
  {
    std::array<std::uint32_t, 256> table{};
    for ( std::uint32_t n = 0; n < 256; ++n )
    {
      std::uint32_t c = n;
      for ( int k = 0; k < 8; ++k )
        c = ( c & 1u ) != 0 ? 0xedb88320u ^ ( c >> 1 ) : c >> 1;
      table[n] = c;
    }
    return table;
  }();
  crc = ~crc;
  for ( char const ch : data )
    crc = TABLE[( crc ^ static_cast<std::uint8_t>( ch ) ) & 0xffu] ^ ( crc >> 8 );
  return ~crc;
}

std::uint32_t adler32( std::string_view data )
{
  std::uint32_t a = 1;
  std::uint32_t b = 0;
  for ( char const ch : data )
  {
    a = ( a + static_cast<std::uint8_t>( ch ) ) % 65521u;
    b = ( b + a ) % 65521u;
  }
  return b << 16 | a;
}

void putBigEndian( std::string& out, std::uint32_t v )
{
  for ( int shift = 24; shift >= 0; shift -= 8 )
    out.push_back( static_cast<char>( v >> shift ) );
}

void chunk( std::string& png, std::string_view type, std::string_view data )
{
  putBigEndian( png, static_cast<std::uint32_t>( data.size() ) );
  std::string body{ type };
  body += data;
  png += body;
  putBigEndian( png, crc32( body ) );
}

// The frame's lines of RGB bytes as a PNG of 8-bit RGB, its width the longest line's, shorter
// lines padded with black. The image data is deflate's stored blocks: no compression, nothing
// to depend on.
std::string encodePng( Session::Lines const& lines, std::size_t width )
{
  std::string raw;
  raw.reserve( lines.size() * ( ( 3 * width ) + 1 ) );
  for ( auto const& line : lines )
  {
    raw.push_back( 0 ); // filter: none
    std::size_t const bytes = std::min( line.size(), 3 * width );
    raw.append( reinterpret_cast<char const*>( line.data() ), bytes );
    raw.append( ( 3 * width ) - bytes, 0 );
  }

  std::string zlib{ "\x78\x01", 2 };
  constexpr std::size_t BLOCK = 65535;
  for ( std::size_t at = 0; at < raw.size() || at == 0; at += BLOCK )
  {
    std::size_t const size = std::min( BLOCK, raw.size() - at );
    bool const last = at + size >= raw.size();
    zlib.push_back( last ? 1 : 0 );
    zlib.push_back( static_cast<char>( size ) );
    zlib.push_back( static_cast<char>( size >> 8 ) );
    zlib.push_back( static_cast<char>( ~size ) );
    zlib.push_back( static_cast<char>( ~size >> 8 ) );
    zlib.append( raw, at, size );
    if ( last )
      break;
  }
  putBigEndian( zlib, adler32( raw ) );

  std::string png{ "\x89PNG\r\n\x1a\n", 8 };
  std::string header;
  putBigEndian( header, static_cast<std::uint32_t>( width ) );
  putBigEndian( header, static_cast<std::uint32_t>( lines.size() ) );
  header += std::string{ "\x08\x02\x00\x00\x00", 5 }; // 8 bits, RGB, deflate, no filter, no interlace
  chunk( png, "IHDR", header );
  chunk( png, "IDAT", zlib );
  chunk( png, "IEND", {} );
  return png;
}

} // namespace

void addVideoMethods( Dispatcher& dispatcher, Session& session )
{
  dispatcher.add(
      "video.screenshot",
      info( "The last complete frame of the video output, as a PNG: written to `path`, or answered "
            "in base64 when it is left out.",
            { { "path", "string", "Where to write the PNG.", false } } ),
      [&session]( Json const& params ) -> Outcome
      {
        if ( !session.machine() )
          return std::unexpected( notLoaded() );
        Session::Lines const& lines = session.lastFrame();
        std::size_t width = 0;
        for ( auto const& line : lines )
          width = std::max( width, line.size() / 3 );
        if ( width == 0 )
          return std::unexpected( Error{ .code = "screenshot_failed", .message = "No frame is complete yet" } );
        std::string const png = encodePng( lines, width );
        Json result{ { "width", width }, { "height", lines.size() }, { "frame", session.frames() } };
        if ( !params.contains( "path" ) )
        {
          result["png_base64"] = base64( png );
          return result;
        }
        auto const path = requireString( params, "path" );
        if ( !path )
          return std::unexpected( path.error() );
        std::ofstream f( *path, std::ios::binary );
        f.write( png.data(), static_cast<std::streamsize>( png.size() ) );
        if ( !f )
          return std::unexpected( Error{ .code = "screenshot_failed", .message = "Cannot write " + *path } );
        result["path"] = *path;
        return result;
      } );
}

} // namespace jag::control
