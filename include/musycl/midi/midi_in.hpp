#ifndef MUSYCL_MIDI_IN_HPP
#define MUSYCL_MIDI_IN_HPP

/** \file SYCL abstraction for a MIDI input pipe

    Based on RtMidi library.
*/

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include <boost/callable_traits/args.hpp>
#include <boost/fiber/buffered_channel.hpp>

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

  /// The buffered channel used on MIDI input with the right size by default
  struct pipe_channel : boost::fibers::buffered_channel<midi::msg> {
    pipe_channel()
        : boost::fibers::buffered_channel<midi::msg> { pipe_min_capacity } {}
  };

  /** The handlers to control the MIDI input interfaces

      Use a pointer because RtMidiIn is a broken type and is neither
      copyable nor movable */
  static inline std::vector<std::unique_ptr<RtMidiIn>> interfaces;

  /// FIFO used to implement the pipe of MIDI messages on each port
  static inline std::map<std::int8_t, pipe_channel> channels;

  /* FIFO used to postpone event dispatch at a time picked by the user
     to avoid race condition */
  static inline std::map<std::int8_t, pipe_channel> dispatch_channels;

  /// A key to dispatch MIDI messages from this index
  struct port_msg_header {
    std::int8_t port;
    midi::msg_header header;
    // Just use some lexicographic comparison by default
    auto operator<=>(const port_msg_header&) const = default;
  };

  /// Actions to run for each received CC message from each MIDI port
  static inline std::multimap<port_msg_header,
                              std::function<void(const midi::msg&)>>
      midi_actions;

  /// Check for RtMidi errors
  static auto constexpr check_error = [](auto&& function) {
    try {
      return function();
    } catch (const RtMidiError& error) {
      error.printMessage();
      std::exit(EXIT_FAILURE);
    }
  };

  /// Process the incomming MIDI messages
  static inline void process_midi_in(double time_stamp,
                                     std::vector<std::uint8_t>* p_midi_message,
                                     void* port) {
    auto& midi_message = *p_midi_message;
    auto n_bytes = midi_message.size();
    auto p = reinterpret_cast<std::intptr_t>(port);
    std::cout << "Received from port " << p << " at time stamp = " << time_stamp
              << std::endl
              << '\t';
    for (int i = 0; i < n_bytes; ++i)
      std::cout << "Byte " << i << " = 0x" << std::hex << std::setw(2)
                << std::setfill('0') << static_cast<int>(midi_message[i])
                << ", " << std::resetiosflags(std::cout.flags());
    std::cout << std::endl;

    auto m = musycl::midi::parse(midi_message);
    /* Enqueue the midi message for future event dispatch by
       dispatch_registered_actions(). If it is full, just drop the
       message */
    dispatch_channels[p].try_push(m);
    /* Also enqueue the midi event for explicit consumption. If it is
       full, just drop the message */
    channels[p].try_push(m);
  }

 public:
  void open(const std::string& application_name, const std::string& port_name,
            RtMidi::Api backend) {
    std::cout << "RtMidi version " << RtMidi::getVersion() << std::endl;
    // Create a throwable MIDI input just to get later the number of port
    auto midi_in = check_error([&] {
      return RtMidiIn { backend, "muSYCLtest" };
    });
    auto n_in_ports = midi_in.getPortCount();
    std::cout << "\nThere are " << n_in_ports
              << " MIDI input sources available.\n";

    for (auto i = 0; i < n_in_ports; ++i) {
      interfaces.push_back(check_error([&] {
        return std::make_unique<RtMidiIn>(backend, application_name);
      }));

      auto port_name = check_error([&] { return midi_in.getPortName(i); });
      std::cout << "  Input Port #" << i << ": " << port_name << std::endl;

      // Open the port and give it a fancy name
      check_error([&] { interfaces[i]->openPort(i, port_name); });

      // Don't ignore sysex, timing, or active sensing messages
      check_error([&] { interfaces[i]->ignoreTypes(false, false, false); });

      // Drain the message queue to avoid leftover MIDI messages
      std::vector<std::uint8_t> message;
      do {
        // There is a race condition in RtMidi where the messages are
        // not seen if these is not some sleep here
        std::this_thread::sleep_for(1ms);
        check_error([&] { interfaces[i]->getMessage(&message); });
      } while (!message.empty());

      // Handle MIDI messages with this callback function
      check_error([&] {
        interfaces[i]->setCallback(process_midi_in, reinterpret_cast<void*>(i));
      });
    }
  }

  /// The sycl::pipe::read-like interface to read a MIDI message
  static midi::msg read(std::int8_t port) { return channels[port].value_pop(); }

  /// The non-blocking sycl::pipe::read-like interface to read a MIDI message
  static bool try_read(std::int8_t port, midi::msg& m) {
    return channels[port].try_pop(m) ==
           boost::fibers::channel_op_status::success;
  }

  /// Insert a new MIDI message in the input flow
  static void insert(std::int8_t port, const midi::msg& m) {
    channels[port].push(m);
  }

  /** Dispatch the registered actions for a MIDI input event

      This is decoupled from the MIDI system call-back function to be
      called by the user at the right time, typically when this will
      not cause race condition, compared to asynchronous call back
      happening in a background thread.
   */
  static void dispatch_registered_actions() {
    for (auto&& [port, channel] : dispatch_channels) {
      midi::msg m;
      while (channel.try_pop(m) == boost::fibers::channel_op_status::success) {
        std::cout << m << std::endl;
        auto [first, last] = midi_actions.equal_range({ port, m });
        std::for_each(first, last, [&](auto&& action) { action.second(m); });
      }
    }
  }

  /** Associate an action to a MIDI message

      \param[in] port is the input MIDI port

      \param[in] header is the MIDI message header to dispatch

      \param[in] action is the action to call with the matching MIDI
      message
  */
  template <typename Callable>
  static void add_action(std::int8_t port, const midi::msg_header& header,
                         Callable&& action) {
    std::cout << "midi_in::add_action on port " << int { port } << " for "
              << header << std::endl;
    midi_actions.emplace(port_msg_header { port, header }, action);
  }

  /** Associate an action to a channel controller (CC)

      \param[in] port is the input MIDI port

      \param[in] number is the CC number

      \param[in] action is the action to call with the value
      returned by the CC as a parameter.

      If the action parameter has a floating point type, the value is
      scaled to [0, 1] first.
  */
  template <typename Callable>
  static void cc_action(std::int8_t port, std::int8_t channel, std::int8_t number,
                        Callable&& action) {
    using arg0_t =
        std::tuple_element_t<0, boost::callable_traits::args_t<Callable>>;
    // Register an action producing the right value for the action
    if constexpr (std::is_floating_point_v<arg0_t>)
      // If we have a floating point type, scale the value in [0, 1]
      midi_actions.emplace(
          port_msg_header { port, midi::control_change_header { channel, number } },
          [action = std::forward<Callable>(action)](const midi::msg& m) {
            action(midi::control_change::get_value_as<arg0_t>(
                std::get<midi::control_change>(m).value));
          });
    else
      // Just provides the CC value directly to the action
      midi_actions.emplace(
          port_msg_header { port, midi::control_change_header { channel, number } },
          [action = std::forward<Callable>(action)](const midi::msg& m) {
            action(std::get<midi::control_change>(m).value);
          });
  }

  /** Associate an action to a channel controller (CC) on the first
      MIDI input port

      \param[in] number is the CC number

      \param[in] action is the action to call with the value
      returned by the CC as a parameter.

      If the action parameter has a floating point type, the value is
      scaled to [0, 1] first.
  */
  template <typename Callable>
  static void cc_action(std::int8_t number, Callable&& action) {
    cc_action(0, 0, number, std::forward<Callable>(action));
  }

  /** Associate an action to a channel controller (CC)

      \param[in] number is the CC number

      \param[in] action is the action to call with the value
      returned by the CC as a parameter.

      If the action parameter has a floating point type, the value is
      scaled to [0, 1] first.

      \todo Remove this templated API
  */
  template <std::int8_t number, typename Callable>
  static void cc_action(Callable&& action) {
    cc_action(number, std::forward<Callable>(action));
  }

  /** Associate a variable to a channel controller (CC)

      \param[in] number is the CC number

      \param[out] variable is the variable to set to the value
      returned by the CC. If the variable has a floating point type,
      the value is scaled to [0, 1] first.

      \todo Remove this templated API
  */
  template <std::int8_t number, typename T>
  static void cc_variable(T& variable) {
    cc_action<number>([&](T v) { variable = v; });
  }
};

} // namespace musycl

#endif // MUSYCL_MIDI_IN_HPP
