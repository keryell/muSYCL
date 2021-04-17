/** \file Example of a music synthesizer written in SYCL

    Rely on some triSYCL extensions (kernel I/O) and muSYCL extensions
    (MIDI and audio input/output)
*/
#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <variant>

#include <sycl/sycl.hpp>
#include <sycl/vendor/triSYCL/pipe/cout.hpp>
#include <triSYCL/detail/overloaded.hpp>

auto constexpr debug_midi_input = true;

#include <musycl/musycl.hpp>
#include <musycl/midi/controller/keylab_essential.hpp>

#include <range/v3/all.hpp>

auto constexpr application_name = "musycl_synth";

namespace ts = sycl::vendor::trisycl;

int main() {
  // The MIDI input interface
  musycl::midi_in midi_in;
  // Access to the right input on the system
  // - Jack
  midi_in.open(application_name, "input", RtMidi::UNIX_JACK);
  // - ALSA
  // audio.open_out(application_name, RtMidi::LINUX_ALSA);

  // The audio interface
  musycl::audio audio;

  /// The configuration part to adapt to the context
  // - Jack
  audio.open(application_name, "output", application_name, RtAudio::UNIX_JACK);
  // - ALSA
  // audio.open(application_name, "output", RtAudio::LINUX_ALSA);

  // The MIDI controller needs the MIDI output too to update the display/buttons
  musycl::midi_out midi_out;
  midi_out.open("muSYCL", "output", RtMidi::UNIX_JACK);

  // Assume an Arturia KeyLab essential as a MIDI controller
  musycl::controller::keylab_essential controller;

  // The oscillators generating signal, 1 per running note
  std::map<musycl::midi::note_type, musycl::sound_generator> sounds;

  // MIDI message to be received
  musycl::midi::msg m;

  // Master volume of the output in [ 0, 1 ]
  float master_volume = 1;

  // An arpeggiator
  musycl::arpeggiator arp;

  // The master of time
  musycl::clock::set_tempo_frequency(8);
  // The rotary on the extreme top right of Arturia KeyLab 49
  musycl::midi_in::cc_action<17>
    ([&] (musycl::midi::control_change::value_type v) {
       musycl::clock::set_tempo_frequency(v/10.);
     });

  // The low pass filters for the output channels
  std::array<musycl::low_pass_filter, musycl::audio::channel_number>
    low_pass_filter;

  // The resonance filters for the output channels
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
  lfo.set_frequency(2).set_low(0.5).run();

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
  // Use MIDI CC 0x12 (Param 2/Pan 6) to set the rectification ratio
  musycl::midi_in::cc_variable<75>(rectication_ratio);
  controller.param_2_pan_6.set_variable(rectication_ratio);

  musycl::dco_envelope::param_t dcoe1;
  dcoe1.env.attack_time = 0.1;
  dcoe1.env.decay_time = 0.4;
  dcoe1.env.sustain_level = 0.3;
  dcoe1.env.release_time = 0.5;

  musycl::dco_envelope::param_t dcoe2;
  dcoe2.env.decay_time = .1;
  dcoe2.env.sustain_level = .1;

  // MIDI channel mapping
  musycl::sound_generator::param_t channel_assign[] {
    dcoe1,
    dcoe2,
    musycl::dco::param_t {},
    musycl::noise::param_t {}
  };

  // Control the envelope of CH1 with Attack/CH5 to Release/CH8
  musycl::midi_in::cc_variable<80>(dcoe1.env.attack_time.value);
  musycl::midi_in::cc_variable<81>(dcoe1.env.decay_time.value);
  musycl::midi_in::cc_variable<82>(dcoe1.env.sustain_level.value);
  musycl::midi_in::cc_variable<83>(dcoe1.env.release_time.value);

  // Connect the sustain pedal to its MIDI event
  musycl::midi_in::cc_action<64>([] (int v) { musycl::sustain::value(v); });

  controller.param_1_pan_5.name("Low pass filter").add_action([&](float a) {
    /* Use a frequency logarithmic scale between 1 Hz and half the
       sampling frequency */
    auto cut_off_freq = std::exp((a*std::log(0.5*musycl::sample_frequency)));
    for (auto& f : low_pass_filter)
      f.set_cutoff_frequency(cut_off_freq);
    controller.display("Low pass filter: "
                       + std::to_string(cut_off_freq) + " Hz");
  });
  // The forever time loop
  for(;;) {
    // Process all the potential incoming MIDI events
    while (musycl::midi_in::try_read(m)) {
      arp.midi(m);
      std::visit(trisycl::detail::overloaded {
          [&] (musycl::midi::on& on) {
            ts::pipe::cout::stream() << "MIDI on " << (int)on.note << std::endl;
            if (on.channel < std::size(channel_assign))
              sounds.insert_or_assign
                (on.note,
                 musycl::sound_generator { channel_assign[on.channel] })
                .first->second.start(on);
            else
              std::cerr << "Note on to unassigned MIDI channel "
                        << on.channel + 1 << std::endl;
          },
          [&] (musycl::midi::off& off) {
            ts::pipe::cout::stream() << "MIDI off "
                                     << (int)off.note << std::endl;
            if (off.channel < std::size(channel_assign))
              sounds[off.note].stop(off);
            else
              std::cerr << "Note off to unassigned MIDI channel "
                        << off.channel + 1 << std::endl;
          },
          [&] (musycl::midi::control_change& cc) {
            ts::pipe::cout::stream() << "MIDI cc "
                                     << (int)cc.number << std::endl;
            // Attack/CH1 on Arturia Keylab 49 Essential
            if (cc.number == 73) {
            }
          },
          [] (auto &&other) { ts::pipe::cout::stream() << "other"; }
        }, m);
    }

    // Propagate the clocks to the consumers
    musycl::clock::tick_frame_clock();

    // The output audio frame accumulator
    musycl::audio::frame audio {};
    // For each sound generator
    for (auto it = sounds.begin(); it != sounds.end();) {
      auto&& [note, o] = *it;
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
  }
  return 0;
}
