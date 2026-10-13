// The network transports over real sockets on the loopback interface: the line protocol over TCP,
// a line per request on each connection; MCP over HTTP, its statuses and its Origin check
// (docs/spec/control-protocol.md). The methods themselves are the transcripts' (lines.jsonl).

#include "jag/control/Dispatcher.hpp"
#include "jag/control/Session.hpp"
#include "jag/server/McpHttpServer.hpp"
#include "jag/server/McpServer.hpp"
#include "jag/server/TcpLineServer.hpp"

#include <httplib.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>

using jag::control::Json;

namespace
{

int failures = 0;

void check( bool ok, char const* what )
{
  if ( !ok )
  {
    std::fprintf( stderr, "failed: %s\n", what );
    ++failures;
  }
}

#define CHECK( condition ) check( ( condition ), #condition )

// A client of the line protocol over TCP.
class LineClient
{
public:
  explicit LineClient( std::uint16_t port ) : mSocket{ socket( AF_INET, SOCK_STREAM, 0 ) }
  {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons( port );
    address.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
    if ( connect( mSocket, reinterpret_cast<sockaddr*>( &address ), sizeof( address ) ) != 0 )
    {
      std::fputs( "cannot connect\n", stderr );
      std::exit( 1 );
    }
  }

  ~LineClient()
  {
    close( mSocket );
  }

  LineClient( LineClient const& ) = delete;
  LineClient& operator=( LineClient const& ) = delete;
  LineClient( LineClient&& ) = delete;
  LineClient& operator=( LineClient&& ) = delete;

  void send( std::string const& text ) const
  {
    CHECK( ::send( mSocket, text.data(), text.size(), 0 ) == static_cast<ssize_t>( text.size() ) );
  }

  // The next line the server sends.
  std::string line()
  {
    for ( ;; )
    {
      if ( auto const end = mPending.find( '\n' ); end != std::string::npos )
      {
        std::string line = mPending.substr( 0, end );
        mPending.erase( 0, end + 1 );
        return line;
      }
      std::array<char, 1024> buffer{};
      auto const received = recv( mSocket, buffer.data(), buffer.size(), 0 );
      if ( received <= 0 )
      {
        std::fputs( "connection closed\n", stderr );
        std::exit( 1 );
      }
      mPending.append( buffer.data(), static_cast<std::size_t>( received ) );
    }
  }

private:
  int mSocket;
  std::string mPending;
};

void lineProtocolOverTcp()
{
  jag::control::Session session;
  jag::control::Dispatcher const dispatcher{ session };
  jag::server::TcpLineServer const server{ 0, jag::server::directHandler( dispatcher ) };
  CHECK( server.port() != 0 );

  LineClient first{ server.port() };
  LineClient second{ server.port() };
  first.send( R"({"id":1,"method":"emu.status"})"
              "\r\n\n"
              R"({"id":2,"method":"memory.read","params":{"address":0,"size":1}})"
              "\n" );
  second.send( "not json\n" );

  CHECK( Json::parse( first.line() ).at( "result" ).at( "version" ) == jag::control::version() );
  CHECK( Json::parse( first.line() ).at( "error" ).at( "code" ) == "not_loaded" );
  CHECK( Json::parse( second.line() ).at( "error" ).at( "code" ) == "bad_request" );
}

void mcpOverHttp()
{
  jag::control::Session session;
  jag::control::Dispatcher const dispatcher{ session };
  jag::server::McpServer mcp{ dispatcher };
  jag::server::McpHttpServer const server{ 0, mcp };
  httplib::Client client{ "127.0.0.1", server.port() };

  auto const initialized =
      client.Post( "/mcp",
                   R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18"}})",
                   "application/json" );
  CHECK( initialized && initialized->status == 200 );
  CHECK( initialized && Json::parse( initialized->body ).at( "result" ).at( "serverInfo" ).at( "name" ) == "jagmcp" );

  auto const notified =
      client.Post( "/mcp", R"({"jsonrpc":"2.0","method":"notifications/initialized"})", "application/json" );
  CHECK( notified && notified->status == 202 && notified->body.empty() );

  auto const garbled = client.Post( "/mcp", "{", "application/json" );
  CHECK( garbled && garbled->status == 400 );

  auto const streamed = client.Get( "/mcp" );
  CHECK( streamed && streamed->status == 405 );

  httplib::Headers const foreign{ { "Origin", "http://example.com" } };
  auto const refused =
      client.Post( "/mcp", foreign, R"({"jsonrpc":"2.0","id":2,"method":"ping"})", "application/json" );
  CHECK( refused && refused->status == 403 );

  httplib::Headers const local{ { "Origin", "http://localhost:6274" } };
  auto const pinged = client.Post( "/mcp", local, R"({"jsonrpc":"2.0","id":3,"method":"ping"})", "application/json" );
  CHECK( pinged && pinged->status == 200 );
}

} // namespace

int main()
{
  lineProtocolOverTcp();
  mcpOverHttp();
  if ( failures == 0 )
    std::puts( "network transports: all checks passed" );
  return failures == 0 ? 0 : 1;
}
