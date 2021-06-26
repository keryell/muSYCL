#ifndef MUSYCL_PITCH_BEND_HPP
#define MUSYCL_PITCH_BEND_HPP

/** \file Represent the pitch bend actuator

    The pitch bend broadcasts its status to any interested modules.
*/

#include "musycl/midi/midi_in.hpp"

namespace musycl {

/// The pitch wheel
class pitch_bend {
  /// Store the values from -1 to +1, with 0 as the rest value
  static inline float state = 0;

 public:
  pitch_bend(std::int8_t port, std::int8_t channel) {
    midi_in::add_action(port, midi::pitch_bend_header { channel },
                        [](const midi::msg& m) {
                          value(std::get<midi::pitch_bend>(m).value_1());
                        });
  }

  static float value() { return state; }

  static void value(auto v) { state = v; }
};

} // namespace musycl

#endif // MUSYCL_PITCH_BEND_HPP
