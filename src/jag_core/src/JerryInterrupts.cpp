#include "jag/JerryInterrupts.hpp"

#include <algorithm>

namespace jag
{

// The latches up to the start of `cycle`: each enabled source not latched yet takes the first of its
// requests from mAt on, set at the end of that cycle (fjk2), the line high from the next.
void JerryInterrupts::advance( std::uint64_t cycle )
{
  if ( cycle <= mAt )
    return;
  std::uint64_t first = UINT64_MAX;
  std::uint32_t set = 0;
  auto const take = [&]( std::uint32_t bit, std::uint64_t at )
  {
    if ( at < cycle )
    {
      set |= bit;
      first = std::min( first, at );
    }
  };
  if ( ( mIe & 2u ) && !( mLatch & 2u ) )
    for ( std::uint64_t const p : mDspPulses )
      if ( p >= mAt )
      {
        take( 2u, p );
        break;
      }
  // a prescaler written in mAt may request in it already (Timers::requests)
  std::uint32_t const now = ( mIe & 12u ) ? mTimers.requests( mAt ) : 0u;
  for ( std::uint32_t timer = 0; timer < 2; ++timer )
  {
    std::uint32_t const bit = 4u << timer;
    if ( ( mIe & bit ) && !( mLatch & bit ) )
    {
      std::uint64_t const in = ( now >> timer ) & 1u ? 0 : mTimers.requestIn( mAt, timer );
      if ( in != UINT64_MAX )
        take( bit, mAt + in );
    }
  }
  if ( set != 0 && mLatch == 0 )
    mRiseAt = first + 1;
  mLatch |= set;
  std::erase_if( mDspPulses, [cycle]( std::uint64_t p ) { return p < cycle; } );
  mAt = cycle;
}

void JerryInterrupts::write( std::uint64_t cycle, std::uint16_t value )
{
  advance( cycle );
  mLatch &= ~( ( value >> 8 ) & 0x3fu );
  mIe = value & 0x3fu;
}

std::uint16_t JerryInterrupts::read( std::uint64_t cycle )
{
  advance( cycle );
  return static_cast<std::uint16_t>( mLatch );
}

void JerryInterrupts::dspInterrupt( std::uint64_t cycle )
{
  mDspPulses.push_back( std::max( cycle, mAt ) );
}

std::uint64_t JerryInterrupts::nextRise( std::uint64_t cycle )
{
  advance( cycle );
  if ( mLatch != 0 ) // the line is high: it rises again only once acknowledged
    return UINT64_MAX;
  std::uint64_t first = UINT64_MAX;
  if ( mIe & 2u )
    for ( std::uint64_t const p : mDspPulses )
      if ( p >= mAt )
      {
        first = std::min( first, p );
        break;
      }
  std::uint32_t const now = ( mIe & 12u ) ? mTimers.requests( mAt ) : 0u;
  for ( std::uint32_t timer = 0; timer < 2; ++timer )
    if ( mIe & ( 4u << timer ) )
    {
      std::uint64_t const in = ( now >> timer ) & 1u ? 0 : mTimers.requestIn( mAt, timer );
      if ( in != UINT64_MAX )
        first = std::min( first, mAt + in );
    }
  return first == UINT64_MAX ? UINT64_MAX : first + 1;
}

std::uint64_t JerryInterrupts::riseIn( std::uint64_t from, std::uint64_t to )
{
  advance( to );
  return mRiseAt >= from && mRiseAt < to ? mRiseAt : UINT64_MAX;
}

} // namespace jag
