/** \file Example of a music synthesizer written in SYCL

    Rely on some triSYCL extensions (kernel I/O) and muSYCL extensions
    (MIDI and audio input/output)
*/

#include <SYCL/sycl.hpp>
#include <SYCL/vendor/trisycl/pipe/cout.hpp>

#include <musycl/musycl.hpp>

#include <range/v3/all.hpp>

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
  // - Jack
  midi_in.open(application_name, "input", RtMidi::UNIX_JACK, 0);
  // - ALSA
  // audio.open_out(application_name, RtMidi::LINUX_ALSA, 1);

  // The audio interface
  musycl::audio audio;

  /// The configuration part to adapt to the context
  // - Jack
  audio.open(application_name, "output", "synth", RtAudio::UNIX_JACK);
  // - ALSA
  // audio.open(application_name, "output", RtAudio::LINUX_ALSA);

  // The oscillator generating signal
  musycl::dco osc;

  // MIDI message to be received
  musycl::midi::msg m;

  // The forever time loop
  for(;;) {
    // Process all the potential incoming MIDI events
    while (musycl::midi_in::try_read(m))
      std::visit(overloaded {
          [&] (musycl::midi::on& on) {
            ts::pipe::cout::stream() << "MIDI on " << (int)on.note << std::endl;
            osc.start(on);
          },
          [&] (musycl::midi::off& off) {
            ts::pipe::cout::stream() << "MIDI off "
                                     << (int)off.note << std::endl;
            osc.stop(off);
          },
          [] (auto &&other) { ts::pipe::cout::stream() << "other"; }
        }, m);
    // Then send the computed audio frame to the output
    musycl::audio::write(osc.audio());
  }
  return 0;
}
