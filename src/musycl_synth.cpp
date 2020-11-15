/** \file Example of a music synthesizer written in SYCL

    Rely on some triSYCL extensions (kernel I/O) and muSYCL extensions
    (MIDI and audio input/output)
*/
#include <cmath>

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

  // Master volume of the output in [ 0, 1 ]
  float master_volume = 1;

  // Feedback in IIR output filter of the output in [ 0, 1 ]
  float iir_feedback = 0;

  // Single tap for IIR output filter of the output
  float iir_tap = 0;

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
          [&] (musycl::midi::control_change& cc) {
            ts::pipe::cout::stream() << "MIDI cc "
                                     << (int)cc.number << std::endl;
            // Attack/CH1 on Arturia Keylab 49 Essential
            if (cc.number == 73) {
              // The IIR filter trigger close to 1, so use a function to focus
              // there
              auto c = [] (auto x) { return std::pow(x, 0.005f); };
              auto ref = c(0.5f);
              // Keep the curve only from [0.5, 1] and renormalize in [0, 1]
              iir_feedback = (c(cc.value_1()*.5f + 0.5f) - ref)/(1 - ref);
              std::cout << "IIR " << iir_feedback << std::endl;
            }
            // Master volume on Arturia Keylab 49 Essential
            else if (cc.number == 85)
              master_volume = cc.value_1();
          },
          [] (auto &&other) { ts::pipe::cout::stream() << "other"; }
        }, m);

    // The output audio frame accumulator
    musycl::audio::frame audio {};
    // For each oscillator
    for (auto&& [note, o] : osc) {
      auto out = o.audio();
      // Accumulate its audio output into the main output
      for (auto&& [e, a] : ranges::views::zip(out, audio))
        a += e;
    }

    // Normalize the audio by number of playing voices to avoid saturation
    for (auto& a : audio) {
      // 1 single-tap IIR filter
      a = a*(1 - iir_feedback) + iir_tap*iir_feedback;
      iir_tap = a;
      // Add a constant factor to avoid too much fading between 1 and 2 voices
      a *= master_volume/(4 + osc.size());
    }
    // Then send the computed audio frame to the output
    musycl::audio::write(audio);
  }
  return 0;
}
