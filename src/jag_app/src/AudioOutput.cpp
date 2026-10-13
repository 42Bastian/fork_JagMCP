#include "AudioOutput.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace jag::app
{

namespace
{

// How much sound the queue aims to hold: enough to ride out a late frame of the interface, little
// enough not to be heard as lag.
constexpr double TARGET_SECONDS = 0.06;

// The furthest the resampling is pulled from the true rate: a pitch change of half a percent is
// not heard.
constexpr double MAX_ADJUSTMENT = 0.005;

constexpr int BYTES_PER_FRAME = 4;

// A starting rate, SCLK 19's; push() sets the one the frames come at.
constexpr int INITIAL_RATE = 20777;

SDL_AudioSpec specAt( int rate )
{
  return SDL_AudioSpec{ .format = SDL_AUDIO_S16, .channels = 2, .freq = rate };
}

} // namespace

std::unique_ptr<AudioOutput> AudioOutput::open()
{
  if ( !SDL_InitSubSystem( SDL_INIT_AUDIO ) )
    return nullptr;
  SDL_AudioSpec const spec = specAt( INITIAL_RATE );
  SDL_AudioStream* const stream =
      SDL_OpenAudioDeviceStream( SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr );
  if ( stream == nullptr )
  {
    SDL_QuitSubSystem( SDL_INIT_AUDIO );
    return nullptr;
  }
  SDL_ResumeAudioStreamDevice( stream );
  return std::unique_ptr<AudioOutput>{ new AudioOutput{ stream } };
}

AudioOutput::AudioOutput( SDL_AudioStream* stream ) : mStream{ stream }, mRate{ INITIAL_RATE } {}

AudioOutput::~AudioOutput()
{
  SDL_DestroyAudioStream( mStream );
  SDL_QuitSubSystem( SDL_INIT_AUDIO );
}

void AudioOutput::push( std::span<std::int16_t const> samples, double rate )
{
  auto const wanted = static_cast<int>( std::lround( rate ) );
  if ( wanted != mRate )
  {
    SDL_AudioSpec const spec = specAt( wanted );
    SDL_SetAudioStreamFormat( mStream, &spec, nullptr );
    mRate = wanted;
  }
  SDL_PutAudioStreamData( mStream, samples.data(), static_cast<int>( samples.size_bytes() ) );

  // Above the target the stream plays a little faster, below it a little slower, in proportion to
  // how far off it is.
  double const error = std::clamp( ( queuedSeconds() - TARGET_SECONDS ) / TARGET_SECONDS, -1.0, 1.0 );
  SDL_SetAudioStreamFrequencyRatio( mStream, static_cast<float>( 1.0 + ( error * MAX_ADJUSTMENT ) ) );
}

double AudioOutput::queuedSeconds() const
{
  int const bytes = SDL_GetAudioStreamQueued( mStream );
  return bytes <= 0 ? 0.0 : static_cast<double>( bytes ) / BYTES_PER_FRAME / mRate;
}

void AudioOutput::clear()
{
  SDL_ClearAudioStream( mStream );
}

} // namespace jag::app
