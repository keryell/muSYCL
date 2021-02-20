#ifndef MUSYCL_ARPEGGIATOR_HPP
#define MUSYCL_ARPEGGIATOR_HPP

/// \file Arpeggiator to generate notes from a note flow

#include <vector>

#include "config.hpp"

#include "clock.hpp"
#include "midi.hpp"
#include "pipe/midi_in.hpp"

namespace musycl {

class arpeggiator : public clock::follow<arpeggiator> {

  std::vector<musycl::midi::on> notes;

  /// Index of the next note to play
  int note_index = 0;

  /// Current note
  std::optional<musycl::midi::on> current_note;

public:


  /** Handle MIDI note events

      \param[in] a MIDI event to process

      \return the arpeggiator itself to enable command chaining
  */
  auto& midi(const musycl::midi::msg& m) {
    std::visit(trisycl::detail::overloaded {
        [&] (const musycl::midi::on& on) {
          if (on.channel == 0 && on.note < 60)
            notes.push_back(on);
        },
        [&] (const musycl::midi::off& off) {
          if (off.channel == 0 && off.note < 60)
            // Remove the same note without looking at the velocity
            std::erase_if(notes, [&] (const auto& n) {
                                   return n.channel == off.channel
                                     && n.note == off.note; });
        },
        [] (auto &&other) {}
          }, m);
    return *this;
  }


  void beat() {
    if (current_note) {
      musycl::midi_in::insert(current_note->as_off());
      current_note.reset();
    }
    if (!notes.empty()) {
      // Wrap around if we reached the end
      if (note_index >= notes.size())
        note_index = 0;
      auto n = notes[note_index];
      // Replay this note on channel 2 except the first one going on 3
      n.channel = 1 + (note_index == 0);
      n.note += 12 - 24*(note_index == 0);
      current_note = n;
      musycl::midi_in::insert(n);
      ++note_index;
    }
  }

};

}

#endif // MUSYCL_ARPEGGIATOR_HPP
