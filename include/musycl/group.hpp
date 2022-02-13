#ifndef MUSYCL_GROUP_HPP
#define MUSYCL_GROUP_HPP

/** \file Abstractions for group of control items which can be
    activated on the user interface at some point

*/

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

//#include "triSYCL/detail/cache.hpp"

#include "musycl/control.hpp"
#include <musycl/midi/controller/keylab_essential.hpp>
#include <musycl/user_interface.hpp>

namespace musycl {

/// Represent a set of control which can be activated on the user-interface
class group {
 public:
  /// User-facing name
  std::string name;

  /// A group can be associated to a MIDI channel \todo unused?
  std::optional<midi::channel_type> channel;

 private:
  /// Action to dispatch from a control item
  std::map<control::physical_item*, std::function<void()>> physical_items;

  /// A group can also have subgroup to have controllers associated
  std::vector<group*> sub_groups;

  /// A cache to the already created groups
  // static trisycl::detail::cache<std::string, detail::group> cache;

 protected:
  /// The \c user_interface associated to the \c group
  user_interface* ui;

  /// The \c controller associated to the \c group
  controller::keylab_essential* controller;

 public:
  group(user_interface& ui, const std::string& n,
        std::optional<midi::channel_type> midi_channel = {})
      : name { n }
      , channel { midi_channel }
      , ui { &ui }
      , controller { ui.c } {
    // Add the group to the user-interface
    std::cerr << "Add group layer " << name << std::endl;
    ui.add_layer(*this);
  }

  group() = default;

  /// Assign an action to a control item
  void assign(control::physical_item& ci, std::function<void()> f) {
    std::cout << "Register assign group " << (void*)this << " PI "
              << (void*)&ci << std::endl;
    physical_items.emplace(&ci, f);
  }

  /// Add a sub-group part of this group
  void add_as_sub_group_to(group& group_owner) {
    group_owner.sub_groups.push_back(this);
  }

  /** Try to dispatch an action associate to a control item

      \return true if the dispatch was successful or false if there is
      no action for this control item
   */
  bool try_dispatch(control::physical_item& ci) const {
    std::cout << "Try dispatch group " << (void*)this << " PI "
              << (void*)&ci << std::endl;
    auto v = physical_items.find(&ci);
    if (v == physical_items.end())
      return false;
    std::cerr << "Dispatch from group " << name << std::endl;
    v->second();
    return true;
  }
};

// To break a cycle from user_interface.hpp
bool inline try_dispatch(const group* g, control::physical_item& pi) {
  return g->try_dispatch(pi);
};

} // namespace musycl
#endif // MUSYCL_GROUP_HPP
