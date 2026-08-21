#pragma once

#include <string>

#include "telltale/errors.hpp"

namespace telltale {

// Thin storage-backends facade; logging goes through telltale::Logger.
class StorageBackendsManager {
 public:
  StorageBackendsManager();
  Result initialize(const std::string& root_path);
  Result shutdown();
  bool initialized() const { return initialized_; }
  const std::string& root_path() const { return root_path_; }

 private:
  bool initialized_;
  std::string root_path_;
};

}  // namespace telltale
