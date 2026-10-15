#pragma once

#include "jag/Debug.hpp"
#include "jag/Machine.hpp"
#include "jag/Runner.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace jag::control
{

// A machine as the control API drives it: the program loaded last, run on request, and what its
// run makes kept for a look (the last frame, the sound being captured). Whoever owns the session
// runs it on one thread: the CLI's, or the window's emulation thread, which hands it the requests.
class Session
{
public:
  using Lines = std::vector<std::vector<std::uint8_t>>;
  using FrameObserver = std::function<void( Lines const& lines )>;
  using SampleObserver = std::function<void( std::uint16_t left, std::uint16_t right )>;

  Session();
  ~Session();

  Session( Session const& ) = delete;
  Session& operator=( Session const& ) = delete;
  Session( Session&& ) = delete;
  Session& operator=( Session&& ) = delete;

  // Loads `options`' program in place of what runs, as the runner starts it but without a limit and
  // without the runner's files, with a debugger's probes; the pads held are kept. Returns why not
  // when it cannot; what runs then runs on.
  std::optional<std::string> load( RunOptions const& options );

  // Loads the program loaded last again, from reset, its breakpoints and watchpoints kept. False
  // when none was.
  bool restart();

  // The machine, null until a program is loaded.
  Machine* machine()
  {
    return mMachine.get();
  }

  // What was loaded last.
  RunOptions const* options() const
  {
    return mOptions ? &*mOptions : nullptr;
  }

  // The file the program loaded is: its BS94, COFF or raw binary, or the cartridge or the OS ROM it
  // boots from; empty before one is loaded.
  std::string program() const;

  // The program's processor has not stopped.
  bool running() const
  {
    return mRunning;
  }

  // Frames completed since the load, and the last of them, as lines of RGB bytes.
  std::uint64_t frames() const
  {
    return mFrames;
  }

  Lines const& lastFrame() const
  {
    return mLastFrame;
  }

  // How a run on request ended.
  enum class Stop
  {
    Completed,  // what was asked was run
    Halted,     // the program's processor stopped
    Timeout,    // the frames asked did not come in the time allowed
    Breakpoint, // a debugger's probe stopped it: `stop` says where
    Watchpoint,
    Step,  // a single step ended: `stop` says where
    Frame, // a frame was completed, as asked
  };

  struct Ran
  {
    Stop reason = Stop::Completed;
    std::uint64_t cycles = 0; // DSP cycles run
    std::uint64_t frames = 0; // frames completed in them
    std::optional<DebugStop> stop;
  };

  // Whoever runs the program by itself (the window) leaves it be while it is paused; a run on
  // request runs all the same. A debugger's stop pauses it.
  bool paused() const
  {
    return mPaused;
  }

  void setPaused( bool paused )
  {
    mPaused = paused;
  }

  // The last debugger's stop, kept until the next run.
  std::optional<DebugStop> const& lastStop() const
  {
    return mLastStop;
  }

  // The buttons held on the pad in port 0 or 1 (MainBus::setPad), kept across loads.
  void setPad( std::size_t port, std::uint32_t buttons );

  std::uint32_t pad( std::size_t port ) const
  {
    return mPads[port & 1u];
  }

  // Runs `count` DSP cycles, or until the program's processor stops.
  Ran runCycles( std::uint64_t count );

  // Runs until `count` more frames are complete, or the processor stops, or `timeout` cycles pass.
  Ran runFrames( std::uint64_t count, std::uint64_t timeout );

  // A single step: runs until the processor `probe` is the probe of is about to execute its next
  // instruction, the other processors beside it, or a probe stops the run first, or the program's
  // processor stops, or `timeout` cycles pass.
  Ran step( Probe& probe, std::uint64_t timeout );

  // Runs until the next frame is complete, each processor stopped at its next instruction, or as
  // step() ends.
  Ran stepFrame( std::uint64_t timeout );

  // The I2S output written to `path` as a WAV from now until stopCapture, the frames' rate its
  // sample rate.
  std::optional<std::string> startCapture( std::string const& path );

  bool capturing() const
  {
    return mCapturePath.has_value();
  }

  struct Captured
  {
    std::string path;
    std::uint64_t frames = 0;
    std::uint32_t sampleRate = 0;
  };

  std::expected<Captured, std::string> stopCapture();

  // What else is given the frames and the sound as the machine makes them: the window's display
  // and audio. Kept across loads.
  void setObservers( FrameObserver frames, SampleObserver samples );

private:
  static void frame( void* context, Lines const& lines );
  std::optional<std::string> load( RunOptions const& options, bool keepProbes );
  // Runs the machine to `cycle`; false when it stops short, by itself or at a debugger's stop
  // (taken into `ran`).
  bool runTo( std::uint64_t cycle, Ran& ran );
  static void sample( void* context, std::uint16_t left, std::uint16_t right );

  std::optional<RunOptions> mOptions;
  std::unique_ptr<Machine> mMachine;
  bool mRunning = false;
  bool mPaused = false;
  bool mStopAtFrame = false; // the frame completed next stops the run (stepFrame)
  std::optional<DebugStop> mLastStop;
  std::array<std::uint32_t, 2> mPads{};
  std::uint64_t mFrames = 0;
  Lines mLastFrame;
  FrameObserver mFrameObserver;
  SampleObserver mSampleObserver;
  std::optional<std::string> mCapturePath;
  std::vector<std::int16_t> mCaptured; // left and right
};

} // namespace jag::control
