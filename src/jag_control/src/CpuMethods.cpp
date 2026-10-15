#include "Methods.hpp"

#include "jag/Machine.hpp"
#include "jag/Risc.hpp"
#include "jag/RiscDisassembler.hpp"

#include <string>
#include <vector>

namespace jag::control
{

namespace
{

// The most instructions a disassembly answers.
constexpr std::uint64_t MAX_INSTRUCTIONS = 1000;

Risc* risc( Machine& machine, Cpu cpu )
{
  return cpu == Cpu::Gpu ? machine.gpu() : machine.dsp();
}

// A RISC's instruction at `address`, its words read as memory.read reads them; 0 where nothing is.
RiscInstruction riscInstruction( Machine const& machine, bool gpu, std::uint32_t address )
{
  return disassembleRisc( gpu,
                          address,
                          [&machine, address]( std::uint32_t n )
                          {
                            std::uint32_t const at = address + ( 2 * n );
                            return static_cast<std::uint16_t>( machine.peek( at ).value_or( 0 ) << 8 |
                                                               machine.peek( at + 1 ).value_or( 0 ) );
                          } );
}

} // namespace

std::pair<std::string, std::uint32_t> instructionAt( Machine& machine, Cpu cpu, std::uint32_t address )
{
  if ( cpu == Cpu::M68k )
    return machine.disassemble68k( address ).value_or( std::pair<std::string, std::uint32_t>{} );
  RiscInstruction const instruction = riscInstruction( machine, cpu == Cpu::Gpu, address );
  return { instruction.text, 2 * instruction.words };
}

void addCpuMethods( Dispatcher& dispatcher, Session& session )
{
  dispatcher.add(
      "cpu.get_state",
      info( "A processor's registers between runs: the GPU's or the DSP's 64 (bank 0, then bank 1), "
            "PC, FLAGS and CTRL; or the 68000's. The instruction at PC is disassembled.",
            { { "cpu", "string", "m68k, gpu or dsp; the program's own when left out.", false } } ),
      [&session]( Json const& params ) -> Outcome
      {
        auto const machine = loadedMachine( session );
        if ( !machine )
          return std::unexpected( machine.error() );
        auto const cpu = cpuParam( params, session );
        if ( !cpu )
          return std::unexpected( cpu.error() );
        if ( *cpu == Cpu::M68k )
        {
          auto const state = ( *machine )->m68kState();
          if ( !state )
            return std::unexpected( noCpu( *cpu ) );
          auto const disasm = ( *machine )->disassemble68k( state->pc );
          return Json{
            { "pc", state->pc },           { "d", state->d },           { "a", state->a },
            { "sr", state->sr },           { "usp", state->usp },       { "isp", state->isp },
            { "stopped", state->stopped }, { "halted", state->halted }, { "disasm", disasm ? disasm->first : "" }
          };
        }
        Risc* r = risc( **machine, *cpu );
        if ( !r )
          return std::unexpected( noCpu( *cpu ) );
        Json registers = Json::array();
        for ( int i = 0; i < 64; ++i )
          registers.push_back( r->reg( i ) );
        std::uint32_t const pc = r->pc();
        return Json{ { "pc", pc },
                     { "flags", r->flagsRegister() },
                     { "control", r->controlRegister() },
                     { "registers", std::move( registers ) },
                     { "started", r->started() },
                     { "cycle", r->now() },
                     { "disasm", riscInstruction( **machine, *cpu == Cpu::Gpu, pc ).text } };
      } );

  dispatcher.add( "cpu.disassemble",
                  info( "Disassembles `count` instructions from `address`, read from memory without a bus cycle. "
                        "Each comes with its bytes, as memory.read gives them.",
                        { { "address", "integer", "Where to start (an integer, or a string \"0x...\")." },
                          { "count", "integer", "Instructions, at most 1000." },
                          { "cpu", "string", "m68k, gpu or dsp; the program's own when left out.", false } } ),
                  [&session]( Json const& params ) -> Outcome
                  {
                    auto const address = requireUnsigned( params, "address" );
                    if ( !address )
                      return std::unexpected( address.error() );
                    auto const count = requireUnsigned( params, "count" );
                    if ( !count )
                      return std::unexpected( count.error() );
                    if ( *count > MAX_INSTRUCTIONS )
                      return std::unexpected( badRequest( "Field count is above 1000" ) );
                    auto const machine = loadedMachine( session );
                    if ( !machine )
                      return std::unexpected( machine.error() );
                    auto const cpu = cpuParam( params, session );
                    if ( !cpu )
                      return std::unexpected( cpu.error() );
                    Json lines = Json::array();
                    auto at = static_cast<std::uint32_t>( *address & 0xffffffu );
                    for ( std::uint64_t i = 0; i < *count; ++i )
                    {
                      std::string text;
                      std::uint32_t length = 0;
                      if ( *cpu == Cpu::M68k )
                      {
                        auto const instruction = ( *machine )->disassemble68k( at );
                        if ( !instruction )
                          return std::unexpected( noCpu( *cpu ) );
                        text = instruction->first;
                        length = instruction->second;
                      }
                      else
                      {
                        RiscInstruction const instruction = riscInstruction( **machine, *cpu == Cpu::Gpu, at );
                        text = instruction.text;
                        length = 2 * instruction.words;
                      }
                      std::vector<std::uint8_t> bytes( length );
                      for ( std::uint32_t k = 0; k < length; ++k )
                        bytes[k] = ( *machine )->peek( ( at + k ) & 0xffffffu ).value_or( 0 );
                      lines.push_back( Json{ { "address", at },
                                             { "length", length },
                                             { "opcode_hex", toHex( bytes.data(), bytes.size() ) },
                                             { "text", text } } );
                      at = ( at + length ) & 0xffffffu;
                    }
                    return lines;
                  } );
}

} // namespace jag::control
