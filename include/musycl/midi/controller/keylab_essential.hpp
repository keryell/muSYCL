#ifndef MUSYCL_MIDI_CONTROLLER_KEYLAB_ESSENTIAL_HPP
#define MUSYCL_MIDI_CONTROLLER_KEYLAB_ESSENTIAL_HPP

/** \file Represent a MIDI controller like the Arturia KeyLab49 Essential
*/

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <range/v3/all.hpp>

#include "musycl/midi.hpp"
#include "musycl/midi/midi_out.hpp"

namespace musycl::midi::controller {

  // To use time unit literals directly
  using namespace std::chrono_literals;

/** An Arturia KeyLab MIDI controller

    This is made by gathering some information on-line, such as

    https://forum.arturia.com/index.php?topic=90496.0
    https://forum.renoise.com/t/tool-development-arturia-keylab-mkii-49-61-mcu-midi-messages/57343
    https://community.cantabilesoftware.com/t/questions-about-expressions-in-sysex-data/5175
*/
class keylab_essential {
  /** Arturia MIDI SysEx Id
      https://www.midi.org/specifications/midi-reference-tables/manufacturer-sysex-id-numbers
  */
  static auto constexpr sysex_id = { '\0', '\x20', '\x6b' };

  /// The device ID seems just "broadcast"
  static auto constexpr dev_id = { '\x7f' };

  /// The sub device ID?
  static auto constexpr sub_dev_id = { '\x42' };

  musycl::midi_out midi_out;

public:

  keylab_essential() {
    midi_out.open("muSYCL", "output", RtMidi::UNIX_JACK);

    display("Salut les petits amis");
    //button_light_fuzzing();
  }


  template <typename... Ranges>
  void send_sysex(Ranges&&... messages) {
    static auto constexpr sysex_start = { '\xf0' };
    static auto constexpr sysex_end = { '\xf7' };

    musycl::midi_out::write
      (ranges::to<std::vector<std::uint8_t>>
       (ranges::view::concat(sysex_start, sysex_id, dev_id,
                             sub_dev_id, messages..., sysex_end)));
  }


  void button_light(std::int8_t button, std::int8_t level) {
    static auto constexpr sysex_button_light = { '\x2', '\0', '\x10' };
    send_sysex(sysex_button_light, ranges::view::single(button),
               ranges::view::single(level));
  }


  /// Display a message on the LCD display
  void display(const std::string& message) {
    // Split the string in at most 2 lines of 16 characters max
    auto r = ranges::view::all(message) | ranges::view::chunk(16)
      | ranges::view::take(2) | ranges::view::enumerate
      | ranges::view::transform([](auto&& enumeration) {
          auto [line_number, line_content] = enumeration;
          return ranges::view::concat
            (ranges::view::single(char(line_number + 1)),
             line_content, ranges::view::single('\0'));
        })
      | ranges::view::join;
    static auto constexpr sysex_display_command = { '\x4', '\0', '\x60' };
    send_sysex(sysex_display_command, r);
  }


  /** Display a blinking cursor

      It looks like having 0 as a line number erase the fist line with
      a blinking cursor but the first line can be then displayed on
      the second line.

      Actually it looks more like a random bug which is triggered
      here...
  */
  void blink() {
    static auto constexpr blink_display_command =
      { '\x4', '\0', '\x60', '\0', '\0' };
    send_sysex(blink_display_command);
  }


  /** Button light fuzzing

      Experiment with commands

      A button 14 or 15 starts Vegas mode

      Pad blue 0x70-0x7f
  */
  void button_light_fuzzing() {
    for(auto b = 0x50; b <= 0x5f; ++b) {
      for(auto l = 0; l <= 127; ++l) {
        button_light(b, l);
        std::this_thread::sleep_for(10ms);
      }
      std::this_thread::sleep_for(2s);
    }
    // exit(0);
    for(auto l = 0; l <= 127; ++l)
      for(auto b = 0; b <= 127; ++b) {
        button_light(b, l);
        std::this_thread::sleep_for(10ms);
      }
  }

};

}

#endif // MUSYCL_MIDI_CONTROLLER_KEYLAB_ESSENTIAL_HPP
