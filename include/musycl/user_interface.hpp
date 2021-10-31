#ifndef MUSYCL_USER_INTERFACE_HPP
#define MUSYCL_USER_INTERFACE_HPP

/** \file

    Represent the user interface of the synthesizer
*/

#include <algorithm>
#include <functional>
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

class user_interface {
  /** The user interface is made of a stack of active layers

      For a given \c physical_item, the current action is the first
      physical_item found in the active layers

      The top layer (the most visible) is the back of the vector and
      the bottom layer is the front of the vector.
  */
  std::vector<const group*> active_layers;

 public:
  /** Add a layer on top of the user interface

      \param[in] g is the layer to add
  */
  void add_layer(const group& g) { active_layers.emplace_back(&g); }

  /** Remove a layer from the user interface

      \param[in] g is the layer to remove
  */
  void remove_layer(const group& g) { ranges::remove(active_layers, &g); }

  /** Process an action on a physical_item into the current user interface

      \param[in] pi is the physical_item to process
  */
  void dispatch(control::physical_item& pi) {
    // Dispatch the physical_item with the first matching dispatcher
    // across the layer stack
    for (auto layer : active_layers | ranges::views::reverse)
      if (try_dispatch(layer, pi))
        break;
  }
};

/// Break an inclusion cycle in control.hpp
void inline user_interface_dispach_physical_item(user_interface& ui,
                                                 control::physical_item& pi) {
  ui.dispatch(pi);
}
} // namespace musycl

#endif // MUSYCL_USER_INTERFACE_HPP
