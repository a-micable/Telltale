#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "telltale/binary_io.hpp"
#include "telltale/errors.hpp"
#include "telltale/types.hpp"

namespace telltale {

enum class FilterFieldOp {
  Equal,
  NotEqual,
  GreaterThan,
  LessThan,
  GreaterOrEqual,
  LessOrEqual,
  Contains,
  StartsWith,
  EndsWith
};

enum class FilterCombineOp { And, Or };

struct TypeIdRangeFilter {
  uint16_t min_type;
  uint16_t max_type;
  bool enabled;

  TypeIdRangeFilter();
  bool matches(uint16_t type_id) const;
};

struct TimeRangeFilter {
  uint64_t min_epoch_millis;
  uint64_t max_epoch_millis;
  bool enabled;
  bool use_record_index_fallback;

  TimeRangeFilter();
  bool matches(uint64_t record_time_millis) const;
};

struct PayloadFieldFilter {
  std::string field_name;
  std::string string_value;
  int64_t int_value;
  bool use_int_value;
  FilterFieldOp op;
  bool enabled;

  PayloadFieldFilter();
  bool matches(uint16_t type_id, const std::vector<uint8_t>& payload) const;
};

struct FilterCriteria {
  TypeIdRangeFilter type_range;
  TimeRangeFilter time_range;
  std::vector<PayloadFieldFilter> payload_fields;
  FilterCombineOp combine_op;
  bool include_schema_updates;

  FilterCriteria();
  bool matches(uint16_t type_id, const std::vector<uint8_t>& payload,
               uint64_t record_time_millis) const;
};

struct FilteredRecord {
  uint32_t source_index;
  EventRecord record;
  uint64_t inferred_time_millis;
};

class FilterEngine {
 public:
  FilterEngine();
  ~FilterEngine();

  FilterEngine(const FilterEngine&) = delete;
  FilterEngine& operator=(const FilterEngine&) = delete;

  Result load(const std::string& path);
  void set_criteria(const FilterCriteria& criteria);
  const FilterCriteria& criteria() const { return criteria_; }

  Result run_filter();
  size_t match_count() const { return matches_.size(); }
  const std::vector<FilteredRecord>& matches() const { return matches_; }

  Result write_matches(const std::string& output_path) const;

  using MatchCallback = std::function<void(const FilteredRecord&)>;
  void set_match_callback(MatchCallback cb);

  uint32_t total_records_loaded() const { return total_records_; }
  const std::vector<EventRecord>& all_records() const { return records_; }
  const std::vector<uint64_t>& record_times() const { return record_times_; }

  static FilterCriteria parse_criteria_from_args(const std::vector<std::string>& args);
  static std::string field_op_name(FilterFieldOp op);
  static FilterFieldOp parse_field_op(const std::string& s);

 private:
  std::string source_path_;
  std::vector<EventRecord> records_;
  std::vector<uint64_t> record_times_;
  FilterCriteria criteria_;
  std::vector<FilteredRecord> matches_;
  uint32_t total_records_;
  MatchCallback match_callback_;

  void build_timeline();
  uint64_t infer_time_for_index(size_t index) const;
};

namespace payload_fields {

struct DecodedFields {
  std::map<std::string, std::string> string_fields;
  std::map<std::string, int64_t> int_fields;
  std::map<std::string, uint64_t> uint_fields;
};

Result decode_all_fields(uint16_t type_id, const std::vector<uint8_t>& payload, DecodedFields& out);
bool field_matches(const PayloadFieldFilter& filter, const DecodedFields& fields);
std::vector<std::string> list_field_names(uint16_t type_id);
std::string describe_field(uint16_t type_id, const std::string& field_name,
                           const std::vector<uint8_t>& payload);

}  // namespace payload_fields

std::string filter_engine_explain(const FilterEngine& engine);

}  // namespace telltale
