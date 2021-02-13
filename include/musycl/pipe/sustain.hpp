#ifndef MUSYCL_PIPE_SUSTAIN_HPP
#define MUSYCL_PIPE_SUSTAIN_HPP

/** \file SYCL abstraction for a pipe transmitting sustain stain

    The sustain pedal broadcast its status to any interested modules,
    so try a SYCL pipe-like abstraction for this.
*/

#include "musycl/midi.hpp"

namespace musycl {

/// The sustain pedal exposed as a SYCL pipe
class sustain {
  static inline bool state = false;

public:

  static bool value() {
    return state;
  }


  static void value(bool v) {
    state = v;
  }
};

}

#endif // MUSYCL_PIPE_SUSTAIN_HPP
