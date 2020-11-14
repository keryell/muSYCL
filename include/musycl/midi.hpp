#ifndef MUSYCL_MIDI_HPP
#define MUSYCL_MIDI_HPP

/** \file Abstractions for MIDI messages

    Some MIDI background information can be found on
    https://www.midi.org/ (free registration) in documents such as the
    "MIDI 1.0 Detailed Specification", Document Version 4.2.1, Revised
    February 1996
    https://www.midi.org/specifications/midi1-specifications/m1-v4-2-1-midi-1-0-detailed-specification-96-1-4
*/

#include <cstdint>
#include <string>
#include <variant>

namespace musycl::midi {

/// A "note" MIDI message implementation detail
class note_base {
public :
  /* Use signed integers because it is less disturbing when doing
     arithmetic on values */

  /// The channel number between 0 and 15
  std::int8_t channel;

  /// The note number between 0 and 127, C3 note is 60
  std::int8_t note;

  /// The note velocity between 0 and 127
  std::int8_t velocity;

  note_base() = default;

  /// A specific constructor to handle unsigned to signed narrowing
  template <typename Channel, typename Note, typename Velocity>
  note_base(Channel&& c, Note&& n, Velocity&& v)
    : channel { static_cast<int8_t>(c) }
    , note { static_cast<int8_t>(n) }
    , velocity { static_cast<int8_t>(v) }
  {}
};

/// A "note on" MIDI message is just a kind of note
class on : public note_base {
  /// Inherit the note_base constructors
  using note_base::note_base;
};

/// A "note off" MIDI message is just a kind of note
class off : public note_base {
  /// Inherit the note_base constructors
  using note_base::note_base;
};

/** A MIDI message can be one of different types, including the
    monostate for empty message at initialization */
using msg = std::variant<std::monostate, midi::on, midi::off>;


/// Get the 4 MSB bits of the MIDI status byte that give the command kind
static inline std::int8_t high_status(std::uint8_t first_byte) {
  return first_byte>>4;
}


/// Parse a MIDI byte message into a specific MIDI instruction
msg parse(const std::vector<std::uint8_t>& midi_message) {
  msg m;
  if (!midi_message.empty()) {
    auto hs = high_status(midi_message[0]);
    /// Interesting MIDI messages have 3 bytes
    if (midi_message.size() == 3) {
      if (hs == 9 && midi_message[2] != 0)
        // Start the note
        m = midi::on { 0, midi_message[1], midi_message[2] };
      else if (hs == 8 //< Note-off status
               // But a note-on with a 0-velocity means also a note-off
               || (hs == 9 && midi_message[2] == 0))
        // Stop the note
        m = midi::off { 0, midi_message[1], midi_message[2] };
    }
  }
  return m;
}

}

#endif // MUSYCL_MIDI_HPP
