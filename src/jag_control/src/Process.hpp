#pragma once

// A program run beside the emulator (the recorder's ffmpeg): its standard input a pipe written
// here, its output appended to a log file. POSIX's posix_spawnp, or Windows' CreateProcess.

#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace jag::control
{

class Process
{
public:
  // Starts `args`, args[0] looked for on the PATH; its standard input a pipe when `input`, else
  // empty; its standard output and error appended to the file `log`. Why not when it cannot.
  static std::expected<std::unique_ptr<Process>, std::string>
  start( std::vector<std::string> const& args, bool input, std::string const& log );

  // Closes its input and waits for it.
  ~Process();

  Process( Process const& ) = delete;
  Process& operator=( Process const& ) = delete;
  Process( Process&& ) = delete;
  Process& operator=( Process&& ) = delete;

  // Writes to its input; false once it cannot (it has ended). On POSIX the writing thread must block
  // SIGPIPE.
  bool write( std::span<std::uint8_t const> data );

  // Closes its input and waits for it to end: its exit status, -1 when it did not exit.
  int wait();

private:
  Process() = default;
  void closeInput();

#ifdef _WIN32
  void* mProcess = nullptr;
  void* mInput = nullptr;
#else
  int mPid = -1;
  int mInput = -1;
#endif
  bool mWaited = false;
  int mStatus = -1;
};

} // namespace jag::control
