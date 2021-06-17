#ifndef MUSYCL_SUSTAIN_HPP
#define MUSYCL_SUSTAIN_HPP

/** \file Abstraction transmitting sustain

    The sustain pedal broadcast its status to any interested modules.
*/

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

#endif // MUSYCL_SUSTAIN_HPP
