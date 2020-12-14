/** \file An envelope generator

    https://en.wikipedia.org/wiki/Envelope_(music)
*/

#ifndef MUSYCL_ENVELOPE_HPP
#define MUSYCL_ENVELOPE_HPP

#include <variant>

#include "config.hpp"

namespace musycl {

/// A low frequency oscillator
class envelope {

  // Define the states of the finite state machine (FSM)

  /// The envelope generator is stopped
  struct stopped {};

  /// The envelope generator is in the attack phase after the note started
  struct attack {};

  /// The envelope generator is in the decay phase towards the sustain phase
  struct decay {};

  /** The envelope generator is in the sustain phase, the steady state
      when the note keep playing */
  struct sustain {};

  /// The envelope generator is in the release phase, towards the end
  struct release {};

  /// The possible states of the state machine
  using state_t = std::variant<stopped, attack, decay, sustain, release>;

  /// The current state of the state machine, the first one is the default
  state_t state;

public:

  /// Attack time, immediate sound by default
  float attack_time = 0;

  /// Decay time, go immediately to sustain phase by default
  float decay_time = 0;

  /// Sustain level, maximum level by default in the sustain phase
  float sustain_level = 1;

  /// Release time, go immediately to off by default
  float release_time = 0;

private:

  /// Envelope incremental time, tracking time since in the current state
  float state_time = 0;

  /// The current output level
  float output = 0;

  float release_start_level;

public:

  /** Start the envelope generator from the beginning

      \return the envelope generator itself to enable command chaining
  */
  auto& start() {
    // Go into attack mode
    state = attack {};
    state_time = 0;
    return *this;
  }


  /** Stop the envelope generator

      \return the envelope generator itself to enable command chaining
  */
  auto& stop() {
    // Got into release mode
    state = release {};
    // Starting the fading from current output level
    release_start_level = output;
    state_time = 0;
    return *this;
  }


  /** Update the envelope at the frame frequency

      Since it is an envelope generator, no need to update it at
      the audio frequency.

      \return the envelope generator itself to enable command chaining
  */
  auto& tick_frame_clock() {
    state_time += frame_period;
    // Loop to handle several FSM transitions in the same time step
    for(;;) {
      // To keep track of change
      auto previous = state;
      // Visit the current FSM state
      state = std::visit(trisycl::detail::overloaded {
          [&] (const stopped&) -> state_t {
            // The output level is 0 when the envelope is stopped
            output = 0;
            // Keep the state for ever, except if there is some external event
            return stopped {};
          },
          [&] (attack&) -> state_t {
            if (state_time >= attack_time) {
              // It is time to go into the decay phase
              state_time -= attack_time;
              // The output has reach now the maximum
              output = 1;
              return decay {};
            }
            // Stay in the current state but first compute the increasing output
            output = state_time/attack_time;
            return state;
          },
          [&] (decay&)  -> state_t {
            if (state_time >= decay_time) {
              // It is time to go into the sustain phase
              state_time -= decay_time;
              return sustain {};
            }
            // Stay in the current state but first compute the decreasing output
            output = 1 - (1 - sustain_level)*state_time/decay_time;
            return state;
          },
          [&] (sustain&) -> state_t {
            output = sustain_level;
            // Keep the state for ever, except if there is some external event
            return state;
          },
          [&] (release&)  -> state_t {
            if (state_time >= release_time) {
              // It is time to go into the stopped phase
              return stopped {};
            }
            // Fade the signal out and remain in the release state
            output = release_start_level*(1 - state_time/release_time);
            return state;
          }
        },
        state);

      if (previous.index() == state.index())
        // If the state did not change, no need to evaluate another change
        break;
    }
    return *this;
  }


  /// Return the running status
  bool is_running() {
    return !std::holds_alternative<stopped>(state);
  }


  /// Get the current value between [0, 1]
  float out() const {
    return output;
  }
};

}
#endif // MUSYCL_ENVELOPE_HPP
