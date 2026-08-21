#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "telltale/crc32.hpp"
#include "telltale/filter_engine.hpp"
#include "telltale/schema_update.hpp"
#include "telltale/text_format.hpp"

namespace telltale {

std::string TextImporter::unescape_string(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      char n = s[i + 1];
      if (n == 'n') {
        out.push_back('\n');
        ++i;
      } else if (n == 'r') {
        out.push_back('\r');
        ++i;
      } else if (n == 't') {
        out.push_back('\t');
        ++i;
      } else if (n == '\\' || n == '"') {
        out.push_back(n);
        ++i;
      } else
        out.push_back(s[i]);
    } else
      out.push_back(s[i]);
  }
  return out;
}

static Result validate_import_record(const TextRecord& rec) {
  if (rec.type_id == 0 && !rec.has_raw_payload && rec.fields.empty()) {
    return Result::fail(ErrorCode::InvalidTypeId, "missing type_id");
  }
  if (rec.has_raw_payload && rec.raw_payload.size() > MAX_PAYLOAD_SIZE) {
    return Result::fail(ErrorCode::PayloadTooLarge, "raw payload");
  }
  return Result::success();
}

static Result build_counter_payload(const TextRecord& rec, std::vector<uint8_t>& payload) {
  std::string n = rec.fields.count("name") ? rec.fields.at("name") : "";
  int64_t v =
      rec.fields.count("value") ? std::strtoll(rec.fields.at("value").c_str(), nullptr, 10) : 0;
  bool ab = rec.fields.count("absolute") && rec.fields.at("absolute") != "0";
  payload = EventLogWriter::encode_counter_payload(n, v, ab);
  if (payload.empty()) return Result::fail(ErrorCode::InvalidPayload, "empty");
  return Result::success();
}

static Result build_keyvalue_payload(const TextRecord& rec, std::vector<uint8_t>& payload) {
  std::string k = rec.fields.count("key") ? rec.fields.at("key") : "";
  int64_t v =
      rec.fields.count("value") ? std::strtoll(rec.fields.at("value").c_str(), nullptr, 10) : 0;
  bool ow = rec.fields.count("overwrite") && rec.fields.at("overwrite") != "0";
  payload = EventLogWriter::encode_keyvalue_payload(k, v, ow);
  if (payload.empty()) return Result::fail(ErrorCode::InvalidPayload, "empty");
  return Result::success();
}

static Result build_timestamp_payload(const TextRecord& rec, std::vector<uint8_t>& payload) {
  std::string lb = rec.fields.count("label") ? rec.fields.at("label") : "";
  uint64_t ms = rec.fields.count("epoch_millis")
                    ? std::strtoull(rec.fields.at("epoch_millis").c_str(), nullptr, 10)
                    : 0;
  bool ex = rec.fields.count("explicit_time") && rec.fields.at("explicit_time") != "0";
  payload = EventLogWriter::encode_timestamp_payload(lb, ms, ex);
  if (payload.empty()) return Result::fail(ErrorCode::InvalidPayload, "empty");
  return Result::success();
}

static Result build_checksum_payload(const TextRecord& rec, std::vector<uint8_t>& payload) {
  std::string lb = rec.fields.count("label") ? rec.fields.at("label") : "";
  uint32_t c =
      rec.fields.count("expected_crc")
          ? static_cast<uint32_t>(std::strtoul(rec.fields.at("expected_crc").c_str(), nullptr, 0))
          : 0;
  uint32_t sc =
      rec.fields.count("scope_flags")
          ? static_cast<uint32_t>(std::strtoul(rec.fields.at("scope_flags").c_str(), nullptr, 0))
          : 0;
  payload = EventLogWriter::encode_checksum_payload(lb, c, sc);
  if (payload.empty()) return Result::fail(ErrorCode::InvalidPayload, "empty");
  return Result::success();
}

static Result build_reset_payload(const TextRecord& rec, std::vector<uint8_t>& payload) {
  uint8_t sc =
      rec.fields.count("scope_flags")
          ? static_cast<uint8_t>(std::strtoul(rec.fields.at("scope_flags").c_str(), nullptr, 0))
          : 0xFF;
  payload = EventLogWriter::encode_reset_payload(sc);
  if (payload.empty()) return Result::fail(ErrorCode::InvalidPayload, "empty");
  return Result::success();
}

static Result build_print_payload(const TextRecord& rec, std::vector<uint8_t>& payload) {
  std::string m = rec.fields.count("message") ? rec.fields.at("message") : "";
  uint8_t sev =
      rec.fields.count("severity")
          ? static_cast<uint8_t>(std::strtoul(rec.fields.at("severity").c_str(), nullptr, 10))
          : 1;
  payload = EventLogWriter::encode_print_payload(m, sev);
  if (payload.empty()) return Result::fail(ErrorCode::InvalidPayload, "empty");
  return Result::success();
}

static Result build_stats_payload(const TextRecord& rec, std::vector<uint8_t>& payload) {
  uint8_t fl =
      rec.fields.count("output_flags")
          ? static_cast<uint8_t>(std::strtoul(rec.fields.at("output_flags").c_str(), nullptr, 0))
          : 0x0F;
  std::string p = rec.fields.count("prefix") ? rec.fields.at("prefix") : "";
  payload = EventLogWriter::encode_stats_payload(fl, p);
  if (payload.empty()) return Result::fail(ErrorCode::InvalidPayload, "empty");
  return Result::success();
}

static Result build_batch_payload(const TextRecord& rec, std::vector<uint8_t>& payload) {
  if (!rec.has_raw_payload) return Result::fail(ErrorCode::InvalidPayload, "batch needs raw_hex");
  payload = rec.raw_payload;
  return Result::success();
  if (payload.empty()) return Result::fail(ErrorCode::InvalidPayload, "empty");
  return Result::success();
}

TextImporter::TextImporter() : parse_errors_(0), strict_(true) {}
TextImporter::~TextImporter() = default;
Result TextImporter::parse_key_value(const std::string& line, std::string& key,
                                     std::string& value) {
  size_t eq = line.find('=');
  if (eq == std::string::npos) return Result::fail(ErrorCode::InvalidPayload, "no =");
  key = line.substr(0, eq);
  value = line.substr(eq + 1);
  while (!key.empty() && std::isspace(static_cast<unsigned char>(key.front()))) key.erase(0, 1);
  while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) key.pop_back();
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
    value.erase(0, 1);
  if (!value.empty() && value.front() == '"') {
    if (value.size() < 2 || value.back() != '"')
      return Result::fail(ErrorCode::InvalidPayload, "quote");
    value = unescape_string(value.substr(1, value.size() - 2));
  }
  return Result::success();
}

Result TextImporter::build_payload(const TextRecord& rec, std::vector<uint8_t>& payload) {
  if (rec.has_raw_payload && !rec.raw_payload.empty()) {
    payload = rec.raw_payload;
    return Result::success();
  }
  switch (rec.type_id) {
    case static_cast<uint16_t>(EventType::Counter):
      return build_counter_payload(rec, payload);
    case static_cast<uint16_t>(EventType::KeyValue):
      return build_keyvalue_payload(rec, payload);
    case static_cast<uint16_t>(EventType::Timestamp):
      return build_timestamp_payload(rec, payload);
    case static_cast<uint16_t>(EventType::Checksum):
      return build_checksum_payload(rec, payload);
    case static_cast<uint16_t>(EventType::Reset):
      return build_reset_payload(rec, payload);
    case static_cast<uint16_t>(EventType::Print):
      return build_print_payload(rec, payload);
    case static_cast<uint16_t>(EventType::Stats):
      return build_stats_payload(rec, payload);
    case static_cast<uint16_t>(EventType::Batch):
      return build_batch_payload(rec, payload);
    case SCHEMA_UPDATE_TYPE: {
      uint8_t fl =
          rec.fields.count("flags")
              ? static_cast<uint8_t>(std::strtoul(rec.fields.at("flags").c_str(), nullptr, 0))
              : 0;
      uint16_t tid = rec.fields.count("target_type")
                         ? static_cast<uint16_t>(
                               std::strtoul(rec.fields.at("target_type").c_str(), nullptr, 0))
                         : 0;
      uint16_t hid =
          rec.fields.count("handler_id")
              ? static_cast<uint16_t>(std::strtoul(rec.fields.at("handler_id").c_str(), nullptr, 0))
              : 0;
      payload = SchemaUpdateCodec::encode(fl, tid, hid, rec.fields.count("handler_id") > 0);
      return Result::success();
    }
    default:
      return Result::fail(ErrorCode::InvalidTypeId, "import type");
  }
}

Result TextImporter::finalize_record(TextRecord& rec) {
  Result vr = validate_import_record(rec);
  if (!vr.ok()) return vr;
  std::vector<uint8_t> payload;
  Result r = build_payload(rec, payload);
  if (!r.ok()) return r;
  rec.raw_payload = payload;
  rec.has_raw_payload = true;
  rec.crc32 = Crc32::of_record(rec.type_id, payload);
  return Result::success();
}

Result TextImporter::parse_line(const std::string& line, TextRecord* current, bool& in_record) {
  std::string t = line;
  while (!t.empty() && (t.back() == '\r' || t.back() == ' ' || t.back() == '\t')) t.pop_back();
  if (t.empty() || t[0] == '#') return Result::success();
  if (t == TEXT_RECORD_BEGIN) {
    if (in_record) return Result::fail(ErrorCode::InvalidPayload, "nested");
    current->index = 0;
    current->type_id = 0;
    current->crc32 = 0;
    current->fields.clear();
    current->raw_payload.clear();
    current->has_raw_payload = false;
    in_record = true;
    return Result::success();
  }
  if (t == TEXT_RECORD_END) {
    if (!in_record) return Result::fail(ErrorCode::InvalidPayload, "end");
    Result r = finalize_record(*current);
    if (!r.ok()) return r;
    records_.push_back(*current);
    in_record = false;
    return Result::success();
  }
  // Header metadata lines are informational and appear outside records.
  if (!in_record) {
    if (t.rfind("header_", 0) == 0) return Result::success();
    return Result::fail(ErrorCode::InvalidPayload, "outside");
  }
  std::string k, v;
  Result r = parse_key_value(t, k, v);
  if (!r.ok()) return r;
  if (k == "index")
    current->index = static_cast<uint32_t>(std::strtoul(v.c_str(), nullptr, 10));
  else if (k == "type_id")
    current->type_id = static_cast<uint16_t>(std::strtoul(v.c_str(), nullptr, 0));
  else if (k == "crc32")
    current->crc32 = static_cast<uint32_t>(std::strtoul(v.c_str(), nullptr, 0));
  else if (k == "raw_hex") {
    std::istringstream iss(v);
    std::string tok;
    current->raw_payload.clear();
    while (iss >> tok)
      current->raw_payload.push_back(static_cast<uint8_t>(std::strtoul(tok.c_str(), nullptr, 16)));
    current->has_raw_payload = true;
  } else
    current->fields[k] = v;
  return Result::success();
}

Result TextImporter::parse(const std::string& text_content) {
  records_.clear();
  parse_errors_ = 0;
  source_text_ = text_content;
  std::istringstream iss(text_content);
  std::string line;
  bool in_record = false;
  TextRecord current;
  while (std::getline(iss, line)) {
    Result r = parse_line(line, &current, in_record);
    if (!r.ok()) {
      ++parse_errors_;
      if (strict_) return r;
    }
  }
  if (in_record) return Result::fail(ErrorCode::InvalidPayload, "unclosed");
  return Result::success();
}

Result TextImporter::write_binary(const std::string& output_path) {
  EventLogWriter w(output_path);
  Result r = w.open();
  if (!r.ok()) return r;
  r = w.write_header();
  if (!r.ok()) return r;
  for (const auto& rec : records_) {
    std::vector<uint8_t> p = rec.raw_payload;
    if (p.empty()) {
      r = build_payload(rec, p);
      if (!r.ok()) return r;
    }
    r = w.write_event(rec.type_id, p);
    if (!r.ok()) return r;
  }
  return w.finalize();
}

Result TextImporter::import_file(const std::string& input_path, const std::string& output_path) {
  std::ifstream in(input_path);
  if (!in) return Result::fail(ErrorCode::OpenError, "open in");
  std::ostringstream oss;
  oss << in.rdbuf();
  Result r = parse(oss.str());
  if (!r.ok()) return r;
  return write_binary(output_path);
}

Result text_format_verify_roundtrip(const std::string& original_path,
                                    const std::string& reimported_path) {
  EventLogReader a(original_path);
  EventLogReader b(reimported_path);
  Result r = a.open();
  if (!r.ok()) return r;
  r = b.open();
  if (!r.ok()) return r;
  FileHeader ha, hb;
  r = a.read_header(ha);
  if (!r.ok()) return r;
  r = b.read_header(hb);
  if (!r.ok()) return r;
  if (ha.record_count != hb.record_count)
    return Result::fail(ErrorCode::VerifyFailed, "record count mismatch after roundtrip");
  r = a.seek_to_records();
  if (!r.ok()) return r;
  r = b.seek_to_records();
  if (!r.ok()) return r;
  uint32_t idx = 0;
  while (!a.eof()) {
    EventRecord ra, rb;
    r = a.read_next_record(ra);
    if (!r.ok()) break;
    r = b.read_next_record(rb);
    if (!r.ok()) return r;
    if (ra.type_id != rb.type_id || ra.payload != rb.payload)
      return Result::fail(ErrorCode::VerifyFailed, "record " + std::to_string(idx) + " mismatch");
    ++idx;
  }
  return Result::success();
}

Result text_format_full_verify(const std::string& original, const std::string& reimported) {
  return text_format_verify_roundtrip(original, reimported);
}

}  // namespace telltale
