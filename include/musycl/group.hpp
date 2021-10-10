#ifndef MUSYCL_GROUP_HPP
#define MUSYCL_GROUP_HPP

/** \file Abstractions for group of control items which can be
    activated on the user interface at some point

*/

#include <functional>
#include <map>
#include <string>

//#include "triSYCL/detail/cache.hpp"

#include "musycl/control.hpp"
#include <musycl/midi/controller/keylab_essential.hpp>

namespace musycl {

/// Represent a set of control which can be activated on the user-interface
class group {

  /// User-facing name
  std::string name;

  std::map<control::control_item*, std::function<void()>> control_items;

  /// A cache to the already created groups
  // static trisycl::detail::cache<std::string, detail::group> cache;

 protected:
  musycl::controller::keylab_essential* controller;

  //  controls
 public:
  group(musycl::controller::keylab_essential& c, const std::string& n)
      : name { n }
      , controller { &c } {}

  group() = default;

  void assign(control::control_item& ci, std::function<void()> f) {
    control_items.emplace(&ci, f);
  }

  template <typename T> bool try_dispatch(T& ci) const {
    //    bool try_dispatch(control::control_item& ci) const {
    return false;
  }
};

} // namespace musycl
#endif // MUSYCL_GROUP_HPP
