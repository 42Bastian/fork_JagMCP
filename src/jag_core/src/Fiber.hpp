#pragma once

#include <atomic>
#include <exception>
#include <functional>
#include <thread>

namespace jag
{

// A function run on a thread of its own, by turns with its caller: the caller waits while it runs.
// From inside, it can hand the turn back (suspend) and go on from there when the caller runs it
// again. So a call that cannot return yet (a 68000 bus cycle waiting for the bus, decision 0027)
// lets its caller go on. One of the two runs at a time; what one wrote, the other sees.
class Fiber
{
public:
  explicit Fiber( std::function<void()> body );
  ~Fiber();

  Fiber( Fiber const& ) = delete;
  Fiber& operator=( Fiber const& ) = delete;
  Fiber( Fiber&& ) = delete;
  Fiber& operator=( Fiber&& ) = delete;

  // Runs the body, or the body where it suspended, until it returns or suspends. Rethrows what the
  // body threw.
  void run();

  // From inside the body: hands the turn back to the caller of run, and waits for the next run.
  void suspend();

  // The body has suspended, and has not returned since.
  bool suspended() const
  {
    return mSuspended;
  }

  // Whether this is the body's thread.
  bool inside() const
  {
    return std::this_thread::get_id() == mThread.get_id();
  }

private:
  // Thrown by suspend when the fiber is destroyed: the body unwinds and its thread ends.
  struct Abort
  {
  };

  enum Turn : int
  {
    CALLER,
    BODY,
  };

  void hand( Turn to );
  void await( Turn mine );
  void loop();

  std::function<void()> mBody;
  std::atomic<int> mTurn{ CALLER };
  bool mSuspended = false;
  bool mAborting = false;
  std::exception_ptr mError;
  std::thread mThread;
};

} // namespace jag
