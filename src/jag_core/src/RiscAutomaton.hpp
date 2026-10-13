#pragma once

// The timing automaton's structures (docs/decisions/0007-timing-automaton.md), shared by the
// automaton (RiscAutomaton.cpp) and the datapath (Risc.cpp, where chain<OP> calls executeOp<OP>).

#include "jag/Risc.hpp"

#include "RiscInternal.hpp"

#include <cstring>
#include <deque>
#include <unordered_map>

namespace jag
{

namespace risc
{

constexpr std::uint32_t MAX_SPAN = 32; // cycles a variant may cover
// An outcome's interrupt fields: the cycles to the I2S line's next rise, and to each timer's next
// request, each up to MAX_SPAN (none within the span).
constexpr std::uint32_t RISE_SHIFT = 40;
constexpr std::uint32_t TIMER_SHIFT = 48;
constexpr std::uint64_t TIMERS_FAR = MAX_SPAN | MAX_SPAN << 8;
constexpr std::uint32_t CALENDAR_SPAN = 20; // cycles ahead a planned write can be (the divider's 17)
constexpr std::uint32_t MAX_CALENDAR = 12;  // planned writes a state may hold

enum : std::uint8_t
{
  KIND_NONE,
  KIND_JR,
  KIND_JUMP,
  KIND_MEMORY
};

// Address classes of a load or store, as the memory controller tells them apart.
enum : std::uint8_t
{
  CLASS_LOCAL = 1, // local RAM and the wave table ROM
  CLASS_IO,        // the DSP's registers
  CLASS_EXTERNAL
};

inline std::uint8_t clamp8( std::uint64_t v )
{
  return static_cast<std::uint8_t>( v > 255 ? 255 : v );
}

} // namespace risc

// The pipeline's control state at an issue cycle, every time relative to that cycle, every field
// that can steer the timing of what follows and nothing else: compared and hashed as bytes, so it
// is fully written, padding included.
struct Risc::PipeState
{
  std::uint32_t fetchPc, insdata, pfdata, jumpValue, srcdp;
  std::uint32_t ilatch, intEna, intEnaNext, ilatchClear;
  std::uint32_t immCount, immDst, accessReg, divReg;
  std::uint16_t decode, opcodeWord;
  std::uint8_t queueSize, icount, divCount, accessOp, accessClass, bank, flagsSet, macAt;
  std::uint8_t ready[64]; // cycles until each register reads its new value
  std::uint8_t calendarCount;
  std::uint8_t calendar[risc::MAX_CALENDAR][3]; // planned writes: cycles ahead, register, priority
  std::uint8_t progServ, exec, vins, imm1, imm2, insrdyp, jat2, jrt2, jrel, ioWrite;
  std::uint8_t memIdle, comp1, comp2, loading, drqt2, datServ, compdwait, ildPending;
  std::uint8_t divActive, divPending, divOffset, divOffsetNext;
  std::uint8_t intser, imask, imaskClear, cpuIrq, cpuIrqNext, awaitIdle, await1, await2, multa;
  std::uint8_t regpage, pendingEdge, tooMany;
  std::uint8_t flagsPending[4]; // cycles ahead each pending value of the flags is visible from, 0x80 a write

  bool operator==( PipeState const& o ) const
  {
    return std::memcmp( this, &o, sizeof *this ) == 0;
  }
};

// What followed a node in the stepped core for one outcome of the data its timing depends on. The
// fields the replay reads come first.
struct Risc::Variant
{
  Node* next = nullptr;   // the node of the next issue, null where the DSP stops
  std::uint32_t span = 0; // cycles to the next issue
  bool bare = false;      // the instruction's datapath is all that happens in the span
  bool stepOnly = false;  // recorded but not replayed: an I/O read, an I2S control write, cut
  std::uint64_t outcome = 0;
  std::vector<std::uint32_t> actions; // kind | offset << 8 | word << 16
};

// The pipeline's control state at an issue cycle, with the instruction it issues. The fields the
// chain reads come first; the state that names the node, read only to find it, last.
struct Risc::Node
{
  Chain chain = nullptr;   // the chain function of its opcode
  Exec exec = nullptr;     // its datapath
  Variant* idle = nullptr; // a simple node's variant while the I2S interface is idle
  std::uint32_t pc = 0;    // program_count at the issue
  bool simple = false;     // no jump, no load or store: the timing depends on no data but the I2S line
  bool busSafe = false;    // simple, and the next instruction no load or store either: the timing
                           // does not depend on the main bus (mbusywait), which may lag behind
  bool mmult = false;      // an MMULT in the decode stage or the queue: its sequence is stepped
  std::uint8_t kind = 0;   // what of the data the timing depends on: KIND_NONE, KIND_JR, KIND_JUMP, KIND_MEMORY
  Decoded decoded{};
  Variant* last = nullptr;             // the variant taken last, tried first
  std::uint64_t loopSeen = UINT64_MAX; // a jump node: the cycle it last issued in
  std::vector<Variant*> variants;
  // a simple node's variant for each cycle the I2S interrupt line may next rise in, MAX_SPAN none
  std::array<Variant*, risc::MAX_SPAN + 1> byRise{};
  PipeState entry{};
};

// The data a waiting loop must find the same at its jump, iteration after iteration (the control
// is the node's): registers, flags, accumulator, divider, the values of the writes planned.
struct Risc::LoopData
{
  std::array<std::uint32_t, 64> regs{};
  std::array<std::uint32_t, 64> old{};
  std::array<std::uint32_t, risc::MAX_CALENDAR> calendar{};
  std::array<std::uint32_t, 4> flagsPending{};
  std::uint32_t flags = 0, result = 0, accTop = 0, macProduct = 0, mod = 0;
  std::uint32_t divisor = 0, dividendHi = 0, dividendLo = 0, quotient = 0, divStepsLeft = 0;
  std::uint32_t ildValue = 0, accessAddress = 0, accessData = 0, immValue = 0, jumpValue = 0, srcdp = 0;
  std::uint32_t lastNegN = 0;

  bool operator==( LoopData const& o ) const
  {
    return std::memcmp( this, &o, sizeof *this ) == 0;
  }
};

struct Risc::Automaton
{
  struct Hash
  {
    // eight bytes at a time, in two lanes so that the multiplies overlap
    std::size_t operator()( PipeState const& s ) const
    {
      auto const* p = reinterpret_cast<unsigned char const*>( &s );
      std::uint64_t a = 0x9e3779b97f4a7c15ull, b = 0xc2b2ae3d27d4eb4full;
      std::size_t i = 0;
      for ( ; i + 16 <= sizeof s; i += 16 )
      {
        std::uint64_t x = 0, y = 0;
        std::memcpy( &x, p + i, 8 );
        std::memcpy( &y, p + i + 8, 8 );
        a = ( a ^ x ) * 0xff51afd7ed558ccdull;
        b = ( b ^ y ) * 0xc4ceb9fe1a85ec53ull;
        a ^= a >> 29;
        b ^= b >> 31;
      }
      for ( ; i < sizeof s; ++i )
        a = ( a ^ p[i] ) * 0x100000001b3ull;
      std::uint64_t const h = ( a ^ ( b * 0x9e3779b97f4a7c15ull ) ) * 0xff51afd7ed558ccdull;
      return static_cast<std::size_t>( h ^ ( h >> 32 ) );
    }
  };

  // nodes and variants live in arenas with stable addresses, close together
  std::deque<Node> nodeStore;
  std::deque<Variant> variantStore;
  std::unordered_map<PipeState, Node*, Hash> nodes;
  Node* current = nullptr; // the node of the cycle about to run, null while stepping between issues
  // recording
  Node* recNode = nullptr;
  Variant* recVariant = nullptr;
  std::uint64_t recStart = 0;
  bool flushWanted = false; // a write into fetched code while recording
  // a waiting loop: the jump node it was last seen at, when, with what data, and whether anything
  // since then (a write that changed memory, I/O, the main bus) makes the next visit no repeat
  Node* loopHead = nullptr;
  std::uint64_t loopCycle = 0;
  LoopData loopData{};
  bool loopDirty = true;
  bool loopFull = false; // loopData holds more than the registers
};

} // namespace jag
