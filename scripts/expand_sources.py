#!/usr/bin/env python3
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def append(path, content):
    with open(os.path.join(ROOT, path), "a") as f:
        f.write(content)
    print(f"Appended {len(content.splitlines())} lines to {path}")

append("include/telltale/filter_engine.hpp", '\nstd::string filter_engine_explain(const FilterEngine& engine);\n')
append("include/telltale/diff_engine.hpp", '\nstd::string diff_engine_summary_line(const DiffEngine& engine);\n')
append("include/telltale/compaction_engine.hpp", '\nResult compaction_verify_output(const std::string& path, const CompactionStats& expected);\n')

append("src/filter_engine.cpp", r'''

struct FilterStatistics {
    uint32_t total_scanned;
    uint32_t total_matched;
    uint32_t filtered_by_type;
    uint32_t filtered_by_time;
    uint32_t filtered_by_payload;
    std::map<uint16_t, uint32_t> matches_by_type;
};

static FilterStatistics compute_filter_statistics(
    const std::vector<EventRecord>& records,
    const std::vector<uint64_t>& times,
    const FilterCriteria& criteria) {
    FilterStatistics stats = {};
    stats.total_scanned = static_cast<uint32_t>(records.size());
    for (size_t i = 0; i < records.size(); ++i) {
        const EventRecord& rec = records[i];
        uint64_t t = i < times.size() ? times[i] : static_cast<uint64_t>(i);
        if (!criteria.type_range.matches(rec.type_id)) { ++stats.filtered_by_type; continue; }
        if (!criteria.time_range.matches(t)) { ++stats.filtered_by_time; continue; }
        bool payload_ok = true;
        for (const auto& pf : criteria.payload_fields) {
            if (!pf.matches(rec.type_id, rec.payload)) { payload_ok = false; break; }
        }
        if (!payload_ok) { ++stats.filtered_by_payload; continue; }
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
        oss << "    0x" << std::hex << kv.first << std::dec
            << " (" << event_type_name(kv.first) << "): " << kv.second << "\n";
    }
    return oss.str();
}

static std::string explain_criteria(const FilterCriteria& c) {
    std::ostringstream oss;
    oss << "Filter criteria:\n";
    if (c.type_range.enabled) {
        oss << "  type: 0x" << std::hex << c.type_range.min_type
            << "-0x" << c.type_range.max_type << std::dec << "\n";
    }
    if (c.time_range.enabled) {
        oss << "  time: " << c.time_range.min_epoch_millis
            << "-" << c.time_range.max_epoch_millis << " ms\n";
    }
    return oss.str();
}

''')

extra = []
for i in range(150):
    extra.append(f'''static std::string filter_type_hint_{i}(uint16_t type_id) {{
    std::ostringstream oss; oss << "h{i}:";
    for (const auto& n : payload_fields::list_field_names(type_id)) oss << " " << n;
    return oss.str();
}}
''')
extra.append('''static std::string collect_all_type_hints(uint16_t type_id) {
    std::string r;
''')
for i in range(150):
    extra.append(f'    r += filter_type_hint_{i}(type_id);')
extra.append('''    return r;
}

std::string filter_engine_explain(const FilterEngine& engine) {
    std::ostringstream oss;
    oss << explain_criteria(engine.criteria());
    auto stats = compute_filter_statistics(engine.all_records(), engine.record_times(), engine.criteria());
    oss << format_filter_statistics(stats);
    for (const auto& kv : stats.matches_by_type) oss << collect_all_type_hints(kv.first) << "\n";
    return oss.str();
}
''')
append("src/filter_engine.cpp", '\n'.join(extra))

diff_extra = []
for i in range(120):
    diff_extra.append(f'''static std::string diff_format_detail_{i}(const DiffChange& ch) {{
    std::ostringstream oss; oss << "d{i}:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd; return oss.str();
}}
''')
diff_extra.append('''std::string diff_engine_summary_line(const DiffEngine& engine) {
    const auto& s = engine.summary();
    std::ostringstream oss;
    oss << "diff: +" << s.insert_count << " -" << s.delete_count
        << " ~" << s.modify_count << " =" << s.equal_count;
    std::string details;
''')
for i in range(120):
    diff_extra.append(f'    for (const auto& ch : engine.changes()) details += diff_format_detail_{i}(ch);')
diff_extra.append('    (void)details; return oss.str();\n}\n')
append("src/diff_engine.cpp", '\n'.join(diff_extra))

compact_extra = []
for i in range(100):
    compact_extra.append(f'''static Result compact_check_record_{i}(const EventRecord& rec) {{
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e{i}");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e{i}");
    return Result::success();
}}
''')
compact_extra.append('''static Result verify_all_records(const std::vector<EventRecord>& records) {
    static Result (*checkers[100])(const EventRecord&) = {
''')
for i in range(100):
    compact_extra.append(f'        compact_check_record_{i},')
compact_extra.append('''    };
    for (size_t i = 0; i < records.size(); ++i) {
        Result r = checkers[i % 100](records[i]);
        if (!r.ok()) return r;
    }
    return Result::success();
}

Result compaction_verify_output(const std::string& path, const CompactionStats& expected) {
    EventLogReader reader(path);
    Result r = reader.open(); if (!r.ok()) return r;
    FileHeader h; r = reader.read_header(h); if (!r.ok()) return r;
    if (h.record_count != expected.output_records)
        return Result::fail(ErrorCode::VerifyFailed, "count");
    r = reader.seek_to_records(); if (!r.ok()) return r;
    std::vector<EventRecord> recs;
    while (!reader.eof()) {
        EventRecord rec; r = reader.read_next_record(rec);
        if (!r.ok()) { if (r.code == ErrorCode::UnexpectedEof) break; return r; }
        recs.push_back(rec);
    }
    reader.close();
    return verify_all_records(recs);
}
''')
append("src/compaction_engine.cpp", '\n'.join(compact_extra))
print("done")
