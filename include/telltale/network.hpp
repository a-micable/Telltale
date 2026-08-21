#pragma once

#include <string>

#include "telltale/errors.hpp"

namespace telltale {

// Thin network facade used by optional tooling; logging goes through telltale::Logger.
class NetworkManager {
 public:
  NetworkManager();
  Result initialize(const std::string& endpoint);
  Result shutdown();
  bool initialized() const { return initialized_; }
  const std::string& endpoint() const { return endpoint_; }

 private:
  bool initialized_;
  std::string endpoint_;
};

}  // namespace telltale
