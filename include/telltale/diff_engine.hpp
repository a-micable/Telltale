#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "telltale/binary_io.hpp"
#include "telltale/errors.hpp"
#include "telltale/types.hpp"

namespace telltale {

enum class DiffOp { Equal, Insert, Delete, Modify };

struct IndexedRecord {
  uint32_t index;
  EventRecord record;
};

struct DiffChange {
  DiffOp op;
  uint32_t left_index;
  uint32_t right_index;
  IndexedRecord left_record;
  IndexedRecord right_record;
  std::vector<std::string> field_diffs;
};

struct DiffSummary {
  uint32_t left_count;
  uint32_t right_count;
  uint32_t equal_count;
  uint32_t insert_count;
  uint32_t delete_count;
  uint32_t modify_count;
};

class DiffEngine {
 public:
  DiffEngine();
  ~DiffEngine();

  DiffEngine(const DiffEngine&) = delete;
  DiffEngine& operator=(const DiffEngine&) = delete;

  Result load_left(const std::string& path);
  Result load_right(const std::string& path);
  Result compare();

  const std::vector<DiffChange>& changes() const { return changes_; }
  const DiffSummary& summary() const { return summary_; }

  std::string format_report(bool verbose) const;
  std::string format_change(const DiffChange& change) const;

  const std::vector<IndexedRecord>& left_records() const { return left_records_; }
  const std::vector<IndexedRecord>& right_records() const { return right_records_; }

 private:
  std::vector<IndexedRecord> left_records_;
  std::vector<IndexedRecord> right_records_;
  std::vector<DiffChange> changes_;
  DiffSummary summary_;

  Result load_file(const std::string& path, std::vector<IndexedRecord>& out);
  static bool records_equal(const EventRecord& a, const EventRecord& b);
  static std::vector<std::string> compute_field_diffs(const EventRecord& left,
                                                      const EventRecord& right);
  void compute_lcs_diff();
  static std::string record_signature(const EventRecord& rec);
  static std::string hex_payload_preview(const std::vector<uint8_t>& payload, size_t max);
};

std::string diff_engine_summary_line(const DiffEngine& engine);

}  // namespace telltale
