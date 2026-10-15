#include "jag/control/Session.hpp"

#include "jag/I2s.hpp"
#include "jag/Tom.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <fstream>
#include <utility>

namespace jag::control
{

namespace
{

// A run of frames is made in parts, a frame's end looked for after each.
constexpr std::uint64_t PART_CYCLES = 20'000;

// The steps of a run with the 68000 while a RISC has breakpoints or watchpoints, or the next frame
// stops it.
constexpr std::uint64_t DEBUG_STEP = 32;

// `from` + `count`, no further than the last cycle.
std::uint64_t after( std::uint64_t from, std::uint64_t count )
{
  return count > UINT64_MAX - from ? UINT64_MAX : from + count;
}

Session::Stop stopReason( DebugStop::Kind kind )
{
  switch ( kind )
  {
  case DebugStop::Kind::Breakpoint:
    return Session::Stop::Breakpoint;
  case DebugStop::Kind::Watchpoint:
    return Session::Stop::Watchpoint;
  case DebugStop::Kind::Step:
    return Session::Stop::Step;
  case DebugStop::Kind::Frame:
    return Session::Stop::Frame;
  }
  return Session::Stop::Breakpoint;
}

} // namespace

Session::Session() = default;
Session::~Session() = default;

std::optional<std::string> Session::load( RunOptions const& options )
{
  return load( options, false );
}

std::optional<std::string> Session::load( RunOptions const& options, bool keepProbes )
{
  RunOptions run = options;
  run.framesDir.clear();
  run.wav.clear();
  run.log.clear();
  std::unique_ptr<Machine> machine;
  try
  {
    machine = std::make_unique<Machine>( run, nullptr );
  }
  catch ( std::exception const& error )
  {
    return error.what();
  }
  machine->tom().setFrameSink( &Session::frame, this );
  machine->i2s().setSampleSink( &Session::sample, this );
  Debugger& debugger = machine->debugger();
  if ( mMachine && keepProbes )
  {
    Debugger const& was = mMachine->debugger();
    for ( auto [to, from] : { std::pair{ &debugger.m68k, &was.m68k },
                              std::pair{ &debugger.gpu, &was.gpu },
                              std::pair{ &debugger.dsp, &was.dsp } } )
    {
      to->breakpoints = from->breakpoints;
      to->watchpoints = from->watchpoints;
    }
  }
  for ( std::size_t port = 0; port < mPads.size(); ++port )
    machine->setPad( port, mPads[port] );
  machine->runTo( 0 ); // powered on: the 68000 has read its vectors
  mMachine = std::move( machine );
  mLastStop.reset();
  mOptions = options;
  mRunning = true;
  mFrames = 0;
  mLastFrame.clear();
  return std::nullopt;
}

bool Session::restart()
{
  if ( !mOptions )
    return false;
  RunOptions const options = *mOptions;
  return !load( options, true ).has_value();
}

std::string Session::program() const
{
  if ( mOptions )
    for ( std::string const* file : { &mOptions->input, &mOptions->raw, &mOptions->cart, &mOptions->bios } )
      if ( !file->empty() )
        return *file;
  return {};
}

void Session::setPad( std::size_t port, std::uint32_t buttons )
{
  mPads[port & 1u] = buttons;
  if ( mMachine )
    mMachine->setPad( port, buttons );
}

bool Session::runTo( std::uint64_t cycle, Ran& ran )
{
  Debugger& debugger = mMachine->debugger();
  debugger.stop.reset();
  if ( mMachine->m68kState() && ( mStopAtFrame || debugger.gpu.armed() || debugger.dsp.armed() ) )
  {
    // the 68000 leads the turns (0014): in small steps, so that it is not far on when a RISC stops,
    // nor when the frame ends while a STOP it cannot leave lets it skip to the end of the run
    while ( !debugger.stop && mMachine->now() < cycle )
    {
      mRunning = mMachine->runTo( std::min( cycle, mMachine->now() + DEBUG_STEP ) );
      if ( !mRunning )
        break;
    }
  }
  else
    mRunning = mMachine->runTo( cycle );
  if ( debugger.stop )
  {
    ran.reason = stopReason( debugger.stop->kind );
    if ( debugger.stop->kind != DebugStop::Kind::Frame ) // a processor's
    {
      ran.stop = debugger.stop;
      mLastStop = debugger.stop;
    }
    mPaused = true;
    return false;
  }
  if ( !mRunning )
    ran.reason = Stop::Halted;
  return mRunning;
}

Session::Ran Session::runCycles( std::uint64_t count )
{
  Ran ran;
  if ( !mMachine || !mRunning )
  {
    ran.reason = Stop::Halted;
    return ran;
  }
  mLastStop.reset();
  std::uint64_t const from = mMachine->now();
  std::uint64_t const frames = mFrames;
  runTo( from + count, ran );
  ran.cycles = mMachine->now() - from;
  ran.frames = mFrames - frames;
  return ran;
}

Session::Ran Session::runFrames( std::uint64_t count, std::uint64_t timeout )
{
  Ran ran;
  if ( !mMachine || !mRunning )
  {
    ran.reason = Stop::Halted;
    return ran;
  }
  mLastStop.reset();
  std::uint64_t const from = mMachine->now();
  std::uint64_t const frames = mFrames;
  bool goesOn = true;
  while ( goesOn && mFrames - frames < count )
  {
    std::uint64_t const spent = mMachine->now() - from;
    if ( spent >= timeout )
      break;
    goesOn = runTo( mMachine->now() + std::min( PART_CYCLES, timeout - spent ), ran );
  }
  ran.cycles = mMachine->now() - from;
  ran.frames = mFrames - frames;
  if ( goesOn && ran.frames < count )
    ran.reason = Stop::Timeout;
  return ran;
}

Session::Ran Session::step( Probe& probe, std::uint64_t timeout )
{
  Ran ran;
  if ( !mMachine || !mRunning )
  {
    ran.reason = Stop::Halted;
    return ran;
  }
  mLastStop.reset();
  std::uint64_t const from = mMachine->now();
  std::uint64_t const frames = mFrames;
  mMachine->step( probe );
  if ( runTo( after( from, timeout ), ran ) )
    ran.reason = Stop::Timeout;
  probe.step = false;
  ran.cycles = mMachine->now() - from;
  ran.frames = mFrames - frames;
  return ran;
}

Session::Ran Session::stepFrame( std::uint64_t timeout )
{
  Ran ran;
  if ( !mMachine || !mRunning )
  {
    ran.reason = Stop::Halted;
    return ran;
  }
  mLastStop.reset();
  std::uint64_t const from = mMachine->now();
  std::uint64_t const frames = mFrames;
  mStopAtFrame = true;
  if ( runTo( after( from, timeout ), ran ) )
    ran.reason = Stop::Timeout;
  mStopAtFrame = false;
  ran.cycles = mMachine->now() - from;
  ran.frames = mFrames - frames;
  return ran;
}

std::optional<std::string> Session::startCapture( std::string const& path )
{
  if ( mCapturePath )
    return "A capture runs already, to " + *mCapturePath;
  std::ofstream const probe( path, std::ios::binary );
  if ( !probe )
    return "Cannot write " + path;
  mCapturePath = path;
  mCaptured.clear();
  return std::nullopt;
}

std::expected<Session::Captured, std::string> Session::stopCapture()
{
  if ( !mCapturePath )
    return std::unexpected( "No capture runs" );
  Captured captured;
  captured.path = *mCapturePath;
  captured.frames = mCaptured.size() / 2;
  mCapturePath.reset();
  std::uint64_t const frameCycles = mMachine ? mMachine->i2s().frameCycles() : 0;
  captured.sampleRate =
      frameCycles == 0
          ? 0
          : static_cast<std::uint32_t>( std::lround( CYCLES_PER_SECOND / static_cast<double>( frameCycles ) ) );

  std::ofstream f( captured.path, std::ios::binary );
  auto const u32 = [&f]( std::uint32_t v )
  {
    for ( int k = 0; k < 4; ++k )
      f.put( static_cast<char>( v >> ( 8 * k ) ) );
  };
  auto const u16 = [&f]( std::uint32_t v )
  {
    f.put( static_cast<char>( v ) );
    f.put( static_cast<char>( v >> 8 ) );
  };
  auto const bytes = static_cast<std::uint32_t>( 2 * mCaptured.size() );
  f.write( "RIFF", 4 );
  u32( 36 + bytes );
  f.write( "WAVEfmt ", 8 );
  u32( 16 );
  u16( 1 );
  u16( 2 );
  u32( captured.sampleRate );
  u32( captured.sampleRate * 4 );
  u16( 4 );
  u16( 16 );
  f.write( "data", 4 );
  u32( bytes );
  for ( std::int16_t const s : mCaptured )
    u16( static_cast<std::uint16_t>( s ) );
  mCaptured.clear();
  if ( !f )
    return std::unexpected( "Cannot write " + captured.path );
  return captured;
}

void Session::setObservers( FrameObserver frames, SampleObserver samples )
{
  mFrameObserver = std::move( frames );
  mSampleObserver = std::move( samples );
}

void Session::frame( void* context, Lines const& lines )
{
  auto& self = *static_cast<Session*>( context );
  ++self.mFrames;
  self.mLastFrame = lines;
  // the processors stop at their next instruction, as at a breakpoint
  if ( self.mStopAtFrame && !self.mMachine->debugger().stop )
    self.mMachine->debugger().stop = DebugStop{ .kind = DebugStop::Kind::Frame };
  if ( self.mFrameObserver )
    self.mFrameObserver( lines );
}

void Session::sample( void* context, std::uint16_t left, std::uint16_t right )
{
  auto& self = *static_cast<Session*>( context );
  if ( self.mCapturePath )
  {
    self.mCaptured.push_back( static_cast<std::int16_t>( left ) );
    self.mCaptured.push_back( static_cast<std::int16_t>( right ) );
  }
  if ( self.mSampleObserver )
    self.mSampleObserver( left, right );
}

} // namespace jag::control
