/// \file A low pass filter based on IIR 1-tap integrator

#ifndef MUSYCL_LOW_PASS_FILTER_HPP
#define MUSYCL_LOW_PASS_FILTER_HPP

#include <cmath>
#include <iostream>
#include <numbers>

#include <musycl/config.hpp>

namespace musycl {

/** A low pass filter based on IIR 1-tap integrator

    https://en.wikipedia.org/wiki/Low-pass_filter#Simple_infinite_impulse_response_filter
*/
class low_pass_filter {
  /** Set the contribution of direct input to the output, in [ 0, 1 ],
      initialized to a pass-through */
  float smoothing_factor = 1;

  /// Single tap for the IIR output filter delay
  float iir_tap = 0;

public:

  /** Set the smoothing factor (the direct input ratio rather than the
      IIR feedback)

      \input sf is the smoothing factor in [0, 1]. 0 means pass
      through whil 1 means maximum low pass filter, ie 0 output.

      \return the object itself to enable command chaining
  */
  auto& set_smoothing_factor(float sf) {
    smoothing_factor = sf;
    std::cout << "low_pass_filter smoothing_factor = " << sf << std::endl;
    return *this;
  }


  /** Set the cutoff frequency of the filter

      \return the object itself to enable command chaining
  */
  auto& set_cutoff_frequency(float cf) {
    std::cout << "low_pass_filter cutoff frequency = " << cf << std::endl;
    set_smoothing_factor(2*std::numbers::pi*cf/sample_frequency
                         /(2*std::numbers::pi*cf/sample_frequency + 1));
    return *this;
  }


  /// Get a filtered output from an input value
  float filter(float in) {
    // 1 single-tap IIR filter
    auto out = smoothing_factor*in + (1 - smoothing_factor)*iir_tap;
    // Keep the current value to compute the next value
    iir_tap = out;
    return out;
  }
};

}
#endif // MUSYCL_LOW_PASS_FILTER_HPP
