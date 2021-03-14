#ifndef MUSYCL_MIDI_MIDI_OUT_HPP
#define MUSYCL_MIDI_MIDI_OUT_HPP

/** \file SYCL abstraction for a MIDI output pipe

    Based on RtMidi library.
*/

#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "rtmidi/RtMidi.h"

#include "musycl/midi.hpp"

namespace musycl {

// To use time unit literals directly
using namespace std::chrono_literals;

/** A MIDI output interface exposed as a SYCL pipe.

    In SYCL the type is used to synthesize the connection between
    kernels, so there can be only 1 instance of a MIDI input
    interface. */
class midi_out {
  /** The handlers to control the MIDI output interfaces

      Use a pointer because RtMidiOut is a broken type and is neither
      copyable nor movable */
  static inline std::vector<std::unique_ptr<RtMidiOut>> interfaces;

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

public:

  /// Open all the IMID input ports available
  void open(const std::string& application_name,
            const std::string& port_name,
            RtMidi::Api backend) {
    std::cout << "RtMidi version " << RtMidi::getVersion() << std::endl;
    // Create a throwable MIDI output just to get later the number of port
    auto midi_out = check_error([&] { return RtMidiOut { backend,
                                                         "muSYCLtest" }; });

    auto n_out_ports = midi_out.getPortCount();
    std::cout << "\nThere are " << n_out_ports
              << " MIDI output ports available." << std::endl;;

    for (auto i = 0; i < n_out_ports; ++i) {
      interfaces.push_back(check_error([&] {
        return std::make_unique<RtMidiOut>(backend, application_name);
      }));

      auto port_name = check_error([&] { return midi_out.getPortName(i); });
      std::cout << "  Output Port #" << i << ": " << port_name << std::endl;

      // Open the port and give it a fancy name
      check_error([&] { interfaces[i]->openPort(i, port_name); });
    }
  }


  /// The sycl::pipe::write-like interface to write a MIDI message
  static void write(const std::vector<std::uint8_t>& v) {
#if 0
    for (int e : v)
      std::cout << std::hex << e << ' ';
    std::cout << std::endl;
    for (int e : v)
      std::cout << std::dec << e << ' ';
    std::cout << std::endl;
#endif
    // Hard-code now for KeyLab Essential
    interfaces[1]->sendMessage(&v);
  }


  /// The non-blocking sycl::pipe::write-like interface to write a MIDI message
  static void try_write(const std::vector<std::uint8_t>& v) {
    write(v);
  }


};

}

#endif // MUSYCL_MIDI_MIDI_OUT_HPP
