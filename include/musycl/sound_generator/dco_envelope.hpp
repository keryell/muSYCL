/// \file A preset with a DCO and an envelope generator

#ifndef MUSYCL_SOUND_GENERATOR_DCO_ENVELOPE_HPP
#define MUSYCL_SOUND_GENERATOR_DCO_ENVELOPE_HPP

#include <range/v3/all.hpp>

#include "../config.hpp"

#include "../audio.hpp"
#include "../dco.hpp"
#include "../envelope.hpp"
#include "../group.hpp"
#include "../midi.hpp"
#include "../sustain.hpp"

namespace musycl {

/// A digitally controlled oscillator with an evolving volume envelope
class dco_envelope
    : public dco
    , public clock::follow<dco_envelope> {
  /// Track whether sustain pedal is sustaining this sound
  bool sustain_pedal_in_action = false;

  /// Memorize the note to stop at the end of the sustain pedal action
  midi::off note_off;

 public:
  /// All the parameters behind this sound generator
  class param_detail : public group {
   public:
    param_detail(auto&&... args)
        : group { std::forward<decltype(args)>(args)... }
        , dco { std::forward<decltype(args)>(args)... }
        , env { std::forward<decltype(args)>(args)... } {
      dco->add_as_sub_group_to(*this);
      env->add_as_sub_group_to(*this);
    }

    /// The DCO parameters
    dco::param_t dco;

    /// The envelop parameters
    envelope::param_t env;
  };

  // Shared parameter between all copies of this envelope generator
  using param_t = control::param<param_detail, dco_envelope>;

  /// The sound parameters
  param_t param;

  /// Control the volume evolution of the sound
  envelope env;

  /// Create a sound from its parameters
  dco_envelope(const param_t& p)
      : dco { p->dco }
      , param { p }
      , env { p->env } {}

  /** Start a note

      \param[in] on is the "note on" MIDI event to start with

      \return itself to allow operation chaining
  */
  auto& start(const midi::on& on) {
    env.start();
    dco::start(on);
    volume = env.out();
    std::cout << "Start " << on
              << " with volume " << volume << std::endl;
    std::cout << to_string(env) << std::endl;
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
  bool is_running() { return env.is_running(); }

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

} // namespace musycl
#endif // MUSYCL_SOUND_GENERATOR_DCO_ENVELOPE_HPP
