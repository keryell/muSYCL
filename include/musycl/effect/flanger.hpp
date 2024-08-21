#ifndef MUSYCL_EFFECT_FLANGER_HPP
#define MUSYCL_EFFECT_FLANGER_HPP

/// \file Simple stereo flanger effect with different parameters for
/// the left and right voice

/// #include <algorithm>
#include <cmath>
#include <numbers>
#include <range/v3/all.hpp>

#include <sycl/sycl.hpp>

#include "../config.hpp"

#include "../audio.hpp"

namespace musycl::effect {

/** A flanger effect implemented with a delay line

    The LFO is integrated since we want it computed at the audio
    frequency for a smooth rendering
*/
class flanger {
 public:
  /// Flanger ratio by default, typically between -1 and 1.
  /// The sign has the effect of changing the comb filter pattern.
  audio::sample<float> delay_line_ratio { { .left = 0.7, .right = -0.7 } };

  /// The phase in the waveform, between 0 and 1, at the start of the frame
  audio::sample<float> lfo_phase {};

  /// The phase increment per clock to generate the right frequency,
  /// 0.5 Hz & 0.13 Hz
  audio::sample<float> lfo_dphase { { .left = 0.5 / sample_frequency,
                                      .right = 0.13 / sample_frequency } };

 private:
  /// Keep at most 50 milliseconds of delay
  static constexpr float delay_line_time = 0.005;
  // Minimum delay to insure to avoid audible beats when the delay get
  // close to 0, so the real-time delay will vary between
  // minimum_delay_line_time and delay_line_time.
  static constexpr float minimum_delay_line_time = 0.0;

  /// The delay line size, rounded up since we will interpolate the
  /// signal between 2 consecutive elements + 1 frame to store the
  /// current input + 1, the whole rounded up to a frame simplify the
  /// computation
  static constinit const int delay_size =
      // While waiting for C++23 for constexpr std::ceil
      ((static_cast<int>(delay_line_time * sample_frequency + 1 + frame_size) +
        frame_size - 1) /
       frame_size) *
      frame_size;

  /// A queue to the default device
  sycl::queue q;

  // The buffer implementing the delay line on the accelerator
  sycl::buffer<audio::sample<>> delay_line { delay_size };

  // // The buffer used for debugging
  // sycl::buffer<double> debug_buffer { 10 };

 public:
  flanger() {
    static_assert(delay_size % frame_size == 0,
                  "delay_size is a multiple of frame_size");
    q.submit([&](auto& cgh) {
      // Initialize the delay line to 0
      cgh.fill(
          sycl::accessor { delay_line, cgh, sycl::write_only, sycl::no_init },
          {});
    });
  }

  /** Process an audio frame

      \param[inout] 1 audio frame which is processed
  */
  void process(audio::buffer input_output) {
    assert(lfo_phase >= 0 && lfo_phase < 1);
    // Delay shift in term of sample number
    int shift = delay_line_time * sample_frequency;
    // std::cout << "delay_size = " << delay_size
    //           << " delay_line_time = " << delay_line_time << std::endl;
    // Shift the delay line and insert the new audio sample at the end
    q.submit([&](auto& cgh) {
      // Request a read access to the audio frame on the device
      sycl::accessor io { input_output, cgh, sycl::read_only };
      // Request a read-write access to the delay buffer on the device
      sycl::accessor d { delay_line, cgh };
      cgh.parallel_for(frame_size, [=](int i) {
        // Shift the delay line by a frame towards the beginning,
        // strip-mined by frame
        for (int sample = 0; sample < delay_size - frame_size;
             sample += frame_size)
          d[sample + i] = d[sample + frame_size + i];
        // Insert the audio input at the end of the delay buffer.
        // Use same work-item alignment to avoid a barrier
        d.end()[-frame_size + i] = io[0][i];
      });
    });
    // Compute the flanger effect from the delay line, requiring
    // another kernel as a synchronization since we may access to
    // delayed elements written by other work-items
    q.submit([&](auto& cgh) {
      // Request a read-write access to the audio frame on the device
      sycl::accessor io { input_output, cgh };
      // Request a read access to the delay buffer on the device
      sycl::accessor d { delay_line, cgh, sycl::read_only };
      // sycl::accessor debug { debug_buffer, cgh };
      // Capture explicitly to avoid capturing \c *this
      cgh.parallel_for(frame_size, [=, delay_line_ratio = delay_line_ratio,
                                    lfo_phase = lfo_phase,
                                    lfo_dphase = lfo_dphase](int i) {
        auto single_voice_flanger = [&](int side) {
          // Consider a sinus LFO
          auto lfo = sycl::sin((lfo_phase[side] + i * lfo_dphase[side]) * 2 *
                               std::numbers::pi_v<float>);
          // The delay in sample to consider for this sample
          auto delay_index =
              ((lfo + 1) * (delay_line_time - minimum_delay_line_time) / 2 +
               minimum_delay_line_time) *
              sample_frequency;
          // Since the delay is not an integer number of samples,
          // use a linear interpolation between 2 samples. The
          // amount of sample to consider at the next sample is
          // the fractional part of the delay
          auto delay_ratio_at_p1 = delay_index - sycl::floor(delay_index);
          io[0][i][side] += delay_line_ratio[side] *
                            (d.end()[-frame_size + i - delay_index - 1][side] *
                                 delay_ratio_at_p1 +
                             d.end()[-frame_size + i - delay_index][side] *
                                 (1 - delay_ratio_at_p1));
          // if (i == 255) {
          //   debug[0] = lfo;
          //   debug[1] = lfo_phase;
          //   debug[2] = lfo_dphase;
          //   debug[3] = delay_index;
          //   debug[4] = delay_ratio_at_p1;
          // }
        };
        single_voice_flanger(audio::left);
        single_voice_flanger(audio::right);
      });
    });
    // sycl::host_accessor d { debug_buffer };
    // std::cout << " , " << d[0] << " , " << d[1] << " , " << d[2] << " , "
    //           << d[3] << " , " << d[4] << std::endl;
    // Move forward the LFO phase for the whole frame to catch-up with
    // the time "spent" in the kernel
    lfo_phase += frame_size * lfo_dphase;
    /// Keep only the fractional of the phase to avoid big numbers
    lfo_phase -= lfo_phase.floor();
  }
};

} // namespace musycl::effect

#endif // MUSYCL_EFFECT_FLANGER_HPP
