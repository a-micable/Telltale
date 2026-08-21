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

TextExporter::TextExporter() : include_raw_hex_(false), verbose_(false), records_exported_(0) {}
TextExporter::~TextExporter() = default;

std::string TextExporter::escape_string(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '\\' || c == '"') {
      out.push_back('\\');
      out.push_back(c);
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else if (c == '\t') {
      out += "\\t";
    } else {
      out.push_back(c);
    }
  }
  return out;
}

std::string TextExporter::hex_encode(const std::vector<uint8_t>& data) {
  std::ostringstream oss;
  for (size_t i = 0; i < data.size(); ++i) {
    if (i) oss << ' ';
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
  }
  return oss.str();
}

static Result validate_export_payload(uint16_t type_id, const std::vector<uint8_t>& payload) {
  switch (type_id) {
    case static_cast<uint16_t>(EventType::Counter): {
      CounterDelta cd;
      Result r = EventLogReader::decode_counter_payload(payload, cd);
      if (!r.ok()) return r;
      if (cd.name.size() > MAX_NAME_LENGTH) return Result::fail(ErrorCode::StringTooLong, "name");
      {
        const int64_t value = cd.use_absolute ? cd.absolute : cd.delta;
        if (value < INT64_MIN / 2 || value > INT64_MAX / 2) {
          return Result::fail(ErrorCode::InvalidPayload, "value");
        }
      }
      return Result::success();
    }
    case static_cast<uint16_t>(EventType::KeyValue): {
      KeyValueEntry kv;
      Result r = EventLogReader::decode_keyvalue_payload(payload, kv);
      if (!r.ok()) return r;
      if (kv.key.size() > MAX_NAME_LENGTH) return Result::fail(ErrorCode::StringTooLong, "key");
      if (kv.value < INT64_MIN / 2 || kv.value > INT64_MAX / 2)
        return Result::fail(ErrorCode::InvalidPayload, "value");
      return Result::success();
    }
    case static_cast<uint16_t>(EventType::Timestamp): {
      TimestampMarker tm;
      Result r = EventLogReader::decode_timestamp_payload(payload, tm);
      if (!r.ok()) return r;
      if (tm.label.size() > MAX_NAME_LENGTH) return Result::fail(ErrorCode::StringTooLong, "label");
      (void)tm.epoch_millis;
      return Result::success();
    }
    case static_cast<uint16_t>(EventType::Checksum): {
      ChecksumSpec cs;
      Result r = EventLogReader::decode_checksum_payload(payload, cs);
      if (!r.ok()) return r;
      if (cs.label.size() > MAX_NAME_LENGTH) return Result::fail(ErrorCode::StringTooLong, "label");
      (void)cs.expected_crc;
      return Result::success();
    }
    case static_cast<uint16_t>(EventType::Reset): {
      uint8_t scope = 0;
      Result r = EventLogReader::decode_reset_payload(payload, scope);
      if (!r.ok()) return r;
      (void)scope;
      return Result::success();
    }
    case static_cast<uint16_t>(EventType::Print): {
      PrintMessage pm;
      Result r = EventLogReader::decode_print_payload(payload, pm);
      if (!r.ok()) return r;
      if (pm.message.size() > MAX_NAME_LENGTH)
        return Result::fail(ErrorCode::StringTooLong, "message");
      (void)pm.severity;
      return Result::success();
    }
    case static_cast<uint16_t>(EventType::Stats): {
      StatsRequest sr;
      Result r = EventLogReader::decode_stats_payload(payload, sr);
      if (!r.ok()) return r;
      if (sr.prefix.size() > MAX_NAME_LENGTH)
        return Result::fail(ErrorCode::StringTooLong, "prefix");
      (void)sr.output_flags;
      return Result::success();
    }
    case static_cast<uint16_t>(EventType::Batch): {
      BatchHeader bh;
      Result r = EventLogReader::decode_batch_header(payload, bh);
      if (!r.ok()) return r;
      (void)bh.sub_event_count;
      return Result::success();
    }
    case SCHEMA_UPDATE_TYPE: {
      SchemaUpdatePayload sup;
      return SchemaUpdateCodec::decode(payload, sup);
    }
    default:
      if (payload.size() > MAX_PAYLOAD_SIZE)
        return Result::fail(ErrorCode::PayloadTooLarge, "export");
      return Result::success();
  }
}

static Result export_counter_fields(const std::vector<uint8_t>& payload, std::ostream& out) {
  CounterDelta cd;
  Result r = EventLogReader::decode_counter_payload(payload, cd);
  if (!r.ok()) return r;
  out << "  name=\"" << TextExporter::escape_string(cd.name) << "\"" << std::endl;
  out << "  value=" << (cd.use_absolute ? cd.absolute : cd.delta) << std::endl;
  out << "  absolute=" << (cd.use_absolute ? 1 : 0) << std::endl;
  return Result::success();
}

static Result export_keyvalue_fields(const std::vector<uint8_t>& payload, std::ostream& out) {
  KeyValueEntry kv;
  Result r = EventLogReader::decode_keyvalue_payload(payload, kv);
  if (!r.ok()) return r;
  out << "  key=\"" << TextExporter::escape_string(kv.key) << "\"" << std::endl;
  out << "  value=" << kv.value << std::endl;
  out << "  overwrite=" << (kv.overwrite ? 1 : 0) << std::endl;
  return Result::success();
}

static Result export_timestamp_fields(const std::vector<uint8_t>& payload, std::ostream& out) {
  TimestampMarker tm;
  Result r = EventLogReader::decode_timestamp_payload(payload, tm);
  if (!r.ok()) return r;
  out << "  label=\"" << TextExporter::escape_string(tm.label) << "\"" << std::endl;
  out << "  epoch_millis=" << tm.epoch_millis << std::endl;
  return Result::success();
}

static Result export_checksum_fields(const std::vector<uint8_t>& payload, std::ostream& out) {
  ChecksumSpec cs;
  Result r = EventLogReader::decode_checksum_payload(payload, cs);
  if (!r.ok()) return r;
  out << "  label=\"" << TextExporter::escape_string(cs.label) << "\"" << std::endl;
  out << "  expected_crc=" << cs.expected_crc << std::endl;
  return Result::success();
}

static Result export_reset_fields(const std::vector<uint8_t>& payload, std::ostream& out) {
  uint8_t scope = 0;
  Result r = EventLogReader::decode_reset_payload(payload, scope);
  if (!r.ok()) return r;
  out << "  scope_flags=" << static_cast<unsigned>(scope) << std::endl;
  return Result::success();
}

static Result export_print_fields(const std::vector<uint8_t>& payload, std::ostream& out) {
  PrintMessage pm;
  Result r = EventLogReader::decode_print_payload(payload, pm);
  if (!r.ok()) return r;
  out << "  message=\"" << TextExporter::escape_string(pm.message) << "\"" << std::endl;
  out << "  severity=" << static_cast<unsigned>(pm.severity) << std::endl;
  return Result::success();
}

static Result export_stats_fields(const std::vector<uint8_t>& payload, std::ostream& out) {
  StatsRequest sr;
  Result r = EventLogReader::decode_stats_payload(payload, sr);
  if (!r.ok()) return r;
  out << "  prefix=\"" << TextExporter::escape_string(sr.prefix) << "\"" << std::endl;
  out << "  output_flags=" << static_cast<unsigned>(sr.output_flags) << std::endl;
  return Result::success();
}

static Result export_batch_fields(const std::vector<uint8_t>& payload, std::ostream& out) {
  BatchHeader bh;
  Result r = EventLogReader::decode_batch_header(payload, bh);
  if (!r.ok()) return r;
  out << "  sub_event_count=" << bh.sub_event_count << std::endl;
  return Result::success();
}

static void append_field_docs(uint16_t /*type_id*/, std::ostream& /*out*/) {}

Result TextExporter::write_record_fields(uint16_t type_id, const std::vector<uint8_t>& payload,
                                         std::ostream& out) {
  Result vr = validate_export_payload(type_id, payload);
  if (!vr.ok()) return vr;
  append_field_docs(type_id, out);
  switch (type_id) {
    case static_cast<uint16_t>(EventType::Counter):
      return export_counter_fields(payload, out);
    case static_cast<uint16_t>(EventType::KeyValue):
      return export_keyvalue_fields(payload, out);
    case static_cast<uint16_t>(EventType::Timestamp):
      return export_timestamp_fields(payload, out);
    case static_cast<uint16_t>(EventType::Checksum):
      return export_checksum_fields(payload, out);
    case static_cast<uint16_t>(EventType::Reset):
      return export_reset_fields(payload, out);
    case static_cast<uint16_t>(EventType::Print):
      return export_print_fields(payload, out);
    case static_cast<uint16_t>(EventType::Stats):
      return export_stats_fields(payload, out);
    case static_cast<uint16_t>(EventType::Batch):
      return export_batch_fields(payload, out);
    case SCHEMA_UPDATE_TYPE: {
      SchemaUpdatePayload sup;
      Result r = SchemaUpdateCodec::decode(payload, sup);
      if (!r.ok()) return r;
      out << "  flags=" << static_cast<int>(sup.flags) << std::endl;
      out << "  target_type=0x" << std::hex << sup.type_id << std::dec << std::endl;
      if (sup.has_handler_id)
        out << "  handler_id=0x" << std::hex << sup.handler_id << std::dec << std::endl;
      return Result::success();
    }
    default:
      out << "  payload_size=" << payload.size() << std::endl;
      if (include_raw_hex_ && !payload.empty())
        out << "  raw_hex=" << hex_encode(payload) << std::endl;
      return Result::success();
  }
}

Result TextExporter::export_record(const EventRecord& record, uint32_t index, std::ostream& out) {
  out << TEXT_RECORD_BEGIN << std::endl;
  out << "  index=" << index << std::endl;
  out << "  type_id=0x" << std::hex << record.type_id << std::dec << std::endl;
  out << "  type_name=" << event_type_name(record.type_id) << std::endl;
  out << "  crc32=0x" << std::hex << record.crc32 << std::dec << std::endl;
  out << "  payload_length=" << record.payload.size() << std::endl;
  Result r = write_record_fields(record.type_id, record.payload, out);
  if (!r.ok()) return r;
  out << TEXT_RECORD_END << std::endl;
  ++records_exported_;
  return Result::success();
}

Result TextExporter::export_to_string(const std::string& input_path, std::string& output) {
  std::ostringstream oss;
  oss << TEXT_FORMAT_MAGIC_LINE << std::endl;
  EventLogReader reader(input_path);
  Result r = reader.open();
  if (!r.ok()) return r;
  FileHeader h;
  r = reader.read_header(h);
  if (!r.ok()) return r;
  oss << "header_version=" << h.version << std::endl;
  oss << "header_record_count=" << h.record_count << std::endl;
  r = reader.seek_to_records();
  if (!r.ok()) return r;
  uint32_t idx = 0;
  while (!reader.eof()) {
    EventRecord rec;
    r = reader.read_next_record(rec);
    if (!r.ok()) {
      if (r.code == ErrorCode::UnexpectedEof) break;
      return r;
    }
    r = export_record(rec, idx++, oss);
    if (!r.ok()) return r;
  }
  reader.close();
  output = oss.str();
  return Result::success();
}

Result TextExporter::export_file(const std::string& input_path, const std::string& output_path) {
  std::string content;
  Result r = export_to_string(input_path, content);
  if (!r.ok()) return r;
  std::ofstream out(output_path);
  if (!out) return Result::fail(ErrorCode::IoError, "open out");
  out << content;
  if (verbose_) std::cout << "Exported " << records_exported_ << " records" << std::endl;
  return Result::success();
}

}  // namespace telltale
