#include "Process.hpp"

#include <algorithm>
#include <array>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ; // NOLINT: POSIX's, declared by no header on macOS
#endif

namespace jag::control
{

#ifdef _WIN32

namespace
{

std::wstring widen( std::string const& text )
{
  if ( text.empty() )
    return {};
  int const size = MultiByteToWideChar( CP_UTF8, 0, text.data(), static_cast<int>( text.size() ), nullptr, 0 );
  std::wstring wide( static_cast<std::size_t>( size ), L'\0' );
  MultiByteToWideChar( CP_UTF8, 0, text.data(), static_cast<int>( text.size() ), wide.data(), size );
  return wide;
}

// An argument as CommandLineToArgvW reads it back: quoted, its quotes and the backslashes before
// them escaped.
void appendQuoted( std::wstring& line, std::wstring const& arg )
{
  if ( !line.empty() )
    line += L' ';
  if ( !arg.empty() && arg.find_first_of( L" \t\n\v\"" ) == std::wstring::npos )
  {
    line += arg;
    return;
  }
  line += L'"';
  std::size_t backslashes = 0;
  for ( wchar_t const c : arg )
  {
    if ( c == L'\\' )
    {
      ++backslashes;
      continue;
    }
    if ( c == L'"' )
      backslashes = 2 * backslashes + 1;
    line.append( backslashes, L'\\' );
    backslashes = 0;
    line += c;
  }
  line.append( 2 * backslashes, L'\\' );
  line += L'"';
}

} // namespace

std::expected<std::unique_ptr<Process>, std::string>
Process::start( std::vector<std::string> const& args, bool input, std::string const& log )
{
  SECURITY_ATTRIBUTES inherited{ .nLength = sizeof( SECURITY_ATTRIBUTES ),
                                 .lpSecurityDescriptor = nullptr,
                                 .bInheritHandle = TRUE };
  HANDLE readEnd = nullptr;
  HANDLE writeEnd = nullptr;
  if ( input )
  {
    if ( !CreatePipe( &readEnd, &writeEnd, &inherited, 1u << 20 ) )
      return std::unexpected( "Cannot make a pipe" );
    SetHandleInformation( writeEnd, HANDLE_FLAG_INHERIT, 0 );
  }
  else
    readEnd =
        CreateFileW( L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherited, OPEN_EXISTING, 0, nullptr );
  HANDLE const logFile = CreateFileW( widen( log ).c_str(),
                                      FILE_APPEND_DATA,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      &inherited,
                                      OPEN_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL,
                                      nullptr );

  std::wstring line;
  for ( std::string const& arg : args )
    appendQuoted( line, widen( arg ) );
  STARTUPINFOW startup{};
  startup.cb = sizeof startup;
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = readEnd;
  startup.hStdOutput = logFile;
  startup.hStdError = logFile;
  PROCESS_INFORMATION info{};
  BOOL const started = CreateProcessW(
      nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &info );
  DWORD const error = GetLastError();
  if ( readEnd != nullptr && readEnd != INVALID_HANDLE_VALUE )
    CloseHandle( readEnd );
  if ( logFile != INVALID_HANDLE_VALUE )
    CloseHandle( logFile );
  if ( !started )
  {
    if ( writeEnd != nullptr )
      CloseHandle( writeEnd );
    return std::unexpected( "Cannot run " + args[0] + " (error " + std::to_string( error ) + ")" );
  }
  CloseHandle( info.hThread );
  std::unique_ptr<Process> process{ new Process };
  process->mProcess = info.hProcess;
  process->mInput = writeEnd;
  return process;
}

bool Process::write( std::span<std::uint8_t const> data )
{
  while ( !data.empty() )
  {
    DWORD written = 0;
    DWORD const size = static_cast<DWORD>( std::min<std::size_t>( data.size(), 1u << 30 ) );
    if ( mInput == nullptr || !WriteFile( mInput, data.data(), size, &written, nullptr ) )
      return false;
    data = data.subspan( written );
  }
  return true;
}

void Process::closeInput()
{
  if ( mInput != nullptr )
  {
    CloseHandle( mInput );
    mInput = nullptr;
  }
}

int Process::wait()
{
  closeInput();
  if ( !mWaited && mProcess != nullptr )
  {
    WaitForSingleObject( mProcess, INFINITE );
    DWORD code = 0;
    mStatus = GetExitCodeProcess( mProcess, &code ) ? static_cast<int>( code ) : -1;
    CloseHandle( mProcess );
    mProcess = nullptr;
    mWaited = true;
  }
  return mStatus;
}

#else

std::expected<std::unique_ptr<Process>, std::string>
Process::start( std::vector<std::string> const& args, bool input, std::string const& log )
{
  std::vector<char*> argv;
  argv.reserve( args.size() + 1 );
  for ( std::string const& arg : args )
    argv.push_back( const_cast<char*>( arg.c_str() ) ); // NOLINT: execvp's signature
  argv.push_back( nullptr );

  // both ends kept from whatever else is started; the child's standard input is a copy of the read end
  std::array<int, 2> ends{ -1, -1 };
  if ( input && pipe( ends.data() ) != 0 )
    return std::unexpected( "Cannot make a pipe" );
  for ( int const end : ends )
    if ( end >= 0 )
      fcntl( end, F_SETFD, FD_CLOEXEC );
#ifdef F_SETNOSIGPIPE
  if ( input )
    fcntl( ends[1], F_SETNOSIGPIPE, 1 );
#endif

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init( &actions );
  if ( input )
    posix_spawn_file_actions_adddup2( &actions, ends[0], 0 );
  else
    posix_spawn_file_actions_addopen( &actions, 0, "/dev/null", O_RDONLY, 0 );
  posix_spawn_file_actions_addopen( &actions, 1, log.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644 );
  posix_spawn_file_actions_adddup2( &actions, 1, 2 );
  pid_t pid = -1;
  int const error = posix_spawnp( &pid, argv[0], &actions, nullptr, argv.data(), environ );
  posix_spawn_file_actions_destroy( &actions );
  if ( input )
    close( ends[0] );
  if ( error != 0 )
  {
    if ( input )
      close( ends[1] );
    return std::unexpected( "Cannot run " + args[0] + ": " + std::strerror( error ) );
  }
  std::unique_ptr<Process> process{ new Process };
  process->mPid = pid;
  process->mInput = ends[1];
  return process;
}

bool Process::write( std::span<std::uint8_t const> data )
{
  while ( !data.empty() )
  {
    if ( mInput < 0 )
      return false;
    ssize_t const written = ::write( mInput, data.data(), data.size() );
    if ( written < 0 )
    {
      if ( errno == EINTR )
        continue;
      return false;
    }
    data = data.subspan( static_cast<std::size_t>( written ) );
  }
  return true;
}

void Process::closeInput()
{
  if ( mInput >= 0 )
  {
    close( mInput );
    mInput = -1;
  }
}

int Process::wait()
{
  closeInput();
  if ( !mWaited && mPid > 0 )
  {
    int status = 0;
    pid_t result = 0;
    do
      result = waitpid( mPid, &status, 0 );
    while ( result < 0 && errno == EINTR );
    mStatus = result == mPid && WIFEXITED( status ) ? WEXITSTATUS( status ) : -1;
    mWaited = true;
  }
  return mStatus;
}

#endif

Process::~Process()
{
  wait();
}

} // namespace jag::control
