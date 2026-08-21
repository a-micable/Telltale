#include "telltale/schema_update.hpp"

#include <sstream>

namespace telltale {

std::vector<uint8_t> SchemaUpdateCodec::encode(uint8_t flags, uint16_t type_id, uint16_t handler_id,
                                               bool include_handler_id) {
  std::vector<uint8_t> payload;
  payload.push_back(flags);
  payload.resize(3);
  write_u16_le(payload.data() + 1, type_id);
  if (include_handler_id) {
    size_t old = payload.size();
    payload.resize(old + 2);
    write_u16_le(payload.data() + old, handler_id);
  }
  return payload;
}

bool SchemaUpdateCodec::requires_handler_id(uint8_t flags) {
  return (flags & static_cast<uint8_t>(SchemaUpdateFlag::Register)) != 0 ||
         (flags & static_cast<uint8_t>(SchemaUpdateFlag::Replace)) != 0;
}

bool SchemaUpdateCodec::requires_table_size(uint8_t flags) {
  return (flags & static_cast<uint8_t>(SchemaUpdateFlag::ResizeTable)) != 0;
}

bool SchemaUpdateCodec::is_valid_flags(uint8_t flags) {
  if (flags == 0) return false;
  uint8_t valid_mask = static_cast<uint8_t>(SchemaUpdateFlag::Register) |
                       static_cast<uint8_t>(SchemaUpdateFlag::Deregister) |
                       static_cast<uint8_t>(SchemaUpdateFlag::Replace) |
                       static_cast<uint8_t>(SchemaUpdateFlag::ResizeTable);
  if ((flags & ~valid_mask) != 0) return false;
  int op_count = 0;
  if (flags & static_cast<uint8_t>(SchemaUpdateFlag::Register)) ++op_count;
  if (flags & static_cast<uint8_t>(SchemaUpdateFlag::Deregister)) ++op_count;
  if (flags & static_cast<uint8_t>(SchemaUpdateFlag::Replace)) ++op_count;
  if (flags & static_cast<uint8_t>(SchemaUpdateFlag::ResizeTable)) ++op_count;
  return op_count == 1;
}

size_t SchemaUpdateCodec::expected_payload_size(uint8_t flags) {
  if (requires_table_size(flags)) {
    return RESIZE_PAYLOAD_SIZE;
  }
  if (requires_handler_id(flags)) {
    return FULL_PAYLOAD_SIZE;
  }
  return MIN_PAYLOAD_SIZE;
}

Result SchemaUpdateCodec::validate_payload(const std::vector<uint8_t>& payload) {
  if (payload.size() < MIN_PAYLOAD_SIZE) {
    return Result::fail(ErrorCode::InvalidSchemaUpdate, "Schema update payload too short");
  }
  uint8_t flags = payload[0];
  if (!is_valid_flags(flags)) {
    return Result::fail(ErrorCode::InvalidFlags, "Invalid schema update flags");
  }
  size_t expected = expected_payload_size(flags);
  if (payload.size() < expected) {
    return Result::fail(ErrorCode::InvalidSchemaUpdate,
                        "Schema update payload truncated for flags " + flags_to_string(flags));
  }
  return Result::success();
}

Result SchemaUpdateCodec::decode(const std::vector<uint8_t>& payload, SchemaUpdatePayload& out) {
  return decode(payload.data(), payload.size(), out);
}

Result SchemaUpdateCodec::decode(const uint8_t* payload, size_t payload_len,
                                 SchemaUpdatePayload& out) {
  if (payload_len < MIN_PAYLOAD_SIZE) {
    return Result::fail(ErrorCode::InvalidSchemaUpdate, "Schema update payload too short");
  }
  out.flags = payload[0];
  if (!is_valid_flags(out.flags)) {
    return Result::fail(ErrorCode::InvalidFlags, "Invalid schema update flags");
  }
  out.type_id = read_u16_le(payload + 1);
  out.has_handler_id = requires_handler_id(out.flags);
  out.has_table_size = requires_table_size(out.flags);
  out.table_size = 0;
  if (out.has_handler_id) {
    if (payload_len < FULL_PAYLOAD_SIZE) {
      return Result::fail(ErrorCode::InvalidSchemaUpdate, "Schema update missing handler ID");
    }
    out.handler_id = read_u16_le(payload + 3);
  } else if (out.has_table_size) {
    if (payload_len < RESIZE_PAYLOAD_SIZE) {
      return Result::fail(ErrorCode::InvalidSchemaUpdate, "Schema update missing table size");
    }
    out.handler_id = 0;
    out.table_size = read_u32_le(payload + 3);
    if (out.table_size > MAX_HANDLER_TABLE_SIZE) {
      return Result::fail(ErrorCode::InvalidSchemaUpdate, "Handler table size exceeds maximum");
    }
  } else {
    out.handler_id = 0;
  }
  if (out.type_id == SCHEMA_UPDATE_TYPE) {
    return Result::fail(ErrorCode::InvalidTypeId,
                        "Cannot apply schema update to schema update type");
  }
  return Result::success();
}

std::string SchemaUpdateCodec::flags_to_string(uint8_t flags) {
  std::ostringstream oss;
  bool first = true;
  if (flags & static_cast<uint8_t>(SchemaUpdateFlag::Register)) {
    oss << "Register";
    first = false;
  }
  if (flags & static_cast<uint8_t>(SchemaUpdateFlag::Deregister)) {
    if (!first) oss << "|";
    oss << "Deregister";
    first = false;
  }
  if (flags & static_cast<uint8_t>(SchemaUpdateFlag::Replace)) {
    if (!first) oss << "|";
    oss << "Replace";
    first = false;
  }
  if (flags & static_cast<uint8_t>(SchemaUpdateFlag::ResizeTable)) {
    if (!first) oss << "|";
    oss << "ResizeTable";
  }
  if (first) {
    oss << "None";
  }
  return oss.str();
}

}  // namespace telltale

// Optimization pass 40 - memory iteration 1

// Optimization pass 30 - caching iteration 1

// Optimization pass 25 - indexing iteration 1
