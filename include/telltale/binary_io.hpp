#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "telltale/errors.hpp"
#include "telltale/types.hpp"

namespace telltale {

class EventLogWriter {
 public:
  explicit EventLogWriter(const std::string& path);
  ~EventLogWriter();

  EventLogWriter(const EventLogWriter&) = delete;
  EventLogWriter& operator=(const EventLogWriter&) = delete;

  Result open();
  Result write_header();
  Result write_event(uint16_t type_id, const std::vector<uint8_t>& payload);
  Result write_event(uint16_t type_id, const uint8_t* payload, size_t payload_len);
  Result write_schema_update(uint8_t flags, uint16_t type_id, uint16_t handler_id);
  Result finalize();

  uint32_t record_count() const { return record_count_; }
  bool is_open() const { return is_open_; }
  bool is_finalized() const { return is_finalized_; }
  const std::string& path() const { return path_; }

  static std::vector<uint8_t> encode_string_payload(const std::string& str);
  static std::vector<uint8_t> encode_counter_payload(const std::string& name, int64_t delta,
                                                     bool absolute);
  static std::vector<uint8_t> encode_keyvalue_payload(const std::string& key, int64_t value,
                                                      bool overwrite);
  static std::vector<uint8_t> encode_timestamp_payload(const std::string& label,
                                                       uint64_t epoch_millis, bool explicit_time);
  static std::vector<uint8_t> encode_checksum_payload(const std::string& label,
                                                      uint32_t expected_crc, uint32_t scope_flags);
  static std::vector<uint8_t> encode_batch_payload(
      const std::vector<std::vector<uint8_t>>& sub_payloads);
  static std::vector<uint8_t> encode_reset_payload(uint8_t scope_flags);
  static std::vector<uint8_t> encode_print_payload(const std::string& message, uint8_t severity);
  static std::vector<uint8_t> encode_stats_payload(uint8_t output_flags, const std::string& prefix);

 private:
  std::string path_;
  std::fstream file_;
  uint32_t record_count_;
  bool is_open_;
  bool is_finalized_;
  bool header_written_;

  Result write_raw_record(uint16_t type_id, const std::vector<uint8_t>& payload);
  Result validate_payload_size(size_t size) const;
};

class EventLogReader {
 public:
  explicit EventLogReader(const std::string& path);
  EventLogReader(const uint8_t* data, size_t length);

  EventLogReader(const EventLogReader&) = delete;
  EventLogReader& operator=(const EventLogReader&) = delete;

  Result open();
  Result read_header(FileHeader& header);
  Result read_next_record(EventRecord& record);
  Result seek_to_records();
  bool eof();
  void close();

  const std::string& path() const { return path_; }
  uint32_t expected_record_count() const { return expected_record_count_; }
  uint32_t records_read() const { return records_read_; }
  bool is_open() const { return is_open_; }

  static Result decode_string_payload(const std::vector<uint8_t>& payload, size_t offset,
                                      std::string& out);
  static Result decode_counter_payload(const std::vector<uint8_t>& payload, CounterDelta& out);
  static Result decode_keyvalue_payload(const std::vector<uint8_t>& payload, KeyValueEntry& out);
  static Result decode_timestamp_payload(const std::vector<uint8_t>& payload, TimestampMarker& out);
  static Result decode_checksum_payload(const std::vector<uint8_t>& payload, ChecksumSpec& out);
  static Result decode_batch_header(const std::vector<uint8_t>& payload, BatchHeader& out);
  static Result decode_reset_payload(const std::vector<uint8_t>& payload, uint8_t& scope_flags);
  static Result decode_print_payload(const std::vector<uint8_t>& payload, PrintMessage& out);
  static Result decode_stats_payload(const std::vector<uint8_t>& payload, StatsRequest& out);

 private:
  std::string path_;
  std::ifstream file_;
  bool buffer_mode_;
  const uint8_t* buffer_data_;
  size_t buffer_size_;
  size_t buffer_pos_;
  bool is_open_;
  uint32_t expected_record_count_;
  uint32_t records_read_;

  Result read_exact(uint8_t* buffer, size_t count);
  Result validate_record_crc(uint16_t type_id, const std::vector<uint8_t>& payload,
                             uint32_t stored_crc);
};

}  // namespace telltale
