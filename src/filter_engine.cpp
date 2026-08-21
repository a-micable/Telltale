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

std::string filter_engine_explain(const FilterEngine& engine) {
  std::ostringstream oss;
  oss << "matches=" << engine.match_count() << " loaded=" << engine.total_records_loaded();
  return oss.str();
}

}  // namespace telltale
