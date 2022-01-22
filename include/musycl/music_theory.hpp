#ifndef MUSYCL_MUSIC_THEORY_HPP
#define MUSYCL_MUSIC_THEORY_HPP

/** \file Some music theory content

    https://en.wikipedia.org/wiki/Mode_(music)
*/

#include <array>

namespace musycl {
  /** The ionian (major) scale defined in semi-tone relative to the
      tonic node */
  inline std::array ionian_scale { 0, 2, 4, 5, 7, 9, 11 };

} // namespace musycl

#endif // MUSYCL_MUSIC_THEORY_HPP
