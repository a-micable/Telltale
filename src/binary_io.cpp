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

}  // namespace telltale
