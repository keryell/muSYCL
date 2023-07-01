#ifndef MUSYCL_EFFECT_RANGE_DELAY_HPP
#define MUSYCL_EFFECT_RANGE_DELAY_HPP

/// \file Simple delay implemented with std::ranges on CPU, allowing only a
/// delay by an integral number of frame

#include <algorithm>

#include <range/v3/all.hpp>

#include "../config.hpp"

#include "../audio.hpp"

namespace musycl::effect {

class range_delay {
 public:
  // Keep 5 seconds of delay
  static constexpr auto frame_delay = static_cast<int>(5 * frame_frequency);

  /// Almost a 8th note of delay by default at 120 bpm sounds cool
  float delay_line_time = 0.245;

  /// No delay by default
  float delay_line_ratio = 0;

 private:
  std::array<audio::sample_type, frame_delay * frame_size> delay {};

 public:
  /// Process an audio frame
  void process(musycl::audio::frame& audio) {
    std::shift_left(delay.begin(), delay.end(), frame_size);
    std::ranges::copy(audio, delay.end() - frame_size);
    int shift = delay_line_time * sample_frequency;
    // Left channel
    auto f =
        ranges::subrange(delay.end() - frame_size - shift, delay.end() - shift);
    for (auto&& [a, d] : ranges::views::zip(audio, f))
      a[0] += d[0] * delay_line_ratio;
    // Right channel with twice the delay
    f = ranges::subrange(delay.end() - frame_size - 2 * shift,
                         delay.end() - 2 * shift);
    for (auto&& [a, d] : ranges::views::zip(audio, f))
      a[1] += d[1] * delay_line_ratio;
  }
};

} // namespace musycl::effect

#endif // MUSYCL_EFFECT_RANGE_DELAY_HPP
