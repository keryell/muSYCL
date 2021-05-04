#ifndef MUSYCL_ARPEGGIATOR_HPP
#define MUSYCL_ARPEGGIATOR_HPP

/// \file Arpeggiator to generate notes from a note flow

#include <algorithm>
#include <optional>
#include <variant>
#include <vector>

#include <triSYCL/detail/overloaded.hpp>

#include <range/v3/all.hpp>

#include "config.hpp"

#include "clock.hpp"
#include "midi.hpp"
#include "midi/midi_in.hpp"

namespace musycl {

class arpeggiator : public clock::follow<arpeggiator> {

  /// The notes to play with
  std::vector<midi::on> notes;

  /// Index of the next note to play
  int note_index = 0;

  /// Current note
  std::optional<midi::on> current_note;

public:

  /** Handle MIDI note events

      \param[in] a MIDI event to process

      \return the arpeggiator itself to enable command chaining
  */
  auto& midi(const midi::msg& m) {
    std::visit(trisycl::detail::overloaded {
        [&] (const midi::on& on) {
          if (on.note < 60)
            notes.push_back(on);
        },
        [&] (const midi::off& off) {
          if (off.note < 60)
            // Remove the same note without looking at the velocity
            std::erase_if(notes, [&] (const auto& n) {
                                   return n.channel == off.channel
                                     && n.note == off.note; });
        },
        [] (auto &&other) {} ///< Ignore anything else
          }, m);
    return *this;
  }


  /// This is notified on each beat by the clocking framework
  void beat(clock::type ct) {
    if (current_note) {
      midi_in::insert(0, current_note->as_off());
      current_note.reset();
    }
    if (!notes.empty()) {
      // Find the lowest note
      int bass = std::distance
        (notes.begin(),
         std::ranges::min_element(notes, {}, &midi::on::note));
      // Wrap around if we reached the end
      if (note_index >= notes.size())
        note_index = 0;
      auto n = notes[ct.measure ? bass : note_index];
      // Replay this note on channel 2 except the first one going on 3
      n.channel = ct.measure ? 2 : ct.beat_index == 2 ? 3 : 1;
      n.note += 24 - 36*ct.measure;
      if (ct.beat_index == 2)
        n.velocity = 127;
      current_note = n;
      midi_in::insert(0, n);
      ++note_index;
    }
  }

};

}

#endif // MUSYCL_ARPEGGIATOR_HPP
