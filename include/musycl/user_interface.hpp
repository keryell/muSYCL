#ifndef MUSYCL_USER_INTERFACE_HPP
#define MUSYCL_USER_INTERFACE_HPP

/** \file

    Represent the user interface of the synthesizer
*/

#include <algorithm>
//#include <ranges>
#include <vector>

#include <range/v3/all.hpp>

#include "control.hpp"

// Forward declare for now
//#include "group.hpp"

namespace musycl {

// Forward declare for now
class group;
bool try_dispatch(const group*, control::physical_item&);
namespace controller {
  class keylab_essential;
}

class user_interface {
  /** The user interface is made of a stack of active layers

      For a given \c physical_item, the current action is the first
      physical_item found in the active layers

      The top layer (the most visible) is the back of the vector and
      the bottom layer is the front of the vector.
  */
  std::vector<const group*> active_layers;

  user_interface(const user_interface&) = delete;
  user_interface(user_interface&&) = delete;

 public:

  /// The \c controller associated to the \c user_interface
  controller::keylab_essential* c;

  user_interface() = default;

  /// Add a control group to the user-interface
  void add_layer(const group& g) {
    std::cerr << "UI: Add layer group " << (void*)&g << std::endl;
    active_layers.push_back(&g);
    std::cout << "UI " << (void*)this << " has now " << active_layers.size()
              << " layers" << std::endl;
  }

  /** Remove a layer from the user interface

      \param[in] g is the layer to remove
  */
  void remove_layer(const group& g) { ranges::remove(active_layers, &g); }

  /** Move a layer at the first place

      \param[in] g is the layer to prioritize
  */
  void prioritize_layer(const group& g) {
    std::cerr << "UI: prioritize_layer group " << (void*)&g << std::endl;
    auto i = ranges::find(active_layers, &g);
    if (i == active_layers.end())
      return;
    std::cerr << "swapping " << (void*)*i << " with "
              << (void*)*active_layers.rbegin() << std::endl;
    // Exchange the group with the last element
    std::swap(*i, *active_layers.rbegin());
  }

  /** Process an action on a physical_item into the current user interface

      \param[in] pi is the physical_item to process
  */
  void dispatch(control::physical_item& pi) {
    std::cout << "Dispatch from UI " << (void*)this << " with "
              << active_layers.size() << " layers" << std::endl;
    // Dispatch the physical_item with the first matching dispatcher
    // across the layer stack
    for (auto layer : active_layers | ranges::views::reverse)
      if (try_dispatch(layer, pi))
        break;
  }

  void set_controller(controller::keylab_essential& cont) {
    c = &cont;
  }
};

/// Break an inclusion cycle in control.hpp
void user_interface_dispach_physical_item(user_interface& ui,
                                          control::physical_item& pi) {
  ui.dispatch(pi);
}

} // namespace musycl

#endif // MUSYCL_USER_INTERFACE_HPP
