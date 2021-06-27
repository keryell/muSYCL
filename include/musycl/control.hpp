#ifndef MUSYCL_CONTROL_HPP
#define MUSYCL_CONTROL_HPP

/** \file Abstractions for control

*/

#include <cmath>
#include <cstdint>
#include <string>
#include <variant>

#include "triSYCL/detail/shared_ptr_implementation.hpp"

namespace musycl {

class control {
 public:
  template <typename ValueType> class level {
   public:
    using value_type = ValueType;

    const value_type min_value;

    const value_type max_value;

    level(auto min, auto max)
        : min_value { static_cast<value_type>(min) }
        , max_value { static_cast<value_type>(max) } {}
  };

  template <typename ValueType> class time {
   public:
    using value_type = ValueType;

    const value_type min_value;

    const value_type max_value;

    time(auto min, auto max)
        : min_value { static_cast<value_type>(min) }
        , max_value { static_cast<value_type>(max) } {}
  };

  class group {

    std::string name;

    //  controls
   public:
    group(const std::string& n)
        : name { n } {}
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

  template <typename ControlType> class item {
   public:
    using value_type = typename ControlType::value_type;

    value_type value;

    std::string user_name;

    ControlType control_type;

    item(const value_type& default_value, const std::string& name,
         const ControlType& ct)
        : value { default_value }
        , user_name { name }
        , control_type { ct } {}

    operator value_type&() { return value; }

    void update_display() {
      std::cout << "Control " << user_name << " set to " << value << std::endl;
    }

    void set(const value_type& v) {
      value = v;
      update_display();
    }

    void set_127(int v) {
      set(midi::control_change::get_value_in(v, control_type.min_value,
                                             control_type.max_value));
    }

    value_type& operator=(const value_type& v) {
      set(v);
      return *this;
    }
  };
};

} // namespace musycl
#endif // MUSYCL_CONTROL_HPP
