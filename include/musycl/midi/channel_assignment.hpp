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

  void assign(int channel, const musycl::sound_generator::param_t& sound) {
    std::cerr << "Assign channel " << channel << std::endl;
    channels[channel] = sound;
  }
};

} // namespace musycl::midi

#endif // MUSYCL_MIDI_CHANNEL_ASSIGNMENT_HPP
