#include "telltale/diff_engine.hpp"
#include "telltale/filter_engine.hpp"
#include "telltale/schema_update.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cstring>

namespace telltale {

DiffEngine::DiffEngine() {
    summary_ = {};
}

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

Result DiffEngine::load_left(const std::string& path) {
    return load_file(path, left_records_);
}

Result DiffEngine::load_right(const std::string& path) {
    return load_file(path, right_records_);
}

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
    oss << std::hex << rec.type_id << ":" << std::dec << rec.payload.size() << ":" << std::hex << rec.crc32;
    return oss.str();
}

std::vector<std::string> DiffEngine::compute_field_diffs(const EventRecord& left,
                                                          const EventRecord& right) {
    std::vector<std::string> diffs;
    if (left.type_id != right.type_id) {
        diffs.push_back("type_id: 0x" + std::to_string(left.type_id) + " -> 0x" +
                        std::to_string(right.type_id) + " (" +
                        event_type_name(left.type_id) + " -> " +
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
            case DiffOp::Equal: ++summary_.equal_count; break;
            case DiffOp::Insert: ++summary_.insert_count; break;
            case DiffOp::Delete: ++summary_.delete_count; break;
            case DiffOp::Modify: ++summary_.modify_count; break;
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
            oss << "  = record L" << change.left_index << "/R" << change.right_index
                << " type=0x" << std::hex << change.left_record.record.type_id << std::dec
                << " (" << event_type_name(change.left_record.record.type_id) << ")";
            break;
        case DiffOp::Insert:
            oss << "  + INSERT at right[" << change.right_index << "] type=0x"
                << std::hex << change.right_record.record.type_id << std::dec
                << " (" << event_type_name(change.right_record.record.type_id) << ")"
                << " payload=" << change.right_record.record.payload.size() << " bytes";
            break;
        case DiffOp::Delete:
            oss << "  - DELETE at left[" << change.left_index << "] type=0x"
                << std::hex << change.left_record.record.type_id << std::dec
                << " (" << event_type_name(change.left_record.record.type_id) << ")"
                << " payload=" << change.left_record.record.payload.size() << " bytes";
            break;
        case DiffOp::Modify:
            oss << "  ~ MODIFY left[" << change.left_index << "] -> right["
                << change.right_index << "] type=0x" << std::hex
                << change.left_record.record.type_id << std::dec
                << " (" << event_type_name(change.left_record.record.type_id) << ")";
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

static std::string diff_format_detail_0(const DiffChange& ch) {
    std::ostringstream oss; oss << "d0:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_1(const DiffChange& ch) {
    std::ostringstream oss; oss << "d1:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_2(const DiffChange& ch) {
    std::ostringstream oss; oss << "d2:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_3(const DiffChange& ch) {
    std::ostringstream oss; oss << "d3:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_4(const DiffChange& ch) {
    std::ostringstream oss; oss << "d4:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_5(const DiffChange& ch) {
    std::ostringstream oss; oss << "d5:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_6(const DiffChange& ch) {
    std::ostringstream oss; oss << "d6:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_7(const DiffChange& ch) {
    std::ostringstream oss; oss << "d7:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_8(const DiffChange& ch) {
    std::ostringstream oss; oss << "d8:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_9(const DiffChange& ch) {
    std::ostringstream oss; oss << "d9:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_10(const DiffChange& ch) {
    std::ostringstream oss; oss << "d10:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_11(const DiffChange& ch) {
    std::ostringstream oss; oss << "d11:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_12(const DiffChange& ch) {
    std::ostringstream oss; oss << "d12:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_13(const DiffChange& ch) {
    std::ostringstream oss; oss << "d13:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_14(const DiffChange& ch) {
    std::ostringstream oss; oss << "d14:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_15(const DiffChange& ch) {
    std::ostringstream oss; oss << "d15:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_16(const DiffChange& ch) {
    std::ostringstream oss; oss << "d16:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_17(const DiffChange& ch) {
    std::ostringstream oss; oss << "d17:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_18(const DiffChange& ch) {
    std::ostringstream oss; oss << "d18:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_19(const DiffChange& ch) {
    std::ostringstream oss; oss << "d19:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_20(const DiffChange& ch) {
    std::ostringstream oss; oss << "d20:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_21(const DiffChange& ch) {
    std::ostringstream oss; oss << "d21:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_22(const DiffChange& ch) {
    std::ostringstream oss; oss << "d22:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_23(const DiffChange& ch) {
    std::ostringstream oss; oss << "d23:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_24(const DiffChange& ch) {
    std::ostringstream oss; oss << "d24:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_25(const DiffChange& ch) {
    std::ostringstream oss; oss << "d25:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_26(const DiffChange& ch) {
    std::ostringstream oss; oss << "d26:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_27(const DiffChange& ch) {
    std::ostringstream oss; oss << "d27:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_28(const DiffChange& ch) {
    std::ostringstream oss; oss << "d28:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_29(const DiffChange& ch) {
    std::ostringstream oss; oss << "d29:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_30(const DiffChange& ch) {
    std::ostringstream oss; oss << "d30:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_31(const DiffChange& ch) {
    std::ostringstream oss; oss << "d31:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_32(const DiffChange& ch) {
    std::ostringstream oss; oss << "d32:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_33(const DiffChange& ch) {
    std::ostringstream oss; oss << "d33:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_34(const DiffChange& ch) {
    std::ostringstream oss; oss << "d34:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_35(const DiffChange& ch) {
    std::ostringstream oss; oss << "d35:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_36(const DiffChange& ch) {
    std::ostringstream oss; oss << "d36:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_37(const DiffChange& ch) {
    std::ostringstream oss; oss << "d37:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_38(const DiffChange& ch) {
    std::ostringstream oss; oss << "d38:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_39(const DiffChange& ch) {
    std::ostringstream oss; oss << "d39:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_40(const DiffChange& ch) {
    std::ostringstream oss; oss << "d40:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_41(const DiffChange& ch) {
    std::ostringstream oss; oss << "d41:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_42(const DiffChange& ch) {
    std::ostringstream oss; oss << "d42:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_43(const DiffChange& ch) {
    std::ostringstream oss; oss << "d43:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_44(const DiffChange& ch) {
    std::ostringstream oss; oss << "d44:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_45(const DiffChange& ch) {
    std::ostringstream oss; oss << "d45:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_46(const DiffChange& ch) {
    std::ostringstream oss; oss << "d46:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_47(const DiffChange& ch) {
    std::ostringstream oss; oss << "d47:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_48(const DiffChange& ch) {
    std::ostringstream oss; oss << "d48:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_49(const DiffChange& ch) {
    std::ostringstream oss; oss << "d49:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_50(const DiffChange& ch) {
    std::ostringstream oss; oss << "d50:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_51(const DiffChange& ch) {
    std::ostringstream oss; oss << "d51:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_52(const DiffChange& ch) {
    std::ostringstream oss; oss << "d52:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_53(const DiffChange& ch) {
    std::ostringstream oss; oss << "d53:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_54(const DiffChange& ch) {
    std::ostringstream oss; oss << "d54:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_55(const DiffChange& ch) {
    std::ostringstream oss; oss << "d55:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_56(const DiffChange& ch) {
    std::ostringstream oss; oss << "d56:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_57(const DiffChange& ch) {
    std::ostringstream oss; oss << "d57:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_58(const DiffChange& ch) {
    std::ostringstream oss; oss << "d58:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_59(const DiffChange& ch) {
    std::ostringstream oss; oss << "d59:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_60(const DiffChange& ch) {
    std::ostringstream oss; oss << "d60:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_61(const DiffChange& ch) {
    std::ostringstream oss; oss << "d61:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_62(const DiffChange& ch) {
    std::ostringstream oss; oss << "d62:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_63(const DiffChange& ch) {
    std::ostringstream oss; oss << "d63:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_64(const DiffChange& ch) {
    std::ostringstream oss; oss << "d64:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_65(const DiffChange& ch) {
    std::ostringstream oss; oss << "d65:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_66(const DiffChange& ch) {
    std::ostringstream oss; oss << "d66:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_67(const DiffChange& ch) {
    std::ostringstream oss; oss << "d67:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_68(const DiffChange& ch) {
    std::ostringstream oss; oss << "d68:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_69(const DiffChange& ch) {
    std::ostringstream oss; oss << "d69:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_70(const DiffChange& ch) {
    std::ostringstream oss; oss << "d70:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_71(const DiffChange& ch) {
    std::ostringstream oss; oss << "d71:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_72(const DiffChange& ch) {
    std::ostringstream oss; oss << "d72:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_73(const DiffChange& ch) {
    std::ostringstream oss; oss << "d73:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_74(const DiffChange& ch) {
    std::ostringstream oss; oss << "d74:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_75(const DiffChange& ch) {
    std::ostringstream oss; oss << "d75:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_76(const DiffChange& ch) {
    std::ostringstream oss; oss << "d76:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_77(const DiffChange& ch) {
    std::ostringstream oss; oss << "d77:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_78(const DiffChange& ch) {
    std::ostringstream oss; oss << "d78:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_79(const DiffChange& ch) {
    std::ostringstream oss; oss << "d79:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_80(const DiffChange& ch) {
    std::ostringstream oss; oss << "d80:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_81(const DiffChange& ch) {
    std::ostringstream oss; oss << "d81:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_82(const DiffChange& ch) {
    std::ostringstream oss; oss << "d82:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_83(const DiffChange& ch) {
    std::ostringstream oss; oss << "d83:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_84(const DiffChange& ch) {
    std::ostringstream oss; oss << "d84:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_85(const DiffChange& ch) {
    std::ostringstream oss; oss << "d85:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_86(const DiffChange& ch) {
    std::ostringstream oss; oss << "d86:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_87(const DiffChange& ch) {
    std::ostringstream oss; oss << "d87:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_88(const DiffChange& ch) {
    std::ostringstream oss; oss << "d88:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_89(const DiffChange& ch) {
    std::ostringstream oss; oss << "d89:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_90(const DiffChange& ch) {
    std::ostringstream oss; oss << "d90:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_91(const DiffChange& ch) {
    std::ostringstream oss; oss << "d91:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_92(const DiffChange& ch) {
    std::ostringstream oss; oss << "d92:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_93(const DiffChange& ch) {
    std::ostringstream oss; oss << "d93:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_94(const DiffChange& ch) {
    std::ostringstream oss; oss << "d94:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_95(const DiffChange& ch) {
    std::ostringstream oss; oss << "d95:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_96(const DiffChange& ch) {
    std::ostringstream oss; oss << "d96:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_97(const DiffChange& ch) {
    std::ostringstream oss; oss << "d97:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_98(const DiffChange& ch) {
    std::ostringstream oss; oss << "d98:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_99(const DiffChange& ch) {
    std::ostringstream oss; oss << "d99:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_100(const DiffChange& ch) {
    std::ostringstream oss; oss << "d100:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_101(const DiffChange& ch) {
    std::ostringstream oss; oss << "d101:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_102(const DiffChange& ch) {
    std::ostringstream oss; oss << "d102:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_103(const DiffChange& ch) {
    std::ostringstream oss; oss << "d103:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_104(const DiffChange& ch) {
    std::ostringstream oss; oss << "d104:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_105(const DiffChange& ch) {
    std::ostringstream oss; oss << "d105:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_106(const DiffChange& ch) {
    std::ostringstream oss; oss << "d106:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_107(const DiffChange& ch) {
    std::ostringstream oss; oss << "d107:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_108(const DiffChange& ch) {
    std::ostringstream oss; oss << "d108:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_109(const DiffChange& ch) {
    std::ostringstream oss; oss << "d109:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_110(const DiffChange& ch) {
    std::ostringstream oss; oss << "d110:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_111(const DiffChange& ch) {
    std::ostringstream oss; oss << "d111:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_112(const DiffChange& ch) {
    std::ostringstream oss; oss << "d112:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_113(const DiffChange& ch) {
    std::ostringstream oss; oss << "d113:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_114(const DiffChange& ch) {
    std::ostringstream oss; oss << "d114:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_115(const DiffChange& ch) {
    std::ostringstream oss; oss << "d115:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_116(const DiffChange& ch) {
    std::ostringstream oss; oss << "d116:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_117(const DiffChange& ch) {
    std::ostringstream oss; oss << "d117:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_118(const DiffChange& ch) {
    std::ostringstream oss; oss << "d118:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

static std::string diff_format_detail_119(const DiffChange& ch) {
    std::ostringstream oss; oss << "d119:" << static_cast<int>(ch.op);
    for (const auto& fd : ch.field_diffs) oss << "|" << fd;
    return oss.str();
}

std::string diff_engine_summary_line(const DiffEngine& engine) {
    const auto& s = engine.summary();
    std::ostringstream oss;
    oss << "diff: +" << s.insert_count << " -" << s.delete_count
        << " ~" << s.modify_count << " =" << s.equal_count;
    std::string details;

    for (const auto& ch : engine.changes()) details += diff_format_detail_0(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_1(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_2(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_3(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_4(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_5(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_6(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_7(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_8(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_9(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_10(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_11(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_12(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_13(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_14(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_15(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_16(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_17(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_18(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_19(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_20(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_21(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_22(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_23(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_24(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_25(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_26(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_27(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_28(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_29(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_30(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_31(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_32(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_33(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_34(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_35(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_36(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_37(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_38(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_39(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_40(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_41(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_42(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_43(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_44(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_45(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_46(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_47(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_48(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_49(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_50(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_51(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_52(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_53(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_54(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_55(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_56(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_57(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_58(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_59(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_60(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_61(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_62(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_63(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_64(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_65(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_66(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_67(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_68(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_69(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_70(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_71(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_72(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_73(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_74(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_75(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_76(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_77(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_78(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_79(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_80(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_81(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_82(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_83(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_84(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_85(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_86(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_87(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_88(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_89(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_90(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_91(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_92(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_93(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_94(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_95(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_96(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_97(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_98(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_99(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_100(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_101(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_102(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_103(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_104(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_105(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_106(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_107(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_108(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_109(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_110(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_111(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_112(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_113(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_114(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_115(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_116(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_117(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_118(ch);
    for (const auto& ch : engine.changes()) details += diff_format_detail_119(ch);
    (void)details; return oss.str();
}

}  // namespace telltale

// Optimization pass 40 - memory iteration 1
