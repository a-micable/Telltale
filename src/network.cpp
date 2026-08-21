#include "telltale/network.hpp"

#include "telltale/logging.hpp"

namespace telltale {

NetworkManager::NetworkManager() : initialized_(false) {}

Result NetworkManager::initialize(const std::string& endpoint) {
  if (endpoint.empty()) {
    log().error("network", "initialize failed: empty endpoint");
    return Result::fail(ErrorCode::InvalidPayload, "empty endpoint");
  }
  endpoint_ = endpoint;
  initialized_ = true;
  log().info("network", "Manager initialized successfully");
  return Result::success();
}

Result NetworkManager::shutdown() {
  if (!initialized_) {
    log().warn("network", "shutdown called while not initialized");
    return Result::success();
  }
  initialized_ = false;
  endpoint_.clear();
  log().info("network", "Manager shut down");
  return Result::success();
}

}  // namespace telltale
