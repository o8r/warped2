// -*- C++ -*-
/** Serialization support for Configuration.
 * @author O'HARA Mamoru
 * @date 2016 Mar 12
 */
#ifndef WARPED_SERIALIZATION_CONFIGURATION_HPP_
#define WARPED_SERIALIZATION_CONFIGURATION_HPP_

#include "Configuration.hpp"
#include "serialization.hpp"
#include "utility/memory.hpp"
#include "json/json.h"

namespace warped {

  template <typename Archive>
  void save(Archive& ar, Configuration const& config) {
      ar(config.config_file_name_, config.max_sim_time_);

      std::ostringstream oss;
      oss << *config.root_;
      ar(oss.str());      
    }
    template <typename Archive>
    void load(Archive& ar, Configuration& config) {
      ar(config.config_file_name_, config.max_sim_time_);
      
      std::string json;
      ar(json);

      auto root = make_unique<Json::Value>(json);
      config.root_ = std::move(root);
    }

}  // namespace warped

namespace cereal {

  template<>
  struct LoadAndConstruct<warped::Configuration>
  {
    template <typename Archive>
    static void load_and_construct(Archive& ar, cereal::construct<warped::Configuration>& construct) {
      std::string config_file_name;
      unsigned int max_sim_time;
      ar(config_file_name);
      ar(max_sim_time);

      construct(config_file_name, max_sim_time);

      std::string json;
      ar(json);
      auto root = warped::make_unique<Json::Value>(json);
      construct->root_ = std::move(root);
    }
  };

}  // namespace cereal

#endif
