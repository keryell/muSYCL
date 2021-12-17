/// \file A noise generator

#ifndef MUSYCL_NOISE_HPP
#define MUSYCL_NOISE_HPP

#include <limits>

#include <range/v3/all.hpp>

#include <triSYCL/vendor/triSYCL/random/xorshift.hpp>

#include "config.hpp"

#include "audio.hpp"
#include "group.hpp"
#include "envelope.hpp"
#include "low_pass_filter.hpp"
#include "midi.hpp"
#include "resonance_filter.hpp"

namespace musycl {

/// A digitally controlled oscillator
class noise {
  ///
  //  group cg { "Noise generator" };

  /// Track if the noise generator is generating a signal or just 0
  bool running = false;

  /// Some fast random generator
  static inline trisycl::vendor::trisycl::random::xorshift<> rng;

  /// To filter the noise
  low_pass_filter lpf_filter;

  /// The low pass filter envelope
  envelope lpf_env;

  /// Filter the noise with some resonance
  resonance_filter res_filter;

  /// The resonance filter envelope
  envelope rf_env;

  /// Initial velocity of the note
  float velocity;

  /// Initial frequency of the note
  float frequency;

 public:
  /// Parameters of the noise sound
  class param_detail : public group {
   public:
    /// The user-interface group name for the component
    std::string group_name { "noise" };
  };

  // Shared parameter between all copies of this noise generator
  using param_t = control::param<param_detail, noise>;

  /// Current parameters of the noise
  param_t param;

  /// Output volume of the note
  float volume { 1 };

  noise() {
    lpf_env.param->attack_time = 0;
    lpf_env.param->decay_time = 0.1;
    lpf_env.param->sustain_level = 0.01;
    lpf_env.param->release_time = 0.1;
    rf_env.param->attack_time = 0.05;
    rf_env.param->decay_time = 0.05;
    rf_env.param->sustain_level = 0.1;
    rf_env.param->release_time = 0.01;
  }

  /// \todo
  noise(const param_t& p) {}

  /** Start a note

      \param[in] on is the "note on" MIDI event to start with

      \return itself to allow operation chaining
  */
  auto& start(const midi::on& on) {
    velocity = on.velocity_1();
    frequency = midi::frequency(on);
    running = lpf_env.start().is_running() || rf_env.start().is_running();
    return *this;
  }

  /** Stop the current note

      \param[in] off is the "note off" MIDI event to stop with

      \return itself to allow operation chaining
  */
  auto& stop(const midi::off& off) {
    lpf_env.stop();
    rf_env.stop();
    return *this;
  }

  /// Return the running status
  bool is_running() { return running; }

  /// Generate an audio sample
  musycl::audio::frame audio() {
    lpf_filter.set_cutoff_frequency(frequency * lpf_env.out());
    res_filter.set_resonance(0.99).set_frequency(2 * frequency * rf_env.out());
    running = lpf_env.is_running() || rf_env.is_running();

    musycl::audio::frame f;
    if (running) {
      for (auto& e : f) {
        // A random number between -1 and 1
        auto random =
            rng() * 2. / std::numeric_limits<decltype(rng)::value_type>::max() -
            1;
        // Generate a filtered noise sample with an amplitude directly
        // proportional to the velocity
        e = lpf_filter.filter(random) * 10 * res_filter.filter(random) *
            velocity * volume;
      }
    } else
      // If the DCO is not running, the output is 0
      ranges::fill(f, 0);
    return f;
  }
};

} // namespace musycl
#endif // MUSYCL_NOISE_HPP
