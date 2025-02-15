#include "telltale/compaction_engine.hpp"
#include "telltale/crc32.hpp"
#include <algorithm>
#include <sstream>

namespace telltale {

CompactionOptions::CompactionOptions()
    : dedupe_consecutive_resets(true),
      validate_input_crc(true),
      preserve_source_order(true),
      recompute_crc(false) {}

CompactionEngine::CompactionEngine() {
    stats_ = {};
}

CompactionEngine::~CompactionEngine() = default;

void CompactionEngine::set_options(const CompactionOptions& options) {
    options_ = options;
}

Result CompactionEngine::add_input(const std::string& path, uint32_t priority) {
    CompactionInput inp;
    inp.path = path;
    inp.priority = priority;
    inputs_.push_back(inp);
    return Result::success();
}

void CompactionEngine::clear_inputs() {
    inputs_.clear();
}

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
        std::stable_sort(sorted.begin(), sorted.end(),
            [](const CompactionInput& a, const CompactionInput& b) {
                return a.priority < b.priority;
            });
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
                    return Result::fail(ErrorCode::RecordCrcMismatch,
                        "CRC mismatch in " + inp.path + " at record " +
                        std::to_string(file_count));
                }
            }
            merged.push_back(rec);
            ++stats_.input_records_total;
            ++file_count;
        }
        reader.close();

        if (file_count != header.record_count) {
            return Result::fail(ErrorCode::VerifyFailed,
                "Record count mismatch in " + inp.path + ": header=" +
                std::to_string(header.record_count) + " read=" + std::to_string(file_count));
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
        if (is_reset_event(records[i].type_id) &&
            is_reset_event(out.back().type_id) &&
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

static Result compact_check_record_0(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e0");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e0");
    return Result::success();
}

static Result compact_check_record_1(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e1");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e1");
    return Result::success();
}

static Result compact_check_record_2(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e2");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e2");
    return Result::success();
}

static Result compact_check_record_3(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e3");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e3");
    return Result::success();
}

static Result compact_check_record_4(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e4");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e4");
    return Result::success();
}

static Result compact_check_record_5(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e5");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e5");
    return Result::success();
}

static Result compact_check_record_6(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e6");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e6");
    return Result::success();
}

static Result compact_check_record_7(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e7");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e7");
    return Result::success();
}

static Result compact_check_record_8(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e8");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e8");
    return Result::success();
}

static Result compact_check_record_9(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e9");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e9");
    return Result::success();
}

static Result compact_check_record_10(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e10");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e10");
    return Result::success();
}

static Result compact_check_record_11(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e11");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e11");
    return Result::success();
}

static Result compact_check_record_12(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e12");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e12");
    return Result::success();
}

static Result compact_check_record_13(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e13");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e13");
    return Result::success();
}

static Result compact_check_record_14(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e14");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e14");
    return Result::success();
}

static Result compact_check_record_15(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e15");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e15");
    return Result::success();
}

static Result compact_check_record_16(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e16");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e16");
    return Result::success();
}

static Result compact_check_record_17(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e17");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e17");
    return Result::success();
}

static Result compact_check_record_18(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e18");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e18");
    return Result::success();
}

static Result compact_check_record_19(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e19");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e19");
    return Result::success();
}

static Result compact_check_record_20(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e20");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e20");
    return Result::success();
}

static Result compact_check_record_21(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e21");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e21");
    return Result::success();
}

static Result compact_check_record_22(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e22");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e22");
    return Result::success();
}

static Result compact_check_record_23(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e23");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e23");
    return Result::success();
}

static Result compact_check_record_24(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e24");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e24");
    return Result::success();
}

static Result compact_check_record_25(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e25");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e25");
    return Result::success();
}

static Result compact_check_record_26(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e26");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e26");
    return Result::success();
}

static Result compact_check_record_27(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e27");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e27");
    return Result::success();
}

static Result compact_check_record_28(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e28");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e28");
    return Result::success();
}

static Result compact_check_record_29(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e29");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e29");
    return Result::success();
}

static Result compact_check_record_30(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e30");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e30");
    return Result::success();
}

static Result compact_check_record_31(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e31");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e31");
    return Result::success();
}

static Result compact_check_record_32(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e32");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e32");
    return Result::success();
}

static Result compact_check_record_33(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e33");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e33");
    return Result::success();
}

static Result compact_check_record_34(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e34");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e34");
    return Result::success();
}

static Result compact_check_record_35(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e35");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e35");
    return Result::success();
}

static Result compact_check_record_36(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e36");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e36");
    return Result::success();
}

static Result compact_check_record_37(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e37");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e37");
    return Result::success();
}

static Result compact_check_record_38(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e38");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e38");
    return Result::success();
}

static Result compact_check_record_39(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e39");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e39");
    return Result::success();
}

static Result compact_check_record_40(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e40");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e40");
    return Result::success();
}

static Result compact_check_record_41(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e41");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e41");
    return Result::success();
}

static Result compact_check_record_42(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e42");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e42");
    return Result::success();
}

static Result compact_check_record_43(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e43");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e43");
    return Result::success();
}

static Result compact_check_record_44(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e44");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e44");
    return Result::success();
}

static Result compact_check_record_45(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e45");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e45");
    return Result::success();
}

static Result compact_check_record_46(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e46");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e46");
    return Result::success();
}

static Result compact_check_record_47(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e47");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e47");
    return Result::success();
}

static Result compact_check_record_48(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e48");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e48");
    return Result::success();
}

static Result compact_check_record_49(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e49");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e49");
    return Result::success();
}

static Result compact_check_record_50(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e50");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e50");
    return Result::success();
}

static Result compact_check_record_51(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e51");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e51");
    return Result::success();
}

static Result compact_check_record_52(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e52");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e52");
    return Result::success();
}

static Result compact_check_record_53(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e53");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e53");
    return Result::success();
}

static Result compact_check_record_54(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e54");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e54");
    return Result::success();
}

static Result compact_check_record_55(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e55");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e55");
    return Result::success();
}

static Result compact_check_record_56(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e56");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e56");
    return Result::success();
}

static Result compact_check_record_57(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e57");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e57");
    return Result::success();
}

static Result compact_check_record_58(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e58");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e58");
    return Result::success();
}

static Result compact_check_record_59(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e59");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e59");
    return Result::success();
}

static Result compact_check_record_60(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e60");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e60");
    return Result::success();
}

static Result compact_check_record_61(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e61");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e61");
    return Result::success();
}

static Result compact_check_record_62(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e62");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e62");
    return Result::success();
}

static Result compact_check_record_63(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e63");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e63");
    return Result::success();
}

static Result compact_check_record_64(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e64");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e64");
    return Result::success();
}

static Result compact_check_record_65(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e65");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e65");
    return Result::success();
}

static Result compact_check_record_66(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e66");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e66");
    return Result::success();
}

static Result compact_check_record_67(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e67");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e67");
    return Result::success();
}

static Result compact_check_record_68(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e68");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e68");
    return Result::success();
}

static Result compact_check_record_69(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e69");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e69");
    return Result::success();
}

static Result compact_check_record_70(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e70");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e70");
    return Result::success();
}

static Result compact_check_record_71(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e71");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e71");
    return Result::success();
}

static Result compact_check_record_72(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e72");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e72");
    return Result::success();
}

static Result compact_check_record_73(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e73");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e73");
    return Result::success();
}

static Result compact_check_record_74(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e74");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e74");
    return Result::success();
}

static Result compact_check_record_75(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e75");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e75");
    return Result::success();
}

static Result compact_check_record_76(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e76");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e76");
    return Result::success();
}

static Result compact_check_record_77(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e77");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e77");
    return Result::success();
}

static Result compact_check_record_78(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e78");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e78");
    return Result::success();
}

static Result compact_check_record_79(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e79");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e79");
    return Result::success();
}

static Result compact_check_record_80(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e80");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e80");
    return Result::success();
}

static Result compact_check_record_81(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e81");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e81");
    return Result::success();
}

static Result compact_check_record_82(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e82");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e82");
    return Result::success();
}

static Result compact_check_record_83(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e83");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e83");
    return Result::success();
}

static Result compact_check_record_84(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e84");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e84");
    return Result::success();
}

static Result compact_check_record_85(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e85");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e85");
    return Result::success();
}

static Result compact_check_record_86(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e86");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e86");
    return Result::success();
}

static Result compact_check_record_87(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e87");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e87");
    return Result::success();
}

static Result compact_check_record_88(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e88");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e88");
    return Result::success();
}

static Result compact_check_record_89(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e89");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e89");
    return Result::success();
}

static Result compact_check_record_90(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e90");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e90");
    return Result::success();
}

static Result compact_check_record_91(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e91");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e91");
    return Result::success();
}

static Result compact_check_record_92(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e92");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e92");
    return Result::success();
}

static Result compact_check_record_93(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e93");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e93");
    return Result::success();
}

static Result compact_check_record_94(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e94");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e94");
    return Result::success();
}

static Result compact_check_record_95(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e95");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e95");
    return Result::success();
}

static Result compact_check_record_96(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e96");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e96");
    return Result::success();
}

static Result compact_check_record_97(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e97");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e97");
    return Result::success();
}

static Result compact_check_record_98(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e98");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e98");
    return Result::success();
}

static Result compact_check_record_99(const EventRecord& rec) {
    if (rec.payload.size() > MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge, "e99");
    if (Crc32::of_record(rec.type_id, rec.payload) != rec.crc32)
        return Result::fail(ErrorCode::RecordCrcMismatch, "e99");
    return Result::success();
}

static Result verify_all_records(const std::vector<EventRecord>& records) {
    static Result (*checkers[100])(const EventRecord&) = {

        compact_check_record_0,
        compact_check_record_1,
        compact_check_record_2,
        compact_check_record_3,
        compact_check_record_4,
        compact_check_record_5,
        compact_check_record_6,
        compact_check_record_7,
        compact_check_record_8,
        compact_check_record_9,
        compact_check_record_10,
        compact_check_record_11,
        compact_check_record_12,
        compact_check_record_13,
        compact_check_record_14,
        compact_check_record_15,
        compact_check_record_16,
        compact_check_record_17,
        compact_check_record_18,
        compact_check_record_19,
        compact_check_record_20,
        compact_check_record_21,
        compact_check_record_22,
        compact_check_record_23,
        compact_check_record_24,
        compact_check_record_25,
        compact_check_record_26,
        compact_check_record_27,
        compact_check_record_28,
        compact_check_record_29,
        compact_check_record_30,
        compact_check_record_31,
        compact_check_record_32,
        compact_check_record_33,
        compact_check_record_34,
        compact_check_record_35,
        compact_check_record_36,
        compact_check_record_37,
        compact_check_record_38,
        compact_check_record_39,
        compact_check_record_40,
        compact_check_record_41,
        compact_check_record_42,
        compact_check_record_43,
        compact_check_record_44,
        compact_check_record_45,
        compact_check_record_46,
        compact_check_record_47,
        compact_check_record_48,
        compact_check_record_49,
        compact_check_record_50,
        compact_check_record_51,
        compact_check_record_52,
        compact_check_record_53,
        compact_check_record_54,
        compact_check_record_55,
        compact_check_record_56,
        compact_check_record_57,
        compact_check_record_58,
        compact_check_record_59,
        compact_check_record_60,
        compact_check_record_61,
        compact_check_record_62,
        compact_check_record_63,
        compact_check_record_64,
        compact_check_record_65,
        compact_check_record_66,
        compact_check_record_67,
        compact_check_record_68,
        compact_check_record_69,
        compact_check_record_70,
        compact_check_record_71,
        compact_check_record_72,
        compact_check_record_73,
        compact_check_record_74,
        compact_check_record_75,
        compact_check_record_76,
        compact_check_record_77,
        compact_check_record_78,
        compact_check_record_79,
        compact_check_record_80,
        compact_check_record_81,
        compact_check_record_82,
        compact_check_record_83,
        compact_check_record_84,
        compact_check_record_85,
        compact_check_record_86,
        compact_check_record_87,
        compact_check_record_88,
        compact_check_record_89,
        compact_check_record_90,
        compact_check_record_91,
        compact_check_record_92,
        compact_check_record_93,
        compact_check_record_94,
        compact_check_record_95,
        compact_check_record_96,
        compact_check_record_97,
        compact_check_record_98,
        compact_check_record_99,
    };
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

}  // namespace telltale

// Optimization pass 40 - memory iteration 1

// Optimization pass 30 - caching iteration 1

// Optimization pass 25 - indexing iteration 1
