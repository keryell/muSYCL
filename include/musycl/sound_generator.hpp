#ifndef MUSYCL_SOUND_GENERATOR_HPP
#define MUSYCL_SOUND_GENERATOR_HPP

/// \file Concept of sound generators to be used to play a note

#include <variant>

#include "dco.hpp"
#include "pipe/audio.hpp"
#include "pipe/midi_in.hpp"
#include "sound_generator/dco_envelope.hpp"

namespace musycl {

class sound_generator {

  /** A sound generator is a variant of types which can be used as sound
      generators */
  using sound_generator_t = std::variant<dco, dco_envelope, noise>;

  sound_generator_t sg;

public:

  sound_generator() = default;


  /// Construct from an object copy capable of generating sound
  template <typename T>
  sound_generator(T s)
    : sg { s }
  {}


  /// Get the underlying sound generator
  template <typename T>
  auto& get() {
    return std::get<T>(sg);
  }


  /** Start the sound generator

      \param[in] on is the "note on" MIDI event to start with

      \return itself to allow operation chaining
  */
  sound_generator& start(const musycl::midi::on& on) {
    std::visit([&] (auto &s) { s.start(on); }, sg);
    return *this;
  }


  /** Stop the sound generator

      \param[in] off is the "note off" MIDI event to stop with

      \return itself to allow operation chaining
  */
  sound_generator& stop(const musycl::midi::off& off) {
    std::visit([&] (auto &s) { s.stop(off); }, sg);
    return *this;
  }


  /// Generate an audio sample
  musycl::audio::frame audio() {
    return std::visit([&] (auto &s) { return s.audio(); }, sg);
  }


  /// Return the running status
  bool is_running() {
    return std::visit([&] (auto &s) { return s.is_running(); }, sg);
  }

};


}


#endif // MUSYCL_SOUND_GENERATOR_HPP
