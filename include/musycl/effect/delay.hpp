#ifndef MUSYCL_EFFECT_DELAY_HPP
#define MUSYCL_EFFECT_DELAY_HPP

/// \file Simple delay implemented with std::ranges on CPU, allowing only a
/// delay by an integral number of frame

#include <algorithm>

#include <range/v3/all.hpp>

#include <sycl/sycl.hpp>

#include "../config.hpp"

#include "../audio.hpp"

namespace musycl::effect {

class delay {
 public:
  // Keep 5 seconds of delay
  static constexpr auto frame_delay = static_cast<int>(5 * frame_frequency);
  // Size of the delay line
  static constexpr auto delay_size = frame_delay * frame_size;

  /// Almost a 8th note of delay by default at 120 bpm sounds cool
  float delay_line_time = 0.245;

  /// No delay by default
  float delay_line_ratio = 0;

  /// Feedback from the output into the input
  float feedback_ratio = 0.2;

 private:
  // A queue to the default device
  sycl::queue q;

  // The buffer implementing the delay line on the accelerator
  sycl::buffer<audio::sample_type> delay_line { delay_size };

  // Buffer used for the output but which is also used for the
  // feedback, so keep it alive across following frame computation
  // \todo need to be 0-initialized. Improve SYCL specification
  sycl::buffer<audio::sample_type> output { frame_size };

 public:
  delay() {
    // This crashes DPC++ implementation
    //ranges::fill(sycl::accessor { output }, audio::sample_type {});
  }

  /**  Process an audio frame

       \param[inout] 1 audio frame which is processed
  */
  void process(audio::frame& audio) {
    // Make a buffer from the audio frame so it can processed from a SYCL kernel
    sycl::buffer<audio::sample_type> input_output { audio.data(),
                                                    audio.size() };
    // Delay shift in term of sample number
    int shift = delay_line_time * sample_frequency;

    // Submit a command group to the default device
    q.submit([&](auto& cgh) {
      // Request a read-write access to the delay buffer on the device
      sycl::accessor d { delay_line, cgh };
      // Shift the delay line with a kernel on the device
      cgh.parallel_for(frame_size, [=](int i) {
        // Shift the delay line by a frame towards the beginning,
        // strip-mined by frame
        for (int sample = 0; sample < delay_size - frame_size;
             sample += frame_size)
          d[sample + i] = d[sample + frame_size + i];
      });
    });
    // Complete the delay line with the input and the output feedback
    q.submit([&](auto& cgh) {
      // Request a read access to the audio frame on the device
      sycl::accessor io { input_output, cgh, sycl::read_only };
      // Request a read access to the previous output frame on the device
      sycl::accessor out { output, cgh, sycl::read_only };
      // Request a read-write access to the delay buffer on the device
      sycl::accessor d { delay_line, cgh };
      // Capture explicitly \c feedback_ratio to avoid capture \c *this
      cgh.parallel_for(frame_size, [=, feedback_ratio = feedback_ratio](int i) {
        // Copy the input frame to the end of the delay line
        d.rbegin()[i] = io.rbegin()[i];
        // Re-inject some output on-top of the input for the
        // feedback
        d.rbegin()[i] += feedback_ratio * out.rbegin()[i];
      });
    });
    // Then, use the delay buffer to compute the output
    q.submit([&](auto& cgh) {
      // Request a read-write access to the audio frame on the device
      sycl::accessor io { input_output, cgh };
      // Request a write access to the audio frame on the device
      sycl::accessor out { output, cgh, sycl::write_only };
      // Request a read-write access to the delay buffer on the device
      sycl::accessor d { delay_line, cgh };
      // The delay processing kernel to run on the device.
      // Capture explicitly \c delay_line_ratio to avoid capture \c *this
      cgh.parallel_for(frame_size, [=, delay_line_ratio =
                                           delay_line_ratio](int i) {
        // The output is the input plus some ratio of the delayed signal.
        // Left channel
        out.rbegin()[i][0] =
            io.rbegin()[i][0] + d.rbegin()[shift + i][0] * delay_line_ratio;
        // Right channel with twice the delay
        out.rbegin()[i][1] =
            io.rbegin()[i][1] + d.rbegin()[2 * shift + i][1] * delay_line_ratio;
        // Then copy back the output to the io audio frame to be returned
        io.rbegin()[i] = out.rbegin()[i];
      });
    });
    /* The \c input_output buffer destruction cause the data to be
       transferred from the device and be copy backed to the audio
       frame */
  }
};

} // namespace musycl::effect

#endif // MUSYCL_EFFECT_DELAY_HPP
