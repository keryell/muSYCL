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
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <range/v3/all.hpp>

#include "musycl/midi.hpp"
#include "musycl/midi/midi_out.hpp"

namespace musycl {

// To use time unit literals directly
using namespace std::chrono_literals;

class controller {

 public:
  /// \todo refactor with concern separation
  class control_item {
   public:
    enum class type : std::uint8_t { button, knob, slider };

    class cc {
     public:
      std::int8_t value;

      cc(std::int8_t v)
          : value(v) {}
    };

    class cc_inc {
     public:
      std::int8_t value;

      cc_inc(std::int8_t v)
          : value(v) {}
    };

    class note {
     public:
      std::int8_t value = false;

      note(std::int8_t v)
          : value(v) {}
    };

    std::optional<cc> cc_v;
    std::optional<cc_inc> cc_inc_v;
    std::optional<note> note_v;

    midi::control_change::value_type value;

    bool connected = false;

    std::string current_name;

    std::vector<std::function<void(midi::control_change::value_type)>>
        listeners;

    template <typename... Features>
    control_item(void*, type t, Features... features) {
      // Parse the features
      (
          [&](auto&& f) {
            using f_t = std::remove_cvref_t<decltype(f)>;
            if constexpr (std::is_same_v<f_t, cc>) {
              cc_v = f;
              midi_in::cc_action(f.value,
                                 [&](midi::control_change::value_type v) {
                                   value = v;
                                   dispatch();
                                 });
            } else if constexpr (std::is_same_v<f_t, cc_inc>)
              // \todo
              cc_inc_v = f;
            else if constexpr (std::is_same_v<f_t, note>) {
              note_v = f;
              // Listen a note from port 1 and channel 0
              midi_in::add_action(1, midi::on_header { 0, f.value },
                                  [&](const midi::msg& m) {
                                    // Recycle value as a bool for now
                                    value = !value;
                                    dispatch();
                                  });
            }
          }(std::forward<Features>(features)),
          ...);
    }

    void dispatch() {
      std::cout << "Dispatch" << std::endl;
      for (auto& l : listeners)
        l(value);
    }

    /// Name the control_item
    auto& name(const std::string& new_name) {
      current_name = new_name;
      return *this;
    }

    /// Add an action to the control_item
    template <typename Callable> auto& add_action(Callable&& action) {
      using arg0_t =
          std::tuple_element_t<0, boost::callable_traits::args_t<Callable>>;
      // Register an action producing the right value for the action
      if constexpr (std::is_floating_point_v<arg0_t>)
        // If we have a floating point type, scale the value in [0, 1]
        listeners.push_back([action = std::forward<Callable>(action)](
                                midi::control_change::value_type v) {
          action(midi::control_change::get_value_as<arg0_t>(v));
        });
      else
        // Just provides the CC value directly to the action
        listeners.push_back(action);
      return *this;
    }

    /** Associate a variable to an item

        \param[out] variable is the variable to set to the value
        returned by the controller. If the variable has a floating point
        type, the value is scaled to [0, 1] first.
    */
    template <typename T> auto& set_variable(T& variable) {
      add_action([&](T v) { variable = v; });
      return *this;
    }

    /// The value normalized in [ 0, 1 ]
    float value_1() const {
      return midi::control_change::get_value_as<float>(value);
    }

    /// Connect this control to the real parameter
    template <typename ControlItem> auto& connect(ControlItem& ci) {
      add_action([&](int v) { ci.set_127(v); });
      return *this;
    }
  };

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
    /// List all the control items
    // std::vector<control_item> inputs;

    control_item cutoff_pan_1 { this, control_item::type::knob,
                                control_item::cc { 0x4a },
                                control_item::cc_inc { 0x10 } };

    control_item resonance_pan_2 { this, control_item::type::knob,
                                   control_item::cc { 0x47 },
                                   control_item::cc_inc { 0x11 } };

    control_item lfo_rate_pan_3 { this, control_item::type::knob,
                                  control_item::cc { 0x4c },
                                  control_item::cc_inc { 0x12 } };

    control_item lfo_amt_pan_4 { this, control_item::type::knob,
                                 control_item::cc { 0x4d },
                                 control_item::cc_inc { 0x13 } };

    control_item param_1_pan_5 { this, control_item::type::knob,
                                 control_item::cc { 0x5d },
                                 control_item::cc_inc { 0x14 } };

    control_item param_2_pan_6 { this, control_item::type::knob,
                                 control_item::cc { 0x12 },
                                 control_item::cc_inc { 0x15 } };

    control_item param_3_pan_7 { this, control_item::type::knob, this,
                                 control_item::cc { 0x13 },
                                 control_item::cc_inc { 0x16 } };

    control_item param_4_pan_8 { this, control_item::type::knob,
                                 control_item::cc { 0x10 },
                                 control_item::cc_inc { 0x17 } };

    // The unnamed knob on the top right, non mapped in DAW mode
    control_item top_right_knob_9 { this, control_item::type::knob,
                                    control_item::cc { 0x11 } };

    control_item attack_ch_5 { this, control_item::type::slider,
                               control_item::cc { 0x50 } };

    control_item decay_ch_6 { this, control_item::type::slider,
                              control_item::cc { 0x51 } };

    control_item sustain_ch_7 { this, control_item::type::slider,
                                control_item::cc { 0x52 } };

    control_item release_ch_8 { this, control_item::type::slider,
                                control_item::cc { 0x53 } };

    control_item play_pause { this, control_item::type::button,
                              control_item::note { 0x5e } };

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

    keylab_essential() {
      display("Salut les petits amis");
      // button_light_fuzzing();
    }

    template <typename... Ranges> void send_sysex(Ranges&&... messages) {
      static auto constexpr sysex_start = { '\xf0' };
      static auto constexpr sysex_end = { '\xf7' };

      musycl::midi_out::write(ranges::to<std::vector<std::uint8_t>>(
          ranges::view::concat(sysex_start, sysex_id, dev_id, sub_dev_id,
                               messages..., sysex_end)));
    }

    void button_light(std::int8_t button, std::int8_t level) {
      static auto constexpr sysex_button_light = { '\x2', '\0', '\x10' };
      send_sysex(sysex_button_light, ranges::view::single(button),
                 ranges::view::single(level));
    }

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
      static auto constexpr blink_display_command = { '\x4', '\0', '\x60', '\0',
                                                      '\0' };
      send_sysex(blink_display_command);
    }

    /// This is notified on each beat by the clocking framework
    void midi_clock(clock::type ct) {
      // Blink the Metro light for 1 quarter of a quarter note at the
      // start of each (quarter) beat
      std::cout << ct.midi_clock_index << std::endl;
      button_light((int)button_out::metro,
                   127 * (ct.midi_clock_index < midi::clock_per_quarter / 4));
    }
  };
};

} // namespace musycl

#endif // MUSYCL_MIDI_CONTROLLER_KEYLAB_ESSENTIAL_HPP
