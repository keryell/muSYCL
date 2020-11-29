/** \file A low frequency oscillator (LFO)

    https://en.wikipedia.org/wiki/Low-frequency_oscillation
*/

#ifndef MUSYCL_LFO_HPP
#define MUSYCL_LFO_HPP

#include <cmath>

#include "config.hpp"

namespace musycl {

/// A low frequency oscillator
class lfo {
  /// Track if the LFO is generating a signal or just 0
  bool running = false;

  /// The phase in the waveform, between 0 and 1
  float phase = 0;

  /// The phase increment per clock to generate the right frequency
  float dphase {};

  /// Current LFO value
  float value = 0;

public:

  /** Run the LFO from the current state

      \return the LFO itself to enable command chaining
  */
  auto& run() {
    running = true;
    return *this;
  }


  /** Set the LFO frequency

      \return the LFO itself to enable command chaining
  */
  auto& set_frequency(float frequency) {
    dphase = frequency*frame_size/sample_frequency;
    return *this;
  }


  /** Stop the LFO

      \return the LFO itself to enable command chaining
  */
  auto& stop() {
    running = false;
    return *this;
  }


  /** Update the value at the frame frequency

      Since it is an LFO, no need to update it at the audio
      frequency.

      \return the LFO itself to enable command chaining
  */
  auto& tick_clock() {
    if (running) {
      // Generate a square waveform
      value = 2*(phase > 0.5) - 1;
      // The phase is cyclic modulo 1
      phase = std::fmod(phase + dphase, 1.0f);
    }
    return *this;
  }


  /// Get the current value between -1.0 and +1.0
  float out() {
    return value;
  }


  /// Get the current value between low and high
  float out(float low, float high) {
    return low + 0.5*(value + 1)*(high - low);
  }
};

}
#endif // MUSYCL_LFO_HPP
