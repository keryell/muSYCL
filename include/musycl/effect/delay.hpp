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
  float feedback_ratio = 0.25;

 private:
  // The buffer implementing the delay line on the accelerator
  sycl::buffer<audio::sample_type> delay { delay_size };


 public:
  /**  Process an audio frame

       \param[inout] 1 audio frame which is processed
  */
  void process(audio::frame& audio) {
    // Make a buffer from the audio frame so it can processed from a SYCL kernel
    // sycl::buffer input_output { audio };
    sycl::buffer<audio::sample_type> input_output { audio.data(),
                                                    audio.size() };
    // Delay shift in term of sample number
    int shift = delay_line_time * sample_frequency;
    // A queue to the default device
    static sycl::queue q;
    // Submit a command group to the default device
    q.submit([&](auto& cgh) {
      // Request a read-write access to the delay buffer on the device
      sycl::accessor d { delay, cgh };
      // The delay processing kernel to run on the device
      cgh.parallel_for(frame_size, [=](std::size_t i) {
        // Shift the delay line by a frame towards the beginning,
        // strip-mined by frame. \todo Use SYCL scoped parallelism or
        // introduce algorithms with ad-hoc execution policy
        for (int sample = 0; sample < delay_size - frame_size;
             sample += frame_size)
          d[sample + i] = d[sample + frame_size + i];
      });
    });
    // Then, use the delay buffer
    q.submit([&](auto& cgh) {
      // Request a read-write access to the audio frame on the device
      sycl::accessor io { input_output, cgh };
      // Request a read-write access to the delay buffer on the device
      sycl::accessor d { delay, cgh };
      // The delay processing kernel to run on the device
      cgh.parallel_for(frame_size, [=, delay_line_ratio = delay_line_ratio,
                                    feedback_ratio =
                                        feedback_ratio](std::size_t i) {
        // Copy the audio frame to the end of the delay line
        d.rbegin()[i] = io.rbegin()[i];
        /// \todo add a synchronization here
        // Left channel
        io.rbegin()[i][0] += d.rbegin()[shift + i][0] * delay_line_ratio;
        // Right channel with twice the delay
        io.rbegin()[i][1] += d.rbegin()[2 * shift + i][2] * delay_line_ratio;
        // Re-inject some output into the input
        d.rbegin()[i] += feedback_ratio * io.rbegin()[i];
      });
    });
    /* The buffer destruction cause the data to be transferred from
       the device and be copy backed to the audio frame */
  }
};

} // namespace musycl::effect

#endif // MUSYCL_EFFECT_DELAY_HPP
