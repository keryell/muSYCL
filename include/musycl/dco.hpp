/** \file A digitally controlled oscillator (DCO)

    https://en.wikipedia.org/wiki/Digitally_controlled_oscillator
*/

#ifndef MUSYCL_DCO_HPP
#define MUSYCL_DCO_HPP

#include <range/v3/all.hpp>

#include <triSYCL/vendor/triSYCL/random/xorshift.hpp>

#include "config.hpp"

#include "audio.hpp"
#include "control.hpp"
#include "group.hpp"
#include "midi.hpp"
#include "modulation_actuator.hpp"
#include "pitch_bend.hpp"

namespace musycl {

/// A digitally controlled oscillator
class dco {
  /// Track if the DCO is generating a signal or just 0
  /// \todo should be -1 to avoid a pop for triangle?
  bool running = false;

  /// The base note
  musycl::midi::on note;

  /// The current phase in the waveform, between 0 and 1
  float phase {};

  /// The phase increment per clock to generate the right frequency
  float dphase {};

  /// Some fast random generator
  static inline trisycl::vendor::trisycl::random::xorshift<> rng;

 public:
  /// Amplitude factor for the square waveform: 1 for maximal volume, 0 muted
  float final_square_volume {};

  /// The PWM for the square waveform, 0.5 for a symmetrical square signal
  float square_pwm {};

  /// Amplitude factor for the triangle waveform: 1 for maximal volume, 0 muted
  float final_triangle_volume {};

  /// Part of the period occupied by triangle waveform
  float triangle_ratio {};

  /// Position in the period of the triangle peak
  float triangle_peak_phase {};

  // Tuning factor of the oscillator, 1 for equal temperament
  float tune = 1;

  /// Parameters of the DCO sound
  class param_detail : public group {
   public:
    using group::group;

    /// Level of the square signal
    control::item<control::level<float>> square_volume {
      this, controller->attack_ch_1, "Square volume", { 0, 1, 1 }
    };

    /**  PWM of the square signal

         If 0, it is controlled by the modulation wheel
    */
    control::item<control::level<float>> square_pwm { "Square PWM",
                                                      { 0, 1, 0 } };

    /// Level of triangle signal
    control::item<control::level<float>> triangle_volume {
      this, controller->decay_ch_2, "Triangle volume", { 0, 1, 0 }
    };

    /// The part of the signal  where the triangle is, the rest is low level
    control::item<control::level<float>> triangle_ratio {
      this, controller->sustain_ch_3, "Triangle ratio", { 0.01, 1, 1 }
    };

    /// Ratio of the triangle occupied by the fall part of the triangle signal
    /// 0.5 is symmetrical triangle, 0 is a saw-tooth
    control::item<control::level<float>> triangle_fall_ratio {
      this, controller->release_ch_4, "Triangle fall ratio", { 0, 0.5, 0.5 }
    };
  };

  // Shared parameter between all copies of this DCO generator
  using param_t = control::param<param_detail, dco>;

  /// Current parameters of the DCO
  param_t param;

  /// Output volume of the note
  float volume { 1 };

  /// Create a sound from its parameters
  dco(const param_t& p)
      : param { p } {}

  dco() = default;

  /** Start a note

      \param[in] on is the "note on" MIDI event to start with

      \return itself to allow operation chaining
  */
  auto& start(const musycl::midi::on& on) {
    note = on;
    // Add some random detuning for an analog mood
    tune =
        1 + 0.005 * (rng() * 2. /
                       std::numeric_limits<decltype(rng)::value_type>::max() -
                   1);
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
      dphase =
          frequency(note, 24 * pitch_bend::value()) * tune / sample_frequency;
      set_square_waveform_parameter();
      set_triangle_waveform_parameter();
      for (auto& e : f) {
        e = square_signal() + triangle_signal();
        phase += dphase;
        // The phase is cyclic modulo 1
        if (phase >= 1)
          phase -= 1;
      }
    } else
      // If the DCO is not running, the output is 0
      ranges::fill(f, 0);
    return f;
  }

 private:
  /// Compute current value of square signal
  float square_signal() {
    // -1 or +1 according to current phase ratio compared to PWM ratio
    return final_square_volume * (2 * (phase > square_pwm) - 1);
  }

  /// Set once the square waveform parameters to speed up computation
  void set_square_waveform_parameter() {
    if (auto pwm = param->square_pwm; pwm == 0)
      /// Modulate the PWM with the modulation actuator starting from square
      square_pwm = modulation_actuator::value() * 0.49f + 0.5f;
    else
      square_pwm = pwm;
    // Generate a square waveform with an amplitude directly
    // proportional to the velocity
    final_square_volume = note.velocity_1() * volume * param->square_volume;
  }

  /// Compute current value of triangle signal
  float triangle_signal() {
    if (phase >= triangle_ratio)
      // Low level after the triangle part of the period
      return -final_triangle_volume;
    if (phase <= triangle_peak_phase)
      // Ramp up
      return final_triangle_volume * (2 * phase / triangle_peak_phase - 1);
    // Ramp down
    return final_triangle_volume *
           (1 - 2 * (phase - triangle_peak_phase) /
                    (triangle_ratio - triangle_peak_phase));
  }

  /// Set once the triangle waveform parameters to speed up computation
  void set_triangle_waveform_parameter() {
    /// Cache more locally the value
    triangle_ratio = param->triangle_ratio; // In [0,1]
    // In [0,1]
    triangle_peak_phase = triangle_ratio * (1 - param->triangle_fall_ratio);
    // Generate a square waveform with an amplitude directly
    // proportional to the velocity
    final_triangle_volume = note.velocity_1() * volume * param->triangle_volume;
  }
};

} // namespace musycl
#endif // MUSYCL_DCO_HPP
