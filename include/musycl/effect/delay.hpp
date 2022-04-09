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

  /// Almost a 8th note of delay by default at 120 bpm sounds cool
  float delay_line_time = 0.245;

  /// No delay by default
  float delay_line_ratio = 0;

 private:
  // The buffer implementing the delay line on the accelerator
  sycl::buffer<audio::sample_type> delay { frame_delay * frame_size };


 public:
  /// Process an audio frame
  void process(audio::frame& audio) {
    // Input-audio audio with 1 audio frame
    sycl::buffer<trisycl::vec<double, 2>> input_output { audio };
    sycl::queue {}.submit([&](auto cgh) {
      sycl::accessor io { input_output, cgh };
      sycl::accessor d { delay, cgh };
      static_assert(io.access_mode() == sycl::access::mode::read_write);
      cgh.single_task([=] {
        std::shift_left(d.begin(), d.end(), frame_size);
        std::ranges::copy(io, d.end() - frame_size);
        int shift = delay_line_time * sample_frequency;
        // Left channel
        auto f = ranges::subrange(d.end() - frame_size - shift,
                                  d.end() - shift);
        for (auto&& [a, delayed] : ranges::views::zip(io, f))
          a.x() += delayed.x() * delay_line_ratio;
        // Right channel with twice the delay
        f = ranges::subrange(d.end() - frame_size - 2 * shift,
                             d.end() - 2 * shift);
        for (auto&& [a, delayed] : ranges::views::zip(io, f))
          a.y() += delayed.y() * delay_line_ratio;
      });
    });
  }
};

} // namespace musycl::effect

#endif // MUSYCL_EFFECT_DELAY_HPP
