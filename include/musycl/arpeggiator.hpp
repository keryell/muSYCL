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

  /// Control whether the sequencer is running
  bool running = false;

 public:
  /** Handle MIDI note events

      \param[in] a MIDI event to process

      \return the arpeggiator itself to enable command chaining
  */
  auto& midi(const midi::msg& m) {
    std::visit(trisycl::detail::overloaded {
                   [&](const midi::on& on) {
                     if (on.note < 60)
                       notes.push_back(on);
                   },
                   [&](const midi::off& off) {
                     if (off.note < 60)
                       // Remove the same note without looking at the velocity
                       std::erase_if(notes, [&](const auto& n) {
                         return n.channel == off.channel && n.note == off.note;
                       });
                   },
                   [](auto&& other) {} ///< Ignore anything else
               },
               m);
    return *this;
  }

  /// Start or stop the sequencer
  void run(bool is_running) {
    std::cerr << "Run " << is_running << std::endl;

    if (running && !is_running)
      // If the sequencer is going to stop, stop the current note
      stop_current_note();
    running = is_running;
  }

  /// Stop current note
  void stop_current_note() {
    if (current_note) {
      midi_in::insert(0, current_note->as_off());
      current_note.reset();
    }
  }

  /// This is notified on each MIDI clock by the clocking framework
  void midi_clock(clock::type ct) {
    if (!running)
      return;

    // Work on the 16th of note
    if (ct.midi_clock_index % (midi::clock_per_quarter / 4) == 0) {
      stop_current_note();
      if (!notes.empty()) {
        // Find the lowest note
        int bass =
            std::distance(notes.begin(),
                          std::ranges::min_element(notes, {}, &midi::on::note));
        // Wrap around if we reached the end
        if (note_index >= notes.size())
          note_index = 0;
        auto n = notes[ct.measure ? bass : note_index];
        // Replay this note on channel 2 except the first one going on 3
        n.channel = ct.measure ? 2 : ct.beat_index == 2 ? 3 : +1;
        n.note += 24 - 36 * ct.measure;
        if (ct.beat_index == 2)
          n.velocity = 127;
        current_note = n;
        midi_in::insert(0, n);
        std::cout << "Insert " << n << std::endl;
        ++note_index;
      }
    }
  }
};

} // namespace musycl

#endif // MUSYCL_ARPEGGIATOR_HPP
