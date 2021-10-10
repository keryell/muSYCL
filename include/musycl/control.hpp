#ifndef MUSYCL_CONTROL_HPP
#define MUSYCL_CONTROL_HPP

/** \file Abstractions for control

*/

#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "triSYCL/detail/shared_ptr_implementation.hpp"

namespace musycl {

class control {
 public:
  /// Represent basic API of various type of physical values like time or level
  template <typename ValueType, typename FinalPhysicalValue>
  class physical_value {
   public:
    using value_type = ValueType;

    const value_type min_value;

    const value_type max_value;

    value_type value;

    physical_value(auto min, auto max, value_type default_value = {})
        : min_value { static_cast<value_type>(min) }
        , max_value { static_cast<value_type>(max) }
        , value { default_value } {}

    void set(const value_type& v) { value = v; }

    void set_from_controller(midi::control_change::value_type cc_value) {
      // auto final_physical_value = static_cast<FinalPhysicalValue&>(*this);
      set(midi::control_change::get_value_in(cc_value, min_value, max_value));
    }
  };

  template <typename ValueType>
  class level : public physical_value<ValueType, level<ValueType>> {
   public:
    using value_type = ValueType;
    using physical_value<value_type, level>::physical_value;
  };

  template <typename ValueType>
  class time : public physical_value<ValueType, time<ValueType>> {
   public:
    using value_type = ValueType;
    using physical_value<value_type, time>::physical_value;
  };

  /// \todo refactor with concern separation
  class control_item {
   public:
    enum class type : std::uint8_t { button, knob, slider };

    class cc {
     public:
      std::int8_t value;

      cc(std::int8_t v)
          : value(v) {}
    };

    class cc_inc {
     public:
      std::int8_t value;

      cc_inc(std::int8_t v)
          : value(v) {}
    };

    class note {
     public:
      std::int8_t value = false;

      note(std::int8_t v)
          : value(v) {}
    };

    class pad {
     public:
      std::int8_t value = false;
      std::int8_t red_bo;
      std::int8_t green_bo;
      std::int8_t blue_bo;

      template <typename Enum>
      requires std::is_same_v<std::underlying_type_t<Enum>, std::int8_t>
      pad(std::int8_t v, Enum r, Enum g, Enum b)
          : value(v)
          , red_bo { static_cast<std::int8_t>(r) }
          , green_bo { static_cast<std::int8_t>(g) }
          , blue_bo { static_cast<std::int8_t>(b) } {}
    };

    std::optional<cc> cc_v;
    std::optional<cc_inc> cc_inc_v;
    std::optional<note> note_v;
    std::optional<pad> pad_v;

    midi::control_change::value_type value;

    bool connected = false;

    std::string current_name;

    std::vector<std::function<void(midi::control_change::value_type)>>
        listeners;

    template <typename... Features>
    control_item(void*, type t, Features... features) {
      // Parse the features
      (
          [&](auto&& f) {
            using f_t = std::remove_cvref_t<decltype(f)>;
            if constexpr (std::is_same_v<f_t, cc>) {
              cc_v = f;
              midi_in::cc_action(f.value,
                                 [&](midi::control_change::value_type v) {
                                   value = v;
                                   dispatch();
                                 });
            } else if constexpr (std::is_same_v<f_t, cc_inc>)
              // \todo
              cc_inc_v = f;
            else if constexpr (std::is_same_v<f_t, note>) {
              note_v = f;
              // Listen a note from port 1 and channel 0
              midi_in::add_action(1, midi::on_header { 0, f.value },
                                  [&](const midi::msg& m) {
                                    // Recycle value as a bool for now
                                    value = !value;
                                    dispatch();
                                  });
            } else if constexpr (std::is_same_v<f_t, pad>) {
              pad_v = f;
              // Listen a note from port 0 and channel 10
              midi_in::add_action(0, midi::on_header { 9, f.value },
                                  [&](const midi::msg& m) {
                                    // Recycle value as a bool for now
                                    value = !value;
                                    dispatch();
                                    // button_light(f.red_bo, value*127);
                                    // button_light(f.green_bo, value*127);
                                    // button_light(f.blue_bo, value*127);
                                  });
            }
          }(std::forward<Features>(features)),
          ...);
    }

    void dispatch() {
      std::cout << "Dispatch" << std::endl;
      for (auto& l : listeners)
        l(value);
    }

    /// Name the control_item
    auto& name(const std::string& new_name) {
      current_name = new_name;
      return *this;
    }

    /// Add an action to the control_item
    template <typename Callable> auto& add_action(Callable&& action) {
      using arg0_t =
          std::tuple_element_t<0, boost::callable_traits::args_t<Callable>>;
      // Register an action producing the right value for the action
      if constexpr (std::is_floating_point_v<arg0_t>)
        // If we have a floating point type, scale the value in [0, 1]
        listeners.push_back([action = std::forward<Callable>(action)](
                                midi::control_change::value_type v) {
          action(midi::control_change::get_value_as<arg0_t>(v));
        });
      else
        // Just provides the CC value directly to the action
        listeners.push_back(action);
      return *this;
    }

    /** Associate a variable to an item

        \param[out] variable is the variable to set to the value
        returned by the controller. If the variable has a floating point
        type, the value is scaled to [0, 1] first.
    */
    template <typename T> auto& set_variable(T& variable) {
      add_action([&](T v) { variable = v; });
      return *this;
    }

    /// The value normalized in [ 0, 1 ]
    float value_1() const {
      return midi::control_change::get_value_as<float>(value);
    }

    /// Connect this control to the real parameter
    template <typename ControlItem> auto& connect(ControlItem& ci) {
      add_action([&](int v) { ci.set_from_controller(v); });
      return *this;
    }
  };

  /** A parameter set shared across various owner instance with a
      shared pointer façades

      \param[in] ParamDetail is the real implementation of the
      parameter set

      \param[in] Owner is the class owning the parameter set, just to
      be introspected by sound_generator::from_param through owner_t
  */
  template <typename ParamDetail, typename Owner>
  class param
      : public trisycl::detail::shared_ptr_implementation<
            param<ParamDetail, Owner>, ParamDetail> {
   public:
    /// The real implementation of the parameter set
    using param_detail = ParamDetail;

    /// The class using this parameter set
    using owner_t = Owner;

    /// The type encapsulating the implementation
    using implementation_t =
        trisycl::detail::shared_ptr_implementation<param<param_detail, owner_t>,
                                                   param_detail>;

    /// Import the constructors
    using implementation_t::implementation_t;

    // Make the implementation member directly accessible in this class
    using implementation_t::implementation;

    param()
        : implementation_t { new param_detail } {}

    // Forward everything to the implementation detail
    auto& operator->() const { return implementation; }
  };

  template <typename PhysicalValue> class item {
   public:
    using physical_value_type = PhysicalValue;

    physical_value_type physical_value;

    using value_type = typename physical_value_type::value_type;

    std::string user_name;

    std::optional<std::reference_wrapper<control_item>> c_i;

    item(const std::string& name, const physical_value_type& a_physical_value)
        : physical_value { a_physical_value }
        , user_name { name } {}

    template <typename Group>
    item(Group* g, control_item& ci, const std::string& name,
         const physical_value_type& a_physical_value)
        : physical_value { a_physical_value }
        , user_name { name }
        , c_i { ci } {
      g->assign(ci, [&] { physical_value.set_from_controller(ci.value); });
    }

    value_type& value() { return physical_value.value; }

    operator value_type&() { return value(); }

    void update_display() {
      std::cout << "Control " << user_name << " set to " << value()
                << std::endl;
    }

    void set(const value_type& v) {
      physical_value.set(v);
      update_display();
    }

    void set_from_controller(midi::control_change::value_type v) {
      physical_value.set_from_controller(v);
    }

    value_type& operator=(const value_type& v) {
      set(v);
      return *this;
    }
  };
};

} // namespace musycl
#endif // MUSYCL_CONTROL_HPP
