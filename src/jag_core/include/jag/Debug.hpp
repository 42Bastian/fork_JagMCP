#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace jag
{

// A range of addresses whose reads or writes stop a run.
struct Watchpoint
{
  std::uint32_t address = 0;
  std::uint32_t size = 1;
  bool read = false;
  bool write = true;
};

// An instruction a processor executed: where, and the cycle it issued (the RISCs) or began in (the
// 68000).
struct Executed
{
  std::uint32_t pc = 0;
  std::uint64_t cycle = 0;
};

// Why a run stopped short of what was asked.
struct DebugStop
{
  enum class Kind
  {
    Breakpoint, // before the instruction at `pc`
    Watchpoint  // the access of the instruction at `pc` was made
  };

  Kind kind = Kind::Breakpoint;
  std::string cpu; // m68k, gpu or dsp
  std::uint32_t pc = 0;
  std::uint64_t cycle = 0;
  // a watchpoint's access: where, a write or a read, what was written, how many bytes
  std::uint32_t address = 0;
  bool write = false;
  std::optional<std::uint32_t> value;
  std::uint32_t bytes = 0;
  // A RISC that met the stop while the 68000 waited on the bus for it could not stop there: it ran
  // on to the end of the 68000's instruction. Its registers as they were at the stop are kept.
  bool exact = true;
  std::optional<std::array<std::uint32_t, 64>> registers;
};

// What a debugger has set on one processor, and what it executed lately.
class Probe
{
public:
  explicit Probe( std::string cpu ) : mCpu{ std::move( cpu ) } {}

  std::string const& cpu() const
  {
    return mCpu;
  }

  std::vector<std::uint32_t> breakpoints;
  std::vector<Watchpoint> watchpoints;

  // Breakpoints or watchpoints are set: a RISC then runs on its stepped core alone.
  bool armed() const
  {
    return !breakpoints.empty() || !watchpoints.empty();
  }

  bool breaksAt( std::uint32_t pc ) const
  {
    return std::ranges::find( breakpoints, pc ) != breakpoints.end();
  }

  // The watchpoint an access of `bytes` at `address` meets, if any.
  Watchpoint const* watches( std::uint32_t address, std::uint32_t bytes, bool write ) const
  {
    for ( Watchpoint const& w : watchpoints )
      if ( ( write ? w.write : w.read ) && address < w.address + w.size && w.address < address + bytes )
        return &w;
    return nullptr;
  }

  // The cycle a run stopped before a breakpoint in: the instruction there is executed when the run
  // goes on, not stopped at again.
  std::uint64_t passAt = UINT64_MAX;

  void executed( std::uint32_t pc, std::uint64_t cycle )
  {
    mRecent[mNext++ % RECENT] = Executed{ pc, cycle };
  }

  // The last `count` instructions executed, oldest first.
  std::vector<Executed> recent( std::size_t count ) const
  {
    count = std::min<std::size_t>( { count, RECENT, static_cast<std::size_t>( mNext ) } );
    std::vector<Executed> out;
    out.reserve( count );
    for ( std::uint64_t i = mNext - count; i < mNext; ++i )
      out.push_back( mRecent[i % RECENT] );
    return out;
  }

  static constexpr std::size_t RECENT = 1024;

private:
  std::string mCpu;
  std::array<Executed, RECENT> mRecent{};
  std::uint64_t mNext = 0;
};

// The probes of a machine's processors and the stop one of them made. Only a debugger installs
// them (Machine::debugger()): a run without one checks nothing.
struct Debugger
{
  Probe m68k{ "m68k" };
  Probe gpu{ "gpu" };
  Probe dsp{ "dsp" };
  std::optional<DebugStop> stop;
  // Whether a processor that meets a stop can stop there: not a RISC run while the 68000 waits on
  // the bus for it, in the middle of an instruction.
  bool freezable = true;
};

} // namespace jag
