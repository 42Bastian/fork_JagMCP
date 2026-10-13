#include "jag/control/Dispatcher.hpp"

#include "Methods.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <utility>

namespace jag::control
{

namespace
{

Json errorResponse( std::uint64_t id, Error const& error )
{
  return Json{ { "id", id }, { "ok", false }, { "error", { { "code", error.code }, { "message", error.message } } } };
}

Json resultResponse( std::uint64_t id, Json result )
{
  return Json{ { "id", id }, { "ok", true }, { "result", std::move( result ) } };
}

} // namespace

Dispatcher::Dispatcher( Session& session )
{
  addEmuMethods( *this, session );
  addMemoryMethods( *this, session );
  addCpuMethods( *this, session );
  addVideoMethods( *this, session );
  addAudioMethods( *this, session );
  addDebugMethods( *this, session );
  addInputMethods( *this, session );
}

Json Dispatcher::handle( Json const& request ) const
{
  // The id is read first, so that every later failure is answered under it; until it is known the
  // answer is under 0.
  if ( !request.is_object() )
    return errorResponse( 0, badRequest( "Request must be a JSON object" ) );
  auto const id = requireUnsigned( request, "id" );
  if ( !id )
    return errorResponse( 0, id.error() );
  auto const methodField = request.find( "method" );
  if ( methodField == request.end() || !methodField->is_string() )
    return errorResponse( *id, badRequest( "Missing or invalid field: method" ) );
  auto const& method = methodField->get_ref<std::string const&>();

  // `params` may be left out: a method that takes none sees an empty object
  static Json const EMPTY_PARAMS = Json::object();
  auto const paramsField = request.find( "params" );
  bool const hasParams = paramsField != request.end();
  if ( hasParams && !paramsField->is_object() )
    return errorResponse( *id, badRequest( "Field params must be an object" ) );

  auto const handler = mHandlers.find( method );
  if ( handler == mHandlers.end() )
    return errorResponse( *id, Error{ .code = "unknown_method", .message = "Unknown method: " + method } );
  try
  {
    auto outcome = handler->second( hasParams ? *paramsField : EMPTY_PARAMS );
    if ( !outcome )
      return errorResponse( *id, outcome.error() );
    return resultResponse( *id, std::move( *outcome ) );
  }
  catch ( std::exception const& error ) // a parameter of the wrong JSON type, say
  {
    return errorResponse( *id, badRequest( error.what() ) );
  }
}

std::vector<Method> Dispatcher::methods() const
{
  std::vector<Method> methods;
  methods.reserve( mInfo.size() );
  for ( auto const& [name, info] : mInfo )
    methods.push_back( Method{ .name = name, .info = info } );
  std::ranges::sort( methods, {}, &Method::name );
  return methods;
}

void Dispatcher::add( std::string_view name, MethodInfo info, Handler handler )
{
  mHandlers.emplace( name, std::move( handler ) );
  mInfo.emplace( name, std::move( info ) );
}

} // namespace jag::control
