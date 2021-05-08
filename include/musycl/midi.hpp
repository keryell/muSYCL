#ifndef MUSYCL_MIDI_HPP
#define MUSYCL_MIDI_HPP

/** \file Abstractions for MIDI messages

    Some MIDI background information can be found on
    https://www.midi.org/ (free registration) in documents such as the
    "MIDI 1.0 Detailed Specification", Document Version 4.2.1, Revised
    February 1996
    https://www.midi.org/specifications/midi1-specifications/m1-v4-2-1-midi-1-0-detailed-specification-96-1-4
*/

#include <cmath>
#include <cstdint>
#include <string>
#include <variant>

namespace musycl::midi {

/// MIDI has 128 notes
auto constexpr note_number = 128;

/// Type for MIDI channel values
using channel_type = std::int8_t;

/// Type for MIDI note values
using note_type = std::int8_t;

/// Type for MIDI velocity values
using velocity_type = std::int8_t;

/// The "note" MIDI message header, split from note_base for indexing purpose
class note_base_header {
 public:
  /* Use signed integers because it is less disturbing when doing
     arithmetic on values */

  /// The channel number between 0 and 15
  channel_type channel;

  /// The note number between 0 and 127, C3 note is 60
  note_type note;

  note_base_header() = default;

  /// A specific constructor to handle unsigned to signed narrowing
  template <typename Channel, typename Note>
  note_base_header(Channel&& c, Note&& n)
      : channel { static_cast<channel_type>(c) }
      , note { static_cast<note_type>(n) } {}
};

/// A "note" MIDI message implementation detail
template <typename Header> class note_base : public Header {
 public:
  /// The note velocity between 0 and 127
  velocity_type velocity;

  note_base() = default;

  /// A specific constructor to handle unsigned to signed narrowing
  template <typename Channel, typename Note, typename Velocity>
  note_base(Channel&& c, Note&& n, Velocity&& v)
      : Header { c, n }
      , velocity { static_cast<velocity_type>(v) } {}

  /// The velocity normalized in [ 0, 1 ]
  float velocity_1() const { return velocity / 127.f; }

  /// Get the header of this message
  const Header& header() const { return *this; }
};

/// Compute the frequency of a MIDI note with an optional transposition
float frequency(int n, const float transpose_semi_tone = 0) {
  /* The frequency for a 12-tone equal temperament scale with 440 Hz
     A3 note being MIDI note 69 */
  return 440 * std::pow(2.f, (n + transpose_semi_tone - 69) / 12);
}

/// Compute the frequency of a MIDI note with an optional transposition
float frequency(const note_base_header& n,
                const float transpose_semi_tone = 0) {
  return frequency(n.note, transpose_semi_tone);
}

/// A "note off" MIDI header is just a kind of note header
class off_header : public note_base_header {
  /// Inherit the note_base constructors
  using note_base_header::note_base_header;
};

/// A "note off" MIDI message is just a kind of note
class off : public note_base<off_header> {
  /// Inherit the note_base constructors
  using note_base<off_header>::note_base;
};

/// A "note on" MIDI header is just a kind of note header
class on_header : public note_base_header {
  /// Inherit the note_base constructors
  using note_base_header::note_base_header;
};

/// A "note on" MIDI message is just a kind of note
class on : public note_base<on_header> {
  /// Inherit the note_base constructors
  using note_base<on_header>::note_base;

 public:
  /// Get an note-off message for this note
  const off& as_off() { return *reinterpret_cast<off*>(this); }
};

/// he MIDI "control change" header, split from message for indexing purpose
class control_change_header {
 public:
  /* Use signed integers because it is less disturbing when doing
     arithmetic on values */

  /// The channel number between 0 and 15
  channel_type channel;

  /// Type for MIDI control change number
  using number_type = std::int8_t;

  /// The number control change
  number_type number;

  /// A specific constructor to handle unsigned to signed narrowing
  template <typename Channel, typename Number>
  control_change_header(Channel&& c, Number&& n)
      : channel { static_cast<channel_type>(c) }
      , number { static_cast<number_type>(n) } {}
};

/// The MIDI "control change" message
class control_change : public control_change_header {
 public:
  /// Type for MIDI control change value
  using value_type = std::int8_t;

  /// The value associated to this control change
  value_type value;

  /// A specific constructor to handle unsigned to signed narrowing
  template <typename Channel, typename Number, typename Value>
  control_change(Channel&& c, Number&& n, Value&& v)
      : control_change_header { c, n }
      , value { static_cast<value_type>(v) } {}

  /// Get the header of this message
  const control_change_header& header() const { return *this; }

  /// The value normalized in [ 0, 1 ] as a given type
  template <typename Value> constexpr static Value get_value_as(value_type v) {
    return v / Value { 127 };
  }

  /// The value normalized in [ low, high ] as a given type
  /// \todo To move out of this namespace
  constexpr static float get_value_in(value_type v, float low, float high) {
    return low + get_value_as<float>(v) * (high - low);
  }

  /// The value normalized in [ low, high ] as a given type
  /// \todo To move out of this namespace
  constexpr static float get_log_scale_value_in(value_type v, float low,
                                                float high) {
    return low * std::exp(std::log(high / low) * get_value_as<float>(v));
  }

  /// The value normalized in [ 0, 1 ]
  float value_1() const { return get_value_as<float>(value); }
};

/** A MIDI message can be one of different types, including the
    monostate for empty message at initialization */
using msg =
    std::variant<std::monostate, midi::on, midi::off, midi::control_change>;

/** A type representing the processed MIDI messages headers without
    the value, just with the message type for indexing purpose */
class msg_header {
  using variant_t = std::variant<std::monostate, midi::on_header,
                                 midi::off_header, midi::control_change_header>;
  variant_t variant;

  msg_header(const msg& m)
      : variant { std::visit(
            [&](const auto& e) {
              if constexpr (std::is_same_v<decltype(e), const std::monostate&>)
                return variant_t {};
              else
                return variant_t { e.header() };
            },
            m) } {}
};

/// Get the 4 MSB bits of the MIDI status byte that give the command kind
static inline std::int8_t status_high(std::uint8_t first_byte) {
  return first_byte >> 4;
}

/// Get the channel number of the MIDI status byte
static inline std::int8_t channel(std::uint8_t first_byte) {
  return first_byte & 0b1111;
}

/// Parse a MIDI byte message into a specific MIDI instruction
msg parse(const std::vector<std::uint8_t>& midi_message) {
  msg m;
  if (!midi_message.empty()) {
    auto sh = status_high(midi_message[0]);
    /// Interesting MIDI messages have 3 bytes
    if (midi_message.size() == 3) {
      if (sh == 9 && midi_message[2] != 0)
        // Start the note on the given channel if the velocity is non 0
        m = midi::on { channel(midi_message[0]), midi_message[1],
                       midi_message[2] };
      else if (sh == 8 //< Note-off status
                       // But a note-on with a 0-velocity means also a note-off
               || (sh == 9 && midi_message[2] == 0))
        // Stop the note on the given channel
        m = midi::off { channel(midi_message[0]), midi_message[1],
                        midi_message[2] };
      else if (sh == 0xb)
        // This is a control change message
        m = midi::control_change { channel(midi_message[0]), midi_message[1],
                                   midi_message[2] };
    }
  }
  return m;
}

} // namespace musycl::midi

#endif // MUSYCL_MIDI_HPP
