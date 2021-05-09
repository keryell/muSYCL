#include "rtmidi/RtMidi.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

/// The configuration part to adapt to the context
// Jack
auto constexpr midi_api = RtMidi::UNIX_JACK;
auto constexpr midi_in_port = 0;
// ALSA
//auto constexpr midi_api = RtMidi::LINUX_ALSA;
//auto constexpr midi_in_port = 1;

// To use time unit literals directly
using namespace std::chrono_literals;

namespace {

/// Set to true to end the infinite loop
std::atomic<bool> done = false;

}

/// Execute some RtMidi code while checking for error
auto check_error = [] (auto&& function) {
  try {
    return function();
  }
  catch (const RtMidiError &error) {
    error.printMessage();
    std::exit(EXIT_FAILURE);
  }
};

int main() {
  std::cout << "RtMidi version " << RtMidi::getVersion() << std::endl;
  /* Only from RtMidi 4.0.0...

  std::cout << "RtMidi version " << RtMidi::getVersion()
            << "\nAPI availables:" << std::endl;
  std::vector<RtMidi::Api> apis;
  RtMidi::getCompiledApi(apis);
  for (auto a : apis)
    std::cout << '\t' << RtMidi::getApiName(a) << std::endl;
  */

  // Create a MIDI input using Jack and a fancy client name
  auto midi_in = check_error([] { return RtMidiIn { midi_api,
                                                    "muSYCLtest" }; });

  // Check inputs
  auto n_in_ports = midi_in.getPortCount();
  std::cout << "\nThere are " << n_in_ports
            << " MIDI input sources available.\n";

  // Use some shared point because RtMidiIn is a broken type, it is
  // neither copyable neither movable
  std::vector<std::shared_ptr<RtMidiIn>> midi_ins;
  for (auto i = 0; i < n_in_ports; ++i) {
    auto port_name = check_error([&] { return midi_in.getPortName(i); });
    std::cout << "  Input Port #" << i << ": " << port_name << '\n';
    midi_ins.push_back(check_error([] {
      return std::make_shared<RtMidiIn>(midi_api, "muSYCLtest", 1000);
    }));
    check_error([&] { midi_ins[i]->openPort(i, "testMIDIinput"); });
    // Don't ignore sysex, timing, or active sensing messages
    midi_ins[i]->ignoreTypes(false, false, false);
  }

  // Create a MIDI output
  auto midi_out = check_error([] { return RtMidiOut {}; });

  // Check outputs.
  auto n_out_ports = midi_out.getPortCount();
  std::cout << "\nThere are " << n_out_ports
            << " MIDI output ports available.\n";
  for (auto i = 0; i < n_out_ports; ++i) {
    auto port_name = check_error([&] { return midi_out.getPortName(i); });
    std::cout << "  Output Port #" << i << ": " << port_name << std::endl;
  }

  std::cout << "\nReading MIDI from port ... quit with Ctrl-C.\n\n";
  // Trap SIGINT often related to Ctrl-C
  std::signal(SIGINT, [] (int) { done = true; });
  std::vector<std::uint8_t> message;
  while (!done) {
    for (auto i = 0; i < n_in_ports; ++i) {
      auto stamp = midi_ins[i]->getMessage(&message);
      if (!message.empty()) {
        std::cout << "Received from port " << i << " at stamp = "
                  << stamp << " seconds:" <<std::endl << '\t';
        for (auto c : message)
          std::cout << std::hex << int { c } << ' ';
        std::cout << std::endl << '\t';
        auto n_bytes = message.size();
        for (int i = 0; i < n_bytes; ++i)
          std::cout << std::dec <<  "Byte " << i
                    << " = " << static_cast<int>(message[i])
                    << ", ";
        std::cout << std::endl;
      }
    }
  }
  std::cout << "\nDone!" << std::endl;
  return 0;
}
