#include "Methods.hpp"

#include "jag/Machine.hpp"

#include <charconv>
#include <utility>

namespace jag::control
{

Error badRequest( std::string message )
{
  return Error{ .code = "bad_request", .message = std::move( message ) };
}

Error notLoaded()
{
  return Error{ .code = "not_loaded", .message = "No program is loaded (emu.load)" };
}

namespace
{

Error missing( std::string_view name )
{
  return badRequest( "Missing or invalid field: " + std::string{ name } );
}

std::optional<std::uint64_t> parseHex( std::string_view text )
{
  if ( text.starts_with( "0x" ) || text.starts_with( "0X" ) )
    text.remove_prefix( 2 );
  else if ( text.starts_with( "$" ) )
    text.remove_prefix( 1 );
  else
    return std::nullopt;
  std::uint64_t value = 0;
  auto const [end, error] = std::from_chars( text.data(), text.data() + text.size(), value, 16 );
  if ( error != std::errc{} || end != text.data() + text.size() || text.empty() )
    return std::nullopt;
  return value;
}

} // namespace

std::expected<std::string, Error> requireString( Json const& params, std::string_view name )
{
  auto const field = params.find( name );
  if ( field == params.end() || !field->is_string() )
    return std::unexpected( missing( name ) );
  return field->get<std::string>();
}

std::expected<std::uint64_t, Error> requireUnsigned( Json const& params, std::string_view name )
{
  // parsed text holds a non-negative integer as unsigned, a request built in C++ from an int as
  // signed: the value is what is checked
  auto const field = params.find( name );
  if ( field == params.end() )
    return std::unexpected( missing( name ) );
  if ( field->is_string() )
  {
    if ( auto const value = parseHex( field->get_ref<std::string const&>() ) )
      return *value;
    return std::unexpected( missing( name ) );
  }
  if ( !field->is_number_integer() || ( !field->is_number_unsigned() && field->get<std::int64_t>() < 0 ) )
    return std::unexpected( missing( name ) );
  return field->get<std::uint64_t>();
}

std::expected<std::uint64_t, Error>
optionalUnsigned( Json const& params, std::string_view name, std::uint64_t fallback )
{
  if ( !params.contains( name ) )
    return fallback;
  return requireUnsigned( params, name );
}

std::expected<Machine*, Error> loadedMachine( Session& session )
{
  if ( Machine* machine = session.machine() )
    return machine;
  return std::unexpected( notLoaded() );
}

std::expected<Cpu, Error> cpuParam( Json const& params, Session const& session )
{
  if ( !params.contains( "cpu" ) )
  {
    RunOptions const& options = *session.options();
    if ( options.m68k )
      return Cpu::M68k;
    return options.gpu ? Cpu::Gpu : Cpu::Dsp;
  }
  auto const name = requireString( params, "cpu" );
  if ( !name )
    return std::unexpected( name.error() );
  if ( *name == "m68k" )
    return Cpu::M68k;
  if ( *name == "gpu" )
    return Cpu::Gpu;
  if ( *name == "dsp" )
    return Cpu::Dsp;
  return std::unexpected( badRequest( "Field cpu must be m68k, gpu or dsp" ) );
}

Error noCpu( Cpu cpu )
{
  static constexpr std::string_view NAMES[] = { "68000", "GPU", "DSP" };
  return Error{ .code = "no_cpu",
                .message = std::string{ "This run has no " } + std::string{ NAMES[static_cast<int>( cpu )] } };
}

bool hasCpu( Machine& machine, Cpu cpu )
{
  switch ( cpu )
  {
  case Cpu::M68k:
    return machine.m68kState().has_value();
  case Cpu::Gpu:
    return machine.gpu() != nullptr;
  case Cpu::Dsp:
    return machine.dsp() != nullptr;
  }
  return false;
}

Probe& probeOf( Debugger& debugger, Cpu cpu )
{
  return cpu == Cpu::M68k ? debugger.m68k : cpu == Cpu::Gpu ? debugger.gpu : debugger.dsp;
}

Json stopJson( DebugStop const& stop )
{
  Json json{ { "cpu", stop.cpu }, { "pc", stop.pc }, { "cycle", stop.cycle }, { "exact", stop.exact } };
  if ( stop.kind == DebugStop::Kind::Watchpoint )
  {
    json["address"] = stop.address;
    json["access"] = stop.write ? "write" : "read";
    json["bytes"] = stop.bytes;
    json["value"] = stop.value ? Json( *stop.value ) : Json( nullptr );
  }
  if ( !stop.exact && stop.registers )
    json["registers"] = *stop.registers;
  return json;
}

namespace
{

std::string_view reasonName( Session::Stop reason )
{
  switch ( reason )
  {
  case Session::Stop::Completed:
    return "completed";
  case Session::Stop::Halted:
    return "halted";
  case Session::Stop::Timeout:
    return "timeout";
  case Session::Stop::Breakpoint:
    return "breakpoint";
  case Session::Stop::Watchpoint:
    return "watchpoint";
  case Session::Stop::Step:
    return "step";
  case Session::Stop::Frame:
    return "frame";
  }
  return "completed";
}

} // namespace

Json ranResult( Session::Ran const& ran, Session const& session )
{
  Json result{ { "reason", reasonName( ran.reason ) },
               { "cycles_executed", ran.cycles },
               { "frames_executed", ran.frames },
               { "running", session.running() } };
  if ( ran.stop )
    result["stop"] = stopJson( *ran.stop );
  return result;
}

MethodInfo info( std::string_view description, std::initializer_list<Param> params )
{
  Json properties = Json::object();
  Json required = Json::array();
  for ( Param const& param : params )
  {
    properties[std::string{ param.name }] = Json{ { "type", param.type }, { "description", param.description } };
    if ( param.required )
      required.push_back( param.name );
  }
  Json schema{ { "type", "object" }, { "properties", std::move( properties ) } };
  if ( !required.empty() )
    schema["required"] = std::move( required );
  return MethodInfo{ .description = std::string{ description }, .params = std::move( schema ) };
}

std::string toHex( std::uint8_t const* data, std::size_t size )
{
  static constexpr char DIGITS[] = "0123456789abcdef";
  std::string text;
  text.reserve( 2 * size );
  for ( std::size_t i = 0; i < size; ++i )
  {
    text.push_back( DIGITS[data[i] >> 4] );
    text.push_back( DIGITS[data[i] & 15u] );
  }
  return text;
}

std::optional<std::string> fromHex( std::string_view hex )
{
  if ( hex.size() % 2 != 0 )
    return std::nullopt;
  std::string bytes;
  bytes.reserve( hex.size() / 2 );
  for ( std::size_t i = 0; i < hex.size(); i += 2 )
  {
    std::uint8_t byte = 0;
    auto const [end, error] = std::from_chars( hex.data() + i, hex.data() + i + 2, byte, 16 );
    if ( error != std::errc{} || end != hex.data() + i + 2 )
      return std::nullopt;
    bytes.push_back( static_cast<char>( byte ) );
  }
  return bytes;
}

namespace
{

std::uint32_t byteAt( std::string_view data, std::size_t i )
{
  return static_cast<std::uint8_t>( data[i] );
}

} // namespace

std::string base64( std::string_view data )
{
  static constexpr char ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve( ( data.size() + 2 ) / 3 * 4 );
  std::size_t i = 0;
  for ( ; i + 2 < data.size(); i += 3 )
  {
    std::uint32_t const v = byteAt( data, i ) << 16 | byteAt( data, i + 1 ) << 8 | byteAt( data, i + 2 );
    out += ALPHABET[v >> 18];
    out += ALPHABET[( v >> 12 ) & 63u];
    out += ALPHABET[( v >> 6 ) & 63u];
    out += ALPHABET[v & 63u];
  }
  if ( i < data.size() )
  {
    std::uint32_t v = byteAt( data, i ) << 16;
    if ( i + 1 < data.size() )
      v |= byteAt( data, i + 1 ) << 8;
    out += ALPHABET[v >> 18];
    out += ALPHABET[( v >> 12 ) & 63u];
    out += i + 1 < data.size() ? ALPHABET[( v >> 6 ) & 63u] : '=';
    out += '=';
  }
  return out;
}

} // namespace jag::control
