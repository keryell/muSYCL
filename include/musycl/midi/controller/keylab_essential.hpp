#ifndef MUSYCL_MIDI_CONTROLLER_KEYLAB_ESSENTIAL_HPP
#define MUSYCL_MIDI_CONTROLLER_KEYLAB_ESSENTIAL_HPP

/** \file

    Represent a MIDI controller like the Arturia KeyLab49 Essential
 */

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <range/v3/all.hpp>

#include "musycl/control.hpp"
#include "musycl/midi.hpp"
#include "musycl/midi/midi_out.hpp"
#include "musycl/user_interface.hpp"

namespace musycl::controller {

// To use time unit literals directly
using namespace std::chrono_literals;

  /** An Arturia KeyLab MIDI controller

      This is made by gathering some information on-line, such as

      https://forum.arturia.com/index.php?topic=90496.0
      https://forum.renoise.com/t/tool-development-arturia-keylab-mkii-49-61-mcu-midi-messages/57343
      https://community.cantabilesoftware.com/t/questions-about-expressions-in-sysex-data/5175

      Some SysEx values can be seen in the MIDI console of the MIDI
      Control Center from Arturia.
  */
  class keylab_essential : public clock::follow<keylab_essential> {
    /** Arturia MIDI SysEx Id
        https://www.midi.org/specifications/midi-reference-tables/manufacturer-sysex-id-numbers
    */
    static auto constexpr sysex_id = { '\0', '\x20', '\x6b' };

    /// The device ID seems just "broadcast"
    static auto constexpr dev_id = { '\x7f' };

    /// The sub device ID?
    static auto constexpr sub_dev_id = { '\x42' };

    static auto constexpr sysex_vegas_mode_off = { '\x2', '\0', '\x40', '\x50',
                                                   '\0' };

    static auto constexpr sysex_vegas_mode_on = { '\x2', '\0', '\x40', '\x50',
                                                  '\x1' };

    /// The user interface logic
    user_interface& ui;

    /** Button light fuzzing

        Experiment with some light commands
    */
    void button_light_fuzzing() {
      // Pick a button range to check
      for (auto b = 0x00; b <= 0x0f; ++b) {
        for (auto l = 0; l <= 127; ++l) {
          button_light(b, l);
          std::this_thread::sleep_for(10ms);
        }
        std::this_thread::sleep_for(2s);
        button_light(b, 0);
      }
      /* Increase light level across all the buttons.
         The problem is that it triggers the Vegas light show mode */
      for (auto l = 0; l <= 127; ++l)
        for (auto b = 0; b <= 127; ++b) {
          button_light(b, l);
          std::this_thread::sleep_for(10ms);
        }
    }

 public:
  /** Mapping of button light to SySex button light command

      Some names have the suffix \c _bis and \c _ter because they are
      alternative code to do the same action.

      It is not clear which value trigger the Vegas light show mode.
  */
  enum class button_out : std::int8_t {
    vegas_mode = 0x0d,
    vegas_mode_bis = 0x0e,
    vegas_mode_ter = 0x0f,
    octave_minus = 0x10,
    octave_plus = 0x11,
    chord = 0x12,
    transpose = 0x13,
    midi_channel = 0x14,
    map_select = 0x15,
    cat_char = 0x16,
    preset = 0x17,
    backward = 0x18,
    forward = 0x19,
    part_1_next = 0x1a,
    part_2_prev = 0x1b,
    live_bank = 0x1c,
    metro = 0x1d,
    fast_forward = 0x1e,
    record = 0x1f,
    pad_1_blue = 0x20,
    pad_1_green = 0x21,
    pad_1_red = 0x22,
    pad_2_blue = 0x23,
    pad_2_green = 0x24,
    pad_2_red = 0x25,
    pad_3_blue = 0x26,
    pad_3_green = 0x27,
    pad_3_red = 0x28,
    pad_4_blue = 0x29,
    pad_4_green = 0x2a,
    pad_4_red = 0x2b,
    pad_5_blue = 0x2c,
    pad_5_green = 0x2d,
    pad_5_red = 0x2e,
    pad_6_blue = 0x2f,
    pad_6_green = 0x30,
    pad_6_red = 0x31,
    pad_7_blue = 0x32,
    pad_7_green = 0x33,
    pad_7_red = 0x34,
    pad_8_blue = 0x35,
    pad_8_green = 0x36,
    pad_8_red = 0x37,
    chord_bis = 0x38,
    transpose_bis = 0x39,
    octave_minus_bis = 0x3a,
    octave_plus_bis = 0x3b,
    map_select_bis = 0x3c,
    midi_channel_bis = 0x3d,
    save = 0x3e,
    punch = 0x3f,
    save_bis = 0x56,
    undo = 0x57,
    punch_bis = 0x58,
    metro_bis = 0x59,
    loop = 0x5a,
    rewind = 0x5b,
    fast_forward_bis = 0x5c,
    stop = 0x5d,
    play_pause = 0x5e,
    record_bis = 0x5f,
    pad_1_blue_bis = 0x70,
    pad_2_blue_bis = 0x71,
    pad_3_blue_bis = 0x72,
    pad_4_blue_bis = 0x73,
    pad_5_blue_bis = 0x74,
    pad_6_blue_bis = 0x75,
    pad_7_blue_bis = 0x76,
    pad_8_blue_bis = 0x77,
    pad_1_blue_ter = 0x78,
    pad_2_blue_ter = 0x79,
    pad_3_blue_ter = 0x7a,
    pad_4_blue_ter = 0x7b,
    pad_5_blue_ter = 0x7c,
    pad_6_blue_ter = 0x7d,
    pad_7_blue_ter = 0x7e,
    pad_8_blue_ter = 0x7f,
  };

   public:
    /// List all the control items
    // std::vector<control::physical_item> inputs;

    control::physical_item cutoff_pan_1 {
      ui, control::physical_item::type::knob,
      control::physical_item::cc { 0x4a }, control::physical_item::cc_inc { 0x10 }
    };

    control::physical_item resonance_pan_2 {
      ui, control::physical_item::type::knob,
      control::physical_item::cc { 0x47 }, control::physical_item::cc_inc { 0x11 }
    };

    control::physical_item lfo_rate_pan_3 {
      ui, control::physical_item::type::knob,
      control::physical_item::cc { 0x4c }, control::physical_item::cc_inc { 0x12 }
    };

    control::physical_item lfo_amt_pan_4 {
      ui, control::physical_item::type::knob,
      control::physical_item::cc { 0x4d }, control::physical_item::cc_inc { 0x13 }
    };

    control::physical_item param_1_pan_5 {
      ui, control::physical_item::type::knob,
      control::physical_item::cc { 0x5d }, control::physical_item::cc_inc { 0x14 }
    };

    control::physical_item param_2_pan_6 {
      ui, control::physical_item::type::knob,
      control::physical_item::cc { 0x12 }, control::physical_item::cc_inc { 0x15 }
    };

    control::physical_item param_3_pan_7 {
      ui, control::physical_item::type::knob,
      control::physical_item::cc { 0x13 }, control::physical_item::cc_inc { 0x16 }
    };

    control::physical_item param_4_pan_8 {
      ui, control::physical_item::type::knob,
      control::physical_item::cc { 0x10 }, control::physical_item::cc_inc { 0x17 }
    };

    // The unnamed knob on the top right, non mapped in DAW mode
    control::physical_item top_right_knob_9 {
      ui, control::physical_item::type::knob,
      control::physical_item::cc { 0x11 }
    };

    control::physical_item attack_ch_1 { ui,
                                        control::physical_item::type::slider,
                                        control::physical_item::cc { 0x49 } };

    control::physical_item decay_ch_2 { ui,
                                       control::physical_item::type::slider,
                                       control::physical_item::cc { 0x4b } };

    control::physical_item sustain_ch_3 { ui,
                                         control::physical_item::type::slider,
                                         control::physical_item::cc { 0x4f } };

    control::physical_item release_ch_4 { ui,
                                         control::physical_item::type::slider,
                                         control::physical_item::cc { 0x48 } };

    control::physical_item attack_ch_5 { ui,
                                        control::physical_item::type::slider,
                                        control::physical_item::cc { 0x50 } };

    control::physical_item decay_ch_6 { ui,
                                       control::physical_item::type::slider,
                                       control::physical_item::cc { 0x51 } };

    control::physical_item sustain_ch_7 { ui,
                                         control::physical_item::type::slider,
                                         control::physical_item::cc { 0x52 } };

    control::physical_item release_ch_8 { ui,
                                         control::physical_item::type::slider,
                                         control::physical_item::cc { 0x53 } };

    control::physical_item play_pause { ui,
                                       control::physical_item::type::button,
                                       control::physical_item::note { 0x5e } };

    control::physical_item pad_1 = { ui, control::physical_item::type::button,
                                    control::physical_item::pad {
                                        0x24, button_out::pad_1_red,
                                        button_out::pad_1_blue,
                                        button_out::pad_1_green } };

    control::physical_item pad_2 = { ui, control::physical_item::type::button,
                                    control::physical_item::pad {
                                        0x25, button_out::pad_2_red,
                                        button_out::pad_2_blue,
                                        button_out::pad_2_green } };

    /// Start the KeyLab controller
    keylab_essential(user_interface& ui)
        : ui { ui } {
      ui.set_controller(*this);
      display("Salut les petits amis");
      // button_light_fuzzing();
      // Refresh the LCD display because it is garbled by various information
      clock::scheduler.appoint_cyclic(0.25s,
                                      [this](auto) { refresh_display(); });
    }

    /** Send a MIDI SysEx message by prepending SysEx Start and
        appending SysEx End

        \return a copy of the full MIDI message sent, for example for
        later replay.
     */
    template <typename... Ranges> auto send_sysex(Ranges&&... messages) {
      static auto constexpr sysex_start = { '\xf0' };
      static auto constexpr sysex_end = { '\xf7' };

      auto sysex_message = ranges::to<std::vector<std::uint8_t>>(
          ranges::view::concat(sysex_start, sysex_id, dev_id, sub_dev_id,
                               messages..., sysex_end));
      midi_out::write(sysex_message);
      return sysex_message;
    }

    /// Return the underlying user interface
    user_interface& user_interface() { return ui; }

    void button_light(std::int8_t button, std::int8_t level) {
      static auto constexpr sysex_button_light = { '\x2', '\0', '\x10' };
      send_sysex(sysex_button_light, ranges::view::single(button),
                 ranges::view::single(level));
    }

    /// Store the last displayed message to refresh the display regularly
    std::vector<std::uint8_t> last_displayed_sysex_message;

    /// Display a message on the LCD display
    void display(const std::string& message) {
      // Split the string in at most 2 lines of 16 characters max
      auto r = ranges::view::all(message) | ranges::view::chunk(16) |
               ranges::view::take(2) | ranges::view::enumerate |
               ranges::view::transform([](auto&& enumeration) {
                 auto [line_number, line_content] = enumeration;
                 return ranges::view::concat(
                     ranges::view::single(char(line_number + 1)), line_content,
                     ranges::view::single('\0'));
               }) |
               ranges::view::join;
      static auto constexpr sysex_display_command = { '\x4', '\0', '\x60' };
      last_displayed_sysex_message = send_sysex(sysex_display_command, r);
    }

    /// Refresh the LCD display with the last displayed message
    void refresh_display() { midi_out::write(last_displayed_sysex_message); }

    /** Display a blinking cursor

        It looks like having 0 as a line number erase the fist line with
        a blinking cursor but the first line can be then displayed on
        the second line.

        Actually it looks more like a random bug which is triggered
        here...
    */
    void blink() {
      static auto constexpr blink_display_command = { '\x4', '\0', '\x60', '\0',
                                                      '\0' };
      send_sysex(blink_display_command);
    }

    /// This is notified on each beat by the clocking framework
    void midi_clock(clock::type ct) {
      /* Blink the Metro light for 16th of note at the start of each
         (quarter) beat at half the brightness, with the first beat of
         the measure being full brightness */
      auto light_level = (ct.midi_clock_index < midi::clock_per_quarter / 4) *
                         (32 + 95 * (ct.beat_index == 0));
      button_light((int)button_out::metro, light_level);
    }
  };

} // namespace musycl

#endif // MUSYCL_MIDI_CONTROLLER_KEYLAB_ESSENTIAL_HPP
