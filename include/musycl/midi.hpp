#ifndef MUSYCL_MIDI_HPP
#define MUSYCL_MIDI_HPP

/// \file Abstractions for MIDI messages

#include <cstdint>
#include <string>
#include <variant>

namespace musycl::midi {

/// A "note" MIDI message implementation detail
class note {
public :
  std::int8_t channel;
  std::int8_t note;
  std::int8_t velocity;
};

/// A "note on" MIDI message is just a kind of note
class on : public note {};

/// A "note off" MIDI message is just a kind of note
class off : public note {};

/** A MIDI message can be one of different types, including the
    monostate for empty message at initialization */
using msg = std::variant<std::monostate, midi::on, midi::off>;

}

#endif // MUSYCL_MIDI_HPP
