#ifndef MUSYCL_SOUND_GENERATOR_HPP
#define MUSYCL_SOUND_GENERATOR_HPP

/// \file Concept of sound generators to be used to play a note

#include <type_traits>
#include <variant>

#include "audio.hpp"
#include "dco.hpp"
#include "group.hpp"
#include "midi.hpp"
#include "noise.hpp"
#include "sound_generator/dco_envelope.hpp"

namespace musycl {

class sound_generator {
 public:
  /** A sound generator is a variant of types which can be used as sound
      generators */
  using sound_generator_t = std::variant<dco, dco_envelope, noise>;

  sound_generator_t sg;

  using pointer = sound_generator*;

  ///  Parameter of the sound generators
  class param_t {
    /// \todo generate from sound_generator_t
    using detail =
        std::variant<dco::param_t, dco_envelope::param_t, noise::param_t>;

   public:
    detail param;

    param_t() = default;

    /// Construct from an object copy capable of generating sound
    template <typename T>
    param_t(T p)
        : param { p } {}

    // Create a sound_generator object from a sound parameter
    auto from_param() const {
      return std::visit(
          [&](const auto& s) {
            return sound_generator {
              typename std::remove_cvref_t<decltype(s)>::owner_t { s }
            };
          },
          param);
    }

    // Get the name of a sound parameter set
    const std::string& name() const {
      return std::visit(
          [&](const auto& s) -> const std::string& { return s->name; }, param);
    }

    // Get the controlling group of a sound parameter set
    const group& get_group() const {
      return std::visit(
          // Need "class" here to avoid conflict with the member function
          // around.
          [&](const auto& s) -> const class group& {
            // Get the group parent of the sound parameter
            return *s.implementation;
          },
          param);
    }
  };

  sound_generator() = default;


  /// Construct from an object copy capable of generating sound
  template <typename T>
  sound_generator(T s)
    : sg { s }
  {}


  /// Create a sound generator from a parameter set
  sound_generator(const param_t& param)
    : sound_generator { param.from_param() } {}

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
