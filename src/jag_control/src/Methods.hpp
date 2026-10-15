#pragma once

// The methods of docs/spec/control-protocol.md, a group per file, and what they share for reading
// parameters. Private to jag_control: a transport sees only the Dispatcher.

#include "jag/control/Dispatcher.hpp"
#include "jag/control/Session.hpp"

#include <cstdint>
#include <expected>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace jag::control
{

void addEmuMethods( Dispatcher& dispatcher, Session& session );
void addMemoryMethods( Dispatcher& dispatcher, Session& session );
void addCpuMethods( Dispatcher& dispatcher, Session& session );
void addVideoMethods( Dispatcher& dispatcher, Session& session );
void addAudioMethods( Dispatcher& dispatcher, Session& session );
void addDebugMethods( Dispatcher& dispatcher, Session& session );
void addInputMethods( Dispatcher& dispatcher, Session& session );

// What a run gives up after when no `timeout_cycles` is given: 20 s of console time.
constexpr std::uint64_t DEFAULT_TIMEOUT = static_cast<std::uint64_t>( 20 * CYCLES_PER_SECOND );

Error badRequest( std::string message );
Error notLoaded();

// One parameter of a method, as its schema describes it: a JSON Schema type ("string",
// "integer", "boolean", "object", "array"), and what it means.
struct Param
{
  std::string_view name;
  std::string_view type;
  std::string_view description;
  bool required{ true };
};

// A method's description and the schema of its params.
MethodInfo info( std::string_view description, std::initializer_list<Param> params = {} );

// The string `params[name]`, or the `bad_request` that says it is missing or is not a string.
std::expected<std::string, Error> requireString( Json const& params, std::string_view name );

// The non-negative integer `params[name]`, or the `bad_request` that says it is missing or is not
// one. A string of hex digits after `0x` or `$` is taken too, as addresses are written.
std::expected<std::uint64_t, Error> requireUnsigned( Json const& params, std::string_view name );

// The same, or `fallback` where the parameter is left out.
std::expected<std::uint64_t, Error>
optionalUnsigned( Json const& params, std::string_view name, std::uint64_t fallback );

// The machine loaded, or `not_loaded`.
std::expected<Machine*, Error> loadedMachine( Session& session );

// The processors a method may name.
enum class Cpu
{
  M68k,
  Gpu,
  Dsp
};

// The processor `params.cpu` names (m68k, gpu or dsp), the program's own when it is left out. The
// session must have a program loaded.
std::expected<Cpu, Error> cpuParam( Json const& params, Session const& session );

// `no_cpu`, for a processor the run does not have.
Error noCpu( Cpu cpu );

// Whether the run has the processor; its probe.
bool hasCpu( Machine& machine, Cpu cpu );
Probe& probeOf( Debugger& debugger, Cpu cpu );

// The instruction at `address` as text, and its length in bytes; empty where the run has no such
// processor.
std::pair<std::string, std::uint32_t> instructionAt( Machine& machine, Cpu cpu, std::uint32_t address );

// A debugger's stop as the answers give it.
Json stopJson( DebugStop const& stop );

// What a run on request answers: why it ended, what it took, and where it stopped.
Json ranResult( Session::Ran const& ran, Session const& session );

// Bytes as lower case hex digits, and back.
std::string toHex( std::uint8_t const* data, std::size_t size );
std::optional<std::string> fromHex( std::string_view hex );

// Bytes in base64.
std::string base64( std::string_view data );

} // namespace jag::control
