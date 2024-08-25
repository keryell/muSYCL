// This is inspired from libremidi/examples/midiprobe.cpp plus some pieces

#include <ios>
#include <libremidi/client.hpp>
#include <libremidi/libremidi.hpp>

#include <iostream>
#include <vector>

/// The configuration part to adapt to the context
// PipeWire
auto constexpr midi_api = libremidi::API::PIPEWIRE;
// Jack
// auto constexpr midi_api = libremidi::API::JACK_MIDI;
// ALSA
// auto constexpr midi_api = libremidi::API::ALSA_RAW;

namespace {

/// Set to true to end the infinite loop
std::atomic<bool> done = false;

} // namespace

// This comes from libremidi/examples/utils.hpp and should be provided by the
// libremidi
inline std::ostream& operator<<(std::ostream& s,
                                const libremidi::port_information& rhs) {
  s << "[ client: " << rhs.client << ", port: " << rhs.port;
  if (!rhs.manufacturer.empty())
    s << ", manufacturer: " << rhs.manufacturer;
  if (!rhs.device_name.empty())
    s << ", device: " << rhs.device_name;
  if (!rhs.port_name.empty())
    s << ", portname: " << rhs.port_name;
  if (!rhs.display_name.empty())
    s << ", display: " << rhs.display_name;
  return s << "]";
}

int main() {
  std::vector<libremidi::input_port> input_ports;
  std::vector<libremidi::output_port> output_ports;
  for (auto& api : libremidi::available_apis()) {
    auto api_name = libremidi::get_api_display_name(api);
    std::cout << "Displaying ports for: " << api_name << std::endl;

    libremidi::observer midi { {}, libremidi::observer_configuration_for(api) };
    auto display_and_gather = [&](auto& message, auto&& ports,
                                  auto& gather_port_container) {
      std::cout << ports.size() << message;
      for (auto& port : ports) {
        std::cout << " - " << port << '\n';
        // Just focus on this API
        if (api == midi_api)
          gather_port_container.push_back(port);
      }
    };
    display_and_gather(" MIDI input sources:\n", midi.get_input_ports(),
                       input_ports);
    display_and_gather(" MIDI output sources:\n", midi.get_output_ports(),
                       output_ports);
  }

  libremidi::midi1::client client {
    { .api = midi_api,
      .client_name = "midi-libre",
      .on_message =
          [&](const libremidi::input_port& port,
              const libremidi::message& message) {
            std::cerr << std::dec << port.display_name << ", received "
                      << message.bytes.size()
                      << " bytes, timestamp: " << message.timestamp << "\n ";
            for (int c : message)
              std::cerr << std::hex << c << ' ';
            std::cerr << '\n';
          },
      .ignore_sysex = false,
      .ignore_timing = false,
      .ignore_sensing = false }
  };

  // The port list could actually just come from client::get_input_ports()
  for (auto& port : input_ports) {
    client.add_input(port, "midi-libre for " + port.display_name);
  }

  std::cout << "\nReading MIDI inputs from backend "
            << libremidi::get_api_display_name(midi_api)
            << "... press <enter> to quit.\n";
  char input;
  std::cin.get(input);
  std::cout << "\nDone!" << std::endl;
  return 0;
}
