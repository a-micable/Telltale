#include "telltale/diff_engine.hpp"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>

#include "telltale/filter_engine.hpp"
#include "telltale/schema_update.hpp"

namespace telltale {

DiffEngine::DiffEngine() { summary_ = {}; }

DiffEngine::~DiffEngine() = default;

Result DiffEngine::load_file(const std::string& path, std::vector<IndexedRecord>& out) {
  out.clear();
  EventLogReader reader(path);
  Result r = reader.open();
  if (!r.ok()) return r;
  FileHeader header;
  r = reader.read_header(header);
  if (!r.ok()) return r;
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
    IndexedRecord ir;
    ir.index = idx++;
    ir.record = rec;
    out.push_back(ir);
  }
  reader.close();
  return Result::success();
}

Result DiffEngine::load_left(const std::string& path) { return load_file(path, left_records_); }

Result DiffEngine::load_right(const std::string& path) { return load_file(path, right_records_); }

bool DiffEngine::records_equal(const EventRecord& a, const EventRecord& b) {
  return a.type_id == b.type_id && a.payload == b.payload && a.crc32 == b.crc32;
}

std::string DiffEngine::hex_payload_preview(const std::vector<uint8_t>& payload, size_t max) {
  std::ostringstream oss;
  size_t n = std::min(payload.size(), max);
  for (size_t i = 0; i < n; ++i) {
    if (i > 0) oss << ' ';
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(payload[i]);
  }
  if (payload.size() > max) oss << " ...";
  return oss.str();
}

std::string DiffEngine::record_signature(const EventRecord& rec) {
  std::ostringstream oss;
  oss << std::hex << rec.type_id << ":" << std::dec << rec.payload.size() << ":" << std::hex
      << rec.crc32;
  return oss.str();
}

std::vector<std::string> DiffEngine::compute_field_diffs(const EventRecord& left,
                                                         const EventRecord& right) {
  std::vector<std::string> diffs;
  if (left.type_id != right.type_id) {
    diffs.push_back("type_id: 0x" + std::to_string(left.type_id) + " -> 0x" +
                    std::to_string(right.type_id) + " (" + event_type_name(left.type_id) + " -> " +
                    event_type_name(right.type_id) + ")");
    return diffs;
  }

  payload_fields::DecodedFields lf;
  payload_fields::DecodedFields rf;
  payload_fields::decode_all_fields(left.type_id, left.payload, lf);
  payload_fields::decode_all_fields(right.type_id, right.payload, rf);

  for (const auto& kv : lf.string_fields) {
    auto it = rf.string_fields.find(kv.first);
    if (it == rf.string_fields.end()) {
      diffs.push_back(kv.first + ": removed (was '" + kv.second + "')");
    } else if (it->second != kv.second) {
      diffs.push_back(kv.first + ": '" + kv.second + "' -> '" + it->second + "'");
    }
  }
  for (const auto& kv : rf.string_fields) {
    if (lf.string_fields.find(kv.first) == lf.string_fields.end()) {
      diffs.push_back(kv.first + ": added ('" + kv.second + "')");
    }
  }
  for (const auto& kv : lf.int_fields) {
    auto it = rf.int_fields.find(kv.first);
    if (it == rf.int_fields.end()) {
      diffs.push_back(kv.first + ": removed (was " + std::to_string(kv.second) + ")");
    } else if (it->second != kv.second) {
      diffs.push_back(kv.first + ": " + std::to_string(kv.second) + " -> " +
                      std::to_string(it->second));
    }
  }
  for (const auto& kv : rf.int_fields) {
    if (lf.int_fields.find(kv.first) == lf.int_fields.end()) {
      diffs.push_back(kv.first + ": added (" + std::to_string(kv.second) + ")");
    }
  }
  for (const auto& kv : lf.uint_fields) {
    auto it = rf.uint_fields.find(kv.first);
    if (it == rf.uint_fields.end()) {
      diffs.push_back(kv.first + ": removed (was " + std::to_string(kv.second) + ")");
    } else if (it->second != kv.second) {
      diffs.push_back(kv.first + ": " + std::to_string(kv.second) + " -> " +
                      std::to_string(it->second));
    }
  }
  for (const auto& kv : rf.uint_fields) {
    if (lf.uint_fields.find(kv.first) == lf.uint_fields.end()) {
      diffs.push_back(kv.first + ": added (" + std::to_string(kv.second) + ")");
    }
  }

  if (left.payload != right.payload && diffs.empty()) {
    diffs.push_back("raw payload differs");
    diffs.push_back("  left:  [" + hex_payload_preview(left.payload, 32) + "]");
    diffs.push_back("  right: [" + hex_payload_preview(right.payload, 32) + "]");
  }
  if (left.crc32 != right.crc32) {
    std::ostringstream oss;
    oss << "crc32: 0x" << std::hex << left.crc32 << " -> 0x" << right.crc32 << std::dec;
    diffs.push_back(oss.str());
  }
  return diffs;
}

void DiffEngine::compute_lcs_diff() {
  changes_.clear();
  const size_t n = left_records_.size();
  const size_t m = right_records_.size();

  std::vector<std::vector<size_t>> dp(n + 1, std::vector<size_t>(m + 1, 0));
  for (size_t i = 1; i <= n; ++i) {
    for (size_t j = 1; j <= m; ++j) {
      if (records_equal(left_records_[i - 1].record, right_records_[j - 1].record)) {
        dp[i][j] = dp[i - 1][j - 1] + 1;
      } else {
        dp[i][j] = std::max(dp[i - 1][j], dp[i][j - 1]);
      }
    }
  }

  size_t i = n;
  size_t j = m;
  std::vector<DiffChange> reversed;
  while (i > 0 || j > 0) {
    if (i > 0 && j > 0 &&
        records_equal(left_records_[i - 1].record, right_records_[j - 1].record)) {
      DiffChange ch;
      ch.op = DiffOp::Equal;
      ch.left_index = left_records_[i - 1].index;
      ch.right_index = right_records_[j - 1].index;
      ch.left_record = left_records_[i - 1];
      ch.right_record = right_records_[j - 1];
      reversed.push_back(ch);
      --i;
      --j;
    } else if (j > 0 && (i == 0 || dp[i][j - 1] >= dp[i - 1][j])) {
      DiffChange ch;
      ch.op = DiffOp::Insert;
      ch.left_index = UINT32_MAX;
      ch.right_index = right_records_[j - 1].index;
      ch.right_record = right_records_[j - 1];
      reversed.push_back(ch);
      --j;
    } else {
      DiffChange ch;
      ch.op = DiffOp::Delete;
      ch.left_index = left_records_[i - 1].index;
      ch.right_index = UINT32_MAX;
      ch.left_record = left_records_[i - 1];
      reversed.push_back(ch);
      --i;
    }
  }
  std::reverse(reversed.begin(), reversed.end());

  for (size_t k = 0; k < reversed.size(); ++k) {
    DiffChange ch = reversed[k];
    if (ch.op == DiffOp::Equal) {
      changes_.push_back(ch);
      continue;
    }
    if (ch.op == DiffOp::Delete && k + 1 < reversed.size() &&
        reversed[k + 1].op == DiffOp::Insert &&
        reversed[k + 1].left_record.record.type_id == ch.left_record.record.type_id) {
      DiffChange mod;
      mod.op = DiffOp::Modify;
      mod.left_index = ch.left_index;
      mod.right_index = reversed[k + 1].right_index;
      mod.left_record = ch.left_record;
      mod.right_record = reversed[k + 1].right_record;
      mod.field_diffs = compute_field_diffs(mod.left_record.record, mod.right_record.record);
      changes_.push_back(mod);
      ++k;
      continue;
    }
    changes_.push_back(ch);
  }

  summary_.left_count = static_cast<uint32_t>(n);
  summary_.right_count = static_cast<uint32_t>(m);
  summary_.equal_count = 0;
  summary_.insert_count = 0;
  summary_.delete_count = 0;
  summary_.modify_count = 0;
  for (const auto& ch : changes_) {
    switch (ch.op) {
      case DiffOp::Equal:
        ++summary_.equal_count;
        break;
      case DiffOp::Insert:
        ++summary_.insert_count;
        break;
      case DiffOp::Delete:
        ++summary_.delete_count;
        break;
      case DiffOp::Modify:
        ++summary_.modify_count;
        break;
    }
  }
}

Result DiffEngine::compare() {
  compute_lcs_diff();
  return Result::success();
}

std::string DiffEngine::format_change(const DiffChange& change) const {
  std::ostringstream oss;
  switch (change.op) {
    case DiffOp::Equal:
      oss << "  = record L" << change.left_index << "/R" << change.right_index << " type=0x"
          << std::hex << change.left_record.record.type_id << std::dec << " ("
          << event_type_name(change.left_record.record.type_id) << ")";
      break;
    case DiffOp::Insert:
      oss << "  + INSERT at right[" << change.right_index << "] type=0x" << std::hex
          << change.right_record.record.type_id << std::dec << " ("
          << event_type_name(change.right_record.record.type_id) << ")"
          << " payload=" << change.right_record.record.payload.size() << " bytes";
      break;
    case DiffOp::Delete:
      oss << "  - DELETE at left[" << change.left_index << "] type=0x" << std::hex
          << change.left_record.record.type_id << std::dec << " ("
          << event_type_name(change.left_record.record.type_id) << ")"
          << " payload=" << change.left_record.record.payload.size() << " bytes";
      break;
    case DiffOp::Modify:
      oss << "  ~ MODIFY left[" << change.left_index << "] -> right[" << change.right_index
          << "] type=0x" << std::hex << change.left_record.record.type_id << std::dec << " ("
          << event_type_name(change.left_record.record.type_id) << ")";
      for (const auto& fd : change.field_diffs) {
        oss << "\n      " << fd;
      }
      break;
  }
  return oss.str();
}

std::string DiffEngine::format_report(bool verbose) const {
  std::ostringstream oss;
  oss << "Diff Report" << std::endl;
  oss << "===========" << std::endl;
  oss << "Left records:  " << summary_.left_count << std::endl;
  oss << "Right records: " << summary_.right_count << std::endl;
  oss << "Equal:   " << summary_.equal_count << std::endl;
  oss << "Insert:  " << summary_.insert_count << std::endl;
  oss << "Delete:  " << summary_.delete_count << std::endl;
  oss << "Modify:  " << summary_.modify_count << std::endl;
  oss << std::endl;
  oss << "Changes:" << std::endl;
  for (const auto& ch : changes_) {
    if (!verbose && ch.op == DiffOp::Equal) continue;
    oss << format_change(ch) << std::endl;
  }
  return oss.str();
}

std::string diff_engine_summary_line(const DiffEngine& engine) {
  const DiffSummary& s = engine.summary();
  std::ostringstream oss;
  oss << "equal=" << s.equal_count << " insert=" << s.insert_count << " delete=" << s.delete_count
      << " modify=" << s.modify_count;
  return oss.str();
}

}  // namespace telltale
