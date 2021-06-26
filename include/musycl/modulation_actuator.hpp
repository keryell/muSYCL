#ifndef MUSYCL_MODULATION_ACTUATOR_HPP
#define MUSYCL_MODULATION_ACTUATOR_HPP

/** \file Represent the modulation actuator, like a modulation wheel or lever

    The modulation actuator broadcasts its status to any interested modules.
*/

#include "musycl/midi/midi_in.hpp"

namespace musycl {

/// The modulation actuator, such as a modulation wheel
class modulation_actuator {
  /// Store the values from 0 to +1
  static inline float state = 0;

 public:
  modulation_actuator(std::int8_t port, std::int8_t channel) {
    midi_in::cc_action(port, channel, 1, [](float v) { value(v); });
  }

  static float value() { return state; }

  static void value(auto v) { state = v; }
};

} // namespace musycl

#endif // MUSYCL_MODULATION_ACTUATOR_HPP
