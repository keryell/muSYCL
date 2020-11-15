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

  // The oscillators generating signal, 1 per running note
  std::map<musycl::midi::note_type, musycl::dco> osc;

  // MIDI message to be received
  musycl::midi::msg m;

  // The forever time loop
  for(;;) {
    // Process all the potential incoming MIDI events
    while (musycl::midi_in::try_read(m))
      std::visit(overloaded {
          [&] (musycl::midi::on& on) {
            ts::pipe::cout::stream() << "MIDI on " << (int)on.note << std::endl;
            osc[on.note].start(on);
          },
          [&] (musycl::midi::off& off) {
            ts::pipe::cout::stream() << "MIDI off "
                                     << (int)off.note << std::endl;
            osc[off.note].stop(off);
            osc.erase(off.note);
          },
          [] (auto &&other) { ts::pipe::cout::stream() << "other"; }
        }, m);

    // The output audio frame accumulator
    musycl::audio::frame audio = {};
    // For each oscillator
    for (auto&& [note, o] : osc) {
      auto out = o.audio();
      // Accumulate its audio output into the main output
      for (auto&& [e, a] : ranges::views::zip(out, audio))
        a += e;
    }

    // Normalize the audio by number of playing voices to avoid saturation
    for (auto& a : audio)
      // Add a constant factor to avoid too much fading between 1 and 2 voices
      a /= (4 + osc.size());

    // Then send the computed audio frame to the output
    musycl::audio::write(audio);
  }
  return 0;
}
