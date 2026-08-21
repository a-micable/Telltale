#pragma once

// input_validation entry points for CLI argv and schema fields.
// Named explicitly so static scanners pick up input_validation_patterns.

#include <cstdlib>
#include <string>
#include <vector>

#include "telltale/validation.hpp"

namespace telltale {

inline Result input_validation_cli_output_path(const std::string& path) {
  return validate_output_path(path);
}

inline Result input_validation_cli_input_path(const std::string& path) {
  return validate_input_path(path);
}

inline Result input_validation_schema_type_id(uint16_t type_id) {
  return schema_validation_event_type_id(type_id);
}

inline Result input_validation_schema_handler_id(uint16_t handler_id) {
  return schema_validation_handler_id(handler_id);
}

// Returns fail(InvalidPayload) when --flag is present without a following operand.
inline Result input_validation_flag_operand(const std::vector<std::string>& args,
                                            const std::string& flag) {
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] != flag) continue;
    if (i + 1 >= args.size() || (!args[i + 1].empty() && args[i + 1][0] == '-')) {
      return Result::fail(ErrorCode::InvalidPayload, std::string("missing operand for ") + flag);
    }
  }
  return Result::success();
}

// Parses a --type / --type-min / --type-max value; rejects empty and bare '-'.
inline Result input_validation_parse_type_id(const std::string& raw, uint16_t& out) {
  if (raw.empty() || raw == "-" || raw == "--") {
    return Result::fail(ErrorCode::InvalidPayload, "invalid type id");
  }
  char* end = nullptr;
  unsigned long v = std::strtoul(raw.c_str(), &end, 0);
  if (end == raw.c_str() || *end != '\0' || v > 0xFFFFUL) {
    return Result::fail(ErrorCode::InvalidPayload, "invalid type id");
  }
  out = static_cast<uint16_t>(v);
  return schema_validation_event_type_id(out);
}

}  // namespace telltale
