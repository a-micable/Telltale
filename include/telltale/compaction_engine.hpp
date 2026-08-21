#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "telltale/binary_io.hpp"
#include "telltale/errors.hpp"
#include "telltale/types.hpp"

namespace telltale {

struct CompactionInput {
  std::string path;
  uint32_t priority;
};

struct CompactionOptions {
  bool dedupe_consecutive_resets;
  bool validate_input_crc;
  bool preserve_source_order;
  bool recompute_crc;

  CompactionOptions();
};

struct CompactionStats {
  uint32_t input_files;
  uint32_t input_records_total;
  uint32_t output_records;
  uint32_t resets_deduped;
  uint32_t records_skipped_invalid;
};

class CompactionEngine {
 public:
  CompactionEngine();
  ~CompactionEngine();

  CompactionEngine(const CompactionEngine&) = delete;
  CompactionEngine& operator=(const CompactionEngine&) = delete;

  void set_options(const CompactionOptions& options);
  const CompactionOptions& options() const { return options_; }

  Result add_input(const std::string& path, uint32_t priority = 0);
  void clear_inputs();

  Result compact(const std::string& output_path);
  const CompactionStats& stats() const { return stats_; }

  static bool is_reset_event(uint16_t type_id);
  static bool resets_equivalent(const EventRecord& a, const EventRecord& b);

 private:
  std::vector<CompactionInput> inputs_;
  CompactionOptions options_;
  CompactionStats stats_;

  Result load_all_records(std::vector<EventRecord>& merged);
  static void dedupe_consecutive_resets(std::vector<EventRecord>& records, uint32_t& deduped_count);
  Result write_output(const std::string& output_path,
                      const std::vector<EventRecord>& records) const;
};

Result compaction_verify_output(const std::string& path, const CompactionStats& expected);

}  // namespace telltale
