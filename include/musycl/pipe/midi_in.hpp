#ifndef MUSYCL_PIPE_MIDI_IN_HPP
#define MUSYCL_PIPE_MIDI_IN_HPP

/** \file SYCL abstraction for a MIDI input pipe

    Based on RtMidi library.
*/

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <variant>
#include <vector>

#include <boost/fiber/buffered_channel.hpp>
#include <boost/function_types/parameter_types.hpp>
#include <boost/mpl/at.hpp>

#include "rtmidi/RtMidi.h"

#include <triSYCL/detail/overloaded.hpp>

#include "musycl/midi.hpp"

namespace musycl {

// To use time unit literals directly
using namespace std::chrono_literals;

/** A MIDI input interface exposed as a SYCL pipe.

    In SYCL the type is used to synthesize the connection between
    kernels, so there can be only 1 instance of a MIDI input
    interface. */
class midi_in {
  /// Capacity of the MIDI message pipe
  static auto constexpr pipe_min_capacity = 64;

  /** The handler to control the MIDI input interface

      Use a pointer because RtMidiIn uses some dynamic polymorphism
      and it crashes otherwise */
  static inline std::unique_ptr<RtMidiIn> interface;

  /// A FIFO used to implement the pipe of MIDI messages
  static inline boost::fibers::buffered_channel<midi::msg>
  channel { pipe_min_capacity };

  /// Actions to run for each received CC message
  static inline
  std::multimap<std::uint8_t, std::function<void(std::uint8_t)>> cc_actions;

  /// Check for RtMidi errors
  static auto constexpr check_error = [] (auto&& function) {
    try {
      return function();
    }
    catch (const RtMidiError &error) {
      error.printMessage();
      std::exit(EXIT_FAILURE);
    }
  };


  /// Process the incomming MIDI messages
  static inline void process_midi_in(double time_stamp,
                                     std::vector<std::uint8_t>* p_midi_message,
                                     void*) {
    auto &midi_message = *p_midi_message;
    auto n_bytes = midi_message.size();
    for (int i = 0; i < n_bytes; ++i)
      std::cout << "Byte " << i << " = 0x"
                << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(midi_message[i]) << ", "
                << std::resetiosflags(std::cout.flags());
    std::cout << "time stamp = " << time_stamp << std::endl;

    auto m = musycl::midi::parse(midi_message);
    dispatch_registered_actions(m);
    // Also enqueue the midi event for explicit consumption
    // \todo do not block on full
    channel.push(m);
  }

public:

  void open(const std::string& application_name,
            const std::string& port_name,
            RtMidi::Api backend,
            unsigned int port_number) {
    // Create a MIDI input with a fancy client name
    interface = check_error([&] {
      return std::make_unique<RtMidiIn>(backend, application_name);
    });

    // Open the first port and give it a fancy name
    check_error([&] { interface->openPort(port_number, port_name); });


    // Don't ignore sysex, timing, or active sensing messages
    check_error([&] { interface->ignoreTypes(false, false, false); });

    // Drain the message queue to avoid leftover MIDI messages
    std::vector<std::uint8_t> message;
    do {
      // There is a race condition in RtMidi where the messages are
      // not seen if these is not some sleep here
      std::this_thread::sleep_for(1ms);
      check_error([&] { interface->getMessage(&message); });
    } while (!message.empty());

    // Handle MIDI messages with this callback function
    check_error([&] { interface->setCallback(process_midi_in, nullptr); });
  }


  /// The sycl::pipe::read-like interface to read a MIDI message
  static midi::msg read() {
    return channel.value_pop();
  }


  /// The non-blocking sycl::pipe::read-like interface to read a MIDI message
  static bool try_read(midi::msg& m) {
    return channel.try_pop(m) == boost::fibers::channel_op_status::success;
  }


  /// Dispatch the registered actions for a MIDI input event
  static void dispatch_registered_actions(const midi::msg& m) {
    std::visit(trisycl::detail::overloaded {
        [&] (const musycl::midi::control_change& cc) {
          auto [first, last] = cc_actions.equal_range(cc.number);
          std::for_each(first, last, [&] (auto&& action) {
            action.second(cc.value);
          });
        },
        [] (auto &&other) {}
      },
      m);
  }


  /** Associate an action to a channel controller (CC)

      \param[in] number is the CC number

      \param[in] action is the action to call with the value
      returned by the CC as a parameter.

      If the action parameter has a floating point type, the value is
      scaled to [0, 1] first.
  */
  template <int number, typename Lambda>
  static void cc_action(Lambda&& action) {
    // Get the lambda call operator type
    using f_type = decltype(&Lambda::operator());
    // Get the type of the first parameter of the lambda
    using arg_1_type = typename boost::mpl::at_c
      <typename boost::function_types::parameter_types<f_type>::type, 1>::type;
    // Register an action producing the right value for the lambda
    if constexpr (std::is_floating_point_v<arg_1_type>)
      // If we have a floating point type, scale the value in [0, 1]
      cc_actions.emplace(number, [action = std::forward<Lambda>(action)]
                         (midi::control_change::value_type v) {
        action(midi::control_change::get_value_as<arg_1_type>(v));
      });
    else
      // Just provides the CC value directly to the action
      cc_actions.emplace(number, action);
  }


  /** Associate a variable to a channel controller (CC)

      \param[in] number is the CC number

      \param[out] variable is the variable to set to the value
      returned by the CC. If the variable has a floating point type,
      the value is scaled to [0, 1] first.
  */
  template <int number, typename T>
  static void cc_variable(T& variable) {
    cc_action<number>([&] (T v) {
      variable = v;
    });
  }
};

}

#endif // MUSYCL_PIPE_MIDI_IN_HPP
