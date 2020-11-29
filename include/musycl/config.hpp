/// \file Basic configuration to be used all over the place

#ifndef MUSYCL_CONFIG_HPP
#define MUSYCL_CONFIG_HPP

#include <cmath>

namespace musycl {

  /// The sampling frequency of the audio input/output
  static constexpr auto sample_frequency = 48000;

  /// Number of elements in an audio frame
  static constexpr auto frame_size = 256;

}

#endif // MUSYCL_CONFIG_HPP
