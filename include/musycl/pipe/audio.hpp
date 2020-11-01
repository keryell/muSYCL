#ifndef MUSYCL_PIPE_AUDIO_HPP
#define MUSYCL_PIPE_AUDIO_HPP

/** \file SYCL abstraction for an audio pipe

    Based on RtAudio library.
*/

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include <boost/fiber/buffered_channel.hpp>

#include <range/v3/all.hpp>

#include "rtaudio/RtAudio.h"

namespace musycl {

// To use time unit literals directly
using namespace std::chrono_literals;

/** An audio input/output interface exposed as a SYCL pipe.

    In SYCL the type is used to synthesize the connection between
    kernels, so there can be only 1 instance of a MIDI input
    interface. */
class audio {

public:

  /// Audio value type, with data in [ -1, +1 ]
  using value_type = double;

  /// Number of elements in an audio frame
  static constexpr auto frame_size = 256;

  /// The type of an audio frame
  /// \todo Use a movable type?
  using frame = std::array<value_type, frame_size>;

private:

  /// Capacity of the MIDI message pipe
  static constexpr auto pipe_min_capacity = 2;

  /// The handler to control the audio input/output interface
  static inline std::optional<RtAudio> interface;

  /// A FIFO used to implement the pipe of MIDI messages
  static inline boost::fibers::buffered_channel<frame>
  output_frames { pipe_min_capacity };

  /// Check for RtAudio errors
  static constexpr auto check_error = [] (auto&& function) {
    try {
      return function();
    }
    catch (const RtAudioError &error) {
      error.printMessage();
      std::exit(EXIT_FAILURE);
    }
  };


/// Callback function used by RtAudio to read/write audio samples
static inline int
audio_callback(void *output_buffer, void *input_buffer,
               unsigned int rtaudio_frame_size, double time_stamp,
               RtAudioStreamStatus status, void * /* user_data */) {
  if (status)
    std::cerr << "Stream underflow detected!" << std::endl;
  assert(rtaudio_frame_size == frame_size
         && "frame_size needs to be the same as the one used by RtAudio");
  // Copy 1 ready frame to the output
  ranges::copy(output_frames.value_pop(),
               static_cast<value_type*>(output_buffer));
  // 0 to continue mormal operation
  return 0;
}


public:

  void open(const std::string& application_name,
            const std::string& port_name,
            const std::string& stream_name,
            RtAudio::Api backend) {
    check_error([&] { interface.emplace(backend); });
    auto device = interface->getDefaultOutputDevice();

    RtAudio::StreamParameters parameters;
    parameters.deviceId = device;
    // Mono output for now
    parameters.nChannels = 1;
    // Use channel(s) starting at 0
    parameters.firstChannel = 0;

    RtAudio::StreamOptions options;
    options.streamName = stream_name;
    auto sample_rate = interface->getDeviceInfo(device).preferredSampleRate;
    unsigned int actual_frame_size = frame_size;

    check_error([&] {
      interface->openStream(&parameters, nullptr, RTAUDIO_FLOAT64, sample_rate,
                       &actual_frame_size, audio_callback, nullptr, &options,
                       [] (RtAudioError::Type type,
                           const std::string &error_text) {
                         std::cerr << error_text << std::endl;
                       });
    });
    std::cout << "Sample rate: " << sample_rate
              << "\nSamples per frame: " << actual_frame_size
              << "\nRequested samples per frame: " << frame_size << std::endl;
    // Start the audio streaming
    check_error([&] { interface->startStream(); });
  }


  /// The sycl::pipe::write-like interface to write a MIDI message
  template <typename MusyclAudioSample>
  static inline void write(MusyclAudioSample&& s) {
    output_frames.push(std::forward<MusyclAudioSample>(s));
  }
};

}

#endif // MUSYCL_PIPE_AUDIO_HPP
