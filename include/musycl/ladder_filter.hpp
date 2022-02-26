/// \file A ladder resonance filter, crude approximation of a Moog's one

#ifndef MUSYCL_LADDER_RESONANCE_FILTER_HPP
#define MUSYCL_LADDER_RESONANCE_FILTER_HPP

#include <array>
#include <cmath>
#include <numbers>

#include "musycl/low_pass_filter.hpp"

namespace musycl {

class ladder_filter {
  /// Looping on the filter
  float loop = 0;

  /// Resonance factor in [0, 1]
  float resonance = 0;

  /// Implement the delay for the IIR and FIR
  std::array<low_pass_filter, 4> filters;

public:

  /** Set the resonance frequency of the filter

      \return the object itself to enable command chaining
  */
  auto& set_frequency(float f) {
    std::cout << "resonance_filter frequency = " << f << std::endl;
    for (auto& filter : filters)
      filter.set_cutoff_frequency(f);
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
    return *this;
  }


  /// Get a filtered output from an input value
  float filter(float in) {
    /* Apply 4 low-pass filters in a row with some negative feedback for the
       resonance.

       Clamp the output to avoid divergence for high resonance,=. */
    loop = std::clamp(filters[0].filter(filters[1].filter(filters[2].filter(
                          filters[3].filter(in - loop * resonance)))),
                      -1.f, 1.f);
    return loop;
  }
};
}
#endif // MUSYCL_LADDER_RESONANCE_FILTER_HPP
