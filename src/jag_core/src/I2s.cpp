// Jerry's I2S interface: Jaguar_MiSTer rtl/Jerry/j_i2s.v, read at Jaguar_MiSTer 43a761d.

#include "jag/I2s.hpp"

namespace jag
{

void I2s::writeLtxd( std::uint16_t value )
{
  mLeft = value;
}

void I2s::writeRtxd( std::uint16_t value )
{
  mRight = value;
}

void I2s::writeSclk( std::uint16_t value )
{
  mChangeKnown = false;
  mSclkWrite = true;
  mSclkValue = value;
}

void I2s::writeSmode( std::uint16_t value )
{
  mChangeKnown = false;
  mSmodeWrite = true;
  mSmodeValue = value;
}

// The serial clock and word select generator (p, t, sckout, b, wsout). A write to SCLK resets the
// serial clock and its counter as it is served (bresl) and loads the divider; t then counts down
// from it. The external serial clock (sckin) is low.
void I2s::generatorStep( bool sclkWrite )
{
  bool const tZero = mT == 0;
  bool const ben = !sclkWrite && mSckout && tZero;
  bool const wsBefore = mWsout;
  mWsout = ( ben && mB == 15 && mWsen ) || ( mWsout && !( ben && ( mB & 15 ) == 15 ) );
  framed( wsBefore );
  if ( sclkWrite )
  {
    mSckout = false;
    mB = 0;
    mP = mSclkValue & 0xff;
  }
  else
  {
    if ( tZero )
      mSckout = !mSckout;
    mB = ( mB + ( ben ? 1u : 0u ) ) & 31;
  }
  mT = tZero || sclkWrite ? mP : mT - 1;
}

void I2s::step()
{
  // the cycles skipped while the interface was disabled: the generator's state is periodic in them
  if ( mSkipped )
  {
    std::uint64_t const period = 64 * ( mP + 1 );
    for ( std::uint64_t n = mSkipped % period; n; --n )
      generatorStep( false );
    mSkipped = 0;
  }

  // i2int takes wsp of this cycle: the receiver as the last rising serial clock left it
  bool const ws0 = mWs & 1;
  bool const interrupt = wsp();

  bool const sckBefore = mEnable && mSckout && !mSclkWrite;
  mPrev = mI2int;
  generatorStep( mSclkWrite );
  if ( mSmodeWrite )
  {
    mEnable = mSmodeValue & 1;
    mMode32 = mSmodeValue & 2;
    mWsen = mSmodeValue & 4;
    mRising = mSmodeValue & 8;
    mFalling = mSmodeValue & 16;
    mEveryWord = mSmodeValue & 32;
  }
  mI2int = interrupt;

  // the receiver, on the serial clock's rising edge
  bool const sckAfter = mEnable && mSckout;
  if ( !sckBefore && sckAfter )
  {
    bool const wss = mWsout;
    bool const mode16 = !mMode32;
    bool const lsb = !mOv && mBc == 15;
    bool const bcresl = !( !ws0 && wss ) && !( ws0 && !wss && mode16 ) && !( lsb && mMode32 );
    bool const bcen = !( mode16 && mOv );
    bool const ovd = ( mOv && bcresl ) || ( lsb && mode16 && bcresl );
    bool const leftd = ( !ws0 && wss ) || ( mLeftWord != lsb );
    mBc = !bcresl ? 0u : bcen ? ( mBc + 1 ) & 15 : mBc;
    mOv = ovd;
    mLeftWord = leftd;
    mMsb = !( ovd || bcresl );
    mWs = ( ( mWs << 1 ) | ( wss ? 1u : 0u ) ) & 3;
  }

  mSettle = mSclkWrite || mSmodeWrite;
  mSclkWrite = false;
  mSmodeWrite = false;
}

// The serial clock toggles (t has counted down to 0): on its fall the frame counter and the word
// select move, on its rise the receiver; i2int takes the receiver's state of before.
void I2s::toggle()
{
  mPrev = mI2int;
  mI2int = wsp();
  bool const falling = mSckout;
  bool const wsBefore = mWsout;
  mWsout = ( falling && mB == 15 && mWsen ) || ( mWsout && !( falling && ( mB & 15 ) == 15 ) );
  framed( wsBefore );
  mSckout = !mSckout;
  if ( falling )
    mB = ( mB + 1 ) & 31;
  mT = mP;
  if ( !falling && mEnable )
  {
    bool const ws0 = mWs & 1;
    bool const wss = mWsout;
    bool const mode16 = !mMode32;
    bool const lsb = !mOv && mBc == 15;
    bool const bcresl = !( !ws0 && wss ) && !( ws0 && !wss && mode16 ) && !( lsb && mMode32 );
    bool const bcen = !( mode16 && mOv );
    bool const ovd = ( mOv && bcresl ) || ( lsb && mode16 && bcresl );
    bool const leftd = ( !ws0 && wss ) || ( mLeftWord != lsb );
    mBc = !bcresl ? 0u : bcen ? ( mBc + 1 ) & 15 : mBc;
    mOv = ovd;
    mLeftWord = leftd;
    mMsb = !( ovd || bcresl );
    mWs = ( ( mWs << 1 ) | ( wss ? 1u : 0u ) ) & 3;
  }
}

// Steps in which the serial clock does not toggle: only i2int follows the receiver's state.
void I2s::settle( std::uint64_t steps )
{
  if ( steps >= 2 )
    mPrev = mI2int = wsp();
  else if ( steps == 1 )
  {
    mPrev = mI2int;
    mI2int = wsp();
  }
}

void I2s::advance( std::uint64_t n )
{
  if ( !mEnable && !mSettle )
  {
    mSkipped += n;
    mPrev = mI2int;
    return;
  }
  if ( mSettle || mSclkWrite || mSmodeWrite || mSkipped )
  {
    step();
    --n;
  }
  while ( n )
  {
    std::uint64_t const untilToggle = mT + 1; // the step the clock toggles in
    if ( untilToggle > n )
    {
      mT -= static_cast<std::uint32_t>( n );
      settle( n );
      return;
    }
    settle( untilToggle - 1 );
    toggle();
    n -= untilToggle;
  }
}

std::uint64_t I2s::findInterruptRise( std::uint64_t now )
{
  // two frames at the slowest serial clock; past that the mode raises no interrupt
  constexpr std::uint64_t LIMIT = 2 * 2 * 256 * 32;
  I2s copy = *this;
  copy.mSampleSink = nullptr; // what the copy steps through is not heard
  std::uint64_t m = 0;
  while ( m < LIMIT )
  {
    // to the next toggle, and the step after it, in which i2int takes what the toggle made
    std::uint64_t const k = copy.mT + 1;
    copy.advance( k );
    m += k;
    if ( copy.rose() )
      break;
    copy.advance( 1 );
    ++m;
    if ( copy.rose() )
      break;
  }
  mChangeKnown = true;
  mChangeAt = m < LIMIT ? now + m : UINT64_MAX;
  return m < LIMIT ? m : UINT64_MAX;
}

} // namespace jag
