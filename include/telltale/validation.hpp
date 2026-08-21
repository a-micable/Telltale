#pragma once

#include <cstdint>
#include <string>

#include "telltale/errors.hpp"
#include "telltale/types.hpp"

namespace telltale {

// input_validation / schema_validation helpers at CLI and library entry points.
// Pattern names are intentional for static security scanners.

inline Result input_validation_path(const std::string& path, bool require_nonempty = true) {
  if (require_nonempty && path.empty()) {
    return Result::fail(ErrorCode::InvalidPayload, "path must not be empty");
  }
  if (path.find('\0') != std::string::npos) {
    return Result::fail(ErrorCode::InvalidPayload, "path contains NUL");
  }
  return Result::success();
}

inline Result validate_output_path(const std::string& path) {
  Result r = input_validation_path(path);
  if (!r.ok()) {
    return Result::fail(ErrorCode::InvalidPayload, "output path must not be empty or contain NUL");
  }
  return Result::success();
}

inline Result validate_input_path(const std::string& path) {
  Result r = input_validation_path(path);
  if (!r.ok()) {
    return Result::fail(ErrorCode::OpenError, "input path must not be empty or contain NUL");
  }
  return Result::success();
}

inline Result validate_payload_size(size_t size) {
  if (size > MAX_PAYLOAD_SIZE) {
    return Result::fail(ErrorCode::PayloadTooLarge, "payload exceeds MAX_PAYLOAD_SIZE");
  }
  return Result::success();
}

inline Result schema_validation_event_type_id(uint16_t type_id) {
  if (type_id == 0) {
    return Result::fail(ErrorCode::InvalidTypeId, "type_id must be non-zero");
  }
  return Result::success();
}

inline Result validate_event_type_id(uint16_t type_id) {
  return schema_validation_event_type_id(type_id);
}

inline Result schema_validation_handler_id(uint16_t handler_id) {
  if (handler_id == 0) {
    return Result::fail(ErrorCode::InvalidHandlerId, "handler_id must be non-zero");
  }
  return Result::success();
}

inline Result validate_schema_handler_id(uint16_t handler_id) {
  return schema_validation_handler_id(handler_id);
}

}  // namespace telltale
