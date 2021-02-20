#ifndef MUSYCL_CLOCK_HPP
#define MUSYCL_CLOCK_HPP

/// \file Manage the time and distribute the various clocks

#include <functional>
#include <map>
#include <utility>

#include "config.hpp"

namespace musycl {

class clock {

  /// The phase in the beat
  static inline float phase = 0;

  /// The phase increment per frame clock to generate the right
  /// frequency
  static inline float dphase = 0;

  static inline std::map<void *, std::function<void()>> beat_consumers;

  static void notify_beat() {
    for (const auto& [obj, bc] : beat_consumers)
      bc();
  }

public:

  /// Set the global clock frequency in Hz
  static void set_frequency(float frequency) {
    dphase = frequency*frame_size/sample_frequency;
    std::cout << "Global clock frequency = " << frequency << " Hz, period = "
              << 1/frequency << " s, "
              << static_cast<int>(frequency*60) << " bpm" << std::endl;
  }


  /// Set the global clock beats-per-minute
  static void set_bpm(float bpm) {
    set_frequency(bpm/60);
  }


  static void tick_frame_clock() {
    phase = phase + dphase;
    if (phase >= 1) {
      phase -= 1;
      notify_beat();
    }
  }


  /// To receive a clocks, a class just needs to inherit from this CRTP class
  template <typename T>
  class follow {

  public:

    /// Register the object to receive the clocks
    follow() {
      if constexpr (requires { std::declval<T>().beat(); }) {
        beat_consumers[this] = [this] { static_cast<T&>(*this).beat(); };
      }
    }


    /// Unregister the object to receive the clocks
    ~follow() {
      if constexpr (requires { std::declval<T>().beat(); }) {
        beat_consumers.erase(this);
      }
    }
  };

};

}

#endif // MUSYCL_CLOCK_HPP
