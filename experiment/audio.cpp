#include <rtaudio/RtAudio.h>

#include <iostream>
#include <vector>

int main() {
  std::cout << "RtAudio version " << RtAudio::getVersion()
            << "\nAPI availables:" << std::endl;
  std::vector<RtAudio::Api> apis;
  RtAudio::getCompiledApi(apis);
  for (auto a : apis)
    std::cout << '\t' << RtAudio::getApiName(a) << std::endl;

  RtAudio audio;
  // Determine the number of devices available
  unsigned int devices = audio.getDeviceCount();
  // Scan through devices for various capabilities
  RtAudio::DeviceInfo info;
  for (auto i = 0; i < devices; ++i) {
    info = audio.getDeviceInfo(i);
    if (info.probed == true) {
      std::cout << "device " << i << ": \"" << info.name << '\"'
                << "\n\tmaximum input channels = " << info.inputChannels
                << "\n\tmaximum output channels = " << info.outputChannels
                << "\n\tmaximum duplex channels = " << info.duplexChannels
                << "\n\tis the default input device: " << info.isDefaultInput
                << "\n\tis the default output device: " << info.isDefaultOutput
                << "\n\tSupported sample rates";
      for (auto sr : info.sampleRates)
        std::cout << "\n\t\t" << sr << " Hz";
      std::cout << "\n\tPrefered sample rates" << info.preferredSampleRate
                << " Hz";
      std::cout << "\n\tNative formats: " << std::hex << info.nativeFormats
                << std::endl;
    }
  }
  return 0;
}
