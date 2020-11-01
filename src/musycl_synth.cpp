/** \file Example of a music synthesizer written in SYCL

    Rely on some triSYCL extensions (kernel I/O) and muSYCL extensions
    (MIDI and audio input/output)
*/

#include <SYCL/sycl.hpp>
#include <SYCL/vendor/trisycl/pipe/cout.hpp>
#include <musycl/musycl.hpp>

auto constexpr debug_midi_input = true;

auto constexpr application_name = "musycl_synth";

namespace ts = sycl::vendor::trisycl;

/// Helper type for the visitor to do pattern matching on invokables
/// https://en.cppreference.com/w/cpp/utility/variant/visit
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
// Explicit deduction guide (not needed as of C++20)
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

int main() {
  // The MIDI input interface
  musycl::midi_in midi_in;

  // Access to the right input on the system
  midi_in.open(application_name, "input", RtMidi::UNIX_JACK, 0);
  //audio.open_out(application_name, RtMidi::UNIX_JACK, 0);
  //audio.open_out(application_name, RtMidi::LINUX_ALSA, 1);

  // The input loop
  for(;;) {
    ts::pipe::cout::write("Waiting for MIDI data...\n");
    std::visit(overloaded {
        [] (musycl::midi::on &&on) {
          ts::pipe::cout::stream() << "MIDI on " << (int)on.note << std::endl;
        },
        [] (musycl::midi::off &&off) {
          ts::pipe::cout::stream() << "MIDI off " << (int)off.note << std::endl;
        },
        [] (auto &&other) { ts::pipe::cout::stream() << "other"; }
      }, musycl::midi_in::read());
    ts::pipe::cout::write("MIDI data has been processed.\n");
  }
  return 0;
}
