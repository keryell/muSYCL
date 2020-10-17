#include <rtaudio/RtAudio.h>

#include <cstdlib>
#include <iostream>
#include <vector>

auto check_error = [] (auto&& function) {
  try {
    return function();
  }
  catch (const RtAudioError &error) {
    error.printMessage();
    std::exit(EXIT_FAILURE);
  }
};

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
  return 0;
}
