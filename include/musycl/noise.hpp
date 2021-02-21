/// \file A noise generator

#ifndef MUSYCL_NOISE_HPP
#define MUSYCL_NOISE_HPP

#include <range/v3/all.hpp>

#include <triSYCL/vendor/triSYCL/random/xorshift.hpp>

#include "config.hpp"
#include "midi.hpp"
#include "pipe/audio.hpp"
#include "pipe/midi_in.hpp"

namespace musycl {

/// A digitally controlled oscillator
class noise {
  /// Some fast random generator
  static inline trisycl::vendor::trisycl::random::xorshift<> rng;

  /// To filter the noise
  musycl::low_pass_filter filter;

  /// Initial velocity of the note
  float velocity;

public:

  /// Output volume of the note
  float volume { 1 };

  /** Start a note

      \param[in] on is the "note on" MIDI event to start with

      \return itself to allow operation chaining
  */
  auto& start(const musycl::midi::on& on) {
    filter.set_cutoff_frequency(frequency(on));
    velocity = on.velocity_1();
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
  bool is_running() {
    return running;
  }


  /// Generate an audio sample
  musycl::audio::frame audio() {
    musycl::audio::frame f;
    if (running) {
      for (auto &e : f) {
        // A random number between -1 and 1
        auto random =
          rng()*2.f/std::numeric_limits<decltype(rng)::value_type>>::max - 1;
        // Generate a filtered noise sample with an amplitude directly
        // proportional to the velocity
        e = filter.filter(random)*velocity*volume;
      }
    }
    else
      // If the DCO is not running, the output is 0
      ranges::fill(f, 0);
    return f;
  }

};

}
#endif // MUSYCL_NOISE_HPP
