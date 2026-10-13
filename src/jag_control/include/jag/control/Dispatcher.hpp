#pragma once

#include <nlohmann/json.hpp>

#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace jag::control
{

class Session;

// Keys keep the order they were written in, so a response reads `id`, `ok`, `result`, and two
// transcripts can be compared as text.
using Json = nlohmann::ordered_json;

// Why a request failed, as the protocol reports it: `code` one of the stable identifiers of
// docs/spec/control-protocol.md, `message` for a person.
struct Error
{
  std::string code;
  std::string message;
};

// What a method answers: the `result` object, or the error that replaces it.
using Outcome = std::expected<Json, Error>;

// What a client is told about a method: what it does, and the JSON Schema of its `params`. MCP's
// tool list is made from these.
struct MethodInfo
{
  std::string description;
  Json params = Json{ { "type", "object" } };
};

// The emulator's version, as emu.status and MCP's serverInfo give it: the tag the build was made
// from without its `v`, `git describe`'s answer after it, or `devel` (cmake/Version.cmake).
std::string_view version();

// A method under its own name, with what is told about it.
struct Method
{
  std::string name;
  MethodInfo info;
};

// The one place every capability of the emulator is reached through, whatever the transport
// (docs/decisions/0018-one-control-api.md): it takes a request object and answers a response
// object; framing, sockets and MCP are the transports' (jag_server). Every method is specified in
// docs/spec/control-protocol.md.
class Dispatcher
{
public:
  using Handler = std::function<Outcome( Json const& params )>;

  // Answers requests about `session`, which must outlive the dispatcher.
  explicit Dispatcher( Session& session );

  // Answers one request. Never throws and always answers: a malformed request is answered with a
  // `bad_request` error, not refused.
  [[nodiscard]] Json handle( Json const& request ) const;

  // Every method under its own name, ordered by name.
  [[nodiscard]] std::vector<Method> methods() const;

  // Makes `name` a method answered by `handler`, described by `info`.
  void add( std::string_view name, MethodInfo info, Handler handler );

private:
  std::unordered_map<std::string, Handler> mHandlers;
  std::unordered_map<std::string, MethodInfo> mInfo;
};

} // namespace jag::control
