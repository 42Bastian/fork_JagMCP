#include "Methods.hpp"

#include "jag/Machine.hpp"
#include "jag/Runner.hpp"

#include <exception>
#include <span>
#include <string>
#include <vector>

namespace jag::control
{

namespace
{

// What a run of frames gives up after when no `timeout_cycles` is given: 20 s of console time.
constexpr std::uint64_t DEFAULT_TIMEOUT = static_cast<std::uint64_t>( 20 * CYCLES_PER_SECOND );

std::string_view programCpu( RunOptions const& options )
{
  if ( options.m68k )
    return "m68k";
  return options.gpu ? "gpu" : "dsp";
}

} // namespace

std::string_view version()
{
  return JAG_VERSION;
}

void addEmuMethods( Dispatcher& dispatcher, Session& session )
{
  dispatcher.add( "emu.status",
                  info( "The emulator's version, the program loaded and its BIOS, the cycle and the frame reached." ),
                  [&session]( Json const& ) -> Outcome
                  {
                    Json result{ { "version", version() } };
                    RunOptions const* options = session.options();
                    Machine* machine = session.machine();
                    if ( !options || !machine )
                    {
                      result["program"] = nullptr;
                      return result;
                    }
                    result["program"] = session.program();
                    if ( !options->bios.empty() )
                      result["bios"] = options->bios;
                    result["cpu"] = programCpu( *options );
                    result["cycles"] = machine->now();
                    result["seconds"] = static_cast<double>( machine->now() ) / CYCLES_PER_SECOND;
                    result["frame"] = session.frames();
                    result["running"] = session.running();
                    result["paused"] = session.paused();
                    result["diagnostics"] = machine->undrivenReads().size();
                    if ( auto const wait = machine->busWait() )
                      result["bus_wait"] = Json{ { "cpu", "m68k" },
                                                 { "pc", wait->pc },
                                                 { "address", wait->address },
                                                 { "access", wait->read ? "read" : "write" },
                                                 { "since", wait->since },
                                                 { "holder", wait->holder } };
                    if ( session.lastStop() )
                      result["stop"] = stopJson( *session.lastStop() );
                    return result;
                  } );

  dispatcher.add( "emu.load",
                  info( "Loads a program in place of what runs, from reset: `args` as jagmcp-cli takes them "
                        "(a program and its options), or a single .run file. --cycles and the runner's "
                        "outputs (--frames, --wav, --log) are not heeded.",
                        { { "args", "array", "The arguments, as strings." } } ),
                  [&session]( Json const& params ) -> Outcome
                  {
                    auto const field = params.find( "args" );
                    if ( field == params.end() || !field->is_array() || field->empty() )
                      return std::unexpected( badRequest( "Missing or invalid field: args" ) );
                    std::vector<std::string> args;
                    for ( Json const& arg : *field )
                    {
                      if ( !arg.is_string() )
                        return std::unexpected( badRequest( "Field args must hold strings" ) );
                      args.push_back( arg.get<std::string>() );
                    }
                    RunOptions options;
                    try
                    {
                      options = args.size() == 1 && args[0].ends_with( ".run" )
                                    ? readRunFile( args[0] )
                                    : parseRunOptions( std::span<std::string const>{ args } );
                    }
                    catch ( std::exception const& error )
                    {
                      return std::unexpected( Error{ .code = "load_failed", .message = error.what() } );
                    }
                    if ( auto const why = session.load( options ) )
                      return std::unexpected( Error{ .code = "load_failed", .message = *why } );
                    Json result{ { "program", session.program() } };
                    if ( !options.bios.empty() )
                      result["bios"] = options.bios;
                    result["cpu"] = programCpu( options );
                    return result;
                  } );

  dispatcher.add( "emu.reset",
                  info( "Loads the program loaded last again, from reset, as emu.load loaded it." ),
                  [&session]( Json const& ) -> Outcome
                  {
                    if ( !session.machine() )
                      return std::unexpected( notLoaded() );
                    if ( !session.restart() )
                      return std::unexpected(
                          Error{ .code = "load_failed", .message = "The program cannot be loaded again" } );
                    return Json::object();
                  } );

  dispatcher.add( "emu.pause",
                  info( "Pauses the program where it runs by itself (the window): it runs then only on "
                        "request. A breakpoint or a watchpoint met pauses it too." ),
                  [&session]( Json const& ) -> Outcome
                  {
                    session.setPaused( true );
                    return Json::object();
                  } );

  dispatcher.add( "emu.resume",
                  info( "Lets the program run by itself again where it does (the window)." ),
                  [&session]( Json const& ) -> Outcome
                  {
                    session.setPaused( false );
                    return Json::object();
                  } );

  dispatcher.add( "emu.run_cycles",
                  info( "Runs `count` DSP cycles (26.59 MHz), or until the program's processor stops.",
                        { { "count", "integer", "DSP cycles to run." } } ),
                  [&session]( Json const& params ) -> Outcome
                  {
                    auto const count = requireUnsigned( params, "count" );
                    if ( !count )
                      return std::unexpected( count.error() );
                    if ( !session.machine() )
                      return std::unexpected( notLoaded() );
                    return ranResult( session.runCycles( *count ), session );
                  } );

  dispatcher.add( "emu.run_frames",
                  info( "Runs until `count` more frames of the video output are complete, or the program's "
                        "processor stops, or `timeout_cycles` DSP cycles pass (20 s of console time by default).",
                        { { "count", "integer", "Frames to complete." },
                          { "timeout_cycles", "integer", "DSP cycles after which to give up.", false } } ),
                  [&session]( Json const& params ) -> Outcome
                  {
                    auto const count = requireUnsigned( params, "count" );
                    if ( !count )
                      return std::unexpected( count.error() );
                    auto const timeout = optionalUnsigned( params, "timeout_cycles", DEFAULT_TIMEOUT );
                    if ( !timeout )
                      return std::unexpected( timeout.error() );
                    if ( !session.machine() )
                      return std::unexpected( notLoaded() );
                    return ranResult( session.runFrames( *count, *timeout ), session );
                  } );
}

} // namespace jag::control
