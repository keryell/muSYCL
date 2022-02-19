#ifndef MUSYCL_SUSTAIN_HPP
#define MUSYCL_SUSTAIN_HPP

/** \file Abstraction transmitting sustain

    The sustain pedal broadcast its status to any interested modules.

    \todo Implement as a range transform
*/

#include <queue>
#include <variant>

#include <triSYCL/detail/overloaded.hpp>

#include "midi/midi_in.hpp"
#include "musycl/midi.hpp"

namespace musycl {

/// The sustain pedal
class sustain {
  bool state = false;

  bool just_released = false;

  std::queue<musycl::midi::msg> sustained_notes;

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

  \param[out] MIDI message produced after processing the MIDI input according to
  sustain

  \return whether a MIDI message is produced to output for further
  consumption
  */
  bool process(int midi_port, musycl::midi::msg& m) {
    // If the sustain pedal has just been released, stop any pending note
    if (just_released && !sustained_notes.empty()) {
      m = sustained_notes.front();
      sustained_notes.pop();
      // We have produced a MIDI message to process
      return true;
    }
    // No longer any sustained note back-log to process
    just_released = false;
    if (musycl::midi_in::try_read(0, m)) {
      // \todo Improve behavior when a holding note is replayed
      if (state && std::holds_alternative<musycl::midi::off>(m)) {
        sustained_notes.push(m);
        // Do not return the note-off message for now
        return false;
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
