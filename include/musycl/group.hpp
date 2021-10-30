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

  /// Action to dispatch from a control item
  std::map<control::control_item*, std::function<void()>> control_items;

  /// A cache to the already created groups
  // static trisycl::detail::cache<std::string, detail::group> cache;

 protected:
  /// The controller associated to the group
  musycl::controller::keylab_essential* controller;

  //  controls
 public:
  group(musycl::controller::keylab_essential& c, const std::string& n)
      : name { n }
      , controller { &c } {}

  group() = default;

  /// Assign an action to a control item
  void assign(control::control_item& ci, std::function<void()> f) {
    control_items.emplace(&ci, f);
  }

  /** Try to dispatch an action associate to a control item

      \return true if the dispatch was successful or false if there is
      no action for this control item
   */
  template <typename ControlItem> bool try_dispatch(ControlItem& ci) const {
    return false;
  }
};

} // namespace musycl
#endif // MUSYCL_GROUP_HPP
