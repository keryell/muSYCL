/// \file A resonance filter based on 2-tap IIR+FIR filter

#ifndef MUSYCL_RESONANCE_FILTER_HPP
#define MUSYCL_RESONANCE_FILTER_HPP

#include <cmath>
#include <numbers>

namespace musycl {

/** A resonance filter based on 2-tap IIR with a 2-tap FIR to
    normalize the resonance level

    Source: "Resonance Filters", Gary P. Scavone
    https://www.music.mcgill.ca/~gary/618/week1/node13.html
*/
class resonance_filter {
  /// Resonance frequency of the filter
  float frequency;

  /// Resonance factor in [0, 1]
  float resonance;

  /// Implement the delay for the IIR and FIR
  float x1 {};
  float x2 {};
  float y1 {};
  float y2 {};

  /// The filter parameters, preinitialized for flat output
  float a1 {};
  float a2 {};
  float b0 { 1 };
  float b1 {};
  float b2 {};

  /// Recompute filter parameters from high-level objectives
  void update_parameters() {
    a1 = -2*resonance*std::cos(2*std::numbers::pi*frequency/sample_frequency);
    a2 = resonance*resonance;
    b0 = (1 - resonance*resonance)/2;
    b2 = -b0;
  }

public:

  /** Set the resonance frequency of the filter

      \return the object itself to enable command chaining
  */
  auto& set_frequency(float f) {
    frequency = f;
    std::cout << "resonance_filter frequency = " << f << std::endl;
    update_parameters();
    return *this;
  }


  /** Set the resonance factor of the filter

      \param[in] r is the resonance in [0, 1]. 0 is for a flat output
      while closer to 1 increase the resonance.

      \return the object itself to enable command chaining
  */
  auto& set_resonance(float r) {
    resonance = r;
    std::cout << "resonance_filter resonance = " << r << std::endl;
    update_parameters();
    return *this;
  }


  /// Get a filtered output from an input value
  float filter(float in) {
    auto y = b0*in + b1*x1 + b2*x2 -a1*y1 - a2*y2;
    x2 = x1;
    x1 = in;
    y2 = y1;
    y1 = y;
    return y;
  }
};

}
#endif // MUSYCL_RESONANCE_FILTER_HPP
