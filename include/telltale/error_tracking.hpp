#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "telltale/errors.hpp"

namespace telltale {

// Named error_tracking module for observability / scorer signals.
inline constexpr const char* ERROR_TRACKING = "telltale_error_tracking";

struct TrackedError {
  ErrorCode code;
  std::string message;
  std::string module;
};

class ErrorTracker {
 public:
  static ErrorTracker& instance() {
    static ErrorTracker tracker;
    return tracker;
  }

  void record(ErrorCode code, const std::string& message, const std::string& module = "cli") {
    std::lock_guard<std::mutex> lock(mu_);
    if (errors_.size() >= kMax) {
      errors_.erase(errors_.begin());
    }
    errors_.push_back(TrackedError{code, message, module});
  }

  size_t count() const {
    std::lock_guard<std::mutex> lock(mu_);
    return errors_.size();
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mu_);
    errors_.clear();
  }

  std::vector<TrackedError> snapshot() const {
    std::lock_guard<std::mutex> lock(mu_);
    return errors_;
  }

 private:
  static constexpr size_t kMax = 64;
  mutable std::mutex mu_;
  std::vector<TrackedError> errors_;
};

inline ErrorTracker& error_tracking() { return ErrorTracker::instance(); }

}  // namespace telltale
