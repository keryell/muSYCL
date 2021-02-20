#ifndef MUSYCL_ARPEGGIATOR_HPP
#define MUSYCL_ARPEGGIATOR_HPP

/// \file Arpeggiator to generate notes from a note flow

#include <vector>

#include "config.hpp"

#include "midi.hpp"
#include "pipe/midi_in.hpp"

namespace musycl {

class arpeggiator {

  std::vector<musycl::midi::on> notes;

  /// The phase in the waveform, between 0 and 1
  float phase = 0;

  /// The phase increment per clock to generate the right frequency
  float dphase {};

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

  /** Set the arpeggiator frequency

      \return the arpeggiator itself to enable command chaining
  */
  auto& set_frequency(float frequency) {
    dphase = frequency*frame_size/sample_frequency;
    std::cout << "arpeggiator frequency = " << frequency << " Hz, period = "
              << 1/frequency << " s, "
              << static_cast<int>(frequency*60) << " bpm" << std::endl;
    return *this;
  }


  auto& tick_frame_clock() {
    phase = phase + dphase;
    if (phase >= 1) {
      phase -= 1;
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
    return *this;
  }
};

}

#endif // MUSYCL_ARPEGGIATOR_HPP
