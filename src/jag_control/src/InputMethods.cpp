#include "Methods.hpp"

#include <array>
#include <string>

namespace jag::control
{

namespace
{

// The buttons of a pad, in the order of their bits (MainBus::setPad).
constexpr std::array<std::string_view, 21> BUTTONS = { "right",  "left",  "down", "up", "a", "b",    "c",
                                                       "option", "pause", "1",    "2",  "3", "4",    "5",
                                                       "6",      "7",     "8",    "9",  "0", "star", "hash" };

// The frames a press holds the buttons for, and leaves them released after, when not told.
constexpr std::uint64_t PRESS_FRAMES = 2;

std::expected<std::size_t, Error> portParam( Json const& params )
{
  auto const port = optionalUnsigned( params, "port", 1 );
  if ( !port )
    return std::unexpected( port.error() );
  if ( *port != 1 && *port != 2 )
    return std::unexpected( badRequest( "Field port must be 1 or 2" ) );
  return static_cast<std::size_t>( *port - 1 );
}

std::expected<std::uint32_t, Error> buttonsParam( Json const& params )
{
  auto const field = params.find( "buttons" );
  if ( field == params.end() || !field->is_array() )
    return std::unexpected( badRequest( "Missing or invalid field: buttons" ) );
  std::uint32_t buttons = 0;
  for ( Json const& name : *field )
  {
    if ( !name.is_string() )
      return std::unexpected( badRequest( "Field buttons must hold names" ) );
    auto const at = std::ranges::find( BUTTONS, name.get_ref<std::string const&>() );
    if ( at == BUTTONS.end() )
      return std::unexpected( Error{ .code = "invalid_input", .message = "No button " + name.get<std::string>() } );
    buttons |= 1u << ( at - BUTTONS.begin() );
  }
  return buttons;
}

Json buttonNames( std::uint32_t buttons )
{
  Json names = Json::array();
  for ( std::size_t i = 0; i < BUTTONS.size(); ++i )
    if ( buttons >> i & 1u )
      names.push_back( BUTTONS[i] );
  return names;
}

Json padsJson( Session const& session )
{
  return Json{ { "port1", buttonNames( session.pad( 0 ) ) }, { "port2", buttonNames( session.pad( 1 ) ) } };
}

Param const PORT_PARAM{ "port", "integer", "The pad's port, 1 or 2; 1 when left out.", false };
Param const BUTTONS_PARAM{ "buttons",
                           "array",
                           "Button names: up, down, left, right, a, b, c, option, pause, 0 to 9, star, hash." };

} // namespace

void addInputMethods( Dispatcher& dispatcher, Session& session )
{
  dispatcher.add( "input.set",
                  info( "Holds `buttons` on the pad in `port`, and releases the others, until changed. Read "
                        "through Jerry's JOYSTICK and JOYBUTS, with --m68k.",
                        { BUTTONS_PARAM, PORT_PARAM } ),
                  [&session]( Json const& params ) -> Outcome
                  {
                    auto const port = portParam( params );
                    if ( !port )
                      return std::unexpected( port.error() );
                    auto const buttons = buttonsParam( params );
                    if ( !buttons )
                      return std::unexpected( buttons.error() );
                    session.setPad( *port, *buttons );
                    return padsJson( session );
                  } );

  dispatcher.add( "input.clear",
                  info( "Releases every button of the pad in `port`, or of both when it is left out.",
                        { { "port", "integer", "The pad's port, 1 or 2.", false } } ),
                  [&session]( Json const& params ) -> Outcome
                  {
                    if ( !params.contains( "port" ) )
                    {
                      session.setPad( 0, 0 );
                      session.setPad( 1, 0 );
                      return padsJson( session );
                    }
                    auto const port = portParam( params );
                    if ( !port )
                      return std::unexpected( port.error() );
                    session.setPad( *port, 0 );
                    return padsJson( session );
                  } );

  dispatcher.add(
      "input.press",
      info( "Holds `buttons` on the pad in `port` for `frames` frames, releases them and runs as "
            "many more; answers as emu.run_frames does, for the whole.",
            { BUTTONS_PARAM,
              PORT_PARAM,
              { "frames", "integer", "Frames held, and frames released after; 2 when left out.", false } } ),
      [&session]( Json const& params ) -> Outcome
      {
        auto const port = portParam( params );
        if ( !port )
          return std::unexpected( port.error() );
        auto const buttons = buttonsParam( params );
        if ( !buttons )
          return std::unexpected( buttons.error() );
        auto const frames = optionalUnsigned( params, "frames", PRESS_FRAMES );
        if ( !frames )
          return std::unexpected( frames.error() );
        if ( !session.machine() )
          return std::unexpected( notLoaded() );
        // a frame of the video output, or 20 s of console time without one
        constexpr auto TIMEOUT = static_cast<std::uint64_t>( 20 * CYCLES_PER_SECOND );
        std::uint32_t const held = session.pad( *port );
        session.setPad( *port, held | *buttons );
        Session::Ran ran = session.runFrames( *frames, TIMEOUT );
        session.setPad( *port, held & ~*buttons );
        if ( ran.reason == Session::Stop::Completed )
        {
          Session::Ran const after = session.runFrames( *frames, TIMEOUT );
          ran.reason = after.reason;
          ran.cycles += after.cycles;
          ran.frames += after.frames;
          ran.stop = after.stop;
        }
        return ranResult( ran, session );
      } );

  dispatcher.add( "input.get_state",
                  info( "The buttons held on each pad." ),
                  [&session]( Json const& ) -> Outcome { return padsJson( session ); } );
}

} // namespace jag::control
