#ifndef MUSYCL_AUDIO_HPP
#define MUSYCL_AUDIO_HPP

/** \file SYCL abstraction for an audio pipe

    Based on RtAudio library.
*/

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

#include <sycl/sycl.hpp>

#include <boost/fiber/buffered_channel.hpp>

#include <range/v3/all.hpp>

#include "rtaudio/RtAudio.h"

#include "musycl/config.hpp"

namespace musycl {

/** An audio input/output interface exposed as a SYCL pipe.

    In SYCL the type is used to synthesize the connection between
    kernels, so there can be only 1 instance of a MIDI input
    interface. */
class audio {

 public:
  /// Audio value type, with data in [ -1, +1 ]
  using value_type = double;

  /// Stereo mode: use 2 channels
  static constexpr auto channel_number = 2;

  /** Audio sample type

      In a stereo system, the element .x() or .s0() is the left voice
      and .y() or .s1() is the right voice */
  using sample_type = sycl::marray<value_type, channel_number>;

  /// Type suitable to store an audio sample
  template <typename T>
    requires(channel_number >= 2)
  class sample : public sycl::marray<T, channel_number> {
    /// Helper class allowing named initialization with 0 as default
    /// value
    struct initializer {
      T left = {};
      T right = {};
    };

   public:
    // Inherit from all the base constructors
    using sycl::marray<T, channel_number>::marray;

    /// Accessor for the left voice of the audio sample
    T& left() { return (*this)[0]; }

    /// Accessor for the right voice of the audio sample
    T& right() { return (*this)[1]; }

    /// Allow construction like {{ .left = 3}}, {{ .right = 2}} or {{.right =
    /// .left = -1, 7}}
    sample(const initializer i) { *this = { i.left, i.right }; }
  };

  // Left index in a stereo sample
  static constexpr int left = 0;

  // Right index in a stereo sample
  static constexpr int right = 1;

  /// The type of an audio frame
  /// \todo Use a movable type?
  using frame = std::array<sample_type, frame_size>;

  /// An audio frame in a SYCL buffer can live between the host and
  /// the accelerator
  using buffer = sycl::buffer<frame>;

 private:
  /// Capacity of the MIDI message pipe
  static constexpr auto pipe_min_capacity = 2;

  /// The handler to control the audio input/output interface
  static inline std::optional<RtAudio> interface;

  /// A FIFO used to implement the pipe of MIDI messages
  static inline boost::fibers::buffered_channel<frame> output_frames {
    pipe_min_capacity
  };

  /// Check for RtAudio errors
  static constexpr auto check_error = [](auto&& function) {
    try {
      return function();
    } catch (const RtAudioError& error) {
      error.printMessage();
      std::exit(EXIT_FAILURE);
    }
  };

  /** Callback function used by RtAudio to read/write audio samples
   */
  static inline int audio_callback(void* output_buffer, void* input_buffer,
                                   unsigned int rtaudio_frame_size,
                                   double time_stamp,
                                   RtAudioStreamStatus status,
                                   void* /* user_data */) {
    if (status)
      std::cerr << "Stream underflow detected!" << std::endl;
    assert(rtaudio_frame_size == frame_size &&
           "frame_size needs to be the same as the one used by RtAudio");
    // Copy 1 ready frame to the output
    ranges::copy(output_frames.value_pop(),
                 static_cast<sample_type*>(output_buffer));
    // 0 to continue mormal operation
    return 0;
  }

 public:
  void open(const std::string& application_name, const std::string& port_name,
            const std::string& stream_name, RtAudio::Api backend) {
    check_error([&] { interface.emplace(backend); });
    auto device = interface->getDefaultOutputDevice();

    RtAudio::StreamParameters parameters;
    parameters.deviceId = device;
    parameters.nChannels = channel_number;
    // Use channel(s) starting at 0
    parameters.firstChannel = 0;

    RtAudio::StreamOptions options;
    options.streamName = stream_name;
    auto sample_rate = interface->getDeviceInfo(device).preferredSampleRate;
    if (sample_rate != sample_frequency) {
      std::cerr
          << "Warning: the preferred sample rate " << sample_rate
          << " of the audio interface is not the same as the one "
             "configured in musycl/config.hpp so the quality might be reduced."
          << std::endl;
      // Forcing the sampling frequency anyway
      sample_rate = sample_frequency;
    }
    unsigned int actual_frame_size = frame_size;

    check_error([&] {
      interface->openStream(
          &parameters, nullptr, RTAUDIO_FLOAT64, sample_rate,
          &actual_frame_size, audio_callback, nullptr, &options,
          [](RtAudioError::Type type, const std::string& error_text) {
            std::cerr << error_text << std::endl;
          });
    });
    if (sample_rate != sample_frequency && actual_frame_size != frame_size) {
      std::cerr << "Actual sample rate: " << sample_rate
                << "Requested sample rate: " << sample_frequency
                << "\nActual samples per frame: " << actual_frame_size
                << "\nRequested samples per frame: " << frame_size << '.'
                << std::endl
                << "Please update musycl/config.hpp accordingly." << std::endl;
      std::terminate();
    }
    // Start the audio streaming
    check_error([&] { interface->startStream(); });
  }

  /// The sycl::pipe::write-like interface to write a MIDI message
  template <typename MusyclAudioSample>
  static inline void write(MusyclAudioSample&& s) {
    // Check that the output lands in the authorized values
    auto min = ranges::min(
        ranges::views::transform(s, [](auto e) { return ranges::min(e); }));
    auto max = ranges::max(
        ranges::views::transform(s, [](auto e) { return ranges::max(e); }));
    if (min < -1)
      std::cerr << "Min saturation detected: " << min;
    if (max > 1)
      std::cerr << "Max saturation detected: " << max;

    output_frames.push(std::forward<MusyclAudioSample>(s));
  }
};

} // namespace musycl

#endif // MUSYCL_AUDIO_HPP
