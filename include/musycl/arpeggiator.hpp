#ifndef MUSYCL_ARPEGGIATOR_HPP
#define MUSYCL_ARPEGGIATOR_HPP

/// \file Arpeggiator to generate notes from a note flow

#include <algorithm>
#include <functional>
#include <optional>
#include <variant>
#include <vector>

#include <triSYCL/detail/overloaded.hpp>

#include <range/v3/all.hpp>

#include "config.hpp"

#include "clock.hpp"
#include "midi.hpp"
#include "midi/midi_in.hpp"
#include "music_theory.hpp"

namespace musycl {

class arpeggiator : public clock::follow<arpeggiator> {
 public:
  /// Ignore note lower than this one
  midi::note_type low_input_limit;

  /// Ignore note equal to or higher than this one
  midi::note_type high_input_end;

  /// The notes to play with
  std::vector<midi::on> notes;

  /// Index of the next note to play
  int note_index = 0;

  /// Current note
  std::optional<midi::on> current_note;

  /// Current clock time
  clock::type current_clock_time;

  /// Control whether the sequencer is running
  bool running = false;

  /// Type of an arpeggiator procedure
  using arpeggiator_engine_t = std::function<void(arpeggiator&)>;

  /// User-provided arpeggiator procedure
  arpeggiator_engine_t arpeggiator_engine;

  /// An action to call when the arpeggiator is stopped
  std::function<void()> stop_action;

  /** Create an arpeggiator sensitive to notes between low and high inclusive

      \input[in] c is a callable implementing the arpeggiator or use a
      default one
  */
  arpeggiator(midi::note_type low = 0, midi::note_type high = 60,
              arpeggiator_engine_t ae = default_arpeggiator)
      : low_input_limit { low }
      , high_input_end { high }
      , arpeggiator_engine { ae } {}

  /** Handle MIDI note events

      \param[in] a MIDI event to process

      \return the arpeggiator itself to enable command chaining
  */
  auto& midi(const midi::msg& m) {
    std::visit(trisycl::detail::overloaded {
                   [&](const midi::on& on) {
                     if (low_input_limit <= on.note &&
                         on.note < high_input_end && 0 == on.channel)
                       notes.push_back(on);
                   },
                   [&](const midi::off& off) {
                     if (low_input_limit <= off.note &&
                         off.note < high_input_end && 0 == off.channel)
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
    if (stop_action)
      stop_action();
  }

  /// This is notified on each MIDI clock by the clocking framework
  void midi_clock(clock::type ct) {
    if (!running)
      return;

    current_clock_time = ct;
    arpeggiator_engine(*this);
  }

  static void default_arpeggiator(arpeggiator& arp) {
    // Otherwise use a default arpeggiator, work on the 16th of note
    if (arp.current_clock_time.midi_clock_index %
            (midi::clock_per_quarter / 4) ==
        0) {
      arp.stop_current_note();
      if (!arp.notes.empty()) {
        // Find the lowest note
        int bass = std::distance(
            arp.notes.begin(),
            std::ranges::min_element(arp.notes, {}, &midi::on::note));
        // Wrap around if we reached the end
        if (arp.note_index >= arp.notes.size())
          arp.note_index = 0;
        auto n =
            arp.notes[arp.current_clock_time.measure ? bass : arp.note_index];
        // Replay this note on channel 2 except the first one going on 3
        n.channel = arp.current_clock_time.measure           ? 2
                    : arp.current_clock_time.beat_index == 2 ? 3
                                                             : 1;
        n.note += 24 - 36 * arp.current_clock_time.measure;
        if (arp.current_clock_time.beat_index == 2)
          n.velocity = 127;
        arp.current_note = n;
        midi_in::insert(0, n);
        std::cout << "Insert " << n << std::endl;
        ++arp.note_index;
      }
    }
  }
};
} // namespace musycl

#endif // MUSYCL_ARPEGGIATOR_HPP
