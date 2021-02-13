/// \file A preset with a DCO and an envelope generator

#ifndef MUSYCL_SOUND_GENERATOR_DCO_ENVELOPE_HPP
#define MUSYCL_SOUND_GENERATOR_DCO_ENVELOPE_HPP

#include <range/v3/all.hpp>

#include "../config.hpp"
#include "../pipe/audio.hpp"
#include "../pipe/midi_in.hpp"

namespace musycl {

/// A digitally controlled oscillator
class dco_envelope : public dco {

  envelope env;
  midi::off note_off;
  bool sustain_pedal_in_action;

public:

  /// Initialize the envelope with its parameters
  dco_envelope(const envelope::param_t& p) : env { p } {}


  /** Start a note

      \param[in] on is the "note on" MIDI event to start with

      \return itself to allow operation chaining
  */
  auto& start(const midi::on& on) {
    env.start();
    dco::start(on);
    volume = env.out();
    return *this;
  }


  /** Stop the current note

      \param[in] off is the "note off" MIDI event to stop with

      \return itself to allow operation chaining
  */
  auto& stop(const midi::off& off) {
    note_off = off;
    // Skip the "note off" event if the sustain pedal is pressed
    if (sustain::value())
      sustain_pedal_in_action = true;
    else {
      env.stop();
      volume = env.out();
    }
    return *this;
  }


  /// Return the running status
  bool is_running() {
    return env.is_running();
  }


  /** Update the envelope at the frame frequency

      Since it is an envelope generator, no need to update it at
      the audio frequency.

      \return the envelope generator itself to enable command chaining
  */
  auto& tick_frame_clock() {
    if (sustain_pedal_in_action && !sustain::value())
      // The sustain pedal was in action but is just depressed
      env.stop();

    env.tick_frame_clock();
    volume = env.out();
    if (!is_running())
      dco::stop(note_off);
    return *this;
  }
};

}
#endif // MUSYCL_SOUND_GENERATOR_DCO_ENVELOPE_HPP
