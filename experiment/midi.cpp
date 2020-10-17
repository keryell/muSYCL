#include "rtmidi/RtMidi.h"

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

// To use time unit literals directly
using namespace std::chrono_literals;

namespace {

volatile bool done = false;

}

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
  // RtMidiIn constructor
  auto midi_in = check_error([] { return RtMidiIn {}; });

  // Check inputs
  auto n_in_ports = midi_in.getPortCount();
  std::cout << "\nThere are " << n_in_ports
            << " MIDI input sources available.\n";

  for (auto i = 0; i < n_in_ports; ++i) {
    auto port_name = check_error([&] { return midi_in.getPortName(i); });
    std::cout << "  Input Port #" << i << ": " << port_name << '\n';
  }

  // RtMidiOut constructor
  auto midi_out = check_error([] { return RtMidiOut {}; });

  // Check outputs.
  auto n_out_ports = midi_out.getPortCount();
  std::cout << "\nThere are " << n_out_ports
            << " MIDI output ports available.\n";
  for (auto i = 0; i < n_out_ports; ++i) {
    auto port_name = check_error([&] { return midi_out.getPortName(i); });
    std::cout << "  Output Port #" << i << ": " << port_name << '\n';
  }
  std::cout << std::endl;

  // Open the first port after MIDI through
  check_error([&] { midi_in.openPort(1); });

  // Don't ignore sysex, timing, or active sensing messages
  midi_in.ignoreTypes(false, false, false);

  std::cout << "Reading MIDI from port ... quit with Ctrl-C.\n";
  std::signal(SIGINT, [] (int) { done = true; });
  std::vector<std::uint8_t> message;
  while (!done) {
    auto stamp = midi_in.getMessage(&message);
    auto n_bytes = message.size();
    for (int i = 0; i < n_bytes; ++i)
      std::cout << "Byte " << i << " = " << static_cast<int>(message[i])
                << ", ";
    if (n_bytes > 0)
      std::cout << "stamp = " << stamp << std::endl;
    // Sleep for 10 milliseconds ... platform-dependent.
    std::this_thread::sleep_for(10ms);
  }
  std::cout << "\nDone!" << std::endl;
  return 0;
}
