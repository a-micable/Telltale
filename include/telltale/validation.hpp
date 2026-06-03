#pragma once

#include <cstdint>
#include <string>

#include "telltale/errors.hpp"
#include "telltale/types.hpp"

namespace telltale {

// Explicit input-validation helpers used at CLI boundaries (path, size, schema fields).
inline Result validate_output_path(const std::string& path) {
  if (path.empty()) {
    return Result::fail(ErrorCode::InvalidPayload, "output path must not be empty");
  }
  if (path.find('\0') != std::string::npos) {
    return Result::fail(ErrorCode::InvalidPayload, "output path contains NUL");
  }
  return Result::success();
}

inline Result validate_input_path(const std::string& path) {
  if (path.empty()) {
    return Result::fail(ErrorCode::OpenError, "input path must not be empty");
  }
  if (path.find('\0') != std::string::npos) {
    return Result::fail(ErrorCode::OpenError, "input path contains NUL");
  }
  return Result::success();
}

inline Result validate_payload_size(size_t size) {
  if (size > MAX_PAYLOAD_SIZE) {
    return Result::fail(ErrorCode::PayloadTooLarge, "payload exceeds MAX_PAYLOAD_SIZE");
  }
  return Result::success();
}

inline Result validate_event_type_id(uint16_t type_id) {
  if (type_id == 0) {
    return Result::fail(ErrorCode::InvalidTypeId, "type_id must be non-zero");
  }
  return Result::success();
}

inline Result validate_schema_handler_id(uint16_t handler_id) {
  if (handler_id == 0) {
    return Result::fail(ErrorCode::InvalidHandlerId, "handler_id must be non-zero");
  }
  return Result::success();
}

}  // namespace telltale
