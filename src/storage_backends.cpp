#include "telltale/storage_backends.hpp"

#include "telltale/logging.hpp"

namespace telltale {

StorageBackendsManager::StorageBackendsManager() : initialized_(false) {}

Result StorageBackendsManager::initialize(const std::string& root_path) {
  if (root_path.empty()) {
    log().error("storage_backends", "initialize failed: empty root path");
    return Result::fail(ErrorCode::InvalidPayload, "empty root path");
  }
  root_path_ = root_path;
  initialized_ = true;
  log().info("storage_backends", "Manager initialized successfully");
  return Result::success();
}

Result StorageBackendsManager::shutdown() {
  if (!initialized_) {
    log().warn("storage_backends", "shutdown called while not initialized");
    return Result::success();
  }
  initialized_ = false;
  root_path_.clear();
  log().info("storage_backends", "Manager shut down");
  return Result::success();
}

}  // namespace telltale
