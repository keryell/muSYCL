/** \file A low frequency oscillator (LFO)

    https://en.wikipedia.org/wiki/Low-frequency_oscillation
*/

#ifndef MUSYCL_LFO_HPP
#define MUSYCL_LFO_HPP

#include <cmath>

#include "config.hpp"
#include "clock.hpp"

namespace musycl {

/// A low frequency oscillator
class lfo : public clock::follow<lfo> {
  /// Track if the LFO is generating a signal or just 0
  bool running = false;

  /// The phase in the waveform, between 0 and 1
  float phase = 0;

  /// The phase increment per clock to generate the right frequency
  float dphase {};

  /// Low level for the LFO output
  float low = -1;

  /// High level for the LFO output
  float high = 1;

  /// Current LFO value
  float value = low;

public:

  /** Run the LFO from the current state

      \return the LFO itself to enable command chaining
  */
  auto& run() {
    running = true;
    return *this;
  }


  /** Stop the LFO

      \return the LFO itself to enable command chaining
  */
  auto& stop() {
    running = false;
    return *this;
  }


  /** Set the LFO frequency

      \return the LFO itself to enable command chaining
  */
  auto& set_frequency(float frequency) {
    dphase = frequency*frame_size/sample_frequency;
    std::cout << "LFO frequency = " << frequency << " Hz, period = "
              << 1/frequency << " s, "
              << static_cast<int>(frequency*60) << " bpm" << std::endl;
    return *this;
  }


  /// Set the LFO low level of the output
  auto& set_low(float l) {
    low = l;
    std::cout << "LFO low level = " << low << std::endl;
    return *this;
  }


  /// Set the LFO high level of the output
  auto& set_high(float h) {
    high = h;
    std::cout << "LFO high level = " << high << std::endl;
    return *this;
  }


  /** Update the value at the frame frequency

      Since it is an LFO, no need to update it at the audio
      frequency.

      \return the LFO itself to enable command chaining
  */
  void frame_clock() {
    if (running) {
      // Generate a square waveform
      value = 2*(phase > 0.5) - 1;
      // The phase is cyclic modulo 1
      phase = std::fmod(phase + dphase, 1.0f);
    }
  }


  /// Get the current value between registered [low, high]
  float out() {
    return out(low, high);
  }


  /// Get the current value between given low and high
  float out(float low, float high) {
    return low + 0.5*(value + 1)*(high - low);
  }
};

}
#endif // MUSYCL_LFO_HPP
