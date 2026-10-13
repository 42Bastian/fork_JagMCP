#include "Methods.hpp"

namespace jag::control
{

void addAudioMethods( Dispatcher& dispatcher, Session& session )
{
  dispatcher.add( "audio.capture_start",
                  info( "Records the I2S output from now on into a WAV file: 16-bit stereo, a frame per I2S "
                        "frame, at the rate the frames come at. Nothing is resampled. The file is written by "
                        "audio.capture_stop.",
                        { { "path", "string", "Where to write the WAV." } } ),
                  [&session]( Json const& params ) -> Outcome
                  {
                    auto const path = requireString( params, "path" );
                    if ( !path )
                      return std::unexpected( path.error() );
                    if ( !session.machine() )
                      return std::unexpected( notLoaded() );
                    if ( session.capturing() )
                      return std::unexpected( Error{ .code = "capture_running", .message = "A capture is running" } );
                    if ( auto const why = session.startCapture( *path ) )
                      return std::unexpected( Error{ .code = "capture_failed", .message = *why } );
                    return Json{ { "path", *path } };
                  } );

  dispatcher.add( "audio.capture_stop",
                  info( "Ends the capture, writes the WAV and answers what it holds." ),
                  [&session]( Json const& ) -> Outcome
                  {
                    if ( !session.capturing() )
                      return std::unexpected(
                          Error{ .code = "capture_not_running", .message = "No capture is running" } );
                    auto const captured = session.stopCapture();
                    if ( !captured )
                      return std::unexpected( Error{ .code = "capture_failed", .message = captured.error() } );
                    return Json{ { "path", captured->path },
                                 { "frames", captured->frames },
                                 { "sample_rate", captured->sampleRate } };
                  } );
}

} // namespace jag::control
