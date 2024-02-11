#ifndef MUSYCL_MIDI_CHANNEL_ASSIGNMENT_HPP
#define MUSYCL_MIDI_CHANNEL_ASSIGNMENT_HPP

/** \file Map MIDI channel to some sound generators
 */

#include <map>

//#include "musycl/midi.hpp"
#include "musycl/sound_generator.hpp"

namespace musycl::midi {

/** The (MIDI) channel mapping to the sound parameter

    Note that there might be more channels than the 16 real MIDI
    channels for convenience, for example to have more sounds for
    arpeggiators
*/
class channel_assignment {
 public:
  std::map<int, musycl::sound_generator::param_t> channels;

  /// \todo set but unused in musycl_synth.cpp by parsing
  /// musycl::midi::sysex

  /** Typically selected channel in the UI

      -1 when there is no selected channel. Do not use iterator on the
       std::map since it might become invalid. */
  int current_selected_channel = -1;

  /// Assign a sound parameter to a channel
  void assign(int channel, const musycl::sound_generator::param_t& sound) {
    std::cerr << "Assign channel " << channel << std::endl;
    channels[channel] = sound;
  }

  /// \todo What is this code about ? What happens if there is no assignment ?

  /// Select the next channel if any, wrapping around to the first one
  void select_next_channel() {
    if (channels.empty()) {
      current_selected_channel = -1;
      return;
    }
    auto c = channels.find(current_selected_channel);
    if (c != channels.end())
      c = std::next(c);
    // Wrap around if we reach the end
    if (c == channels.end())
      c = channels.begin();
    current_selected_channel = c->first;
  }

  /// Select the previous channel if any, wrapping around to the last one
  void select_previous_channel() {
    if (channels.empty()) {
      current_selected_channel = -1;
      return;
    }
    auto c = channels.find(current_selected_channel);
    if (c == channels.end())
      c = channels.begin();
    // Wrap around if we start from the beginning
    if (c == channels.begin())
      c = channels.end();
    c = std::prev(c);
    current_selected_channel = c->first;
  }
};

} // namespace musycl::midi

#endif // MUSYCL_MIDI_CHANNEL_ASSIGNMENT_HPP
