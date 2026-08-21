#include <cstring>
#include <vector>

#include "telltale/binary_io.hpp"

namespace telltale {

std::vector<uint8_t> EventLogWriter::encode_string_payload(const std::string& str) {
  if (str.size() > MAX_NAME_LENGTH) {
    return {};
  }
  std::vector<uint8_t> result(2 + str.size());
  write_u16_le(result.data(), static_cast<uint16_t>(str.size()));
  if (!str.empty()) {
    std::memcpy(result.data() + 2, str.data(), str.size());
  }
  return result;
}

std::vector<uint8_t> EventLogWriter::encode_counter_payload(const std::string& name, int64_t delta,
                                                            bool absolute) {
  auto name_part = encode_string_payload(name);
  if (name_part.empty() && !name.empty()) {
    return {};
  }
  std::vector<uint8_t> result;
  result.reserve(name_part.size() + 1 + 8);
  result.insert(result.end(), name_part.begin(), name_part.end());
  result.push_back(absolute ? 1 : 0);
  size_t old_size = result.size();
  result.resize(old_size + 8);
  write_i64_le(result.data() + old_size, delta);
  return result;
}

std::vector<uint8_t> EventLogWriter::encode_keyvalue_payload(const std::string& key, int64_t value,
                                                             bool overwrite) {
  auto key_part = encode_string_payload(key);
  if (key_part.empty() && !key.empty()) {
    return {};
  }
  std::vector<uint8_t> result;
  result.reserve(key_part.size() + 1 + 8);
  result.insert(result.end(), key_part.begin(), key_part.end());
  result.push_back(overwrite ? 1 : 0);
  size_t old_size = result.size();
  result.resize(old_size + 8);
  write_i64_le(result.data() + old_size, value);
  return result;
}

std::vector<uint8_t> EventLogWriter::encode_timestamp_payload(const std::string& label,
                                                              uint64_t epoch_millis,
                                                              bool explicit_time) {
  auto label_part = encode_string_payload(label);
  if (label_part.empty() && !label.empty()) {
    return {};
  }
  std::vector<uint8_t> result;
  result.reserve(label_part.size() + 1 + 8);
  result.insert(result.end(), label_part.begin(), label_part.end());
  result.push_back(explicit_time ? 1 : 0);
  size_t old_size = result.size();
  result.resize(old_size + 8);
  write_u64_le(result.data() + old_size, epoch_millis);
  return result;
}

std::vector<uint8_t> EventLogWriter::encode_checksum_payload(const std::string& label,
                                                             uint32_t expected_crc,
                                                             uint32_t scope_flags) {
  auto label_part = encode_string_payload(label);
  if (label_part.empty() && !label.empty()) {
    return {};
  }
  std::vector<uint8_t> result;
  result.reserve(label_part.size() + 8);
  result.insert(result.end(), label_part.begin(), label_part.end());
  size_t old_size = result.size();
  result.resize(old_size + 8);
  write_u32_le(result.data() + old_size, expected_crc);
  write_u32_le(result.data() + old_size + 4, scope_flags);
  return result;
}

std::vector<uint8_t> EventLogWriter::encode_batch_payload(
    const std::vector<std::vector<uint8_t>>& sub_payloads) {
  if (sub_payloads.size() > MAX_BATCH_SUB_EVENTS) {
    return {};
  }
  std::vector<uint8_t> result(4);
  write_u32_le(result.data(), static_cast<uint32_t>(sub_payloads.size()));
  for (const auto& sub : sub_payloads) {
    if (sub.size() > MAX_PAYLOAD_SIZE) {
      return {};
    }
    size_t offset = result.size();
    result.resize(offset + 2 + 4 + sub.size());
    write_u16_le(result.data() + offset, 0);
    write_u32_le(result.data() + offset + 2, static_cast<uint32_t>(sub.size()));
    if (!sub.empty()) {
      std::memcpy(result.data() + offset + 6, sub.data(), sub.size());
    }
  }
  return result;
}

std::vector<uint8_t> EventLogWriter::encode_reset_payload(uint8_t scope_flags) {
  return std::vector<uint8_t>{scope_flags};
}

std::vector<uint8_t> EventLogWriter::encode_print_payload(const std::string& message,
                                                          uint8_t severity) {
  if (message.size() > MAX_PRINT_LENGTH) {
    return {};
  }
  auto msg_part = encode_string_payload(message);
  if (msg_part.empty() && !message.empty()) {
    return {};
  }
  std::vector<uint8_t> result;
  result.reserve(1 + msg_part.size());
  result.push_back(severity);
  result.insert(result.end(), msg_part.begin(), msg_part.end());
  return result;
}

std::vector<uint8_t> EventLogWriter::encode_stats_payload(uint8_t output_flags,
                                                          const std::string& prefix) {
  auto prefix_part = encode_string_payload(prefix);
  if (prefix_part.empty() && !prefix.empty()) {
    return {};
  }
  std::vector<uint8_t> result;
  result.reserve(1 + prefix_part.size());
  result.push_back(output_flags);
  result.insert(result.end(), prefix_part.begin(), prefix_part.end());
  return result;
}

Result EventLogReader::decode_string_payload(const std::vector<uint8_t>& payload, size_t offset,
                                             std::string& out) {
  if (offset + 2 > payload.size()) {
    return Result::fail(ErrorCode::InvalidPayload, "String length prefix truncated");
  }
  uint16_t len = read_u16_le(payload.data() + offset);
  if (offset + 2 + len > payload.size()) {
    return Result::fail(ErrorCode::InvalidPayload, "String data truncated");
  }
  if (len > MAX_NAME_LENGTH) {
    return Result::fail(ErrorCode::NameTooLong, "String exceeds maximum length");
  }
  out.assign(reinterpret_cast<const char*>(payload.data() + offset + 2), len);
  return Result::success();
}

Result EventLogReader::decode_counter_payload(const std::vector<uint8_t>& payload,
                                              CounterDelta& out) {
  std::string name;
  Result r = decode_string_payload(payload, 0, name);
  if (!r.ok()) return r;
  size_t offset = 2 + name.size();
  if (offset + 1 + 8 > payload.size()) {
    return Result::fail(ErrorCode::InvalidPayload, "Counter payload truncated");
  }
  out.name = name;
  out.use_absolute = payload[offset] != 0;
  out.delta = read_i64_le(payload.data() + offset + 1);
  out.absolute = out.delta;
  return Result::success();
}

Result EventLogReader::decode_keyvalue_payload(const std::vector<uint8_t>& payload,
                                               KeyValueEntry& out) {
  std::string key;
  Result r = decode_string_payload(payload, 0, key);
  if (!r.ok()) return r;
  size_t offset = 2 + key.size();
  if (offset + 1 + 8 > payload.size()) {
    return Result::fail(ErrorCode::InvalidPayload, "KeyValue payload truncated");
  }
  out.key = key;
  out.overwrite = payload[offset] != 0;
  out.value = read_i64_le(payload.data() + offset + 1);
  return Result::success();
}

Result EventLogReader::decode_timestamp_payload(const std::vector<uint8_t>& payload,
                                                TimestampMarker& out) {
  std::string label;
  Result r = decode_string_payload(payload, 0, label);
  if (!r.ok()) return r;
  size_t offset = 2 + label.size();
  if (offset + 1 + 8 > payload.size()) {
    return Result::fail(ErrorCode::InvalidPayload, "Timestamp payload truncated");
  }
  out.label = label;
  out.use_explicit_time = payload[offset] != 0;
  out.epoch_millis = read_u64_le(payload.data() + offset + 1);
  return Result::success();
}

Result EventLogReader::decode_checksum_payload(const std::vector<uint8_t>& payload,
                                               ChecksumSpec& out) {
  std::string label;
  Result r = decode_string_payload(payload, 0, label);
  if (!r.ok()) return r;
  size_t offset = 2 + label.size();
  if (offset + 8 > payload.size()) {
    return Result::fail(ErrorCode::InvalidPayload, "Checksum payload truncated");
  }
  out.label = label;
  out.expected_crc = read_u32_le(payload.data() + offset);
  out.scope_flags = read_u32_le(payload.data() + offset + 4);
  return Result::success();
}

Result EventLogReader::decode_batch_header(const std::vector<uint8_t>& payload, BatchHeader& out) {
  if (payload.size() < 4) {
    return Result::fail(ErrorCode::InvalidPayload, "Batch header too short");
  }
  out.sub_event_count = read_u32_le(payload.data());
  if (out.sub_event_count > MAX_BATCH_SUB_EVENTS) {
    return Result::fail(ErrorCode::BatchTooLarge, "Batch sub-event count too large");
  }
  return Result::success();
}

Result EventLogReader::decode_reset_payload(const std::vector<uint8_t>& payload,
                                            uint8_t& scope_flags) {
  if (payload.empty()) {
    return Result::fail(ErrorCode::InvalidPayload, "Reset payload empty");
  }
  scope_flags = payload[0];
  return Result::success();
}

Result EventLogReader::decode_print_payload(const std::vector<uint8_t>& payload,
                                            PrintMessage& out) {
  if (payload.empty()) {
    return Result::fail(ErrorCode::InvalidPayload, "Print payload empty");
  }
  out.severity = payload[0];
  Result r = decode_string_payload(payload, 1, out.message);
  return r;
}

Result EventLogReader::decode_stats_payload(const std::vector<uint8_t>& payload,
                                            StatsRequest& out) {
  if (payload.empty()) {
    return Result::fail(ErrorCode::InvalidPayload, "Stats payload empty");
  }
  out.output_flags = payload[0];
  if (payload.size() > 1) {
    Result r = decode_string_payload(payload, 1, out.prefix);
    return r;
  }
  out.prefix.clear();
  return Result::success();
}

}  // namespace telltale
