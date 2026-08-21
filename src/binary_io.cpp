#include "telltale/binary_io.hpp"

#include <algorithm>
#include <cstring>

#include "telltale/crc32.hpp"
#include "telltale/schema_update.hpp"

namespace telltale {

EventLogWriter::EventLogWriter(const std::string& path)
    : path_(path),
      record_count_(0),
      is_open_(false),
      is_finalized_(false),
      header_written_(false) {}

EventLogWriter::~EventLogWriter() {
  if (is_open_ && !is_finalized_) {
    finalize();
  }
}

Result EventLogWriter::open() {
  if (is_open_) {
    return Result::fail(ErrorCode::StateError, "Writer already open");
  }
  file_.open(path_, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
  if (!file_.is_open()) {
    file_.clear();
    file_.open(path_, std::ios::binary | std::ios::out | std::ios::trunc);
  }
  if (!file_.is_open()) {
    return Result::fail(ErrorCode::OpenError, "Cannot open file for writing: " + path_);
  }
  is_open_ = true;
  return Result::success();
}

Result EventLogWriter::write_header() {
  if (!is_open_) {
    return Result::fail(ErrorCode::StateError, "Writer not open");
  }
  if (header_written_) {
    return Result::fail(ErrorCode::StateError, "Header already written");
  }
  FileHeader header;
  std::memcpy(header.magic, MAGIC, 4);
  header.version = FORMAT_VERSION;
  header.record_count = 0;
  header.header_crc = Crc32::of_header_fields(header.version, header.record_count);

  file_.write(header.magic, 4);
  if (!file_) {
    return Result::fail(ErrorCode::IoError, "Failed to write magic bytes");
  }
  uint8_t version_buf[2];
  write_u16_le(version_buf, header.version);
  file_.write(reinterpret_cast<char*>(version_buf), 2);
  if (!file_) {
    return Result::fail(ErrorCode::IoError, "Failed to write version");
  }
  uint8_t count_buf[4];
  write_u32_le(count_buf, header.record_count);
  file_.write(reinterpret_cast<char*>(count_buf), 4);
  if (!file_) {
    return Result::fail(ErrorCode::IoError, "Failed to write record count placeholder");
  }
  uint8_t crc_buf[4];
  write_u32_le(crc_buf, header.header_crc);
  file_.write(reinterpret_cast<char*>(crc_buf), 4);
  if (!file_) {
    return Result::fail(ErrorCode::IoError, "Failed to write header CRC");
  }
  header_written_ = true;
  return Result::success();
}

Result EventLogWriter::validate_payload_size(size_t size) const {
  if (size > MAX_PAYLOAD_SIZE) {
    return Result::fail(ErrorCode::PayloadTooLarge, "Payload size " + std::to_string(size) +
                                                        " exceeds maximum " +
                                                        std::to_string(MAX_PAYLOAD_SIZE));
  }
  return Result::success();
}

Result EventLogWriter::write_raw_record(uint16_t type_id, const std::vector<uint8_t>& payload) {
  if (!is_open_) {
    return Result::fail(ErrorCode::StateError, "Writer not open");
  }
  if (is_finalized_) {
    return Result::fail(ErrorCode::StateError, "Writer already finalized");
  }
  if (!header_written_) {
    Result hr = write_header();
    if (!hr.ok()) {
      return hr;
    }
  }
  Result vr = validate_payload_size(payload.size());
  if (!vr.ok()) {
    return vr;
  }

  uint8_t header[6];
  write_u16_le(header, type_id);
  write_u32_le(header + 2, static_cast<uint32_t>(payload.size()));

  file_.write(reinterpret_cast<char*>(header), 6);
  if (!file_) {
    return Result::fail(ErrorCode::IoError, "Failed to write record header");
  }
  if (!payload.empty()) {
    file_.write(reinterpret_cast<const char*>(payload.data()),
                static_cast<std::streamsize>(payload.size()));
    if (!file_) {
      return Result::fail(ErrorCode::IoError, "Failed to write payload");
    }
  }

  uint32_t crc = Crc32::of_record(type_id, payload);
  uint8_t crc_buf[4];
  write_u32_le(crc_buf, crc);
  file_.write(reinterpret_cast<char*>(crc_buf), 4);
  if (!file_) {
    return Result::fail(ErrorCode::IoError, "Failed to write record CRC");
  }

  ++record_count_;
  return Result::success();
}

Result EventLogWriter::write_event(uint16_t type_id, const std::vector<uint8_t>& payload) {
  return write_raw_record(type_id, payload);
}

Result EventLogWriter::write_event(uint16_t type_id, const uint8_t* payload, size_t payload_len) {
  std::vector<uint8_t> vec(payload, payload + payload_len);
  return write_raw_record(type_id, vec);
}

Result EventLogWriter::write_schema_update(uint8_t flags, uint16_t type_id, uint16_t handler_id) {
  bool include_handler = SchemaUpdateCodec::requires_handler_id(flags);
  std::vector<uint8_t> payload =
      SchemaUpdateCodec::encode(flags, type_id, handler_id, include_handler);
  return write_raw_record(SCHEMA_UPDATE_TYPE, payload);
}

Result EventLogWriter::finalize() {
  if (!is_open_) {
    return Result::fail(ErrorCode::StateError, "Writer not open");
  }
  if (is_finalized_) {
    return Result::success();
  }
  if (!header_written_) {
    Result hr = write_header();
    if (!hr.ok()) {
      return hr;
    }
  }

  file_.seekp(6, std::ios::beg);
  if (!file_) {
    return Result::fail(ErrorCode::SeekError, "Failed to seek to record count field");
  }
  uint8_t count_buf[4];
  write_u32_le(count_buf, record_count_);
  file_.write(reinterpret_cast<char*>(count_buf), 4);
  if (!file_) {
    return Result::fail(ErrorCode::IoError, "Failed to write final record count");
  }

  uint32_t header_crc = Crc32::of_header_fields(FORMAT_VERSION, record_count_);
  file_.seekp(10, std::ios::beg);
  if (!file_) {
    return Result::fail(ErrorCode::SeekError, "Failed to seek to header CRC field");
  }
  uint8_t crc_buf[4];
  write_u32_le(crc_buf, header_crc);
  file_.write(reinterpret_cast<char*>(crc_buf), 4);
  if (!file_) {
    return Result::fail(ErrorCode::IoError, "Failed to write final header CRC");
  }

  file_.flush();
  is_finalized_ = true;
  return Result::success();
}

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

EventLogReader::EventLogReader(const std::string& path)
    : path_(path),
      buffer_mode_(false),
      buffer_data_(nullptr),
      buffer_size_(0),
      buffer_pos_(0),
      is_open_(false),
      expected_record_count_(0),
      records_read_(0) {}

EventLogReader::EventLogReader(const uint8_t* data, size_t length)
    : path_(),
      buffer_mode_(true),
      buffer_data_(data),
      buffer_size_(length),
      buffer_pos_(0),
      is_open_(false),
      expected_record_count_(0),
      records_read_(0) {}

Result EventLogReader::open() {
  if (is_open_) {
    return Result::fail(ErrorCode::StateError, "Reader already open");
  }
  if (buffer_mode_) {
    if (buffer_size_ == 0) {
      return Result::fail(ErrorCode::EmptyFile, "Buffer is empty");
    }
    buffer_pos_ = 0;
    is_open_ = true;
    return Result::success();
  }
  file_.open(path_, std::ios::binary);
  if (!file_.is_open()) {
    return Result::fail(ErrorCode::OpenError, "Cannot open file for reading: " + path_);
  }
  is_open_ = true;
  return Result::success();
}

Result EventLogReader::read_exact(uint8_t* buffer, size_t count) {
  if (buffer_mode_) {
    if (buffer_pos_ + count > buffer_size_) {
      return Result::fail(ErrorCode::UnexpectedEof, "Unexpected end of buffer");
    }
    if (count > 0) {
      std::memcpy(buffer, buffer_data_ + buffer_pos_, count);
      buffer_pos_ += count;
    }
    return Result::success();
  }
  file_.read(reinterpret_cast<char*>(buffer), static_cast<std::streamsize>(count));
  if (file_.gcount() != static_cast<std::streamsize>(count)) {
    if (file_.eof()) {
      return Result::fail(ErrorCode::UnexpectedEof, "Unexpected end of file");
    }
    return Result::fail(ErrorCode::IoError, "Failed to read expected bytes");
  }
  return Result::success();
}

Result EventLogReader::read_header(FileHeader& header) {
  if (!is_open_) {
    return Result::fail(ErrorCode::StateError, "Reader not open");
  }
  Result r = read_exact(reinterpret_cast<uint8_t*>(header.magic), 4);
  if (!r.ok()) {
    if (!buffer_mode_ && file_.eof() && file_.gcount() == 0) {
      return Result::fail(ErrorCode::EmptyFile, "File is empty");
    }
    if (buffer_mode_ && buffer_pos_ == 0) {
      return Result::fail(ErrorCode::EmptyFile, "Buffer is empty");
    }
    return r;
  }
  if (std::memcmp(header.magic, MAGIC, 4) != 0) {
    return Result::fail(ErrorCode::InvalidMagic, "Invalid magic bytes in header");
  }
  uint8_t version_buf[2];
  r = read_exact(version_buf, 2);
  if (!r.ok()) return r;
  header.version = read_u16_le(version_buf);
  if (header.version != FORMAT_VERSION) {
    return Result::fail(ErrorCode::UnsupportedVersion,
                        "Unsupported format version: " + std::to_string(header.version));
  }
  uint8_t count_buf[4];
  r = read_exact(count_buf, 4);
  if (!r.ok()) return r;
  header.record_count = read_u32_le(count_buf);
  uint8_t crc_buf[4];
  r = read_exact(crc_buf, 4);
  if (!r.ok()) return r;
  header.header_crc = read_u32_le(crc_buf);

  uint32_t computed = Crc32::of_header_fields(header.version, header.record_count);
  if (computed != header.header_crc) {
    return Result::fail(ErrorCode::HeaderCrcMismatch, "Header CRC mismatch: expected " +
                                                          std::to_string(computed) + ", got " +
                                                          std::to_string(header.header_crc));
  }
  expected_record_count_ = header.record_count;
  return Result::success();
}

Result EventLogReader::seek_to_records() {
  if (!is_open_) {
    return Result::fail(ErrorCode::StateError, "Reader not open");
  }
  if (buffer_mode_) {
    buffer_pos_ = HEADER_SIZE;
    records_read_ = 0;
    return Result::success();
  }
  file_.seekg(static_cast<std::streamoff>(HEADER_SIZE), std::ios::beg);
  if (!file_) {
    return Result::fail(ErrorCode::SeekError, "Failed to seek to records");
  }
  records_read_ = 0;
  return Result::success();
}

Result EventLogReader::validate_record_crc(uint16_t type_id, const std::vector<uint8_t>& payload,
                                           uint32_t stored_crc) {
  uint32_t computed = Crc32::of_record(type_id, payload);
  if (computed != stored_crc) {
    return Result::fail(ErrorCode::RecordCrcMismatch,
                        "Record CRC mismatch for type " + std::to_string(type_id) + ": expected " +
                            std::to_string(computed) + ", got " + std::to_string(stored_crc));
  }
  return Result::success();
}

Result EventLogReader::read_next_record(EventRecord& record) {
  if (!is_open_) {
    return Result::fail(ErrorCode::StateError, "Reader not open");
  }
  if (buffer_mode_) {
    if (buffer_pos_ >= buffer_size_) {
      return Result::fail(ErrorCode::UnexpectedEof, "No more records");
    }
  } else if (file_.peek() == std::char_traits<char>::eof()) {
    return Result::fail(ErrorCode::UnexpectedEof, "No more records");
  }

  uint8_t header[6];
  Result r = read_exact(header, 6);
  if (!r.ok()) {
    return Result::fail(ErrorCode::TruncatedRecord, "Truncated record header");
  }

  record.type_id = read_u16_le(header);
  uint32_t payload_len = read_u32_le(header + 2);
  if (payload_len > MAX_PAYLOAD_SIZE) {
    return Result::fail(ErrorCode::PayloadTooLarge,
                        "Record payload too large: " + std::to_string(payload_len));
  }

  record.payload.resize(payload_len);
  if (payload_len > 0) {
    r = read_exact(record.payload.data(), payload_len);
    if (!r.ok()) {
      return Result::fail(ErrorCode::TruncatedRecord, "Truncated record payload");
    }
  }

  uint8_t crc_buf[4];
  r = read_exact(crc_buf, 4);
  if (!r.ok()) {
    return Result::fail(ErrorCode::TruncatedRecord, "Truncated record CRC");
  }
  record.crc32 = read_u32_le(crc_buf);

  r = validate_record_crc(record.type_id, record.payload, record.crc32);
  if (!r.ok()) {
    return r;
  }

  ++records_read_;
  return Result::success();
}

bool EventLogReader::eof() {
  if (!is_open_) return true;
  if (buffer_mode_) {
    return buffer_pos_ >= buffer_size_;
  }
  return file_.peek() == std::char_traits<char>::eof();
}

void EventLogReader::close() {
  if (is_open_) {
    if (!buffer_mode_) {
      file_.close();
    }
    is_open_ = false;
  }
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

// Optimization pass 40 - memory iteration 1

// Optimization pass 30 - caching iteration 1

// Optimization pass 25 - indexing iteration 1
