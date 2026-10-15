#include "Fiber.hpp"

#include <utility>

namespace jag
{

namespace
{

// The looks at the turn before a thread sleeps on it, some tens of microseconds. A hand-over to a
// thread asleep takes microseconds: a debugger's run hands over every 32 cycles, and with threads
// that sleep it took 1.7 times as long (Native, a GPU breakpoint armed), with these 1.07 times.
constexpr int SPINS = 50000;

} // namespace

Fiber::Fiber( std::function<void()> body ) : mBody{ std::move( body ) }, mThread{ [this] { loop(); } } {}

Fiber::~Fiber()
{
  // a suspended body unwinds from suspend; one that returned waits in loop for a run it does not get
  mAborting = true;
  hand( BODY );
  mThread.join();
}

void Fiber::run()
{
  hand( BODY );
  await( CALLER );
  if ( mError )
    std::rethrow_exception( std::exchange( mError, nullptr ) );
}

void Fiber::suspend()
{
  mSuspended = true;
  hand( CALLER );
  await( BODY );
  mSuspended = false;
  if ( mAborting )
    throw Abort{};
}

void Fiber::hand( Turn to )
{
  mTurn.store( to, std::memory_order_release );
  mTurn.notify_one();
}

void Fiber::await( Turn mine )
{
  for ( int i = 0; i < SPINS; ++i )
    if ( mTurn.load( std::memory_order_acquire ) == mine )
      return;
  for ( int t = mTurn.load( std::memory_order_acquire ); t != mine; t = mTurn.load( std::memory_order_acquire ) )
    mTurn.wait( t, std::memory_order_acquire );
}

void Fiber::loop()
{
  for ( ;; )
  {
    await( BODY );
    if ( mAborting )
      return;
    try
    {
      mBody();
    }
    catch ( Abort const& )
    {
      return;
    }
    catch ( ... )
    {
      mError = std::current_exception();
    }
    hand( CALLER );
  }
}

} // namespace jag
