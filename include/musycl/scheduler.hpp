#ifndef MUSYCL_SCHEDULER_HPP
#define MUSYCL_SCHEDULER_HPP

/// \file Schedule actions at some time point or duration from now

#include <chrono>
#include <functional>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

namespace musycl {

/// A simple scheduler based on wall clock time instead of MIDI time
class scheduler {
 public:
  using time_point_t = std::chrono::time_point<std::chrono::steady_clock>;
  using duration_t = time_point_t::duration;
  using action_t = std::function<void(time_point_t)>;
  using appointment = std::pair<time_point_t, action_t>;
  // There are some problems for now to replace this by an unevaluated lambda
  struct greater {
    constexpr bool operator()(const auto& lhs, const auto& rhs) const {
      return lhs.first > rhs.first;
    }
  };
  using pq =
      std::priority_queue<appointment, std::vector<appointment>, greater>;
  pq priority_queue;

  /// Get the current time point
  auto now() { return std::chrono::steady_clock::now(); }

  /** Handle the scheduling of actions underneath

      This is an infrastructure function expected to be called on a
      regular basis */
  void schedule() {
    while (!priority_queue.empty() && priority_queue.top().first <= now()) {
      auto [wake_time, action] =  priority_queue.top();
      action(wake_time);
      priority_queue.pop();
    }
  }

  /// Create an appointment at some time point
  void appoint_at(const time_point_t& tp, action_t action) {
    priority_queue.emplace(tp, action);
  }

  /// Create an appointment at duration from now
  void appoint_in(const duration_t& d, action_t action) {
    appoint_at(now() + d, action);
  }

  /// Create a cyclic appointment every duration
  template <typename Duration>
  void appoint_cyclic(Duration&& d, action_t action) {
    /* Convert the duration to the one used for the time point of the
       scheduler.

       It is required when user use literals like 0.25s which are
       represented as long double */
    auto duration = std::chrono::duration_cast<duration_t>(d);
    // The repetition is an action that re-appoints itself in the future
    auto repeat = std::make_shared<action_t>();
    *repeat = [=, this](time_point_t wake_time) {
      // Note that the self-capture of repeat create a memory leak in
      // the future if we can remove appointments...
      action(wake_time);
      appoint_at(wake_time + duration, *repeat);
    };
    appoint_in(duration, *repeat);
  }
};

} // namespace musycl

#endif // MUSYCL_SCHEDULER_HPP
