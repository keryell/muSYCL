#ifndef MUSYCL_SUSTAIN_HPP
#define MUSYCL_SUSTAIN_HPP

/** \file Abstraction transmitting sustain

    The sustain pedal broadcast its status to any interested modules.

    \todo Implement as a range transform to simplify the state-full logic
*/

#include <map>
#include <optional>
#include <variant>

#include "midi/midi_in.hpp"
#include "musycl/midi.hpp"

namespace musycl {

/// The sustain pedal
class sustain {
  /// Status of the sustain pedal
  bool state = false;

  /// Track whether the sustain pedal has just been released
  bool just_released = false;

  /** Keep track of the sustained notes by holding the off
      messages. This is indexed by the note header so it is possible
      to end a sustained note first if the same note is replayed in
      the meantime while sustain pedal is still hold */
  std::map<midi::note_base_header, midi::msg> sustained_notes;

  /// Keep track of a note-on message which has to be processed later
  std::optional<midi::msg> postponed_note_on;

 public:
  /// Get the current state of the sustain pedal
  bool value() { return state; }

  /// Set the current state of the sustain pedal
  void value(bool v) {
    // If we have a sustain→no sustain transition, book some further processing
    just_released = state && !v;
    state = v;
  }

  /** Add sustain on a MIDI flow by postponing MIDI off-note while
      sustain is on

      \param[in] midi_port is the MIDI port to listen to

      \param[out] MIDI message produced after processing the MIDI
      input according to sustain

      \return whether a MIDI message is produced to output for further
      consumption
  */
  bool process(int midi_port, midi::msg& m) {
    /* If there is a pending note-on message, first process it, to a
       void a corresponding note-off to be scheduled before */
    if (postponed_note_on) {
      m = *postponed_note_on;
      postponed_note_on.reset();
      return true;
    }
    // If the sustain pedal has just been released, stop any pending note
    if (just_released && !sustained_notes.empty()) {
      // Pick a holding note-off and send it
      m = sustained_notes.cbegin()->second;
      sustained_notes.erase(sustained_notes.cbegin());
      // We have produced a MIDI message to process
      return true;
    }
    // No longer any sustained note back-log to process
    just_released = false;
    // Now we can process the MIDI input
    if (midi_in::try_read(0, m)) {
      // If the sustain is on and a not-off comes, just put it on hold
      if (state && std::holds_alternative<midi::off>(m)) {
        sustained_notes.insert_or_assign(std::get<midi::off>(m).base_header(),
                                         m);
        // Do not return the note-off message for now
        return false;
      }
      if (state && std::holds_alternative<midi::on>(m)) {
        // If we replay a sustained note, first stop the sustained note
        if (auto it = sustained_notes.find(std::get<midi::on>(m).base_header());
            it != sustained_notes.end()) {
          postponed_note_on = m;
          // Return the sustained note-off
          m = it->second;
          return true;
        }
      }
      // Pass-through any other message
      return true;
    }
    // No MIDI message to handle
    return false;
  }
};

} // namespace musycl

#endif // MUSYCL_SUSTAIN_HPP
