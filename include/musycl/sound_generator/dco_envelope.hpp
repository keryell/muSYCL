/// \file A preset with a DCO and an envelope generator

#ifndef MUSYCL_SOUND_GENERATOR_DCO_ENVELOPE_HPP
#define MUSYCL_SOUND_GENERATOR_DCO_ENVELOPE_HPP

#include <range/v3/all.hpp>

#include "../config.hpp"
#include "../pipe/audio.hpp"
#include "../pipe/midi_in.hpp"

namespace musycl {

/// A digitally controlled oscillator with an evolving volume envelope
class dco_envelope : public dco, public clock::follow<dco_envelope> {
  /// Track whether sustain pedal is sustaining this sound
  bool sustain_pedal_in_action;

  /// Memorize the note to stop at the end of the sustain pedal action
  midi::off note_off;

public:

  /// Control the volume evolution of the sound
  envelope env;

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
      the audio frequency. */
  void frame_clock() {
    if (sustain_pedal_in_action && !sustain::value())
      // The sustain pedal was in action but is just depressed
      env.stop();

    volume = env.out();
    if (!is_running())
      dco::stop(note_off);
  }

};

}
#endif // MUSYCL_SOUND_GENERATOR_DCO_ENVELOPE_HPP
