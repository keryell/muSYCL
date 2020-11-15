/** \file A digitally controlled oscillator (DCO)

    https://en.wikipedia.org/wiki/Digitally_controlled_oscillator
*/

#ifndef MUSYCL_DCO_HPP

#include "pipe/midi_in.hpp"

namespace musycl {

/// A digitally controlled oscillator
class dco {
  /// Track if the DCO is generating a signal or just 0
  bool running = false;

  /// The phase in the waveform, between 0 and 1
  float phase = 0;

  /// The phase increment per clock to generate the right frequency
  float dt {};

  /// Initial velocity of the note
  float velocity;

public:

  /// Start a note
  void start(const musycl::midi::on& on) {
    // The frequency for a 12-tone equal temperament scale with 440 Hz
    // A3 note being MIDI note 69
    auto frequency = 440*std::pow(2., (on.note - 69)/12.);
    dt = frequency/48000;
    velocity = on.velocity_1();
    std::cout << velocity << std::endl;
    running = true;
  }

  /// Stop the current note
  void stop(const musycl::midi::off& off) {
    running = false;
  }

  /// Generate an audio sample
  musycl::audio::frame audio() {
    musycl::audio::frame f;
    if (running) {
      for (auto &e : f) {
        // Generate a square waveform with an amplitude directly
        // proportional to the velocity
        e = (2*(phase > 0.5) - 1)*velocity;
        phase += dt;
        // The phase is cyclic modulo 1
        if (phase > 1)
          phase -= 1;
      }
    }
    else
      // If the DCO is not running, the output is 0
      ranges::fill(f, 0);
    return f;
  }
};

}
#endif // MUSYCL_DCO_HPP
