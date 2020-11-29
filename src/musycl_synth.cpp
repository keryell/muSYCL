/** \file Example of a music synthesizer written in SYCL

    Rely on some triSYCL extensions (kernel I/O) and muSYCL extensions
    (MIDI and audio input/output)
*/
#include <cmath>

#include <SYCL/sycl.hpp>
#include <SYCL/vendor/trisycl/pipe/cout.hpp>
#include <triSYCL/detail/overloaded.hpp>

#include <musycl/musycl.hpp>

#include <range/v3/all.hpp>

auto constexpr debug_midi_input = true;

auto constexpr application_name = "musycl_synth";

namespace ts = sycl::vendor::trisycl;

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

  // A low pass filter for the output
  musycl::low_pass_filter low_pass_filter;

  // Create an LFO and start it
  musycl::lfo lfo;
  lfo.set_frequency(2).run();

  // Use MIDI CC 76 (LFO Rate on Arturia KeyLab 49) to set the LFO frequency
  musycl::midi_in::cc_action<76>
    ([&] (musycl::midi::control_change::value_type v) {
      lfo.set_frequency(musycl::midi::control_change::get_log_scale_value_in
                        (v, 0.1, 20));
    });

  // Use MIDI CC 85 (master volume) to set the value of the... master_volume!
  musycl::midi_in::cc_variable<85>(master_volume);

  // The forever time loop
  for(;;) {
    // Process all the potential incoming MIDI events
    while (musycl::midi_in::try_read(m))
      std::visit(trisycl::detail::overloaded {
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
              // Use a frequency logarithmic scale between 1 Hz and
              // the 4 times the sampling frequency
              low_pass_filter.set_cutoff_frequency
                (std::exp((cc.value_1()*std::log(4*musycl::sample_frequency))));
            }
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
      a = low_pass_filter.filter(a*lfo.out(0, 1));
      // Add a constant factor to avoid too much fading between 1 and 2 voices
      a *= master_volume/(4 + osc.size());
    }
    // Then send the computed audio frame to the output
    musycl::audio::write(audio);
    lfo.tick_clock();
  }
  return 0;
}
