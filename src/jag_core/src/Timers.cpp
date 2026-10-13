#include "jag/Timers.hpp"

namespace jag
{

// `n` cycles without a write: the prescaler's carries come every prescale + 1 cycles from the
// cycle it is 0 in, the divider's every divide + 1 of those.
void Timers::advance( Timer& timer, std::uint64_t n )
{
  if ( timer.prescale == 0 )
  {
    timer.tp = 0;
    timer.t = 0;
    return;
  }
  if ( n <= timer.tp )
  {
    timer.tp -= static_cast<std::uint32_t>( n );
    return;
  }
  std::uint64_t const period = timer.prescale + 1ull;
  std::uint64_t const carries = 1 + ( n - 1 - timer.tp ) / period;
  std::uint64_t const last = timer.tp + ( carries - 1 ) * period;
  timer.tp = static_cast<std::uint32_t>( timer.prescale - ( n - last - 1 ) );
  if ( carries <= timer.t )
  {
    timer.t -= static_cast<std::uint32_t>( carries );
    return;
  }
  std::uint64_t const rest = carries - timer.t - 1;
  timer.t = static_cast<std::uint32_t>( timer.divide - rest % ( timer.divide + 1ull ) );
}

std::uint64_t Timers::requestIn( Timer const& timer )
{
  if ( timer.prescale == 0 )
    return UINT64_MAX;
  return timer.tp + static_cast<std::uint64_t>( timer.t ) * ( timer.prescale + 1ull );
}

void Timers::advance( std::uint64_t cycle )
{
  if ( cycle <= mAt )
    return;
  if ( mStrobe )
    applyStrobe();
  if ( running() )
  {
    advance( mTimer[0], cycle - mAt );
    advance( mTimer[1], cycle - mAt );
  }
  mAt = cycle;
}

// The requests are the dividers' carries. A prescaler written in a cycle is seen by that cycle's
// carry already (the latch is open while pit1w or pit3w is): written while both counters are 0,
// it makes a request at once.
std::uint32_t Timers::requests( std::uint64_t cycle )
{
  advance( cycle );
  std::uint32_t bits = 0;
  for ( std::uint32_t i = 0; i < 2; ++i )
  {
    std::uint32_t prescale = mTimer[i].prescale;
    if ( mStrobe && mStrobeReg == 2 * i && !mClockedPrescale )
      prescale = mStrobeData;
    if ( prescale != 0 && mTimer[i].tp == 0 && mTimer[i].t == 0 )
      bits |= 1u << i;
  }
  return bits;
}

std::uint64_t Timers::requestIn( std::uint64_t cycle )
{
  advance( cycle );
  std::uint64_t const a = requestIn( mTimer[0] );
  std::uint64_t const b = requestIn( mTimer[1] );
  return a < b ? a : b;
}

std::uint64_t Timers::requestIn( std::uint64_t cycle, std::uint32_t timer )
{
  advance( cycle );
  return requestIn( mTimer[timer] );
}

std::uint64_t Timers::firstRequestFrom( std::uint64_t cycle )
{
  if ( cycle >= mAt )
  {
    std::uint64_t const in = requestIn( cycle, 0 );
    return in == UINT64_MAX ? UINT64_MAX : cycle + in;
  }
  Timer const& timer = mTimer[0];
  std::uint64_t const in = requestIn( timer );
  if ( in == UINT64_MAX )
    return UINT64_MAX;
  std::uint64_t const next = mAt + in;
  std::uint64_t const period = ( timer.prescale + 1ull ) * ( timer.divide + 1ull );
  std::uint64_t const from = cycle > mWritten ? cycle : mWritten;
  return next - ( next - from ) / period * period;
}

std::uint64_t Timers::shortestPeriod() const
{
  std::uint64_t shortest = UINT64_MAX;
  for ( Timer const& timer : mTimer )
    if ( timer.prescale != 0 )
    {
      std::uint64_t const period = ( timer.prescale + 1ull ) * ( timer.divide + 1ull );
      shortest = period < shortest ? period : shortest;
    }
  return shortest;
}

void Timers::strobe( std::uint64_t cycle, std::uint32_t reg, std::uint16_t data )
{
  advance( cycle );
  mNextKnown = false;
  mStrobe = true;
  mStrobeReg = reg;
  mStrobeData = data;
}

// The cycle mAt, with its strobe.
void Timers::applyStrobe()
{
  mStrobe = false;
  mWritten = mAt + 1;
  std::uint32_t const reg = mStrobeReg;
  Timer& timer = mTimer[reg >> 1];
  advance( mTimer[( reg >> 1 ) ^ 1], 1 );
  bool const prescaleWrite = ( reg & 1 ) == 0;
  if ( prescaleWrite && mClockedPrescale )
  {
    // the prescaler reloads the value the register held in the cycle; a stopped timer stays clear
    if ( timer.prescale != 0 )
    {
      bool const tpc = timer.tp == 0;
      bool const tc = tpc && timer.t == 0;
      timer.tp = timer.prescale;
      timer.t = tc ? timer.divide : timer.t - ( tpc ? 1u : 0u );
    }
    timer.prescale = mStrobeData;
    if ( timer.prescale == 0 )
    {
      timer.tp = 0;
      timer.t = 0;
    }
    ++mAt;
    return;
  }
  if ( prescaleWrite )
    timer.prescale = mStrobeData;
  else
    timer.divide = mStrobeData;
  if ( timer.prescale == 0 )
  {
    timer.tp = 0;
    timer.t = 0;
  }
  else
  {
    bool const tpc = timer.tp == 0;
    bool const tc = tpc && timer.t == 0;
    timer.tp = tpc || prescaleWrite ? timer.prescale : timer.tp - 1;
    timer.t = tc || !prescaleWrite ? timer.divide : timer.t - ( tpc ? 1u : 0u );
  }
  ++mAt;
}

std::uint16_t Timers::read( std::uint64_t cycle, std::uint32_t reg )
{
  advance( cycle );
  Timer const& timer = mTimer[reg >> 1];
  return static_cast<std::uint16_t>( ( reg & 1 ) == 0 ? timer.tp : timer.t );
}

} // namespace jag
