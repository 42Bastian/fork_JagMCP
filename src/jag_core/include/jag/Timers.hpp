#pragma once

#include <cstdint>

namespace jag
{

// Jerry's two programmable interval timers (j_jmisc.v, JPIT1 to JPIT4): a 16 bit prescaler that
// counts down every cycle and reloads after 0, and a 16 bit divider that counts the prescaler's
// carries the same way; the divider's carry is the timer's interrupt request to the DSP
// (gpu_irq[2] and [3]), a pulse of one cycle. A timer whose prescaler is 0 is stopped and cleared.
// The counters are kept at the start of a cycle and brought forward when asked, by arithmetic.
// Tom's PIT is the same circuit with one timer (misc.v), but its prescaler register is clocked
// (pd, an slatch): written, it is taken at the end of the cycle, the prescaler reloading the old
// value.
//
// Ports Jaguar_MiSTer rtl/Jerry/j_jmisc.v and rtl/Tom/misc.v, read at 43a761d.
class Timers
{
public:
  // `clockedPrescale`: Tom's PIT.
  explicit Timers( bool clockedPrescale = false ) : mClockedPrescale( clockedPrescale ) {}

  // Whether either timer runs.
  bool running() const
  {
    return mTimer[0].prescale != 0 || mTimer[1].prescale != 0;
  }

  // The requests in `cycle`: bit 0 timer 1, bit 1 timer 2.
  std::uint32_t requests( std::uint64_t cycle );

  // Cycles from `cycle` to the first with a request, or UINT64_MAX; of either timer, or of `timer`.
  std::uint64_t requestIn( std::uint64_t cycle );
  std::uint64_t requestIn( std::uint64_t cycle, std::uint32_t timer );

  // The first cycle from `cycle` on with a request, or UINT64_MAX; kept until a write.
  std::uint64_t nextRequest( std::uint64_t cycle )
  {
    if ( !mNextKnown || mNextAt < cycle )
    {
      std::uint64_t const in = requestIn( cycle );
      mNextAt = in == UINT64_MAX ? UINT64_MAX : cycle + in;
      mNextKnown = true;
    }
    return mNextAt;
  }

  // The first cycle from `cycle` on with a request of timer 0, or UINT64_MAX, also for a cycle the
  // counters have been brought past (they repeat with the period since the last write).
  std::uint64_t firstRequestFrom( std::uint64_t cycle );

  // The shorter period of the timers running, in cycles, or UINT64_MAX.
  std::uint64_t shortestPeriod() const;

  // A write strobe (pit1w to pit4w, `reg` 0 to 3: JPIT1, JPIT2, JPIT3, JPIT4) in `cycle`, held
  // through Tom's I/O cycle; the counter it loads reloads at the end of each such cycle.
  void strobe( std::uint64_t cycle, std::uint32_t reg, std::uint16_t data );

  // A counter as a read in `cycle` sees it: `reg` 0 to 3 for JPIT1 to JPIT4 (F10036 to F1003C).
  std::uint16_t read( std::uint64_t cycle, std::uint32_t reg );

private:
  struct Timer
  {
    std::uint32_t prescale = 0; // pd: JPIT1, JPIT3
    std::uint32_t divide = 0;   // td: JPIT2, JPIT4
    std::uint32_t tp = 0;       // the prescaler
    std::uint32_t t = 0;        // the divider
  };

  void advance( std::uint64_t cycle );
  static void advance( Timer& timer, std::uint64_t n );
  static std::uint64_t requestIn( Timer const& timer );
  void applyStrobe();

  bool const mClockedPrescale;
  Timer mTimer[2];
  std::uint64_t mAt = 0;      // the cycle the counters are at the start of
  std::uint64_t mWritten = 0; // the cycle after the last write
  // the strobe of cycle mAt, made as the counters leave it
  bool mStrobe = false;
  std::uint32_t mStrobeReg = 0;
  std::uint16_t mStrobeData = 0;
  std::uint64_t mNextAt = 0;
  bool mNextKnown = false;
};

} // namespace jag
