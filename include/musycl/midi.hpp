#ifndef MUSYCL_MIDI_HPP
#define MUSYCL_MIDI_HPP

/// \file Abstractions for MIDI messages

#include <cstdint>
#include <string>
#include <variant>

namespace musycl::midi {

/// A "note on" MIDI message
class on {
public :
  std::int8_t channel;
  std::int8_t note;
  std::int8_t velocity;
};


/// A "note off" MIDI message
class off {
public :
  std::int8_t channel;
  std::int8_t note;
  std::int8_t velocity;
};


/** A MIDI message can be one of different types, including the
    monostate for empty message at initialization */
using msg = std::variant<std::monostate, midi::on, midi::off>;

}

#endif // MUSYCL_MIDI_HPP
