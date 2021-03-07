#include "rtaudio/RtAudio.h"
#include "rtmidi/RtMidi.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

/// The configuration part to adapt to the context
// Jack
auto constexpr midi_api = RtMidi::UNIX_JACK;
auto constexpr midi_in_port = 0;
// ALSA
//auto constexpr midi_api = RtMidi::LINUX_ALSA;
//auto constexpr midi_in_port = 1;
/// The configuration part to adapt to the context
// Jack
auto constexpr audio_api = RtAudio::UNIX_JACK;
// ALSA
//auto constexpr audio_api = RtAudio::LINUX_ALSA;

// To use time unit literals directly
using namespace std::chrono_literals;

std::atomic saw_level = 0.;
std::atomic dt = 0.;


/// Check for errors
auto check_error = [] (auto&& function) {
  try {
    return function();
  }
  catch (const RtAudioError &error) {
    error.printMessage();
    std::exit(EXIT_FAILURE);
  }
  catch (const RtMidiError &error) {
    error.printMessage();
    std::exit(EXIT_FAILURE);
  }
};


/// Process the incomming MIDI messages
void midi_in_callback(double time_stamp,
                      std::vector<std::uint8_t>* p_midi_message,
                      void* user_data ) {
  auto &midi_message = *p_midi_message;
  auto n_bytes = midi_message.size();
  for (int i = 0; i < n_bytes; ++i)
    std::cout << "Byte " << i << " = "
              << static_cast<int>(midi_message[i]) << ", ";
  std::cout << "time stamp = " << time_stamp << std::endl;
  if (midi_message[0] == 176 && midi_message[1] == 27)
    // Use the left pedal value of an FCB1010 to change the sound
    saw_level = (midi_message[2] - 64)/70.;
  else if (midi_message[0] == 144 && midi_message[2] != 0) {
    // Start the note
    auto frequency = 440*std::pow(2., (midi_message[1] - 69)/12.);
    dt = 2*frequency/48000;
  }
  else if (midi_message[0] == 128
      || (midi_message[0] == 144 && midi_message[2] == 0))
    // Stop the note
    dt = 0;
}


// 1 channel sawtooth wave generator.
int audio_callback(void *output_buffer, void *input_buffer,
                   unsigned int frame_size, double time_stamp,
                   RtAudioStreamStatus status, void *user_data) {
  auto buffer = static_cast<double*>(output_buffer);
  auto& last_value = *static_cast<double *>(user_data);
  if (status)
    std::cerr << "Stream underflow detected!" << std::endl;
  for (auto i = 0; i < frame_size; ++i) {
    // Add some clamping to the saw signal to change the sound
    buffer[i] = last_value > saw_level ? 1. : last_value;
    // Advance the phase
    last_value += dt;
    // The value is cyclic, between -1 and 1
    if (last_value >= 1.0)
      last_value -= 2.0;
  }

  return 0;
}


int main() {
  std::cout << "RtMidi version " << RtMidi::getVersion() << std::endl;
  /* Only from RtMidi 4.0.0...

  std::cout << "RtMidi version " << RtMidi::getVersion()
            << "\nAPI availables:" << std::endl;
  std::vector<RtMidi::Api> apis;
  RtMidi::getCompiledApi(apis);
  for (auto a : apis)
    std::cout << '\t' << RtMidi::getApiName(a) << std::endl;
  */

  // Create a MIDI input using Jack and a fancy client name
  auto midi_in = check_error([] { return RtMidiIn { midi_api,
                                                    "muSYCLtest" }; });

  auto n_ports = midi_in.getPortCount();
  std::cout << "There are " << n_ports << " MIDI input sources available."
            << std::endl;
  for (int i = 0; i < n_ports; ++i) {
    auto port_name = midi_in.getPortName(i);
    std::cout << "  Input Port #" << i << ": " << port_name << '\n';
  }

  // Open the first port and give it a fancy name
  check_error([&] { midi_in.openPort(midi_in_port, "testMIDIinput"); });

  // Don't ignore sysex, timing, or active sensing messages
  midi_in.ignoreTypes(false, false, false);

  // Drain the message queue to avoid leftover MIDI messages
  std::vector<std::uint8_t> message;
  do {
    // There is some race condition in RtMidi where the messages are
    // not seen if there is not some sleep here
    std::this_thread::sleep_for(1ms);
    midi_in.getMessage(&message);
  } while (!message.empty());

  // Handle MIDI messages with this callback function
  midi_in.setCallback(midi_in_callback, nullptr);

  auto audio = check_error([] { return RtAudio { audio_api }; });
  auto device = audio.getDefaultOutputDevice();
  RtAudio::StreamParameters parameters;
  parameters.deviceId = device;
  parameters.nChannels = 1;
  parameters.firstChannel = 0;
  RtAudio::StreamOptions options;
  options.streamName = "muSYCLtest";
  auto sample_rate = audio.getDeviceInfo(device).preferredSampleRate;
  auto frame_size = 256U; // 256 sample frames
  double data {};
  check_error([&] {
    audio.openStream(&parameters, nullptr, RTAUDIO_FLOAT64,
                     sample_rate, &frame_size, audio_callback, &data, &options,
                     [] (RtAudioError::Type type,
                         const std::string &error_text) {
                       std::cerr << error_text << std::endl;
                     });
  });
  std::cout << "Sample rate: " << sample_rate
            << "\nSamples per frame: " << frame_size << std::endl;
  // Start the sound generation
  check_error([&] { audio.startStream(); });


  std::cout << "\nReading MIDI input ... press <enter> to quit.\n";
  char input;
  std::cin.get(input);

  return 0;
}
