#include "rtmidi/RtMidi.h"

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

/// The configuration part to adapt to the context
// Jack
// auto constexpr midi_api = RtMidi::UNIX_JACK;
// ALSA
// auto constexpr midi_api = RtMidi::LINUX_ALSA;

namespace {

/// Set to true to end the infinite loop
std::atomic<bool> done = false;

} // namespace

/// Execute some RtMidi code while checking for error
auto check_error = [](auto&& function) {
  try {
    return function();
  } catch (const RtMidiError& error) {
    error.printMessage();
    // std::exit(EXIT_FAILURE);
    throw;
  }
};

int main() {
  std::cout << "RtMidi version: " << RtMidi::getVersion()
            << "\nAPI availables:" << std::endl;
  std::vector<RtMidi::Api> apis;
  RtMidi::getCompiledApi(apis);
  struct named_input {
    RtMidiIn in;
    std::string name;
  };
  std::vector<named_input> midi_ins;
  for (auto api : apis) {
    std::cout << "\tAPI name: " << RtMidi::getApiName(api) << std::endl;
    std::cout << "\tAPI display name: " << RtMidi::getApiName(api) << std::endl;

    try {
      // Create a MIDI input using a fancy client name
      auto midi_in = check_error([&] {
        return RtMidiIn { api, "muSYCL_midi_test_in" };
      });

      // Check inputs
      auto n_in_ports = midi_in.getPortCount();
      std::cout << "\t\tThere are " << n_in_ports
                << " MIDI input sources available.\n";

      for (auto i = 0; i < n_in_ports; ++i) {
        auto port_name = check_error([&] { return midi_in.getPortName(i); });
        std::cout << "\t\t\tInput Port #" << i << ": " << port_name << '\n';
        auto full_name =
            "muSYCL_test_midi_in:" + std::to_string(i) + ":" + port_name;
        // Try to open this port
        try {
          auto m = check_error([&] { return RtMidiIn(api, full_name, 1000); });
          check_error([&] { m.openPort(i); });
          // Don't ignore sysex, timing, or active sensing messages
          m.ignoreTypes(false, false, false);
          // Add it to the list of opened MIDI interface
          midi_ins.emplace_back(std::move(m), std::move(full_name));
        } catch (...) {
        }
      }

      // Create a MIDI output
      auto midi_out = check_error([] { return RtMidiOut {}; });

      // Check outputs.
      auto n_out_ports = midi_out.getPortCount();
      std::cout << "\n\t\tThere are " << n_out_ports
                << " MIDI output ports available.\n";
      for (auto i = 0; i < n_out_ports; ++i) {
        auto port_name = check_error([&] { return midi_out.getPortName(i); });
        std::cout << "\t\t\tOutput Port #" << i << ": " << port_name << std::endl;
      }
    } catch (...) {
    }
  }
  std::cout << "\nReading MIDI from port ... quit with Ctrl-C.\n\n";
  // Trap SIGINT often related to Ctrl-C
  std::signal(SIGINT, [](int) { done = true; });
  std::vector<std::uint8_t> message;
  while (!done) {
    for (auto& [in, port_name] : midi_ins) {
      auto stamp = in.getMessage(&message);
      if (!message.empty()) {
        std::cout << "Received from port '" << port_name
                  << "' at stamp = " << stamp << " seconds:" << std::endl
                  << '\t';
        for (auto c : message)
          std::cout << std::hex << int { c } << ' ';
        std::cout << std::endl << '\t';
        auto n_bytes = message.size();
        for (int i = 0; i < n_bytes; ++i)
          std::cout << std::dec << "Byte " << i << " = "
                    << static_cast<int>(message[i]) << ", ";
        std::cout << std::endl;
      }
    }
  }
  std::cout << "\nDone!" << std::endl;
  return 0;
}
