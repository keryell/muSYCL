#ifndef MUSYCL_AUTOMATE_HPP
#define MUSYCL_AUTOMATE_HPP

/// \file Automation framework

#include <utility>

#include "triSYCL/detail/fiber_pool.hpp"

#include "clock.hpp"

namespace musycl {

/// Use a light-weight executor to run the various automate tasks
::trisycl::detail::fiber_pool fiber_executor {
  1, ::trisycl::detail::fiber_pool::sched::round_robin, false
};

/// Automate with some background task
class automate  : public clock::follow<automate> {
  /// A queue just used to notify the to submit work. The value is not used
  boost::fibers::unbuffered_channel<int> tick;

 public:
  /// Automate by launching a callable which is called with this
  /// instance as a context
  automate(auto&& f) {
    fiber_executor.submit(
        [&, f = std::forward<decltype(f)>(f)] mutable { f(*this); });
  }

  /// Wait for some MIDI ticks
  auto& pause(int midi_ticks) {
    int _;
    for (auto i = midi_ticks; i > 0; --i)
      // Consume the value
      tick.pop(_);
    return *this;
  }

  /// This is notified on each MIDI clock by the clocking framework
  void midi_clock(clock::type) {
    // Notify the callable
    tick.push({});
  }
};

} // namespace musycl

#endif // MUSYCL_AUTOMATE_HPP
