/// \file Basic configuration to be used all over the place

#ifndef MUSYCL_CONFIG_HPP
#define MUSYCL_CONFIG_HPP

#include <cmath>

namespace musycl {

  /// The sampling frequency of the audio input/output
  static constexpr auto sample_frequency = 48000;

  /// Number of elements in an audio frame
  static constexpr auto frame_size = 256;

  /// Frame frequency
  static constexpr auto frame_frequency =
    static_cast<float>(sample_frequency/frame_size);

  /// Frame period
  static constexpr auto frame_period = 1/frame_frequency;

}

#endif // MUSYCL_CONFIG_HPP
