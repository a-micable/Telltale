#include "telltale/compaction_engine.hpp"

#include <algorithm>
#include <sstream>

#include "telltale/crc32.hpp"

namespace telltale {

CompactionOptions::CompactionOptions()
    : dedupe_consecutive_resets(true),
      validate_input_crc(true),
      preserve_source_order(true),
      recompute_crc(false) {}

CompactionEngine::CompactionEngine() { stats_ = {}; }

CompactionEngine::~CompactionEngine() = default;

void CompactionEngine::set_options(const CompactionOptions& options) { options_ = options; }

Result CompactionEngine::add_input(const std::string& path, uint32_t priority) {
  CompactionInput inp;
  inp.path = path;
  inp.priority = priority;
  inputs_.push_back(inp);
  return Result::success();
}

void CompactionEngine::clear_inputs() { inputs_.clear(); }

bool CompactionEngine::is_reset_event(uint16_t type_id) {
  return type_id == static_cast<uint16_t>(EventType::Reset);
}

bool CompactionEngine::resets_equivalent(const EventRecord& a, const EventRecord& b) {
  if (!is_reset_event(a.type_id) || !is_reset_event(b.type_id)) {
    return false;
  }
  return a.payload == b.payload;
}

Result CompactionEngine::load_all_records(std::vector<EventRecord>& merged) {
  merged.clear();
  stats_ = {};
  stats_.input_files = static_cast<uint32_t>(inputs_.size());

  std::vector<CompactionInput> sorted = inputs_;
  if (!options_.preserve_source_order) {
    std::stable_sort(
        sorted.begin(), sorted.end(),
        [](const CompactionInput& a, const CompactionInput& b) { return a.priority < b.priority; });
  }

  for (const auto& inp : sorted) {
    EventLogReader reader(inp.path);
    Result r = reader.open();
    if (!r.ok()) {
      return Result::fail(r.code, "Failed to open input " + inp.path + ": " + r.message);
    }
    FileHeader header;
    r = reader.read_header(header);
    if (!r.ok()) {
      return Result::fail(r.code, "Invalid header in " + inp.path + ": " + r.message);
    }
    r = reader.seek_to_records();
    if (!r.ok()) return r;

    uint32_t file_count = 0;
    while (!reader.eof()) {
      EventRecord rec;
      r = reader.read_next_record(rec);
      if (!r.ok()) {
        if (r.code == ErrorCode::UnexpectedEof) break;
        if (options_.validate_input_crc) {
          return Result::fail(r.code, "CRC error in " + inp.path + ": " + r.message);
        }
        ++stats_.records_skipped_invalid;
        continue;
      }
      if (options_.validate_input_crc) {
        uint32_t expected = Crc32::of_record(rec.type_id, rec.payload);
        if (expected != rec.crc32) {
          return Result::fail(
              ErrorCode::RecordCrcMismatch,
              "CRC mismatch in " + inp.path + " at record " + std::to_string(file_count));
        }
      }
      merged.push_back(rec);
      ++stats_.input_records_total;
      ++file_count;
    }
    reader.close();

    if (file_count != header.record_count) {
      return Result::fail(ErrorCode::VerifyFailed,
                          "Record count mismatch in " + inp.path +
                              ": header=" + std::to_string(header.record_count) +
                              " read=" + std::to_string(file_count));
    }
  }
  return Result::success();
}

void CompactionEngine::dedupe_consecutive_resets(std::vector<EventRecord>& records,
                                                 uint32_t& deduped_count) {
  deduped_count = 0;
  if (records.empty()) return;

  std::vector<EventRecord> out;
  out.reserve(records.size());
  out.push_back(records[0]);

  for (size_t i = 1; i < records.size(); ++i) {
    if (is_reset_event(records[i].type_id) && is_reset_event(out.back().type_id) &&
        resets_equivalent(records[i], out.back())) {
      ++deduped_count;
      continue;
    }
    out.push_back(records[i]);
  }
  records.swap(out);
}

Result CompactionEngine::write_output(const std::string& output_path,
                                      const std::vector<EventRecord>& records) const {
  EventLogWriter writer(output_path);
  Result r = writer.open();
  if (!r.ok()) return r;
  r = writer.write_header();
  if (!r.ok()) return r;

  for (const auto& rec : records) {
    r = writer.write_event(rec.type_id, rec.payload);
    if (!r.ok()) {
      return Result::fail(r.code, "Failed writing record to output: " + r.message);
    }
    if (options_.recompute_crc) {
      uint32_t expected = Crc32::of_record(rec.type_id, rec.payload);
      if (expected != rec.crc32) {
        // Writer always recomputes CRC; verify consistency expectation
      }
    }
  }

  r = writer.finalize();
  if (!r.ok()) return r;

  if (writer.record_count() != records.size()) {
    return Result::fail(ErrorCode::FinalizeError, "Output record count mismatch after finalize");
  }
  return Result::success();
}

Result CompactionEngine::compact(const std::string& output_path) {
  if (inputs_.empty()) {
    return Result::fail(ErrorCode::InvalidPayload, "No input files specified for compaction");
  }
  if (inputs_.size() == 1 && !options_.dedupe_consecutive_resets) {
    // Still copy through writer to normalize header
  }

  std::vector<EventRecord> merged;
  Result r = load_all_records(merged);
  if (!r.ok()) return r;

  if (options_.dedupe_consecutive_resets) {
    uint32_t deduped = 0;
    dedupe_consecutive_resets(merged, deduped);
    stats_.resets_deduped = deduped;
  }

  stats_.output_records = static_cast<uint32_t>(merged.size());
  r = write_output(output_path, merged);
  return r;
}

static Result compact_check_record(const EventRecord& rec) {
  if (rec.payload.size() > MAX_PAYLOAD_SIZE) {
    return Result::fail(ErrorCode::PayloadTooLarge, "payload too large");
  }
  if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32) {
    return Result::fail(ErrorCode::RecordCrcMismatch, "record crc mismatch");
  }
  return Result::success();
}

static Result verify_all_records(const std::vector<EventRecord>& records) {
  for (const auto& rec : records) {
    Result r = compact_check_record(rec);
    if (!r.ok()) return r;
  }
  return Result::success();
}

Result compaction_verify_output(const std::string& path, const CompactionStats& expected) {
  EventLogReader reader(path);
  Result r = reader.open();
  if (!r.ok()) return r;
  FileHeader h;
  r = reader.read_header(h);
  if (!r.ok()) return r;
  if (h.record_count != expected.output_records)
    return Result::fail(ErrorCode::VerifyFailed, "count");
  r = reader.seek_to_records();
  if (!r.ok()) return r;
  std::vector<EventRecord> recs;
  while (!reader.eof()) {
    EventRecord rec;
    r = reader.read_next_record(rec);
    if (!r.ok()) {
      if (r.code == ErrorCode::UnexpectedEof) break;
      return r;
    }
    recs.push_back(rec);
  }
  reader.close();
  return verify_all_records(recs);
}

}  // namespace telltale
