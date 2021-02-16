/** \file Example of a music synthesizer written in SYCL

    Rely on some triSYCL extensions (kernel I/O) and muSYCL extensions
    (MIDI and audio input/output)
*/
#include <array>
#include <cmath>

#include <sycl/sycl.hpp>
#include <sycl/vendor/triSYCL/pipe/cout.hpp>
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
  std::map<musycl::midi::note_type, musycl::sound_generator> sounds;

  // MIDI message to be received
  musycl::midi::msg m;

  // Master volume of the output in [ 0, 1 ]
  float master_volume = 1;

  // An arpeggiator
  musycl::arpeggiator arp;
  arp.set_frequency(8);

  // A low pass filter for the output
  std::array<musycl::low_pass_filter, musycl::audio::channel_number>
    low_pass_filter;

  // A resonance filter for the output
  std::array<musycl::resonance_filter, musycl::audio::channel_number>
    resonance_filter;

  // Use "Cutoff" on Arturia KeyLab 49 to set the resonance frequency
  musycl::midi_in::cc_action<74>
    ([&] (musycl::midi::control_change::value_type v) {
       for (auto &f : resonance_filter)
         f.set_frequency
           (musycl::midi::control_change::get_log_scale_value_in(v, 20, 10000));
     });
  // Use "Resonance" on Arturia KeyLab 49 to set the resonance
  musycl::midi_in::cc_action<71>
    ([&] (musycl::midi::control_change::value_type v) {
       for (auto &f : resonance_filter)
         f.set_resonance(std::log(v + 1.f)/std::log(128.f));
     });

  // Create an LFO and start it
  musycl::lfo lfo;
  lfo.set_frequency(2).set_low(0).run();

  // Use MIDI CC 76 (LFO Rate on Arturia KeyLab 49) to set the LFO frequency
  musycl::midi_in::cc_action<76>
    ([&] (musycl::midi::control_change::value_type v) {
      lfo.set_frequency(musycl::midi::control_change::get_log_scale_value_in
                        (v, 0.1, 20));
    });

  // Use MIDI CC 77 (LFO Amt on Arturia KeyLab 49) to set the LFO low level
  musycl::midi_in::cc_action<77>
    ([&] (musycl::midi::control_change::value_type v) {
      lfo.set_low(musycl::midi::control_change::get_value_as<float>(v));
    });

  // Use MIDI CC 85 (master volume) to set the value of the... master_volume!
  musycl::midi_in::cc_variable<85>(master_volume);

  float rectication_ratio = 0;
  // Use MIDI CC 75 (CH 2) to set the rectification ratio
  musycl::midi_in::cc_variable<75>(rectication_ratio);

  // Control the envelope with Attack/CH5 to Release/CH8
  musycl::envelope::param_t env_param;
  musycl::midi_in::cc_variable<80>(env_param.attack_time);
  musycl::midi_in::cc_variable<81>(env_param.decay_time);
  musycl::midi_in::cc_variable<82>(env_param.sustain_level);
  musycl::midi_in::cc_variable<83>(env_param.release_time);

  // Connect the sustain pedal to its MIDI event
  musycl::midi_in::cc_action<64>([] (int v) { musycl::sustain::value(v); });

  // The forever time loop
  for(;;) {
    // Process all the potential incoming MIDI events
    while (musycl::midi_in::try_read(m)) {
      arp.midi(m);
      std::visit(trisycl::detail::overloaded {
          [&] (musycl::midi::on& on) {
            ts::pipe::cout::stream() << "MIDI on " << (int)on.note << std::endl;
            sounds.insert_or_assign
              (on.note, musycl::dco_envelope { env_param }.start(on));
          },
          [&] (musycl::midi::off& off) {
            ts::pipe::cout::stream() << "MIDI off "
                                     << (int)off.note << std::endl;
            sounds[off.note].stop(off);
          },
          [&] (musycl::midi::control_change& cc) {
            ts::pipe::cout::stream() << "MIDI cc "
                                     << (int)cc.number << std::endl;
            // Attack/CH1 on Arturia Keylab 49 Essential
            if (cc.number == 73) {
              // Use a frequency logarithmic scale between 1 Hz and
              // the 4 times the sampling frequency
              for (auto& f : low_pass_filter)
                f.set_cutoff_frequency
                  (std::exp((cc.value_1()
                             *std::log(4*musycl::sample_frequency))));
            }
          },
          [] (auto &&other) { ts::pipe::cout::stream() << "other"; }
        }, m);
    }

    // The output audio frame accumulator
    musycl::audio::frame audio {};
    // For each sound generator
    for (auto it = sounds.begin(); it != sounds.end();) {
      auto&& [note, o] = *it;
      o.tick_frame_clock();
      auto out = o.audio();
      // Accumulate its audio output into the main output
      for (auto&& [e, a] : ranges::views::zip(out, audio))
        a += e;
      if (o.is_running())
        // Just look at the next sound
        ++it;
      else
        // Remove the no longer running sound generator and skip over it
        it = sounds.erase(it);
    }

    // Normalize the audio by number of playing voices to avoid saturation
    for (auto& a : audio) {
      // Insert a rectifier in the output
      a = a*(1 - rectication_ratio) + rectication_ratio*sycl::abs(a);
      /// Dive into each (stereo) channel of the sample...
        // Insert a low pass filter in the output
      for (auto&& [s, f] : ranges::views::zip(a, low_pass_filter))
        // Insert a low pass filter in the output
        s = f.filter(s*lfo.out());
      // Insert a resonance filter in the output
      for (auto&& [s, f] : ranges::views::zip(a, resonance_filter))
        s = f.filter(s);
      // Add a constant factor to avoid too much fading between 1 and 2 voices
      a *= master_volume/(4 + sounds.size());
    }
    // Then send the computed audio frame to the output
    musycl::audio::write(audio);
    arp.tick_frame_clock();
    lfo.tick_frame_clock();
  }
  return 0;
}
