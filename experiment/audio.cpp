#include <rtaudio/RtAudio.h>

#include <cstdlib>
#include <iostream>
#include <vector>

/// The configuration part to adapt to the context
// Jack
auto constexpr audio_api = RtAudio::UNIX_JACK;
// ALSA
//auto constexpr audio_api = RtAudio::LINUX_ALSA;

auto check_error = [] (auto&& function) {
  try {
    return function();
  }
  catch (const RtAudioError &error) {
    error.printMessage();
    std::exit(EXIT_FAILURE);
  }
};

auto seuil = -1.0;

// 1 channel sawtooth wave generator.
int callback(void *output_buffer, void *input_buffer,
             unsigned int frame_size, double stream_time,
             RtAudioStreamStatus status, void *user_data) {
  auto buffer = static_cast<double*>(output_buffer);
  auto& last_value = *static_cast<double *>(user_data);
  if (status)
    std::cerr << "Stream underflow detected!" << std::endl;
  for (auto i = 0; i < frame_size; ++i) {
      buffer[i] = last_value > seuil;
      last_value += 0.01;
      if (last_value >= 1.0)
        last_value -= 2.0;
  }
  seuil += 0.001;
  if (seuil >= 0.95)
    seuil -= 1.9;

  return 0;
}

int main() {
  std::cout << "RtAudio version " << RtAudio::getVersion()
            << "\nAPI availables:" << std::endl;
  std::vector<RtAudio::Api> apis;
  RtAudio::getCompiledApi(apis);
  for (auto a : apis) {
    std::cout << RtAudio::getApiName(a) << std::endl;

    auto audio = check_error([&] { return RtAudio { a }; });

    // Determine the number of devices available
    auto devices = audio.getDeviceCount();
    // Scan through devices for various capabilities
    for (auto i = 0; i < devices; ++i) {
      auto info = audio.getDeviceInfo(i);
      if (info.probed == true) {
        std::cout << "\tdevice " << i << ": \"" << info.name << '\"'
                  << "\n\t\tmaximum input channels = " << info.inputChannels
                  << "\n\t\tmaximum output channels = " << info.outputChannels
                  << "\n\t\tmaximum duplex channels = " << info.duplexChannels
                  << "\n\t\tis the default input device: "
                  << info.isDefaultInput
                  << "\n\t\tis the default output device: "
                  << info.isDefaultOutput
                  << "\n\t\tSupported sample rates";
        for (auto sr : info.sampleRates)
          std::cout << "\n\t\t\t" << sr << " Hz";
        std::cout << "\n\t\tPrefered sample rate: " << info.preferredSampleRate
                  << " Hz";
        std::cout << "\n\t\tNative formats: " << std::hex << info.nativeFormats
                  << std::dec << std::endl;
      }
    }
  }

  auto audio = check_error([] { return RtAudio { audio_api }; });
  auto device = audio.getDefaultOutputDevice();
  RtAudio::StreamParameters parameters;
  parameters.deviceId = device;
  parameters.nChannels = 1;
  parameters.firstChannel = 0;
  RtAudio::StreamOptions options;
  options.streamName = "essai";
  auto sample_rate = audio.getDeviceInfo(device).preferredSampleRate;
  auto frame_size = 256U; // 256 sample frames
  double data {};
  check_error([&] {
    audio.openStream(&parameters, nullptr, RTAUDIO_FLOAT64,
                     sample_rate, &frame_size, callback, &data, &options,
                     [] (RtAudioError::Type type,
                         const std::string &error_text) {
                       std::cerr << error_text << std::endl;
                     });
  });
  std::cout << "Sample rate: " << sample_rate
            << "\nSamples per frame: " << frame_size << std::endl;
  // Start the sound generation
  check_error([&] { audio.startStream(); });
  char input;
  std::cout << "\nPlaying ... press <enter> to quit.\n";
  std::cin.get(input);
  // Stop the stream
  check_error([&] { audio.stopStream(); });
  check_error([&] { audio.closeStream(); });
  return 0;
}
