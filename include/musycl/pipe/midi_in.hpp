#ifndef MUSYCL_PIPE_MIDI_IN_HPP
#define MUSYCL_PIPE_MIDI_IN_HPP

/** \file SYCL abstraction for a MIDI input pipe

    Based on RtMidi library.
*/

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include <boost/fiber/buffered_channel.hpp>

#include "rtmidi/RtMidi.h"

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

    channel.push(musycl::midi::parse(midi_message));
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
};

}

#endif // MUSYCL_PIPE_MIDI_IN_HPP
