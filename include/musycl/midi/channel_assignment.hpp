#ifndef MUSYCL_MIDI_CHANNEL_ASSIGNMENT_HPP
#define MUSYCL_MIDI_CHANNEL_ASSIGNMENT_HPP

/** \file Map MIDI channel to some sound generators
*/

#include <map>

#include "musycl/midi.hpp"

namespace musycl::midi {

class channel_assignment {

  /** The (MIDI) channel mapping to the sound parameter

      Note that there might be more channels than the 16 real MIDI
      channels for convenience, for example to have more sounds for
      arpeggiators
  */

  std::map<int, musycl::sound_generator::param_t> channels;

  void assign(int channel, musycl::sound_generator::param_t sound {}) {
    
  }
};

} // namespace musycl

#endif // MUSYCL_MIDI_CHANNEL_ASSIGNMENT_HPP
