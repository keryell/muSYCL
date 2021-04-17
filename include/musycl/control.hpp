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
  class level {

   public:
    level(auto min, auto max) {}
  };

  class time {

   public:
    time(auto min, auto max) {}
  };

  class group {

    std::string name;

    //  controls
   public:
    group(const std::string& n)
        : name { n } {}
  };

  template <typename ParamDetail>
  class param
      : public trisycl::detail::shared_ptr_implementation<param<ParamDetail>,
                                                          ParamDetail> {
   public:
    using param_detail = ParamDetail;

    /// The type encapsulating the implementation
    using implementation_t =
        trisycl::detail::shared_ptr_implementation<param<param_detail>,
                                                   param_detail>;

    /// Import the constructors
    using implementation_t::implementation_t;

    // Make the implementation member directly accessible in this class
    using implementation_t::implementation;

    param()
        : implementation_t { new param_detail } {}

    // Forward everything to the implementation detail
    auto& operator->() { return implementation; }
  };

  template <typename T> class item {
   public:
    using value_type = T;

    value_type value;

    std::string user_name;

    template <typename ControlType>
    item(void*, const T& default_value, const std::string& name,
         const ControlType& ct)
        : value { default_value }
        , user_name { name } {}

    template <typename ControlType>
    item(const T& default_value)
        : value { default_value } {}

    operator value_type&() { return value; }

    void update_display() {
      std::cout << "Control " << user_name << " set to " << value << std::endl;
    }

    void set(const value_type& v) {
      value = v;
      update_display();
    }

    value_type& operator=(const value_type& v) {
      set(v);
      return *this;
    }
  };
};

} // namespace musycl
#endif // MUSYCL_CONTROL_HPP
