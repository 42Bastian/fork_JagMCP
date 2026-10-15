#include "Methods.hpp"

#include "jag/Machine.hpp"
#include "jag/Risc.hpp"
#include "jag/Tom.hpp"

#include <algorithm>
#include <string>

namespace jag::control
{

namespace
{

// The most instructions debug.trace answers: what a probe keeps.
constexpr std::uint64_t MAX_TRACE = Probe::RECENT;

// The machine and the probe of the processor `params.cpu` names, or why not.
struct Probed
{
  Machine* machine;
  Probe* probe;
};

std::expected<Probed, Error> probed( Json const& params, Session& session )
{
  auto const machine = loadedMachine( session );
  if ( !machine )
    return std::unexpected( machine.error() );
  auto const cpu = cpuParam( params, session );
  if ( !cpu )
    return std::unexpected( cpu.error() );
  if ( !hasCpu( **machine, *cpu ) )
    return std::unexpected( noCpu( *cpu ) );
  return Probed{ *machine, &probeOf( ( *machine )->debugger(), *cpu ) };
}

std::string_view accessName( Watchpoint const& w )
{
  if ( w.read && w.write )
    return "access";
  return w.read ? "read" : "write";
}

Json breakpointsOf( Probe const& probe )
{
  Json list = Json::array();
  for ( std::uint32_t const address : probe.breakpoints )
    list.push_back( address );
  return list;
}

Json watchpointsOf( Probe const& probe )
{
  Json list = Json::array();
  for ( Watchpoint const& w : probe.watchpoints )
    list.push_back( Json{ { "address", w.address }, { "size", w.size }, { "access", accessName( w ) } } );
  return list;
}

// Each processor's, by name.
template <typename Of>
Outcome perCpu( Session& session, Of of )
{
  auto const machine = loadedMachine( session );
  if ( !machine )
    return std::unexpected( machine.error() );
  Debugger& debugger = ( *machine )->debugger();
  Json result = Json::object();
  for ( Cpu const cpu : { Cpu::M68k, Cpu::Gpu, Cpu::Dsp } )
    if ( hasCpu( **machine, cpu ) )
      result[probeOf( debugger, cpu ).cpu()] = of( probeOf( debugger, cpu ) );
  return result;
}

// Whether the processor can execute an instruction: the 68000 has not halted nor waits in a STOP
// whose mask holds back Tom's interrupt (level 2, the only one), a RISC has GO set.
bool runs( Machine& machine, Cpu cpu )
{
  if ( cpu == Cpu::M68k )
  {
    Machine::M68kState const state = *machine.m68kState();
    return !state.halted && !( state.stopped && ( state.sr >> 8 & 7u ) >= 2 );
  }
  Risc const* risc = cpu == Cpu::Gpu ? machine.gpu() : machine.dsp();
  return risc->started() && risc->now() < risc->cycles();
}

Param const CPU_PARAM{ "cpu", "string", "m68k, gpu or dsp; the program's own when left out.", false };

} // namespace

void addDebugMethods( Dispatcher& dispatcher, Session& session )
{
  dispatcher.add( "debug.breakpoint.add",
                  info( "Stops every later run before the processor issues the instruction at `address`; the "
                        "run that goes on from there executes it. While a GPU or a DSP has breakpoints or "
                        "watchpoints it runs on its stepped core alone, slower but cycle for cycle the same.",
                        { { "address", "integer", "The instruction's address." }, CPU_PARAM } ),
                  [&session]( Json const& params ) -> Outcome
                  {
                    auto const address = requireUnsigned( params, "address" );
                    if ( !address )
                      return std::unexpected( address.error() );
                    auto const p = probed( params, session );
                    if ( !p )
                      return std::unexpected( p.error() );
                    auto const pc = static_cast<std::uint32_t>( *address & 0xfffffeu );
                    if ( !p->probe->breaksAt( pc ) )
                      p->probe->breakpoints.push_back( pc );
                    return Json{ { "cpu", p->probe->cpu() }, { "breakpoints", breakpointsOf( *p->probe ) } };
                  } );

  dispatcher.add( "debug.breakpoint.remove",
                  info( "Removes the breakpoint at `address`.",
                        { { "address", "integer", "The instruction's address." }, CPU_PARAM } ),
                  [&session]( Json const& params ) -> Outcome
                  {
                    auto const address = requireUnsigned( params, "address" );
                    if ( !address )
                      return std::unexpected( address.error() );
                    auto const p = probed( params, session );
                    if ( !p )
                      return std::unexpected( p.error() );
                    auto const pc = static_cast<std::uint32_t>( *address & 0xfffffeu );
                    std::erase( p->probe->breakpoints, pc );
                    return Json{ { "cpu", p->probe->cpu() }, { "breakpoints", breakpointsOf( *p->probe ) } };
                  } );

  dispatcher.add( "debug.breakpoint.list",
                  info( "Every processor's breakpoints." ),
                  [&session]( Json const& ) -> Outcome { return perCpu( session, breakpointsOf ); } );

  dispatcher.add( "debug.watchpoint.add",
                  info( "Stops every later run once the processor has read or written a byte of `size` bytes from "
                        "`address`: a RISC once the memory controller has served the load or store (a load from "
                        "outside its local RAM before the data is back), the 68000 after the instruction. Accesses "
                        "of the blitter and the object processor, and the 68000's fetches of its program, are not "
                        "watched.",
                        { { "address", "integer", "The first byte watched." },
                          { "size", "integer", "Bytes watched; 1 when left out.", false },
                          { "access", "string", "read, write (when left out) or access (both).", false },
                          CPU_PARAM } ),
                  [&session]( Json const& params ) -> Outcome
                  {
                    auto const address = requireUnsigned( params, "address" );
                    if ( !address )
                      return std::unexpected( address.error() );
                    auto const size = optionalUnsigned( params, "size", 1 );
                    if ( !size )
                      return std::unexpected( size.error() );
                    if ( *size == 0 || *size > ( 1u << 24 ) )
                      return std::unexpected( badRequest( "Field size must be from 1 to 16777216" ) );
                    std::string access = "write";
                    if ( params.contains( "access" ) )
                    {
                      auto const name = requireString( params, "access" );
                      if ( !name )
                        return std::unexpected( name.error() );
                      access = *name;
                    }
                    if ( access != "read" && access != "write" && access != "access" )
                      return std::unexpected( badRequest( "Field access must be read, write or access" ) );
                    auto const p = probed( params, session );
                    if ( !p )
                      return std::unexpected( p.error() );
                    p->probe->watchpoints.push_back(
                        Watchpoint{ .address = static_cast<std::uint32_t>( *address & 0xffffffu ),
                                    .size = static_cast<std::uint32_t>( *size ),
                                    .read = access != "write",
                                    .write = access != "read" } );
                    return Json{ { "cpu", p->probe->cpu() }, { "watchpoints", watchpointsOf( *p->probe ) } };
                  } );

  dispatcher.add( "debug.watchpoint.remove",
                  info( "Removes the watchpoints from `address`.",
                        { { "address", "integer", "The first byte they watch." }, CPU_PARAM } ),
                  [&session]( Json const& params ) -> Outcome
                  {
                    auto const address = requireUnsigned( params, "address" );
                    if ( !address )
                      return std::unexpected( address.error() );
                    auto const p = probed( params, session );
                    if ( !p )
                      return std::unexpected( p.error() );
                    std::erase_if( p->probe->watchpoints,
                                   [&]( Watchpoint const& w ) { return w.address == ( *address & 0xffffffu ); } );
                    return Json{ { "cpu", p->probe->cpu() }, { "watchpoints", watchpointsOf( *p->probe ) } };
                  } );

  dispatcher.add( "debug.watchpoint.list",
                  info( "Every processor's watchpoints." ),
                  [&session]( Json const& ) -> Outcome { return perCpu( session, watchpointsOf ); } );

  dispatcher.add( "debug.singlestep",
                  info( "Runs until the processor is about to execute its next instruction: a single step of "
                        "it. The other processors run the same cycles beside it, as on the console. With "
                        "`frame`, runs until the next frame of the video output is complete (where the "
                        "vertical blank starts), each processor stopped at its next instruction. A "
                        "breakpoint or a watchpoint met first ends the step there. Answers as "
                        "emu.run_cycles, `reason` step or frame.",
                        { CPU_PARAM,
                          { "frame", "boolean", "Run to the next frame instead; not with cpu.", false },
                          { "timeout_cycles",
                            "integer",
                            "DSP cycles after which to give up; 20 s of console time when left out.",
                            false } } ),
                  [&session]( Json const& params ) -> Outcome
                  {
                    bool frame = false;
                    if ( auto const field = params.find( "frame" ); field != params.end() )
                    {
                      if ( !field->is_boolean() )
                        return std::unexpected( badRequest( "Field frame must be true or false" ) );
                      frame = field->get<bool>();
                    }
                    if ( frame && params.contains( "cpu" ) )
                      return std::unexpected( badRequest( "Fields frame and cpu cannot be given together" ) );
                    auto const timeout = optionalUnsigned( params, "timeout_cycles", DEFAULT_TIMEOUT );
                    if ( !timeout )
                      return std::unexpected( timeout.error() );
                    if ( frame )
                    {
                      if ( !session.machine() )
                        return std::unexpected( notLoaded() );
                      return ranResult( session.stepFrame( *timeout ), session );
                    }
                    auto const p = probed( params, session );
                    if ( !p )
                      return std::unexpected( p.error() );
                    if ( !runs( *p->machine, *cpuParam( params, session ) ) )
                    {
                      Session::Ran ran;
                      ran.reason = Session::Stop::Halted;
                      return ranResult( ran, session );
                    }
                    return ranResult( session.step( *p->probe, *timeout ), session );
                  } );

  dispatcher.add( "debug.trace",
                  info( "The last instructions the processor executed, oldest first, with the cycle each issued "
                        "(a RISC) or began (the 68000) in, disassembled from memory as it is now.",
                        { { "count", "integer", "Instructions, at most 1024; 32 when left out.", false }, CPU_PARAM } ),
                  [&session]( Json const& params ) -> Outcome
                  {
                    auto const count = optionalUnsigned( params, "count", 32 );
                    if ( !count )
                      return std::unexpected( count.error() );
                    if ( *count > MAX_TRACE )
                      return std::unexpected( badRequest( "Field count is above 1024" ) );
                    auto const p = probed( params, session );
                    if ( !p )
                      return std::unexpected( p.error() );
                    auto const cpu = cpuParam( params, session );
                    Json lines = Json::array();
                    for ( Executed const& e : p->probe->recent( *count ) )
                      lines.push_back( Json{ { "pc", e.pc },
                                             { "cycle", e.cycle },
                                             { "text", instructionAt( *p->machine, *cpu, e.pc ).first } } );
                    return lines;
                  } );

  dispatcher.add( "debug.diagnostics",
                  info( "What the program did that works only by chance: reads of addresses no register "
                        "answers (undriven_read), whose value is what the bus held. One entry per processor, "
                        "instruction and address, in the order first made, with the value first read and "
                        "how many times." ),
                  [&session]( Json const& ) -> Outcome
                  {
                    auto const machine = loadedMachine( session );
                    if ( !machine )
                      return std::unexpected( machine.error() );
                    Json list = Json::array();
                    for ( UndrivenRead const& r : ( *machine )->undrivenReads() )
                    {
                      auto const cpu = cpuParam( Json{ { "cpu", r.cpu } }, session );
                      char const* name = Tom::registerName( r.address );
                      list.push_back( Json{ { "kind", "undriven_read" },
                                            { "cpu", r.cpu },
                                            { "pc", r.pc },
                                            { "text", instructionAt( **machine, *cpu, r.pc ).first },
                                            { "address", r.address },
                                            { "register", name ? Json( name ) : Json( nullptr ) },
                                            { "value", r.value },
                                            { "cycle", r.cycle },
                                            { "count", r.count } } );
                    }
                    return list;
                  } );
}

} // namespace jag::control
