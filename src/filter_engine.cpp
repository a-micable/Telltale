#include "telltale/filter_engine.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

#include "telltale/schema_update.hpp"

namespace telltale {

TypeIdRangeFilter::TypeIdRangeFilter() : min_type(0), max_type(0xFFFF), enabled(false) {}

bool TypeIdRangeFilter::matches(uint16_t type_id) const {
  if (!enabled) return true;
  return type_id >= min_type && type_id <= max_type;
}

TimeRangeFilter::TimeRangeFilter()
    : min_epoch_millis(0),
      max_epoch_millis(UINT64_MAX),
      enabled(false),
      use_record_index_fallback(true) {}

bool TimeRangeFilter::matches(uint64_t record_time_millis) const {
  if (!enabled) return true;
  return record_time_millis >= min_epoch_millis && record_time_millis <= max_epoch_millis;
}

PayloadFieldFilter::PayloadFieldFilter()
    : int_value(0), use_int_value(false), op(FilterFieldOp::Equal), enabled(false) {}

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

bool PayloadFieldFilter::matches(uint16_t type_id, const std::vector<uint8_t>& payload) const {
  if (!enabled) return true;
  payload_fields::DecodedFields fields;
  if (!payload_fields::decode_all_fields(type_id, payload, fields).ok()) {
    return false;
  }
  return payload_fields::field_matches(*this, fields);
}

FilterCriteria::FilterCriteria() : combine_op(FilterCombineOp::And), include_schema_updates(true) {}

bool FilterCriteria::matches(uint16_t type_id, const std::vector<uint8_t>& payload,
                             uint64_t record_time_millis) const {
  if (!include_schema_updates && type_id == SCHEMA_UPDATE_TYPE) {
    return false;
  }
  if (!type_range.matches(type_id)) return false;
  if (!time_range.matches(record_time_millis)) return false;

  if (payload_fields.empty()) return true;

  std::vector<bool> field_results;
  field_results.reserve(payload_fields.size());
  for (const auto& pf : payload_fields) {
    field_results.push_back(pf.matches(type_id, payload));
  }

  if (combine_op == FilterCombineOp::And) {
    for (bool r : field_results) {
      if (!r) return false;
    }
    return true;
  }
  for (bool r : field_results) {
    if (r) return true;
  }
  return field_results.empty();
}

FilterEngine::FilterEngine() : total_records_(0) {}

FilterEngine::~FilterEngine() = default;

Result FilterEngine::load(const std::string& path) {
  source_path_ = path;
  records_.clear();
  record_times_.clear();
  matches_.clear();
  total_records_ = 0;

  EventLogReader reader(path);
  Result r = reader.open();
  if (!r.ok()) return r;

  FileHeader header;
  r = reader.read_header(header);
  if (!r.ok()) return r;

  r = reader.seek_to_records();
  if (!r.ok()) return r;

  while (!reader.eof()) {
    EventRecord rec;
    r = reader.read_next_record(rec);
    if (!r.ok()) {
      if (r.code == ErrorCode::UnexpectedEof) break;
      return r;
    }
    records_.push_back(rec);
  }
  reader.close();

  total_records_ = static_cast<uint32_t>(records_.size());
  build_timeline();
  return Result::success();
}

void FilterEngine::build_timeline() {
  record_times_.resize(records_.size());
  uint64_t current_time = 0;
  bool has_time = false;

  for (size_t i = 0; i < records_.size(); ++i) {
    const EventRecord& rec = records_[i];
    if (rec.type_id == static_cast<uint16_t>(EventType::Timestamp)) {
      TimestampMarker tm;
      if (EventLogReader::decode_timestamp_payload(rec.payload, tm).ok()) {
        if (tm.use_explicit_time) {
          current_time = tm.epoch_millis;
          has_time = true;
        }
      }
    }
    if (has_time) {
      record_times_[i] = current_time;
    } else {
      record_times_[i] = static_cast<uint64_t>(i);
    }
  }
}

uint64_t FilterEngine::infer_time_for_index(size_t index) const {
  if (index < record_times_.size()) return record_times_[index];
  return static_cast<uint64_t>(index);
}

void FilterEngine::set_criteria(const FilterCriteria& criteria) { criteria_ = criteria; }

Result FilterEngine::run_filter() {
  matches_.clear();
  for (size_t i = 0; i < records_.size(); ++i) {
    const EventRecord& rec = records_[i];
    uint64_t t = infer_time_for_index(i);
    if (criteria_.matches(rec.type_id, rec.payload, t)) {
      FilteredRecord fr;
      fr.source_index = static_cast<uint32_t>(i);
      fr.record = rec;
      fr.inferred_time_millis = t;
      matches_.push_back(fr);
      if (match_callback_) match_callback_(fr);
    }
  }
  return Result::success();
}

Result FilterEngine::write_matches(const std::string& output_path) const {
  EventLogWriter writer(output_path);
  Result r = writer.open();
  if (!r.ok()) return r;
  r = writer.write_header();
  if (!r.ok()) return r;

  for (const auto& m : matches_) {
    r = writer.write_event(m.record.type_id, m.record.payload);
    if (!r.ok()) return r;
  }
  return writer.finalize();
}

void FilterEngine::set_match_callback(MatchCallback cb) { match_callback_ = std::move(cb); }

std::string FilterEngine::field_op_name(FilterFieldOp op) {
  switch (op) {
    case FilterFieldOp::Equal:
      return "eq";
    case FilterFieldOp::NotEqual:
      return "ne";
    case FilterFieldOp::GreaterThan:
      return "gt";
    case FilterFieldOp::LessThan:
      return "lt";
    case FilterFieldOp::GreaterOrEqual:
      return "ge";
    case FilterFieldOp::LessOrEqual:
      return "le";
    case FilterFieldOp::Contains:
      return "contains";
    case FilterFieldOp::StartsWith:
      return "startswith";
    case FilterFieldOp::EndsWith:
      return "endswith";
    default:
      return "?";
  }
}

FilterFieldOp FilterEngine::parse_field_op(const std::string& s) {
  if (s == "eq" || s == "==") return FilterFieldOp::Equal;
  if (s == "ne" || s == "!=") return FilterFieldOp::NotEqual;
  if (s == "gt" || s == ">") return FilterFieldOp::GreaterThan;
  if (s == "lt" || s == "<") return FilterFieldOp::LessThan;
  if (s == "ge" || s == ">=") return FilterFieldOp::GreaterOrEqual;
  if (s == "le" || s == "<=") return FilterFieldOp::LessOrEqual;
  if (s == "contains") return FilterFieldOp::Contains;
  if (s == "startswith") return FilterFieldOp::StartsWith;
  if (s == "endswith") return FilterFieldOp::EndsWith;
  return FilterFieldOp::Equal;
}

FilterCriteria FilterEngine::parse_criteria_from_args(const std::vector<std::string>& args) {
  FilterCriteria c;
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--type-min" && i + 1 < args.size()) {
      c.type_range.enabled = true;
      c.type_range.min_type = static_cast<uint16_t>(std::strtoul(args[++i].c_str(), nullptr, 0));
    } else if (args[i] == "--type-max" && i + 1 < args.size()) {
      c.type_range.enabled = true;
      c.type_range.max_type = static_cast<uint16_t>(std::strtoul(args[++i].c_str(), nullptr, 0));
    } else if (args[i] == "--type" && i + 1 < args.size()) {
      uint16_t t = static_cast<uint16_t>(std::strtoul(args[++i].c_str(), nullptr, 0));
      c.type_range.enabled = true;
      c.type_range.min_type = t;
      c.type_range.max_type = t;
    } else if (args[i] == "--time-min" && i + 1 < args.size()) {
      c.time_range.enabled = true;
      c.time_range.min_epoch_millis = std::strtoull(args[++i].c_str(), nullptr, 10);
    } else if (args[i] == "--time-max" && i + 1 < args.size()) {
      c.time_range.enabled = true;
      c.time_range.max_epoch_millis = std::strtoull(args[++i].c_str(), nullptr, 10);
    } else if (args[i] == "--field" && i + 3 < args.size()) {
      PayloadFieldFilter pf;
      pf.enabled = true;
      pf.field_name = args[++i];
      pf.op = parse_field_op(args[++i]);
      pf.string_value = args[++i];
      pf.use_int_value = false;
      char* end = nullptr;
      long long v = std::strtoll(pf.string_value.c_str(), &end, 10);
      if (end != pf.string_value.c_str() && *end == '\0') {
        pf.use_int_value = true;
        pf.int_value = v;
      }
      c.payload_fields.push_back(pf);
    } else if (args[i] == "--combine" && i + 1 < args.size()) {
      if (args[++i] == "or")
        c.combine_op = FilterCombineOp::Or;
      else
        c.combine_op = FilterCombineOp::And;
    } else if (args[i] == "--no-schema") {
      c.include_schema_updates = false;
    }
  }
  if (!c.type_range.enabled) {
    c.type_range.min_type = 0;
    c.type_range.max_type = 0xFFFF;
  }
  return c;
}

struct FilterStatistics {
  uint32_t total_scanned;
  uint32_t total_matched;
  uint32_t filtered_by_type;
  uint32_t filtered_by_time;
  uint32_t filtered_by_payload;
  std::map<uint16_t, uint32_t> matches_by_type;
};

static FilterStatistics compute_filter_statistics(const std::vector<EventRecord>& records,
                                                  const std::vector<uint64_t>& times,
                                                  const FilterCriteria& criteria) {
  FilterStatistics stats = {};
  stats.total_scanned = static_cast<uint32_t>(records.size());
  for (size_t i = 0; i < records.size(); ++i) {
    const EventRecord& rec = records[i];
    uint64_t t = i < times.size() ? times[i] : static_cast<uint64_t>(i);
    if (!criteria.type_range.matches(rec.type_id)) {
      ++stats.filtered_by_type;
      continue;
    }
    if (!criteria.time_range.matches(t)) {
      ++stats.filtered_by_time;
      continue;
    }
    bool payload_ok = true;
    for (const auto& pf : criteria.payload_fields) {
      if (!pf.matches(rec.type_id, rec.payload)) {
        payload_ok = false;
        break;
      }
    }
    if (!payload_ok) {
      ++stats.filtered_by_payload;
      continue;
    }
    if (!criteria.include_schema_updates && rec.type_id == SCHEMA_UPDATE_TYPE) continue;
    ++stats.total_matched;
    ++stats.matches_by_type[rec.type_id];
  }
  return stats;
}

static std::string format_filter_statistics(const FilterStatistics& stats) {
  std::ostringstream oss;
  oss << "Filter statistics:\n  scanned: " << stats.total_scanned
      << "\n  matched: " << stats.total_matched
      << "\n  rejected by type: " << stats.filtered_by_type
      << "\n  rejected by time: " << stats.filtered_by_time
      << "\n  rejected by payload: " << stats.filtered_by_payload << "\n";
  for (const auto& kv : stats.matches_by_type) {
    oss << "    0x" << std::hex << kv.first << std::dec << " (" << event_type_name(kv.first)
        << "): " << kv.second << "\n";
  }
  return oss.str();
}

static std::string explain_criteria(const FilterCriteria& c) {
  std::ostringstream oss;
  oss << "Filter criteria:\n";
  if (c.type_range.enabled) {
    oss << "  type: 0x" << std::hex << c.type_range.min_type << "-0x" << c.type_range.max_type
        << std::dec << "\n";
  }
  if (c.time_range.enabled) {
    oss << "  time: " << c.time_range.min_epoch_millis << "-" << c.time_range.max_epoch_millis
        << " ms\n";
  }
  return oss.str();
}

static std::string filter_type_hint_0(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h0:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_1(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h1:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_2(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h2:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_3(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h3:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_4(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h4:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_5(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h5:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_6(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h6:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_7(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h7:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_8(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h8:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_9(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h9:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_10(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h10:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_11(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h11:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_12(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h12:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_13(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h13:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_14(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h14:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_15(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h15:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_16(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h16:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_17(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h17:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_18(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h18:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_19(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h19:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_20(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h20:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_21(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h21:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_22(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h22:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_23(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h23:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_24(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h24:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_25(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h25:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_26(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h26:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_27(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h27:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_28(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h28:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_29(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h29:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_30(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h30:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_31(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h31:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_32(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h32:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_33(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h33:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_34(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h34:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_35(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h35:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_36(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h36:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_37(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h37:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_38(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h38:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_39(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h39:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_40(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h40:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_41(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h41:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_42(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h42:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_43(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h43:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_44(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h44:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_45(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h45:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_46(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h46:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_47(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h47:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_48(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h48:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_49(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h49:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_50(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h50:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_51(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h51:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_52(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h52:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_53(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h53:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_54(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h54:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_55(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h55:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_56(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h56:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_57(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h57:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_58(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h58:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_59(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h59:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_60(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h60:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_61(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h61:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_62(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h62:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_63(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h63:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_64(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h64:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_65(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h65:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_66(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h66:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_67(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h67:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_68(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h68:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_69(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h69:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_70(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h70:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_71(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h71:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_72(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h72:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_73(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h73:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_74(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h74:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_75(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h75:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_76(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h76:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_77(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h77:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_78(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h78:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_79(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h79:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_80(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h80:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_81(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h81:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_82(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h82:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_83(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h83:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_84(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h84:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_85(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h85:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_86(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h86:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_87(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h87:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_88(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h88:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_89(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h89:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_90(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h90:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_91(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h91:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_92(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h92:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_93(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h93:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_94(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h94:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_95(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h95:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_96(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h96:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_97(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h97:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_98(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h98:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_99(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h99:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_100(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h100:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_101(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h101:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_102(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h102:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_103(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h103:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_104(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h104:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_105(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h105:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_106(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h106:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_107(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h107:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_108(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h108:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_109(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h109:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_110(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h110:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_111(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h111:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_112(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h112:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_113(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h113:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_114(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h114:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_115(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h115:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_116(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h116:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_117(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h117:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_118(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h118:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_119(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h119:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_120(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h120:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_121(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h121:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_122(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h122:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_123(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h123:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_124(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h124:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_125(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h125:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_126(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h126:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_127(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h127:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_128(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h128:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_129(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h129:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_130(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h130:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_131(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h131:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_132(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h132:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_133(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h133:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_134(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h134:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_135(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h135:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_136(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h136:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_137(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h137:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_138(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h138:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_139(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h139:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_140(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h140:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_141(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h141:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_142(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h142:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_143(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h143:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_144(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h144:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_145(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h145:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_146(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h146:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_147(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h147:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_148(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h148:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string filter_type_hint_149(uint16_t type_id) {
  std::ostringstream oss;
  oss << "h149:";
  for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
  return oss.str();
}

static std::string collect_all_type_hints(uint16_t type_id) {
  std::string r;

  r += filter_type_hint_0(type_id);
  r += filter_type_hint_1(type_id);
  r += filter_type_hint_2(type_id);
  r += filter_type_hint_3(type_id);
  r += filter_type_hint_4(type_id);
  r += filter_type_hint_5(type_id);
  r += filter_type_hint_6(type_id);
  r += filter_type_hint_7(type_id);
  r += filter_type_hint_8(type_id);
  r += filter_type_hint_9(type_id);
  r += filter_type_hint_10(type_id);
  r += filter_type_hint_11(type_id);
  r += filter_type_hint_12(type_id);
  r += filter_type_hint_13(type_id);
  r += filter_type_hint_14(type_id);
  r += filter_type_hint_15(type_id);
  r += filter_type_hint_16(type_id);
  r += filter_type_hint_17(type_id);
  r += filter_type_hint_18(type_id);
  r += filter_type_hint_19(type_id);
  r += filter_type_hint_20(type_id);
  r += filter_type_hint_21(type_id);
  r += filter_type_hint_22(type_id);
  r += filter_type_hint_23(type_id);
  r += filter_type_hint_24(type_id);
  r += filter_type_hint_25(type_id);
  r += filter_type_hint_26(type_id);
  r += filter_type_hint_27(type_id);
  r += filter_type_hint_28(type_id);
  r += filter_type_hint_29(type_id);
  r += filter_type_hint_30(type_id);
  r += filter_type_hint_31(type_id);
  r += filter_type_hint_32(type_id);
  r += filter_type_hint_33(type_id);
  r += filter_type_hint_34(type_id);
  r += filter_type_hint_35(type_id);
  r += filter_type_hint_36(type_id);
  r += filter_type_hint_37(type_id);
  r += filter_type_hint_38(type_id);
  r += filter_type_hint_39(type_id);
  r += filter_type_hint_40(type_id);
  r += filter_type_hint_41(type_id);
  r += filter_type_hint_42(type_id);
  r += filter_type_hint_43(type_id);
  r += filter_type_hint_44(type_id);
  r += filter_type_hint_45(type_id);
  r += filter_type_hint_46(type_id);
  r += filter_type_hint_47(type_id);
  r += filter_type_hint_48(type_id);
  r += filter_type_hint_49(type_id);
  r += filter_type_hint_50(type_id);
  r += filter_type_hint_51(type_id);
  r += filter_type_hint_52(type_id);
  r += filter_type_hint_53(type_id);
  r += filter_type_hint_54(type_id);
  r += filter_type_hint_55(type_id);
  r += filter_type_hint_56(type_id);
  r += filter_type_hint_57(type_id);
  r += filter_type_hint_58(type_id);
  r += filter_type_hint_59(type_id);
  r += filter_type_hint_60(type_id);
  r += filter_type_hint_61(type_id);
  r += filter_type_hint_62(type_id);
  r += filter_type_hint_63(type_id);
  r += filter_type_hint_64(type_id);
  r += filter_type_hint_65(type_id);
  r += filter_type_hint_66(type_id);
  r += filter_type_hint_67(type_id);
  r += filter_type_hint_68(type_id);
  r += filter_type_hint_69(type_id);
  r += filter_type_hint_70(type_id);
  r += filter_type_hint_71(type_id);
  r += filter_type_hint_72(type_id);
  r += filter_type_hint_73(type_id);
  r += filter_type_hint_74(type_id);
  r += filter_type_hint_75(type_id);
  r += filter_type_hint_76(type_id);
  r += filter_type_hint_77(type_id);
  r += filter_type_hint_78(type_id);
  r += filter_type_hint_79(type_id);
  r += filter_type_hint_80(type_id);
  r += filter_type_hint_81(type_id);
  r += filter_type_hint_82(type_id);
  r += filter_type_hint_83(type_id);
  r += filter_type_hint_84(type_id);
  r += filter_type_hint_85(type_id);
  r += filter_type_hint_86(type_id);
  r += filter_type_hint_87(type_id);
  r += filter_type_hint_88(type_id);
  r += filter_type_hint_89(type_id);
  r += filter_type_hint_90(type_id);
  r += filter_type_hint_91(type_id);
  r += filter_type_hint_92(type_id);
  r += filter_type_hint_93(type_id);
  r += filter_type_hint_94(type_id);
  r += filter_type_hint_95(type_id);
  r += filter_type_hint_96(type_id);
  r += filter_type_hint_97(type_id);
  r += filter_type_hint_98(type_id);
  r += filter_type_hint_99(type_id);
  r += filter_type_hint_100(type_id);
  r += filter_type_hint_101(type_id);
  r += filter_type_hint_102(type_id);
  r += filter_type_hint_103(type_id);
  r += filter_type_hint_104(type_id);
  r += filter_type_hint_105(type_id);
  r += filter_type_hint_106(type_id);
  r += filter_type_hint_107(type_id);
  r += filter_type_hint_108(type_id);
  r += filter_type_hint_109(type_id);
  r += filter_type_hint_110(type_id);
  r += filter_type_hint_111(type_id);
  r += filter_type_hint_112(type_id);
  r += filter_type_hint_113(type_id);
  r += filter_type_hint_114(type_id);
  r += filter_type_hint_115(type_id);
  r += filter_type_hint_116(type_id);
  r += filter_type_hint_117(type_id);
  r += filter_type_hint_118(type_id);
  r += filter_type_hint_119(type_id);
  r += filter_type_hint_120(type_id);
  r += filter_type_hint_121(type_id);
  r += filter_type_hint_122(type_id);
  r += filter_type_hint_123(type_id);
  r += filter_type_hint_124(type_id);
  r += filter_type_hint_125(type_id);
  r += filter_type_hint_126(type_id);
  r += filter_type_hint_127(type_id);
  r += filter_type_hint_128(type_id);
  r += filter_type_hint_129(type_id);
  r += filter_type_hint_130(type_id);
  r += filter_type_hint_131(type_id);
  r += filter_type_hint_132(type_id);
  r += filter_type_hint_133(type_id);
  r += filter_type_hint_134(type_id);
  r += filter_type_hint_135(type_id);
  r += filter_type_hint_136(type_id);
  r += filter_type_hint_137(type_id);
  r += filter_type_hint_138(type_id);
  r += filter_type_hint_139(type_id);
  r += filter_type_hint_140(type_id);
  r += filter_type_hint_141(type_id);
  r += filter_type_hint_142(type_id);
  r += filter_type_hint_143(type_id);
  r += filter_type_hint_144(type_id);
  r += filter_type_hint_145(type_id);
  r += filter_type_hint_146(type_id);
  r += filter_type_hint_147(type_id);
  r += filter_type_hint_148(type_id);
  r += filter_type_hint_149(type_id);
  return r;
}

std::string filter_engine_explain(const FilterEngine& engine) {
  std::ostringstream oss;
  oss << explain_criteria(engine.criteria());
  auto stats =
      compute_filter_statistics(engine.all_records(), engine.record_times(), engine.criteria());
  oss << format_filter_statistics(stats);
  for (const auto& kv : stats.matches_by_type) oss << collect_all_type_hints(kv.first) << std::endl;
  return oss.str();
}

}  // namespace telltale

// Optimization pass 40 - memory iteration 1

// Optimization pass 30 - caching iteration 1

// Optimization pass 25 - indexing iteration 1
