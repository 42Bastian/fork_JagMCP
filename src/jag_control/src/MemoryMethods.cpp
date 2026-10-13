#include "Methods.hpp"

#include "jag/Machine.hpp"

#include <format>
#include <string>
#include <vector>

namespace jag::control
{

namespace
{

// The most a read answers at once.
constexpr std::uint64_t MAX_READ = 1 << 20;

// The console's address space: 24 bits.
constexpr std::uint64_t ADDRESS_SPACE = 1 << 24;

Error invalidRange( std::uint64_t address, std::string_view what )
{
  return Error{ .code = "invalid_range", .message = std::format( "No {} memory at ${:06x}", what, address ) };
}

} // namespace

void addMemoryMethods( Dispatcher& dispatcher, Session& session )
{
  dispatcher.add( "memory.read",
                  info( "Reads `size` bytes from `address` without a bus cycle: main RAM, the OS ROM and the "
                        "cartridge as the board maps them, the GPU's and the DSP's local RAM. Registers are not "
                        "read.",
                        { { "address", "integer", "The first byte's address (an integer, or a string \"0x...\")." },
                          { "size", "integer", "Bytes to read, at most 1048576." } } ),
                  [&session]( Json const& params ) -> Outcome
                  {
                    auto const address = requireUnsigned( params, "address" );
                    if ( !address )
                      return std::unexpected( address.error() );
                    auto const size = requireUnsigned( params, "size" );
                    if ( !size )
                      return std::unexpected( size.error() );
                    if ( *size > MAX_READ )
                      return std::unexpected( badRequest( "Field size is above 1048576" ) );
                    auto const machine = loadedMachine( session );
                    if ( !machine )
                      return std::unexpected( machine.error() );
                    std::vector<std::uint8_t> bytes( *size );
                    for ( std::uint64_t i = 0; i < *size; ++i )
                    {
                      std::uint64_t const at = *address + i;
                      auto const byte =
                          at < ADDRESS_SPACE ? ( *machine )->peek( static_cast<std::uint32_t>( at ) ) : std::nullopt;
                      if ( !byte )
                        return std::unexpected( invalidRange( at, "readable" ) );
                      bytes[i] = *byte;
                    }
                    return Json{ { "address", *address }, { "data_hex", toHex( bytes.data(), bytes.size() ) } };
                  } );

  dispatcher.add(
      "memory.write",
      info( "Writes bytes from `address` without a bus cycle, where memory.read reads them; the OS "
            "ROM and the cartridge excepted. A write into a RISC's local RAM drops what the timing "
            "automaton knew of the code there.",
            { { "address", "integer", "The first byte's address (an integer, or a string \"0x...\")." },
              { "data_hex", "string", "The bytes, two hex digits each." } } ),
      [&session]( Json const& params ) -> Outcome
      {
        auto const address = requireUnsigned( params, "address" );
        if ( !address )
          return std::unexpected( address.error() );
        auto const hex = requireString( params, "data_hex" );
        if ( !hex )
          return std::unexpected( hex.error() );
        auto const bytes = fromHex( *hex );
        if ( !bytes )
          return std::unexpected( badRequest( "Field data_hex is not hex digits in pairs" ) );
        auto const machine = loadedMachine( session );
        if ( !machine )
          return std::unexpected( machine.error() );
        // checked whole first, so that a write either happens or does not
        for ( std::uint64_t i = 0; i < bytes->size(); ++i )
        {
          std::uint64_t const at = *address + i;
          if ( at >= ADDRESS_SPACE || !( *machine )->writable( static_cast<std::uint32_t>( at ) ) )
            return std::unexpected( invalidRange( at, "writable" ) );
        }
        for ( std::uint64_t i = 0; i < bytes->size(); ++i )
          ( *machine )->poke( static_cast<std::uint32_t>( *address + i ), static_cast<std::uint8_t>( ( *bytes )[i] ) );
        return Json{ { "address", *address }, { "size", bytes->size() } };
      } );
}

} // namespace jag::control
