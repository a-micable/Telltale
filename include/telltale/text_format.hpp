#pragma once

#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "telltale/binary_io.hpp"
#include "telltale/errors.hpp"
#include "telltale/types.hpp"

namespace telltale {

constexpr char TEXT_FORMAT_MAGIC_LINE[] = "# Telltale Text Format v1";
constexpr char TEXT_RECORD_BEGIN[] = "BEGIN_RECORD";
constexpr char TEXT_RECORD_END[] = "END_RECORD";

struct TextRecord {
  uint32_t index;
  uint16_t type_id;
  uint32_t crc32;
  std::map<std::string, std::string> fields;
  std::vector<uint8_t> raw_payload;
  bool has_raw_payload;
};

class TextExporter {
 public:
  TextExporter();
  ~TextExporter();

  Result export_file(const std::string& input_path, const std::string& output_path);
  Result export_to_string(const std::string& input_path, std::string& output);
  Result export_record(const EventRecord& record, uint32_t index, std::ostream& out);

  void set_include_raw_hex(bool v) { include_raw_hex_ = v; }
  void set_verbose(bool v) { verbose_ = v; }

  uint32_t records_exported() const { return records_exported_; }

  static std::string escape_string(const std::string& s);
  static std::string hex_encode(const std::vector<uint8_t>& data);

 private:
  bool include_raw_hex_;
  bool verbose_;
  uint32_t records_exported_;

  Result write_record_fields(uint16_t type_id, const std::vector<uint8_t>& payload,
                             std::ostream& out);
};

class TextImporter {
 public:
  TextImporter();
  ~TextImporter();

  Result import_file(const std::string& input_path, const std::string& output_path);
  Result parse(const std::string& text_content);
  Result write_binary(const std::string& output_path);

  const std::vector<TextRecord>& records() const { return records_; }
  uint32_t parse_errors() const { return parse_errors_; }

  void set_strict(bool v) { strict_ = v; }

 private:
  std::vector<TextRecord> records_;
  uint32_t parse_errors_;
  bool strict_;
  std::string source_text_;

  Result parse_line(const std::string& line, TextRecord* current, bool& in_record);
  Result finalize_record(TextRecord& rec);
  static std::string unescape_string(const std::string& s);
  static Result build_payload(const TextRecord& rec, std::vector<uint8_t>& payload);
  static Result parse_key_value(const std::string& line, std::string& key, std::string& value);
};

Result text_format_full_verify(const std::string& original, const std::string& reimported);

// Implementations: src/text_export.cpp (TextExporter) and src/text_import.cpp (TextImporter).

}  // namespace telltale
