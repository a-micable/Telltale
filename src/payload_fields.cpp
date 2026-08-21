#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

#include "telltale/filter_engine.hpp"
#include "telltale/schema_update.hpp"

namespace telltale {

namespace payload_fields {

static bool compare_string_op(FilterFieldOp op, const std::string& actual,
                              const std::string& expected) {
  switch (op) {
    case FilterFieldOp::Equal:
      return actual == expected;
    case FilterFieldOp::NotEqual:
      return actual != expected;
    case FilterFieldOp::Contains:
      return actual.find(expected) != std::string::npos;
    case FilterFieldOp::StartsWith:
      return actual.size() >= expected.size() && actual.compare(0, expected.size(), expected) == 0;
    case FilterFieldOp::EndsWith:
      return actual.size() >= expected.size() &&
             actual.compare(actual.size() - expected.size(), expected.size(), expected) == 0;
    default:
      return false;
  }
}

static bool compare_int_op(FilterFieldOp op, int64_t actual, int64_t expected) {
  switch (op) {
    case FilterFieldOp::Equal:
      return actual == expected;
    case FilterFieldOp::NotEqual:
      return actual != expected;
    case FilterFieldOp::GreaterThan:
      return actual > expected;
    case FilterFieldOp::LessThan:
      return actual < expected;
    case FilterFieldOp::GreaterOrEqual:
      return actual >= expected;
    case FilterFieldOp::LessOrEqual:
      return actual <= expected;
    default:
      return false;
  }
}

Result decode_all_fields(uint16_t type_id, const std::vector<uint8_t>& payload,
                         DecodedFields& out) {
  out.string_fields.clear();
  out.int_fields.clear();
  out.uint_fields.clear();

  switch (type_id) {
    case static_cast<uint16_t>(EventType::Counter): {
      CounterDelta cd;
      Result r = EventLogReader::decode_counter_payload(payload, cd);
      if (!r.ok()) return r;
      out.string_fields["name"] = cd.name;
      out.int_fields["value"] = cd.use_absolute ? cd.absolute : cd.delta;
      out.int_fields["absolute"] = cd.use_absolute ? 1 : 0;
      out.string_fields["mode"] = cd.use_absolute ? "absolute" : "delta";
      break;
    }
    case static_cast<uint16_t>(EventType::KeyValue): {
      KeyValueEntry kv;
      Result r = EventLogReader::decode_keyvalue_payload(payload, kv);
      if (!r.ok()) return r;
      out.string_fields["key"] = kv.key;
      out.int_fields["value"] = kv.value;
      out.int_fields["overwrite"] = kv.overwrite ? 1 : 0;
      break;
    }
    case static_cast<uint16_t>(EventType::Timestamp): {
      TimestampMarker tm;
      Result r = EventLogReader::decode_timestamp_payload(payload, tm);
      if (!r.ok()) return r;
      out.string_fields["label"] = tm.label;
      out.uint_fields["epoch_millis"] = tm.epoch_millis;
      out.int_fields["explicit_time"] = tm.use_explicit_time ? 1 : 0;
      break;
    }
    case static_cast<uint16_t>(EventType::Checksum): {
      ChecksumSpec cs;
      Result r = EventLogReader::decode_checksum_payload(payload, cs);
      if (!r.ok()) return r;
      out.string_fields["label"] = cs.label;
      out.uint_fields["expected_crc"] = cs.expected_crc;
      out.uint_fields["scope_flags"] = cs.scope_flags;
      break;
    }
    case static_cast<uint16_t>(EventType::Reset): {
      uint8_t scope = 0;
      Result r = EventLogReader::decode_reset_payload(payload, scope);
      if (!r.ok()) return r;
      out.uint_fields["scope_flags"] = scope;
      break;
    }
    case static_cast<uint16_t>(EventType::Print): {
      PrintMessage pm;
      Result r = EventLogReader::decode_print_payload(payload, pm);
      if (!r.ok()) return r;
      out.string_fields["message"] = pm.message;
      out.uint_fields["severity"] = pm.severity;
      break;
    }
    case static_cast<uint16_t>(EventType::Stats): {
      StatsRequest sr;
      Result r = EventLogReader::decode_stats_payload(payload, sr);
      if (!r.ok()) return r;
      out.string_fields["prefix"] = sr.prefix;
      out.uint_fields["output_flags"] = sr.output_flags;
      break;
    }
    case static_cast<uint16_t>(EventType::Batch): {
      BatchHeader bh;
      Result r = EventLogReader::decode_batch_header(payload, bh);
      if (!r.ok()) return r;
      out.uint_fields["sub_event_count"] = bh.sub_event_count;
      break;
    }
    case SCHEMA_UPDATE_TYPE: {
      SchemaUpdatePayload sup;
      Result r = SchemaUpdateCodec::decode(payload, sup);
      if (!r.ok()) return r;
      out.uint_fields["flags"] = sup.flags;
      out.uint_fields["target_type"] = sup.type_id;
      if (sup.has_handler_id) {
        out.uint_fields["handler_id"] = sup.handler_id;
      }
      break;
    }
    default:
      out.uint_fields["payload_size"] = static_cast<uint64_t>(payload.size());
      break;
  }
  return Result::success();
}

bool field_matches(const PayloadFieldFilter& filter, const DecodedFields& fields) {
  if (!filter.enabled) return true;
  auto si = fields.string_fields.find(filter.field_name);
  if (si != fields.string_fields.end()) {
    return compare_string_op(filter.op, si->second, filter.string_value);
  }
  auto ii = fields.int_fields.find(filter.field_name);
  if (ii != fields.int_fields.end()) {
    int64_t expected =
        filter.use_int_value
            ? filter.int_value
            : static_cast<int64_t>(std::strtoll(filter.string_value.c_str(), nullptr, 10));
    return compare_int_op(filter.op, ii->second, expected);
  }
  auto ui = fields.uint_fields.find(filter.field_name);
  if (ui != fields.uint_fields.end()) {
    int64_t actual = static_cast<int64_t>(ui->second);
    int64_t expected =
        filter.use_int_value
            ? filter.int_value
            : static_cast<int64_t>(std::strtoull(filter.string_value.c_str(), nullptr, 10));
    return compare_int_op(filter.op, actual, expected);
  }
  return false;
}

std::vector<std::string> list_field_names(uint16_t type_id) {
  switch (type_id) {
    case static_cast<uint16_t>(EventType::Counter):
      return {"name", "value", "absolute", "mode"};
    case static_cast<uint16_t>(EventType::KeyValue):
      return {"key", "value", "overwrite"};
    case static_cast<uint16_t>(EventType::Timestamp):
      return {"label", "epoch_millis", "explicit_time"};
    case static_cast<uint16_t>(EventType::Checksum):
      return {"label", "expected_crc", "scope_flags"};
    case static_cast<uint16_t>(EventType::Reset):
      return {"scope_flags"};
    case static_cast<uint16_t>(EventType::Print):
      return {"message", "severity"};
    case static_cast<uint16_t>(EventType::Stats):
      return {"prefix", "output_flags"};
    case static_cast<uint16_t>(EventType::Batch):
      return {"sub_event_count"};
    case SCHEMA_UPDATE_TYPE:
      return {"flags", "target_type", "handler_id"};
    default:
      return {"payload_size"};
  }
}

std::string describe_field(uint16_t type_id, const std::string& field_name,
                           const std::vector<uint8_t>& payload) {
  DecodedFields fields;
  if (!decode_all_fields(type_id, payload, fields).ok()) {
    return "<decode error>";
  }
  auto si = fields.string_fields.find(field_name);
  if (si != fields.string_fields.end()) return si->second;
  auto ii = fields.int_fields.find(field_name);
  if (ii != fields.int_fields.end()) return std::to_string(ii->second);
  auto ui = fields.uint_fields.find(field_name);
  if (ui != fields.uint_fields.end()) return std::to_string(ui->second);
  return "<unknown>";
}

}  // namespace payload_fields

}  // namespace telltale
