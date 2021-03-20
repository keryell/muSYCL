#ifndef MUSYCL_CONTROL_HPP
#define MUSYCL_CONTROL_HPP

/** \file Abstractions for control

*/

#include <cmath>
#include <cstdint>
#include <string>
#include <variant>

namespace musycl {

class control {
public:

  template <typename T>
  class item {
  public:

    using value_type = T;

    value_type value;

    template <typename ControlType>
    item(void *, const T& default_value, const std::string& name,
         const ControlType& ct) : value { default_value } {}

    template <typename ControlType>
    item(const T& default_value) : value { default_value } {}

    operator value_type&() { return value; }

    void set(const value_type& v) {
      value = v;
    }

    value_type& operator=(const value_type& v) {
      value = v;
      return value;
    }

  };

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

    group(const std::string& n) : name { n } {}
  };


  class param {
  public:

    void update_values(const param& p) {}

  };

};

}
#endif // MUSYCL_CONTROL_HPP
