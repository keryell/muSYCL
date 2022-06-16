/** \file Example of a music synthesizer written in SYCL

    Rely on some triSYCL extensions (kernel I/O) and muSYCL extensions
    (MIDI and audio input/output)
*/
#include <algorithm>
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
  // midi_in.open(application_name, "input", RtMidi::LINUX_ALSA);

  // The audio interface
  musycl::audio audio_interface;
  /// The configuration part to adapt to the context
  // - Jack
  audio_interface.open(application_name, "output", application_name,
                       RtAudio::UNIX_JACK);
  // - ALSA
  // audio_interface.open(application_name, "output", application_name,
  // RtAudio::LINUX_ALSA);

  // The MIDI controller needs the MIDI output too to update the display/buttons
  musycl::midi_out midi_out;
  midi_out.open("muSYCL", "output", RtMidi::UNIX_JACK);

  // The (channel mapping to the sound parameter
  musycl::midi::channel_assignment channel_assignment;

  // The user interface abstraction
  musycl::user_interface ui;

  // Assume an Arturia KeyLab essential as a MIDI controller
  musycl::controller::keylab_essential controller { ui };

  // The sound generators producing the music, 1 per running note & MIDI channel
  std::map<musycl::midi::note_base_header, musycl::sound_generator> sounds;

  // MIDI message to be received
  musycl::midi::msg m;

  // Master volume of the output in [ 0, 1 ]
  float master_volume = 1;

  /// Master pitch bend on MIDI port 0 channel 0
  musycl::pitch_bend pb { 0, 0 };

  /// Master modulation wheel on MIDI port 0 channel 0
  musycl::modulation_actuator ma { 0, 0 };

  // A default arpeggiator
  musycl::arpeggiator arp;
  // controller.play_pause.name("Arpeggiator Start/Stop")
  controller.pad_2.name("Arpeggiator Start/Stop").add_action([&](bool v) {
    arp.run(v);
    controller.display("Arpeggiator running: " + std::to_string(v));
  });
  musycl::arpeggiator arp_bass {
    0, -1,
    [](auto& self) {
      static trisycl::vendor::trisycl::random::xorshift<> rng;
      if (self.current_clock_time.beat) {
        /// Insert a C0 note on each beat
        self.current_note = musycl::midi::on { 5, 24, (rng() & 63) + 64 };
        musycl::midi_in::insert(0, *self.current_note);
      } else
        self.stop_current_note();
    }
  };
  controller.pad_1.name("Bass arpeggiator Start/Stop").add_action([&](bool v) {
    arp_bass.run(v);
    controller.display("Bass arpeggiator running: " + std::to_string(v));
  });
  musycl::arpeggiator arp_low_high {
    60, 127,
    [&, start = false, octave = 0, index = 0](auto& self) mutable {
      // Otherwise use a default arpeggiator, work on the 16th of note
      if (self.current_clock_time.midi_clock_index %
              (musycl::midi::clock_per_quarter / 4) ==
          0) {
        start = !start;
        if (!start)
          self.stop_current_note();
        else if (!self.notes.empty()) {
          std::ranges::sort(self.notes);
          if (--index < 0 || index >= self.notes.size())
            index = self.notes.size() - 1;
          auto n = self.notes[index];
          n.channel = 1;
          octave = !octave;
          n.note += 12 - octave * 48;
          self.current_note = n;
          n.velocity = 60;
          musycl::midi_in::insert(0, n);
          std::cout << "Insert " << n << std::endl;
        }
      }
    }
  };
  controller.pad_3.name("Arpeggiator low & high Start/Stop")
      .add_action([&](bool v) {
        arp_low_high.run(v);
        controller.display("Low & high arpeggiator running: " +
                           std::to_string(v));
      });

  musycl::arpeggiator arp_bass_4 {
    60, 127,
    [&, start = false, running = false, measure = 0,
     n = std::optional<musycl::midi::on> {}](auto& self) mutable {
      // Cycle through 2 consecutive measures
      measure = (measure + self.current_clock_time.measure) % 2;
      // Run only during the first 2 beats of the first measure
      if (0 == measure) {
        if (self.current_clock_time.measure)
          running = true;
        else if (2 == self.current_clock_time.beat_index)
          running = false;
      }
      // Register the currently played lowest note
      if (!self.notes.empty()) {
        std::ranges::sort(self.notes);
        n = self.notes[0];
        n->channel = 2;
        n->note -= 36;
      }
      if (running && self.current_clock_time.midi_clock_index %
                             (musycl::midi::clock_per_quarter / 4) ==
                         0) {
        // Toggle between starting the note and stopping it
        start = !start;
        if (!start)
          self.stop_current_note();
        else if (n) {
          musycl::midi_in::insert(0, *n);
          self.current_note = n;
          std::cout << "Insert " << *n << std::endl;
        }
      }
    }
  };
  controller.pad_5.name("Arpeggiator with 4 basses Start/Stop")
      .add_action([&](bool v) {
        arp_bass_4.run(v);
        controller.display(" 4 bass arpeggiator running: " + std::to_string(v));
      });

  musycl::arpeggiator arp_jupiter_8 {
    60, 127,
    [&, start = false, index = 0,
     n = std::optional<musycl::midi::on> {}](auto& self) mutable {
      if (self.running && self.current_clock_time.midi_clock_index %
                                  (musycl::midi::clock_per_quarter / 4) ==
                              0) {
        // Toggle between starting the note and stopping it
        start = !start;
        if (!start)
          self.stop_current_note();
        else if (!self.notes.empty()) {
          std::ranges::sort(self.notes);
          if (index >= self.notes.size() * 4)
            index = 0;
          auto n = self.notes[index % self.notes.size()];
          n.channel = 1;
          n.note += 12 * (index / self.notes.size()) - 24;
          n.velocity = 100;
          musycl::midi_in::insert(0, n);
          self.current_note = n;
          std::cout << "Insert " << n << " at index " << index << std::endl;
          ++index;
        }
      }
    }
  };
  controller.pad_6.name("Jupiter 8 Arpeggiator Start/Stop")
      .add_action([&](bool v) {
        arp_jupiter_8.run(v);
        controller.display("Jupiter 8 arpeggiator running: " +
                           std::to_string(v));
      });

  // The master of time
  musycl::clock::set_tempo_bpm(120);
  // The rotary on the extreme top right of Arturia KeyLab 49
  controller.top_right_knob_9.name("Tempo rate")
      .add_action([&](musycl::midi::control_change::value_type v) {
        auto tempo = int { v } * 2;
        musycl::clock::set_tempo_bpm(tempo);
        controller.display("Tempo rate: " + std::to_string(tempo) + " bpm");
      });

  // The low pass filters for the output channels
  std::array<musycl::low_pass_filter, musycl::audio::channel_number>
      low_pass_filter;

  // The resonance filters for the output channels
  //std::array<musycl::resonance_filter, musycl::audio::channel_number>
  std::array<musycl::ladder_filter, musycl::audio::channel_number>
      resonance_filter;

  // Use "Cutoff" on Arturia KeyLab 49 to set the resonance frequency
  controller.cutoff_pan_1.name("Cutoff frequency")
      .add_action([&](musycl::midi::control_change::value_type v) {
        auto cut_off_freq =
            musycl::midi::control_change::get_log_scale_value_in(v, 20, 10000);
        for (auto& f : resonance_filter)
          f.set_frequency(cut_off_freq);
        controller.display("Resonance filter: " + std::to_string(cut_off_freq) +
                           " Hz");
      });

  // Use "Resonance" on Arturia KeyLab 49 to set the resonance
  controller.resonance_pan_2.name("Resonance factor")
      .add_action([&](float v) {
        // auto resonance = 10*std::log(v + 1.f) / std::log(128.f);
        auto resonance = 5*v;
        for (auto& f : resonance_filter)
          f.set_resonance(resonance);
        controller.display("Resonance factor: " + std::to_string(resonance));
      });

  // Create an LFO and start it
  musycl::lfo lfo;
  lfo.set_frequency(2).set_low(0.5).run();

  // Use MIDI CC 76 (LFO Rate on Arturia KeyLab 49) to set the LFO frequency
  controller.lfo_rate_pan_3.name("LFO rate")
      .add_action([&](musycl::midi::control_change::value_type v) {
        auto frequency =
            musycl::midi::control_change::get_log_scale_value_in(v, 0.1, 20);
        lfo.set_frequency(frequency);
        controller.display("LFO rate: " + std::to_string(frequency));
      });

  // Use MIDI CC 77 (LFO Amt on Arturia KeyLab 49) to set the LFO low level
  controller.lfo_amt_pan_4.name("LFO amount")
      .add_action([&](musycl::midi::control_change::value_type v) {
        auto low = musycl::midi::control_change::get_value_as<float>(v);
        lfo.set_low(low);
        controller.display("LFO low bar: " + std::to_string(low));
      });

  // Use MIDI CC 85 (master volume) to set the value of the... master_volume!
  musycl::midi_in::cc_variable<85>(master_volume);

  /// No reLU by default
  float rectication_ratio = 0;
  // Use MIDI CC 0x12 (Param 2/Pan 6) to set the rectification ratio
  controller.param_2_pan_6.name("Rectification ratio")
      .set_variable(rectication_ratio);

  // A simple stereo delay implemented with std::ranges
  musycl::effect::delay delay;
  controller.param_3_pan_7.name("Delay line time")
      .add_action([&](musycl::midi::control_change::value_type v) {
        delay.delay_line_time = v * v / 127.f / 127 * 2;
        controller.display(
            "Delay line time: " + std::to_string(delay.delay_line_time) + 's');
      });
  controller.param_4_pan_8.name("Delay line ratio")
      .set_variable(delay.delay_line_ratio);

  musycl::dco_envelope::param_t dcoe1 { ui, "DCO envelope 1", 0 };
  channel_assignment.assign(0, dcoe1);
  dcoe1->env_param->attack_time = 0.1;
  dcoe1->env_param->decay_time = 0.4;
  dcoe1->env_param->sustain_level = 0.3;
  dcoe1->env_param->release_time = 0.5;

  musycl::dco_envelope::param_t dcoe2 { ui, "DCO envelope 2", 1 };
  channel_assignment.assign(1, dcoe2);
  dcoe2->env_param->decay_time = .1;
  dcoe2->env_param->sustain_level = .1;

  // Triangle wave
  musycl::dco::param_t dco3 { ui, "Triangle wave", 2 };
  channel_assignment.assign(2, dco3);
  dco3->square_volume = 0;
  dco3->triangle_volume = 1;

  musycl::noise::param_t noise { ui, "Noise", 3 };
  channel_assignment.assign(3, noise);

  musycl::dco::param_t dco5 { ui, "Plain DCO", 4 };
  channel_assignment.assign(4, dco5);

  // Triangle wave with fast decay
  musycl::dco_envelope::param_t triangle6_fast_decay { ui,
                                                       "Triangle fast decay",
                                                       5 };
  channel_assignment.assign(5, triangle6_fast_decay);
  triangle6_fast_decay->dco_param->square_volume = 0;
  triangle6_fast_decay->dco_param->triangle_volume = 1;
  triangle6_fast_decay->env_param->decay_time = .1;
  triangle6_fast_decay->env_param->sustain_level = .1;

  // Control the DCO 1 & 3 parameters
  controller.attack_ch_1.connect(dcoe1->dco_param->square_volume);
  controller.attack_ch_1.connect(dco3->square_volume);
  controller.decay_ch_2.connect(dcoe1->dco_param->triangle_volume);
  controller.decay_ch_2.connect(dco3->triangle_volume);
  controller.sustain_ch_3.connect(dcoe1->dco_param->triangle_ratio);
  controller.sustain_ch_3.connect(dco3->triangle_ratio);
  controller.release_ch_4.connect(dcoe1->dco_param->triangle_fall_ratio);
  controller.release_ch_4.connect(dco3->triangle_fall_ratio);

  // Control the envelope of CH1 with Attack/CH5 to Release/CH8
  controller.attack_ch_5.connect(dcoe1->env_param->attack_time);
  controller.decay_ch_6.connect(dcoe1->env_param->decay_time);
  controller.sustain_ch_7.connect(dcoe1->env_param->sustain_level);
  controller.release_ch_8.connect(dcoe1->env_param->release_time);

  // Connect the sustain pedal to its MIDI event
  musycl::sustain sustain;
  musycl::midi_in::cc_action<64>(
      [&](std::int8_t v) { sustain.value(v); });

  controller.param_1_pan_5.name("Low pass filter").add_action([&](float a) {
    /* Use a frequency logarithmic scale between 1 Hz and half the
       sampling frequency */
    auto cut_off_freq =
        std::exp((a * std::log(0.5 * musycl::sample_frequency)));
    for (auto& f : low_pass_filter)
      f.set_cutoff_frequency(cut_off_freq);
    controller.display("Low pass filter: " + std::to_string(cut_off_freq) +
                       " Hz");
  });
  // The forever time loop
  for (;;) {
    /* Dispatch here all the potential incoming MIDI registered
       actions, so they will not cause race condition */
    musycl::midi_in::dispatch_registered_actions();
    // Process all the potential incoming MIDI messages on port 0
    while (sustain.process(0, m)) {
      // \todo implement as range transformation
      arp.midi(m);
      arp_low_high.midi(m);
      arp_bass_4.midi(m);
      arp_jupiter_8.midi(m);

      std::visit(
          trisycl::detail::overloaded {
              [&](musycl::midi::on& on) {
                ts::pipe::cout::stream()
                    << "MIDI on " << (int)on.note << std::endl;
                if (auto sp = channel_assignment.channels.find(on.channel);
                    sp != channel_assignment.channels.end())
                  sounds
                      .insert_or_assign(on.base_header(),
                                        musycl::sound_generator { sp->second })
                      .first->second.start(on);
                else
                  std::cerr << "Note on to unassigned MIDI channel "
                            << on.channel + 1 << std::endl;
              },
              [&](musycl::midi::off& off) {
                ts::pipe::cout::stream()
                    << "MIDI off " << (int)off.note << std::endl;
                if (auto s = sounds.find(off.base_header()); s != sounds.end())
                  s->second.stop(off);
                else
                  std::cerr << "No note to stop here on MIDI channel "
                            << off.channel + 1 << std::endl;
              },
              [&](musycl::midi::control_change& cc) {
                ts::pipe::cout::stream()
                    << "MIDI cc " << (int)cc.number << std::endl;
                // Attack/CH1 on Arturia Keylab 49 Essential
                if (cc.number == 73) {
                }
              },
              [&](musycl::midi::sysex& s) {
                // \todo find a better way
                if (s.v == std::vector<std::uint8_t> { 0x00, 0x20, 0x6b, 0x7f,
                                                       0x42, 0x02, 0x00, 0x00,
                                                       0x18, 0x7f }) {
                  // backward button on Arturia Keylab 49 Essential
                  channel_assignment.select_previous_channel();
                  auto sound_param =
                      channel_assignment.channels
                          [channel_assignment.current_selected_channel];
                  controller.display(
                      "Channel:" +
                      std::to_string(
                          channel_assignment.current_selected_channel) +
                      " " + sound_param.name());
                  ui.prioritize_layer(sound_param.get_group());
                } else if (s.v == std::vector<std::uint8_t> {
                                      0x00, 0x20, 0x6b, 0x7f, 0x42, 0x02, 0x00,
                                      0x00, 0x19, 0x7f }) {
                  // forward button on Arturia Keylab 49 Essential
                  channel_assignment.select_next_channel();
                  auto sound_param =
                      channel_assignment.channels
                          [channel_assignment.current_selected_channel];
                  controller.display(
                      "Channel:" +
                      std::to_string(
                          channel_assignment.current_selected_channel) +
                      " " + sound_param.name());
                  ui.prioritize_layer(sound_param.get_group());
                }
              },
              [&](auto&& other) {
                ts::pipe::cout::stream() << "other: " << m << std::endl;
              } },
          m);
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
        // Insert a low pass filter in the output with amplitude
        // controlled by an LFO
        s = f.filter(s*lfo.out());
      // Add a constant factor to avoid too much fading between 1 and 2 voices
      a /= 4 + sounds.size();
      // Insert a resonance filter in the output after volume
      // normalization to avoid too much saturation
      for (auto&& [s, f] : ranges::views::zip(a, resonance_filter))
        s = f.filter(s);
      // Put the master volume control at the end to take over filter
      // loud oscillation
      a *= master_volume;
    }

    // Add some echo-like delay
    delay.process(audio);

    // Then send the computed audio frame to the output
    musycl::audio::write(audio);
  }
  return 0;
}
