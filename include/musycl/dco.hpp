/** \file A digitally controlled oscillator (DCO)

    https://en.wikipedia.org/wiki/Digitally_controlled_oscillator
*/

#ifndef MUSYCL_DCO_HPP
#define MUSYCL_DCO_HPP

#include <range/v3/all.hpp>

#include "config.hpp"

#include "audio.hpp"
#include "midi.hpp"
#include "modulation_actuator.hpp"
#include "pitch_bend.hpp"

namespace musycl {

/// A digitally controlled oscillator
class dco {
  /// Track if the DCO is generating a signal or just 0
  bool running = false;

  /// The phase in the waveform, between 0 and 1
  float phase {};

  /// The phase increment per clock to generate the right frequency
  float dphase {};

  /// Initial velocity of the note
  float velocity;

  /// The base note
  musycl::midi::on note;

 public:
  struct param_t {
    using owner_t = dco;
  };

  /// Output volume of the note
  float volume { 1 };

  dco(const param_t& p) {}

  dco() = default;

  /** Start a note

      \param[in] on is the "note on" MIDI event to start with

      \return itself to allow operation chaining
  */
  auto& start(const musycl::midi::on& on) {
    note = on;
    velocity = on.velocity_1();
    std::cout << "Velocity " << velocity << std::endl;
    running = true;
    return *this;
  }

  /** Stop the current note

      \param[in] off is the "note off" MIDI event to stop with

      \return itself to allow operation chaining
  */
  auto& stop(const musycl::midi::off& off) {
    running = false;
    return *this;
  }

  /// Return the running status
  bool is_running() { return running; }

  /// Generate an audio sample
  musycl::audio::frame audio() {
    musycl::audio::frame f;
    if (running) {
      // Update the output frequency from the note ± 24 semitones from
      // the pitch bend
      dphase = frequency(note, 24 * pitch_bend::value()) / sample_frequency;
      /// Modulate the PWM with the modulation actuator starting from square
      float pwm = modulation_actuator::value() * 0.5f + 0.5f;
      for (auto& e : f) {
        // Generate a square waveform with an amplitude directly
        // proportional to the velocity
        e = (2 * (phase > pwm) - 1) * velocity * volume;
        phase += dphase;
        // The phase is cyclic modulo 1
        if (phase > 1)
          phase -= 1;
      }
    } else
      // If the DCO is not running, the output is 0
      ranges::fill(f, 0);
    return f;
  }
};

} // namespace musycl
#endif // MUSYCL_DCO_HPP
