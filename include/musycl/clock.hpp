#ifndef MUSYCL_CLOCK_HPP
#define MUSYCL_CLOCK_HPP

/// \file Manage the time and distribute the various clock ticks, like
/// audio frame tick, beat tick, measure tick

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

  /// Number of beats per measure (bar)
  static inline int meter = 4;

public:

  /// Describe the type of tick
  struct type {
    int beat_index; ///< Current beat in current measure (bar)
    bool measure; ///< True if this is the start of a measure
    bool beat; ///< True if this is the start of a beat
  };

private:

  /// Describes the type of clock tick to interested customers
  static inline type tick_type;

  /// Consumers of the frame clock
  /// \todo using member function pointer
  static inline std::map<void*, std::function<void()>> frame_clock_consumers;

  /// Distribute the frame clock
  static void notify_frame_clock() {
    for (const auto& [obj, fcc] : frame_clock_consumers)
      fcc();
  }

  /// Consumers of the measure clock
  static inline std::map<void*, std::function<void()>>measure_consumers;

  /// Distribute the measure clock
  static void notify_measure() {
    for (const auto& [obj, mc] : measure_consumers)
      mc();
  }

  /// Consumers of the beat clock
  static inline std::map<void*, std::function<void()>> beat_consumers;

  /// Distribute the beat clock
  static void notify_beat() {
    for (const auto& [obj, bc] : beat_consumers)
      bc();
  }

public:

  /// Set the global clock frequency in Hz
  static void set_tempo_frequency(float frequency) {
    dphase = frequency*frame_size/sample_frequency;
    std::cout << "Global clock frequency = " << frequency << " Hz, period = "
              << 1/frequency << " s, "
              << static_cast<int>(frequency*60) << " bpm" << std::endl;
  }


  /// Set the global clock beats-per-minute
  static void set_tempo_bpm(float bpm) {
    set_tempo_frequency(bpm/60);
  }


  /** Set the meter of the measure

      https://en.wikipedia.org/wiki/Metre_(music)
      https://en.wikipedia.org/wiki/Bar_(music)
  */
  static void set_meter(int beats) {
    meter = beats;
  }


  /// Action to do with the tick of the audio frame clock
  static void tick_frame_clock() {
    tick_type.beat = false;
    tick_type.measure = false;
    phase = phase + dphase;
    if (phase >= 1) {
      phase -= 1;
      tick_type.beat = true;
      if (tick_type.beat_index == 0) {
        tick_type.measure = true;
        notify_measure();
      }
      notify_beat();
    }
    notify_frame_clock();
    if (++tick_type.beat_index == meter)
      tick_type.beat_index = 0;
  }


  /// To receive a clocks, a class just needs to inherit from this CRTP class
  template <typename T>
  class follow {
    /** Track whether an object was registered to avoid unregistration
        at the end of moved objects */
    bool registered = false;


    /** Connect the object only to the right clocking framework
        according to the right receiver function in the customer
        class */
    void register_actions() {
      registered = true;
      if constexpr (requires { std::declval<T>().frame_clock(tick_type); })
        frame_clock_consumers[this] =
          [&, this] { static_cast<T&>(*this).frame_clock(tick_type); };
      else if constexpr (requires { std::declval<T>().frame_clock(); })
        frame_clock_consumers[this] =
          [this] { static_cast<T&>(*this).frame_clock(); };

      if constexpr (requires { std::declval<T>().beat(tick_type); })
        beat_consumers[this] =
          [&, this] { static_cast<T&>(*this).beat(tick_type); };
      else if constexpr (requires { std::declval<T>().beat(); })
        beat_consumers[this] = [this] { static_cast<T&>(*this).beat(); };

      if constexpr (requires { std::declval<T>().measure(tick_type); })
        measure_consumers[this] =
          [&, this] { static_cast<T&>(*this).measure(tick_type); };
      else if constexpr (requires { std::declval<T>().measure(); })
        measure_consumers[this] = [this] { static_cast<T&>(*this).measure(); };
    }


    /// Disconnect the object from the clocking framework
    void unregister_actions() {
      if (registered) {
        registered = false;
        if constexpr (requires { std::declval<T>().frame_clock(); })
          frame_clock_consumers.erase(this);
        if constexpr (requires { std::declval<T>().beat(); })
          beat_consumers.erase(this);
      }
    }

  public:

    /// Register the object to receive the clocks
    follow() {
      register_actions();
    }


    /// If we make a copy, we need to register the new copy
    follow(const follow&) {
      register_actions();
    }


    /** In a copy assignment, the copy is actually already registered
        and it will continue to work, so do nothing special */
    follow& operator=(const follow&) = default;


    /** If we move an object we need to unregister the old one and
        register the new one */
    follow(const follow&& old) {
      old.unregister_actions();
      register_actions();
    }


    /** In a move assignment, the copy is actually already registered
        and it will continue to work, but the moved-from object is no
        longer in a usable state, so unregister it */
    follow& operator=(follow&& old) {
      old.unregister_actions();
    }



    /// Unregister the object to receive the clocks
    ~follow() {
      unregister_actions();
    }

  };

};

}

#endif // MUSYCL_CLOCK_HPP
