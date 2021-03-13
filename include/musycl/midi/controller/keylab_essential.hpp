#ifndef MUSYCL_MIDI_CONTROLLER_KEYLAB_ESSENTIAL_HPP
#define MUSYCL_MIDI_CONTROLLER_KEYLAB_ESSENTIAL_HPP

/** \file Represent a MIDI controller like the Arturia KeyLab49 Essential
*/

#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "musycl/midi.hpp"
#include "musycl/midi/midi_out.hpp"

namespace musycl::midi::controller {

/// An Arturia KeyLab MIDI controller
class keylab_essential {
/** Arturia MIDI SysEx Id
    https://www.midi.org/specifications/midi-reference-tables/manufacturer-sysex-id-numbers
*/
static auto constexpr sysex_id = { 0x00, 0x20, 0x6b };

public:

  keylab_essential() {
    musycl::midi_out midi_out;
    midi_out.open("muSYCL", "output", RtMidi::UNIX_JACK);
    std::vector<std::uint8_t> hello { 0xF0, 0x00, 0x20, 0x6B, 0x7F, 0x42, 0x04,
                                      0x00,
        0x60, 0x01, 0x4C, 0x6F, 0x61, 0x64, 0x65, 0x64, 0x20, 0x4D, 0x75, 0x6C,
        0x74, 0x69, 0x3A, 0x00, 0x02, 0x56, 0x65, 0x6C, 0x25, 0x6C, 0x6F, 0x63,
        0x69, 0x70, 0x65, 0x21, 0x22, 0x23, 'a', 'b', 'c', 'd', 0x00, 0xF7};
    musycl::midi_out::write(hello);
    std::vector<std::uint8_t> header { 0xF0, 0x00, 0x20, 0x6B, 0x7F, 0x42, 0x04,
                                      0x00,
        0x60, 0x01};
    std::vector<std::uint8_t> end { 0x00, 0xF7};
    musycl::midi_out::write(header);
    for (std::uint8_t i = 16; i < 32; ++i)
      musycl::midi_out::write({ i });
    musycl::midi_out::write(end);
  }

};

}

#endif // MUSYCL_MIDI_CONTROLLER_KEYLAB_ESSENTIAL_HPP
