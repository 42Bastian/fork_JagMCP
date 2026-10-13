#pragma once

#include "jag/Timers.hpp"

#include <cstdint>
#include <vector>

namespace jag
{

// Jerry's interrupt control (j_jmisc.v, JINTCTRL at $F10020): a latch for each source, set by its
// request while JINTCTRL enables it and cleared by a write of its acknowledge bit; Jerry's
// interrupt line (xint) is high while any latch is set, and Tom takes its rise into INT1's bit 4
// for the 68000. The sources modelled are the DSP's interrupt to the CPU (D_CTRL's CPUINT, bit 1)
// and Jerry's two timers (bits 2 and 3); the external one, the UART's and the I2S interface's
// (bits 0, 4, 5) are not. A read gives the latches.
//
// The latches are kept at the start of a cycle and brought forward from the timers' requests.
//
// Ports Jaguar_MiSTer rtl/Jerry/j_jmisc.v, read at 43a761d.
class JerryInterrupts
{
public:
  explicit JerryInterrupts( Timers& timers ) : mTimers( timers ) {}

  // A write of JINTCTRL in `cycle` (int1w): the enables (bits 0 to 5), the acknowledges (8 to 13).
  void write( std::uint64_t cycle, std::uint16_t value );

  // A read of JINTCTRL in `cycle` (int1r).
  std::uint16_t read( std::uint64_t cycle );

  // The DSP's interrupt to the CPU (cpu_int), a pulse in `cycle`.
  void dspInterrupt( std::uint64_t cycle );

  // The first cycle in [from, to) from which the line is high after being low, or UINT64_MAX.
  // Asked for windows that follow each other.
  std::uint64_t riseIn( std::uint64_t from, std::uint64_t to );

  // The first cycle from `cycle` on the line may rise in, as far as the latches and the requests
  // known now tell (a write of JINTCTRL, a DSP's interrupt to come, may bring it nearer), or
  // UINT64_MAX.
  std::uint64_t nextRise( std::uint64_t cycle );

private:
  void advance( std::uint64_t cycle );

  Timers& mTimers;
  std::uint64_t mAt = 0;
  std::uint32_t mIe = 0;
  std::uint32_t mLatch = 0;
  std::uint64_t mRiseAt = UINT64_MAX;
  std::vector<std::uint64_t> mDspPulses;
};

} // namespace jag
