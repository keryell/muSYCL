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

namespace musycl {

/// A digitally controlled oscillator with an evolving volume envelope
class dco_envelope
    : public dco
    , public clock::follow<dco_envelope> {
  /// Memorize the note to stop at the end of envelope management
  midi::off note_off;

 public:
  /// All the parameters behind this sound generator
  class param_detail : public group {
   public:
    param_detail(auto&&... args)
        : group { std::forward<decltype(args)>(args)... }
        , dco_param { std::forward<decltype(args)>(args)... }
        , env_param { std::forward<decltype(args)>(args)... } {
      dco_param->add_as_sub_group_to(*this);
      env_param->add_as_sub_group_to(*this);
    }

    /// The DCO parameters
    dco::param_t dco_param;

    /// The envelop parameters
    envelope::param_t env_param;
  };

  // Shared parameter between all copies of this envelope generator
  using param_t = control::param<param_detail, dco_envelope>;

  /// The sound parameters
  param_t param;

  /// Control the volume evolution of the sound
  envelope env;

  /// Create a sound from its parameters
  dco_envelope(const param_t& p)
      : dco { p->dco_param }
      , param { p }
      , env { p->env_param } {}

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
    // Postpone the note-off since it is now handled by the envelope generator
    note_off = off;
    env.stop();
    volume = env.out();
    return *this;
  }

  /// Return the running status
  bool is_running() { return env.is_running(); }

  /** Update the envelope at the frame frequency

      Since it is an envelope generator, no need to update it at
      the audio frequency. */
  void frame_clock() {
    volume = env.out();
    if (!is_running())
      // Finalize the note only when the envelope decides to
      dco::stop(note_off);
  }
};

} // namespace musycl
#endif // MUSYCL_SOUND_GENERATOR_DCO_ENVELOPE_HPP
