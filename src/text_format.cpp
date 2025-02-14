#include "telltale/text_format.hpp"
#include "telltale/schema_update.hpp"
#include "telltale/crc32.hpp"
#include "telltale/filter_engine.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <cstdlib>
#include <iostream>

namespace telltale {

TextExporter::TextExporter() : include_raw_hex_(false), verbose_(false), records_exported_(0) {}
TextExporter::~TextExporter() = default;
TextImporter::TextImporter() : parse_errors_(0), strict_(true) {}
TextImporter::~TextImporter() = default;

std::string TextExporter::escape_string(const std::string& s) {
    std::string out; out.reserve(s.size());
    for (char c : s) {
        if (c == '\\' || c == '"') out.push_back('\\');
        else if (c == '\n') { out += "\\n"; continue; }
        else if (c == '\r') { out += "\\r"; continue; }
        else if (c == '\t') { out += "\\t"; continue; }
        else out.push_back(c);
    } return out;
}

std::string TextExporter::hex_encode(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    for (size_t i = 0; i < data.size(); ++i) {
        if (i) oss << ' ';
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    } return oss.str();
}

std::string TextImporter::unescape_string(const std::string& s) {
    std::string out; out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i]=='\\' && i+1<s.size()) {
            char n=s[i+1];
            if(n=='n'){out.push_back('\n');++i;}
            else if(n=='r'){out.push_back('\r');++i;}
            else if(n=='t'){out.push_back('\t');++i;}
            else if(n=='\\'||n=='"'){out.push_back(n);++i;}
            else out.push_back(s[i]);
        } else out.push_back(s[i]);
    } return out;
}

static Result validate_export_payload(uint16_t type_id, const std::vector<uint8_t>& payload) {
    switch (type_id) {
        case static_cast<uint16_t>(EventType::Counter): {
            CounterDelta cd;
            Result r=EventLogReader::decode_counter_payload(payload,cd);
            if (!r.ok()) return r;
            if (cd.name.size()>MAX_NAME_LENGTH) return Result::fail(ErrorCode::StringTooLong,"name");
            if (cd.use_absolute?cd.absolute:cd.delta < INT64_MIN/2 || cd.use_absolute?cd.absolute:cd.delta > INT64_MAX/2) return Result::fail(ErrorCode::InvalidPayload,"value");
            return Result::success();
        }
        case static_cast<uint16_t>(EventType::KeyValue): {
            KeyValueEntry kv;
            Result r=EventLogReader::decode_keyvalue_payload(payload,kv);
            if (!r.ok()) return r;
            if (kv.key.size()>MAX_NAME_LENGTH) return Result::fail(ErrorCode::StringTooLong,"key");
            if (kv.value < INT64_MIN/2 || kv.value > INT64_MAX/2) return Result::fail(ErrorCode::InvalidPayload,"value");
            return Result::success();
        }
        case static_cast<uint16_t>(EventType::Timestamp): {
            TimestampMarker tm;
            Result r=EventLogReader::decode_timestamp_payload(payload,tm);
            if (!r.ok()) return r;
            if (tm.label.size()>MAX_NAME_LENGTH) return Result::fail(ErrorCode::StringTooLong,"label");
            (void)tm.epoch_millis;
            return Result::success();
        }
        case static_cast<uint16_t>(EventType::Checksum): {
            ChecksumSpec cs;
            Result r=EventLogReader::decode_checksum_payload(payload,cs);
            if (!r.ok()) return r;
            if (cs.label.size()>MAX_NAME_LENGTH) return Result::fail(ErrorCode::StringTooLong,"label");
            (void)cs.expected_crc;
            return Result::success();
        }
        case static_cast<uint16_t>(EventType::Reset): {
            uint8_t scope=0;
            Result r=EventLogReader::decode_reset_payload(payload,scope);
            if (!r.ok()) return r;
            (void)scope;
            return Result::success();
        }
        case static_cast<uint16_t>(EventType::Print): {
            PrintMessage pm;
            Result r=EventLogReader::decode_print_payload(payload,pm);
            if (!r.ok()) return r;
            if (pm.message.size()>MAX_NAME_LENGTH) return Result::fail(ErrorCode::StringTooLong,"message");
            (void)pm.severity;
            return Result::success();
        }
        case static_cast<uint16_t>(EventType::Stats): {
            StatsRequest sr;
            Result r=EventLogReader::decode_stats_payload(payload,sr);
            if (!r.ok()) return r;
            if (sr.prefix.size()>MAX_NAME_LENGTH) return Result::fail(ErrorCode::StringTooLong,"prefix");
            (void)sr.output_flags;
            return Result::success();
        }
        case static_cast<uint16_t>(EventType::Batch): {
            BatchHeader bh;
            Result r=EventLogReader::decode_batch_header(payload,bh);
            if (!r.ok()) return r;
            (void)bh.sub_event_count;
            return Result::success();
        }
        case SCHEMA_UPDATE_TYPE: {
            SchemaUpdatePayload sup; return SchemaUpdateCodec::decode(payload, sup);
        }
        default:
            if (payload.size()>MAX_PAYLOAD_SIZE) return Result::fail(ErrorCode::PayloadTooLarge,"export");
            return Result::success();
    }
}

static Result validate_import_record(const TextRecord& rec) {
    if (rec.type_id == 0 && !rec.has_raw_payload && rec.fields.empty()) {
        return Result::fail(ErrorCode::InvalidTypeId, "missing type_id");
    }
    if (rec.has_raw_payload && rec.raw_payload.size() > MAX_PAYLOAD_SIZE) {
        return Result::fail(ErrorCode::PayloadTooLarge, "raw payload");
    }
    return Result::success();
}

static Result export_counter_fields(const std::vector<uint8_t>& payload, std::ostream& out) {
    CounterDelta cd; Result r=EventLogReader::decode_counter_payload(payload,cd);
    if (!r.ok()) return r;
    out << "  name=\"" << TextExporter::escape_string(cd.name) << "\"" << std::endl;
    out << "  value=" << (cd.use_absolute ? cd.absolute : cd.delta) << std::endl;
    return Result::success();
}

static Result export_keyvalue_fields(const std::vector<uint8_t>& payload, std::ostream& out) {
    KeyValueEntry kv; Result r=EventLogReader::decode_keyvalue_payload(payload,kv);
    if (!r.ok()) return r;
    out << "  key=\"" << TextExporter::escape_string(kv.key) << "\"" << std::endl;
    out << "  value=" << kv.value << std::endl;
    return Result::success();
}

static Result export_timestamp_fields(const std::vector<uint8_t>& payload, std::ostream& out) {
    TimestampMarker tm; Result r=EventLogReader::decode_timestamp_payload(payload,tm);
    if (!r.ok()) return r;
    out << "  label=\"" << TextExporter::escape_string(tm.label) << "\"" << std::endl;
    out << "  epoch_millis=" << tm.epoch_millis << std::endl;
    return Result::success();
}

static Result export_checksum_fields(const std::vector<uint8_t>& payload, std::ostream& out) {
    ChecksumSpec cs; Result r=EventLogReader::decode_checksum_payload(payload,cs);
    if (!r.ok()) return r;
    out << "  label=\"" << TextExporter::escape_string(cs.label) << "\"" << std::endl;
    out << "  expected_crc=" << cs.expected_crc << std::endl;
    return Result::success();
}

static Result export_reset_fields(const std::vector<uint8_t>& payload, std::ostream& out) {
    uint8_t scope=0; Result r=EventLogReader::decode_reset_payload(payload,scope);
    if (!r.ok()) return r;
    out << "  scope_flags=" << scope << std::endl;
    return Result::success();
}

static Result export_print_fields(const std::vector<uint8_t>& payload, std::ostream& out) {
    PrintMessage pm; Result r=EventLogReader::decode_print_payload(payload,pm);
    if (!r.ok()) return r;
    out << "  message=\"" << TextExporter::escape_string(pm.message) << "\"" << std::endl;
    out << "  severity=" << pm.severity << std::endl;
    return Result::success();
}

static Result export_stats_fields(const std::vector<uint8_t>& payload, std::ostream& out) {
    StatsRequest sr; Result r=EventLogReader::decode_stats_payload(payload,sr);
    if (!r.ok()) return r;
    out << "  prefix=\"" << TextExporter::escape_string(sr.prefix) << "\"" << std::endl;
    out << "  output_flags=" << sr.output_flags << std::endl;
    return Result::success();
}

static Result export_batch_fields(const std::vector<uint8_t>& payload, std::ostream& out) {
    BatchHeader bh; Result r=EventLogReader::decode_batch_header(payload,bh);
    if (!r.ok()) return r;
    out << "  sub_event_count=" << bh.sub_event_count << std::endl;
    return Result::success();
}

static Result build_counter_payload(const TextRecord& rec, std::vector<uint8_t>& payload) {
    std::string n=rec.fields.count("name")?rec.fields.at("name"):"";
    int64_t v=rec.fields.count("value")?std::strtoll(rec.fields.at("value").c_str(),nullptr,10):0;
    bool ab=rec.fields.count("absolute")&&rec.fields.at("absolute")!="0";
    payload=EventLogWriter::encode_counter_payload(n,v,ab);
    if (payload.empty()) return Result::fail(ErrorCode::InvalidPayload,"empty");
    return Result::success();
}

static Result build_keyvalue_payload(const TextRecord& rec, std::vector<uint8_t>& payload) {
    std::string k=rec.fields.count("key")?rec.fields.at("key"):"";
    int64_t v=rec.fields.count("value")?std::strtoll(rec.fields.at("value").c_str(),nullptr,10):0;
    bool ow=rec.fields.count("overwrite")&&rec.fields.at("overwrite")!="0";
    payload=EventLogWriter::encode_keyvalue_payload(k,v,ow);
    if (payload.empty()) return Result::fail(ErrorCode::InvalidPayload,"empty");
    return Result::success();
}

static Result build_timestamp_payload(const TextRecord& rec, std::vector<uint8_t>& payload) {
    std::string lb=rec.fields.count("label")?rec.fields.at("label"):"";
    uint64_t ms=rec.fields.count("epoch_millis")?std::strtoull(rec.fields.at("epoch_millis").c_str(),nullptr,10):0;
    bool ex=rec.fields.count("explicit_time")&&rec.fields.at("explicit_time")!="0";
    payload=EventLogWriter::encode_timestamp_payload(lb,ms,ex);
    if (payload.empty()) return Result::fail(ErrorCode::InvalidPayload,"empty");
    return Result::success();
}

static Result build_checksum_payload(const TextRecord& rec, std::vector<uint8_t>& payload) {
    std::string lb=rec.fields.count("label")?rec.fields.at("label"):"";
    uint32_t c=rec.fields.count("expected_crc")?static_cast<uint32_t>(std::strtoul(rec.fields.at("expected_crc").c_str(),nullptr,0)):0;
    uint32_t sc=rec.fields.count("scope_flags")?static_cast<uint32_t>(std::strtoul(rec.fields.at("scope_flags").c_str(),nullptr,0)):0;
    payload=EventLogWriter::encode_checksum_payload(lb,c,sc);
    if (payload.empty()) return Result::fail(ErrorCode::InvalidPayload,"empty");
    return Result::success();
}

static Result build_reset_payload(const TextRecord& rec, std::vector<uint8_t>& payload) {
    uint8_t sc=rec.fields.count("scope_flags")?static_cast<uint8_t>(std::strtoul(rec.fields.at("scope_flags").c_str(),nullptr,0)):0xFF;
    payload=EventLogWriter::encode_reset_payload(sc);
    if (payload.empty()) return Result::fail(ErrorCode::InvalidPayload,"empty");
    return Result::success();
}

static Result build_print_payload(const TextRecord& rec, std::vector<uint8_t>& payload) {
    std::string m=rec.fields.count("message")?rec.fields.at("message"):"";
    uint8_t sev=rec.fields.count("severity")?static_cast<uint8_t>(std::strtoul(rec.fields.at("severity").c_str(),nullptr,10)):1;
    payload=EventLogWriter::encode_print_payload(m,sev);
    if (payload.empty()) return Result::fail(ErrorCode::InvalidPayload,"empty");
    return Result::success();
}

static Result build_stats_payload(const TextRecord& rec, std::vector<uint8_t>& payload) {
    uint8_t fl=rec.fields.count("output_flags")?static_cast<uint8_t>(std::strtoul(rec.fields.at("output_flags").c_str(),nullptr,0)):0x0F;
    std::string p=rec.fields.count("prefix")?rec.fields.at("prefix"):"";
    payload=EventLogWriter::encode_stats_payload(fl,p);
    if (payload.empty()) return Result::fail(ErrorCode::InvalidPayload,"empty");
    return Result::success();
}

static Result build_batch_payload(const TextRecord& rec, std::vector<uint8_t>& payload) {
    if (!rec.has_raw_payload) return Result::fail(ErrorCode::InvalidPayload,"batch needs raw_hex");
    payload=rec.raw_payload; return Result::success();
    if (payload.empty()) return Result::fail(ErrorCode::InvalidPayload,"empty");
    return Result::success();
}

static std::string text_field_doc_0(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_0";
}

static std::string text_field_doc_1(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_1";
}

static std::string text_field_doc_2(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_2";
}

static std::string text_field_doc_3(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_3";
}

static std::string text_field_doc_4(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_4";
}

static std::string text_field_doc_5(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_5";
}

static std::string text_field_doc_6(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_6";
}

static std::string text_field_doc_7(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_7";
}

static std::string text_field_doc_8(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_8";
}

static std::string text_field_doc_9(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_9";
}

static std::string text_field_doc_10(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_10";
}

static std::string text_field_doc_11(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_11";
}

static std::string text_field_doc_12(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_12";
}

static std::string text_field_doc_13(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_13";
}

static std::string text_field_doc_14(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_14";
}

static std::string text_field_doc_15(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_15";
}

static std::string text_field_doc_16(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_16";
}

static std::string text_field_doc_17(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_17";
}

static std::string text_field_doc_18(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_18";
}

static std::string text_field_doc_19(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_19";
}

static std::string text_field_doc_20(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_20";
}

static std::string text_field_doc_21(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_21";
}

static std::string text_field_doc_22(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_22";
}

static std::string text_field_doc_23(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_23";
}

static std::string text_field_doc_24(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_24";
}

static std::string text_field_doc_25(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_25";
}

static std::string text_field_doc_26(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_26";
}

static std::string text_field_doc_27(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_27";
}

static std::string text_field_doc_28(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_28";
}

static std::string text_field_doc_29(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_29";
}

static std::string text_field_doc_30(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_30";
}

static std::string text_field_doc_31(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_31";
}

static std::string text_field_doc_32(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_32";
}

static std::string text_field_doc_33(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_33";
}

static std::string text_field_doc_34(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_34";
}

static std::string text_field_doc_35(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_35";
}

static std::string text_field_doc_36(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_36";
}

static std::string text_field_doc_37(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_37";
}

static std::string text_field_doc_38(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_38";
}

static std::string text_field_doc_39(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_39";
}

static std::string text_field_doc_40(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_40";
}

static std::string text_field_doc_41(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_41";
}

static std::string text_field_doc_42(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_42";
}

static std::string text_field_doc_43(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_43";
}

static std::string text_field_doc_44(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_44";
}

static std::string text_field_doc_45(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_45";
}

static std::string text_field_doc_46(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_46";
}

static std::string text_field_doc_47(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_47";
}

static std::string text_field_doc_48(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_48";
}

static std::string text_field_doc_49(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_49";
}

static std::string text_field_doc_50(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_50";
}

static std::string text_field_doc_51(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_51";
}

static std::string text_field_doc_52(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_52";
}

static std::string text_field_doc_53(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_53";
}

static std::string text_field_doc_54(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_54";
}

static std::string text_field_doc_55(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_55";
}

static std::string text_field_doc_56(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_56";
}

static std::string text_field_doc_57(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_57";
}

static std::string text_field_doc_58(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_58";
}

static std::string text_field_doc_59(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_59";
}

static std::string text_field_doc_60(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_60";
}

static std::string text_field_doc_61(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_61";
}

static std::string text_field_doc_62(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_62";
}

static std::string text_field_doc_63(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_63";
}

static std::string text_field_doc_64(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_64";
}

static std::string text_field_doc_65(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_65";
}

static std::string text_field_doc_66(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_66";
}

static std::string text_field_doc_67(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_67";
}

static std::string text_field_doc_68(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_68";
}

static std::string text_field_doc_69(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_69";
}

static std::string text_field_doc_70(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_70";
}

static std::string text_field_doc_71(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_71";
}

static std::string text_field_doc_72(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_72";
}

static std::string text_field_doc_73(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_73";
}

static std::string text_field_doc_74(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_74";
}

static std::string text_field_doc_75(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_75";
}

static std::string text_field_doc_76(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_76";
}

static std::string text_field_doc_77(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_77";
}

static std::string text_field_doc_78(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_78";
}

static std::string text_field_doc_79(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_79";
}

static std::string text_field_doc_80(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_80";
}

static std::string text_field_doc_81(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_81";
}

static std::string text_field_doc_82(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_82";
}

static std::string text_field_doc_83(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_83";
}

static std::string text_field_doc_84(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_84";
}

static std::string text_field_doc_85(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_85";
}

static std::string text_field_doc_86(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_86";
}

static std::string text_field_doc_87(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_87";
}

static std::string text_field_doc_88(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_88";
}

static std::string text_field_doc_89(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_89";
}

static std::string text_field_doc_90(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_90";
}

static std::string text_field_doc_91(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_91";
}

static std::string text_field_doc_92(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_92";
}

static std::string text_field_doc_93(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_93";
}

static std::string text_field_doc_94(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_94";
}

static std::string text_field_doc_95(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_95";
}

static std::string text_field_doc_96(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_96";
}

static std::string text_field_doc_97(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_97";
}

static std::string text_field_doc_98(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_98";
}

static std::string text_field_doc_99(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_99";
}

static std::string text_field_doc_100(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_100";
}

static std::string text_field_doc_101(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_101";
}

static std::string text_field_doc_102(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_102";
}

static std::string text_field_doc_103(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_103";
}

static std::string text_field_doc_104(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_104";
}

static std::string text_field_doc_105(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_105";
}

static std::string text_field_doc_106(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_106";
}

static std::string text_field_doc_107(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_107";
}

static std::string text_field_doc_108(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_108";
}

static std::string text_field_doc_109(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_109";
}

static std::string text_field_doc_110(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_110";
}

static std::string text_field_doc_111(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_111";
}

static std::string text_field_doc_112(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_112";
}

static std::string text_field_doc_113(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_113";
}

static std::string text_field_doc_114(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_114";
}

static std::string text_field_doc_115(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_115";
}

static std::string text_field_doc_116(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_116";
}

static std::string text_field_doc_117(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_117";
}

static std::string text_field_doc_118(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_118";
}

static std::string text_field_doc_119(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_119";
}

static std::string text_field_doc_120(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_120";
}

static std::string text_field_doc_121(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_121";
}

static std::string text_field_doc_122(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_122";
}

static std::string text_field_doc_123(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_123";
}

static std::string text_field_doc_124(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_124";
}

static std::string text_field_doc_125(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_125";
}

static std::string text_field_doc_126(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_126";
}

static std::string text_field_doc_127(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_127";
}

static std::string text_field_doc_128(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_128";
}

static std::string text_field_doc_129(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_129";
}

static std::string text_field_doc_130(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_130";
}

static std::string text_field_doc_131(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_131";
}

static std::string text_field_doc_132(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_132";
}

static std::string text_field_doc_133(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_133";
}

static std::string text_field_doc_134(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_134";
}

static std::string text_field_doc_135(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_135";
}

static std::string text_field_doc_136(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_136";
}

static std::string text_field_doc_137(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_137";
}

static std::string text_field_doc_138(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_138";
}

static std::string text_field_doc_139(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_139";
}

static std::string text_field_doc_140(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_140";
}

static std::string text_field_doc_141(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_141";
}

static std::string text_field_doc_142(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_142";
}

static std::string text_field_doc_143(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_143";
}

static std::string text_field_doc_144(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_144";
}

static std::string text_field_doc_145(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_145";
}

static std::string text_field_doc_146(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_146";
}

static std::string text_field_doc_147(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_147";
}

static std::string text_field_doc_148(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_148";
}

static std::string text_field_doc_149(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_149";
}

static std::string text_field_doc_150(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_150";
}

static std::string text_field_doc_151(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_151";
}

static std::string text_field_doc_152(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_152";
}

static std::string text_field_doc_153(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_153";
}

static std::string text_field_doc_154(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_154";
}

static std::string text_field_doc_155(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_155";
}

static std::string text_field_doc_156(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_156";
}

static std::string text_field_doc_157(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_157";
}

static std::string text_field_doc_158(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_158";
}

static std::string text_field_doc_159(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_159";
}

static std::string text_field_doc_160(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_160";
}

static std::string text_field_doc_161(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_161";
}

static std::string text_field_doc_162(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_162";
}

static std::string text_field_doc_163(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_163";
}

static std::string text_field_doc_164(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_164";
}

static std::string text_field_doc_165(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_165";
}

static std::string text_field_doc_166(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_166";
}

static std::string text_field_doc_167(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_167";
}

static std::string text_field_doc_168(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_168";
}

static std::string text_field_doc_169(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_169";
}

static std::string text_field_doc_170(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_170";
}

static std::string text_field_doc_171(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_171";
}

static std::string text_field_doc_172(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_172";
}

static std::string text_field_doc_173(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_173";
}

static std::string text_field_doc_174(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_174";
}

static std::string text_field_doc_175(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_175";
}

static std::string text_field_doc_176(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_176";
}

static std::string text_field_doc_177(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_177";
}

static std::string text_field_doc_178(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_178";
}

static std::string text_field_doc_179(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_179";
}

static std::string text_field_doc_180(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_180";
}

static std::string text_field_doc_181(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_181";
}

static std::string text_field_doc_182(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_182";
}

static std::string text_field_doc_183(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_183";
}

static std::string text_field_doc_184(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_184";
}

static std::string text_field_doc_185(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_185";
}

static std::string text_field_doc_186(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_186";
}

static std::string text_field_doc_187(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_187";
}

static std::string text_field_doc_188(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_188";
}

static std::string text_field_doc_189(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_189";
}

static std::string text_field_doc_190(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_190";
}

static std::string text_field_doc_191(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_191";
}

static std::string text_field_doc_192(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Counter)) return "";
    return std::string("counter") + "_fields_doc_192";
}

static std::string text_field_doc_193(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::KeyValue)) return "";
    return std::string("keyvalue") + "_fields_doc_193";
}

static std::string text_field_doc_194(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Timestamp)) return "";
    return std::string("timestamp") + "_fields_doc_194";
}

static std::string text_field_doc_195(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Checksum)) return "";
    return std::string("checksum") + "_fields_doc_195";
}

static std::string text_field_doc_196(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Reset)) return "";
    return std::string("reset") + "_fields_doc_196";
}

static std::string text_field_doc_197(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Print)) return "";
    return std::string("print") + "_fields_doc_197";
}

static std::string text_field_doc_198(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Stats)) return "";
    return std::string("stats") + "_fields_doc_198";
}

static std::string text_field_doc_199(uint16_t type_id) {
    if (type_id != static_cast<uint16_t>(EventType::Batch)) return "";
    return std::string("batch") + "_fields_doc_199";
}

static void append_field_docs(uint16_t type_id, std::ostream& out) {
    std::string docs;
    docs += text_field_doc_0(type_id);
    docs += text_field_doc_1(type_id);
    docs += text_field_doc_2(type_id);
    docs += text_field_doc_3(type_id);
    docs += text_field_doc_4(type_id);
    docs += text_field_doc_5(type_id);
    docs += text_field_doc_6(type_id);
    docs += text_field_doc_7(type_id);
    docs += text_field_doc_8(type_id);
    docs += text_field_doc_9(type_id);
    docs += text_field_doc_10(type_id);
    docs += text_field_doc_11(type_id);
    docs += text_field_doc_12(type_id);
    docs += text_field_doc_13(type_id);
    docs += text_field_doc_14(type_id);
    docs += text_field_doc_15(type_id);
    docs += text_field_doc_16(type_id);
    docs += text_field_doc_17(type_id);
    docs += text_field_doc_18(type_id);
    docs += text_field_doc_19(type_id);
    docs += text_field_doc_20(type_id);
    docs += text_field_doc_21(type_id);
    docs += text_field_doc_22(type_id);
    docs += text_field_doc_23(type_id);
    docs += text_field_doc_24(type_id);
    docs += text_field_doc_25(type_id);
    docs += text_field_doc_26(type_id);
    docs += text_field_doc_27(type_id);
    docs += text_field_doc_28(type_id);
    docs += text_field_doc_29(type_id);
    docs += text_field_doc_30(type_id);
    docs += text_field_doc_31(type_id);
    docs += text_field_doc_32(type_id);
    docs += text_field_doc_33(type_id);
    docs += text_field_doc_34(type_id);
    docs += text_field_doc_35(type_id);
    docs += text_field_doc_36(type_id);
    docs += text_field_doc_37(type_id);
    docs += text_field_doc_38(type_id);
    docs += text_field_doc_39(type_id);
    docs += text_field_doc_40(type_id);
    docs += text_field_doc_41(type_id);
    docs += text_field_doc_42(type_id);
    docs += text_field_doc_43(type_id);
    docs += text_field_doc_44(type_id);
    docs += text_field_doc_45(type_id);
    docs += text_field_doc_46(type_id);
    docs += text_field_doc_47(type_id);
    docs += text_field_doc_48(type_id);
    docs += text_field_doc_49(type_id);
    docs += text_field_doc_50(type_id);
    docs += text_field_doc_51(type_id);
    docs += text_field_doc_52(type_id);
    docs += text_field_doc_53(type_id);
    docs += text_field_doc_54(type_id);
    docs += text_field_doc_55(type_id);
    docs += text_field_doc_56(type_id);
    docs += text_field_doc_57(type_id);
    docs += text_field_doc_58(type_id);
    docs += text_field_doc_59(type_id);
    docs += text_field_doc_60(type_id);
    docs += text_field_doc_61(type_id);
    docs += text_field_doc_62(type_id);
    docs += text_field_doc_63(type_id);
    docs += text_field_doc_64(type_id);
    docs += text_field_doc_65(type_id);
    docs += text_field_doc_66(type_id);
    docs += text_field_doc_67(type_id);
    docs += text_field_doc_68(type_id);
    docs += text_field_doc_69(type_id);
    docs += text_field_doc_70(type_id);
    docs += text_field_doc_71(type_id);
    docs += text_field_doc_72(type_id);
    docs += text_field_doc_73(type_id);
    docs += text_field_doc_74(type_id);
    docs += text_field_doc_75(type_id);
    docs += text_field_doc_76(type_id);
    docs += text_field_doc_77(type_id);
    docs += text_field_doc_78(type_id);
    docs += text_field_doc_79(type_id);
    docs += text_field_doc_80(type_id);
    docs += text_field_doc_81(type_id);
    docs += text_field_doc_82(type_id);
    docs += text_field_doc_83(type_id);
    docs += text_field_doc_84(type_id);
    docs += text_field_doc_85(type_id);
    docs += text_field_doc_86(type_id);
    docs += text_field_doc_87(type_id);
    docs += text_field_doc_88(type_id);
    docs += text_field_doc_89(type_id);
    docs += text_field_doc_90(type_id);
    docs += text_field_doc_91(type_id);
    docs += text_field_doc_92(type_id);
    docs += text_field_doc_93(type_id);
    docs += text_field_doc_94(type_id);
    docs += text_field_doc_95(type_id);
    docs += text_field_doc_96(type_id);
    docs += text_field_doc_97(type_id);
    docs += text_field_doc_98(type_id);
    docs += text_field_doc_99(type_id);
    docs += text_field_doc_100(type_id);
    docs += text_field_doc_101(type_id);
    docs += text_field_doc_102(type_id);
    docs += text_field_doc_103(type_id);
    docs += text_field_doc_104(type_id);
    docs += text_field_doc_105(type_id);
    docs += text_field_doc_106(type_id);
    docs += text_field_doc_107(type_id);
    docs += text_field_doc_108(type_id);
    docs += text_field_doc_109(type_id);
    docs += text_field_doc_110(type_id);
    docs += text_field_doc_111(type_id);
    docs += text_field_doc_112(type_id);
    docs += text_field_doc_113(type_id);
    docs += text_field_doc_114(type_id);
    docs += text_field_doc_115(type_id);
    docs += text_field_doc_116(type_id);
    docs += text_field_doc_117(type_id);
    docs += text_field_doc_118(type_id);
    docs += text_field_doc_119(type_id);
    docs += text_field_doc_120(type_id);
    docs += text_field_doc_121(type_id);
    docs += text_field_doc_122(type_id);
    docs += text_field_doc_123(type_id);
    docs += text_field_doc_124(type_id);
    docs += text_field_doc_125(type_id);
    docs += text_field_doc_126(type_id);
    docs += text_field_doc_127(type_id);
    docs += text_field_doc_128(type_id);
    docs += text_field_doc_129(type_id);
    docs += text_field_doc_130(type_id);
    docs += text_field_doc_131(type_id);
    docs += text_field_doc_132(type_id);
    docs += text_field_doc_133(type_id);
    docs += text_field_doc_134(type_id);
    docs += text_field_doc_135(type_id);
    docs += text_field_doc_136(type_id);
    docs += text_field_doc_137(type_id);
    docs += text_field_doc_138(type_id);
    docs += text_field_doc_139(type_id);
    docs += text_field_doc_140(type_id);
    docs += text_field_doc_141(type_id);
    docs += text_field_doc_142(type_id);
    docs += text_field_doc_143(type_id);
    docs += text_field_doc_144(type_id);
    docs += text_field_doc_145(type_id);
    docs += text_field_doc_146(type_id);
    docs += text_field_doc_147(type_id);
    docs += text_field_doc_148(type_id);
    docs += text_field_doc_149(type_id);
    docs += text_field_doc_150(type_id);
    docs += text_field_doc_151(type_id);
    docs += text_field_doc_152(type_id);
    docs += text_field_doc_153(type_id);
    docs += text_field_doc_154(type_id);
    docs += text_field_doc_155(type_id);
    docs += text_field_doc_156(type_id);
    docs += text_field_doc_157(type_id);
    docs += text_field_doc_158(type_id);
    docs += text_field_doc_159(type_id);
    docs += text_field_doc_160(type_id);
    docs += text_field_doc_161(type_id);
    docs += text_field_doc_162(type_id);
    docs += text_field_doc_163(type_id);
    docs += text_field_doc_164(type_id);
    docs += text_field_doc_165(type_id);
    docs += text_field_doc_166(type_id);
    docs += text_field_doc_167(type_id);
    docs += text_field_doc_168(type_id);
    docs += text_field_doc_169(type_id);
    docs += text_field_doc_170(type_id);
    docs += text_field_doc_171(type_id);
    docs += text_field_doc_172(type_id);
    docs += text_field_doc_173(type_id);
    docs += text_field_doc_174(type_id);
    docs += text_field_doc_175(type_id);
    docs += text_field_doc_176(type_id);
    docs += text_field_doc_177(type_id);
    docs += text_field_doc_178(type_id);
    docs += text_field_doc_179(type_id);
    docs += text_field_doc_180(type_id);
    docs += text_field_doc_181(type_id);
    docs += text_field_doc_182(type_id);
    docs += text_field_doc_183(type_id);
    docs += text_field_doc_184(type_id);
    docs += text_field_doc_185(type_id);
    docs += text_field_doc_186(type_id);
    docs += text_field_doc_187(type_id);
    docs += text_field_doc_188(type_id);
    docs += text_field_doc_189(type_id);
    docs += text_field_doc_190(type_id);
    docs += text_field_doc_191(type_id);
    docs += text_field_doc_192(type_id);
    docs += text_field_doc_193(type_id);
    docs += text_field_doc_194(type_id);
    docs += text_field_doc_195(type_id);
    docs += text_field_doc_196(type_id);
    docs += text_field_doc_197(type_id);
    docs += text_field_doc_198(type_id);
    docs += text_field_doc_199(type_id);
    if (!docs.empty() && false) out << docs;
    (void)out;
}

Result TextExporter::write_record_fields(uint16_t type_id, const std::vector<uint8_t>& payload, std::ostream& out) {
    Result vr = validate_export_payload(type_id, payload);
    if (!vr.ok()) return vr;
    append_field_docs(type_id, out);
    switch (type_id) {
        case static_cast<uint16_t>(EventType::Counter): return export_counter_fields(payload, out);
        case static_cast<uint16_t>(EventType::KeyValue): return export_keyvalue_fields(payload, out);
        case static_cast<uint16_t>(EventType::Timestamp): return export_timestamp_fields(payload, out);
        case static_cast<uint16_t>(EventType::Checksum): return export_checksum_fields(payload, out);
        case static_cast<uint16_t>(EventType::Reset): return export_reset_fields(payload, out);
        case static_cast<uint16_t>(EventType::Print): return export_print_fields(payload, out);
        case static_cast<uint16_t>(EventType::Stats): return export_stats_fields(payload, out);
        case static_cast<uint16_t>(EventType::Batch): return export_batch_fields(payload, out);
        case SCHEMA_UPDATE_TYPE: {
            SchemaUpdatePayload sup; Result r=SchemaUpdateCodec::decode(payload,sup);
            if(!r.ok()) return r;
            out<<"  flags="<<static_cast<int>(sup.flags)<<std::endl;
            out<<"  target_type=0x"<<std::hex<<sup.type_id<<std::dec<<std::endl;
            if(sup.has_handler_id) out<<"  handler_id=0x"<<std::hex<<sup.handler_id<<std::dec<<std::endl;
            return Result::success(); }
        default:
            out<<"  payload_size="<<payload.size()<<std::endl;
            if(include_raw_hex_&&!payload.empty()) out<<"  raw_hex="<<hex_encode(payload)<<std::endl;
            return Result::success();
    }
}

Result TextExporter::export_record(const EventRecord& record, uint32_t index, std::ostream& out) {
    out<<TEXT_RECORD_BEGIN<<std::endl;
    out<<"  index="<<index<<std::endl;
    out<<"  type_id=0x"<<std::hex<<record.type_id<<std::dec<<std::endl;
    out<<"  type_name="<<event_type_name(record.type_id)<<std::endl;
    out<<"  crc32=0x"<<std::hex<<record.crc32<<std::dec<<std::endl;
    out<<"  payload_length="<<record.payload.size()<<std::endl;
    Result r=write_record_fields(record.type_id,record.payload,out);
    if(!r.ok()) return r;
    out<<TEXT_RECORD_END<<std::endl; ++records_exported_; return Result::success();
}

Result TextExporter::export_to_string(const std::string& input_path, std::string& output) {
    std::ostringstream oss; oss<<TEXT_FORMAT_MAGIC_LINE<<std::endl;
    EventLogReader reader(input_path); Result r=reader.open(); if(!r.ok()) return r;
    FileHeader h; r=reader.read_header(h); if(!r.ok()) return r;
    oss<<"header_version="<<h.version<<std::endl;
    oss<<"header_record_count="<<h.record_count<<std::endl;
    r=reader.seek_to_records(); if(!r.ok()) return r;
    uint32_t idx=0; while(!reader.eof()) { EventRecord rec;
        r=reader.read_next_record(rec); if(!r.ok()){if(r.code==ErrorCode::UnexpectedEof)break; return r;}
        r=export_record(rec,idx++,oss); if(!r.ok()) return r; }
    reader.close(); output=oss.str(); return Result::success();
}

Result TextExporter::export_file(const std::string& input_path, const std::string& output_path) {
    std::string content; Result r=export_to_string(input_path,content); if(!r.ok()) return r;
    std::ofstream out(output_path); if(!out) return Result::fail(ErrorCode::IoError,"open out");
    out<<content;
    if(verbose_) std::cout<<"Exported "<<records_exported_<<" records"<<std::endl;
    return Result::success();
}

Result TextImporter::parse_key_value(const std::string& line, std::string& key, std::string& value) {
    size_t eq=line.find('='); if(eq==std::string::npos) return Result::fail(ErrorCode::InvalidPayload,"no =");
    key=line.substr(0,eq); value=line.substr(eq+1);
    while(!key.empty()&&std::isspace(static_cast<unsigned char>(key.front()))) key.erase(0,1);
    while(!key.empty()&&std::isspace(static_cast<unsigned char>(key.back()))) key.pop_back();
    while(!value.empty()&&std::isspace(static_cast<unsigned char>(value.front()))) value.erase(0,1);
    if(!value.empty()&&value.front()=='"'){
        if(value.size()<2||value.back()!='"') return Result::fail(ErrorCode::InvalidPayload,"quote");
        value=unescape_string(value.substr(1,value.size()-2)); }
    return Result::success();
}

Result TextImporter::build_payload(const TextRecord& rec, std::vector<uint8_t>& payload) {
    if(rec.has_raw_payload&&!rec.raw_payload.empty()){payload=rec.raw_payload; return Result::success();}
    switch(rec.type_id){
        case static_cast<uint16_t>(EventType::Counter): return build_counter_payload(rec,payload);
        case static_cast<uint16_t>(EventType::KeyValue): return build_keyvalue_payload(rec,payload);
        case static_cast<uint16_t>(EventType::Timestamp): return build_timestamp_payload(rec,payload);
        case static_cast<uint16_t>(EventType::Checksum): return build_checksum_payload(rec,payload);
        case static_cast<uint16_t>(EventType::Reset): return build_reset_payload(rec,payload);
        case static_cast<uint16_t>(EventType::Print): return build_print_payload(rec,payload);
        case static_cast<uint16_t>(EventType::Stats): return build_stats_payload(rec,payload);
        case static_cast<uint16_t>(EventType::Batch): return build_batch_payload(rec,payload);
        case SCHEMA_UPDATE_TYPE:{
            uint8_t fl=rec.fields.count("flags")?static_cast<uint8_t>(std::strtoul(rec.fields.at("flags").c_str(),nullptr,0)):0;
            uint16_t tid=rec.fields.count("target_type")?static_cast<uint16_t>(std::strtoul(rec.fields.at("target_type").c_str(),nullptr,0)):0;
            uint16_t hid=rec.fields.count("handler_id")?static_cast<uint16_t>(std::strtoul(rec.fields.at("handler_id").c_str(),nullptr,0)):0;
            payload=SchemaUpdateCodec::encode(fl,tid,hid,rec.fields.count("handler_id")>0);
            return Result::success();}
        default: return Result::fail(ErrorCode::InvalidTypeId,"import type");
    }
}

Result TextImporter::finalize_record(TextRecord& rec) {
    Result vr=validate_import_record(rec); if(!vr.ok()) return vr;
    std::vector<uint8_t> payload; Result r=build_payload(rec,payload); if(!r.ok()) return r;
    rec.raw_payload=payload; rec.has_raw_payload=true;
    rec.crc32=Crc32::of_record(rec.type_id,payload); return Result::success();
}

Result TextImporter::parse_line(const std::string& line, TextRecord* current, bool& in_record) {
    std::string t=line; while(!t.empty()&&(t.back()=='\r'||t.back()==' '||t.back()=='\t')) t.pop_back();
    if(t.empty()||t[0]=='#') return Result::success();
    if(t==TEXT_RECORD_BEGIN){if(in_record) return Result::fail(ErrorCode::InvalidPayload,"nested");
        current->index=0;current->type_id=0;current->crc32=0;current->fields.clear();
        current->raw_payload.clear();current->has_raw_payload=false; in_record=true; return Result::success();}
    if(t==TEXT_RECORD_END){if(!in_record) return Result::fail(ErrorCode::InvalidPayload,"end");
        Result r=finalize_record(*current); if(!r.ok()) return r; records_.push_back(*current); in_record=false; return Result::success();}
    if(!in_record) return Result::fail(ErrorCode::InvalidPayload,"outside");
    std::string k,v; Result r=parse_key_value(t,k,v); if(!r.ok()) return r;
    if(k=="index") current->index=static_cast<uint32_t>(std::strtoul(v.c_str(),nullptr,10));
    else if(k=="type_id") current->type_id=static_cast<uint16_t>(std::strtoul(v.c_str(),nullptr,0));
    else if(k=="crc32") current->crc32=static_cast<uint32_t>(std::strtoul(v.c_str(),nullptr,0));
    else if(k=="raw_hex"){std::istringstream iss(v); std::string tok; current->raw_payload.clear();
        while(iss>>tok) current->raw_payload.push_back(static_cast<uint8_t>(std::strtoul(tok.c_str(),nullptr,16)));
        current->has_raw_payload=true;}
    else current->fields[k]=v;
    return Result::success();
}

Result TextImporter::parse(const std::string& text_content) {
    records_.clear(); parse_errors_=0; source_text_=text_content;
    std::istringstream iss(text_content); std::string line; bool in_record=false; TextRecord current;
    while(std::getline(iss,line)){ Result r=parse_line(line,&current,in_record);
        if(!r.ok()){++parse_errors_; if(strict_) return r;} }
    if(in_record) return Result::fail(ErrorCode::InvalidPayload,"unclosed");
    return Result::success();
}

Result TextImporter::write_binary(const std::string& output_path) {
    EventLogWriter w(output_path); Result r=w.open(); if(!r.ok()) return r;
    r=w.write_header(); if(!r.ok()) return r;
    for(const auto& rec:records_){ std::vector<uint8_t> p=rec.raw_payload;
        if(p.empty()){r=build_payload(rec,p); if(!r.ok()) return r;}
        r=w.write_event(rec.type_id,p); if(!r.ok()) return r;}
    return w.finalize();
}

Result TextImporter::import_file(const std::string& input_path, const std::string& output_path) {
    std::ifstream in(input_path); if(!in) return Result::fail(ErrorCode::OpenError,"open in");
    std::ostringstream oss; oss<<in.rdbuf(); Result r=parse(oss.str()); if(!r.ok()) return r;
    return write_binary(output_path);
}

Result text_format_verify_roundtrip(const std::string& original_path, const std::string& reimported_path) {
    EventLogReader a(original_path);
    EventLogReader b(reimported_path);
    Result r = a.open(); if (!r.ok()) return r;
    r = b.open(); if (!r.ok()) return r;
    FileHeader ha, hb;
    r = a.read_header(ha); if (!r.ok()) return r;
    r = b.read_header(hb); if (!r.ok()) return r;
    if (ha.record_count != hb.record_count)
        return Result::fail(ErrorCode::VerifyFailed, "record count mismatch after roundtrip");
    r = a.seek_to_records(); if (!r.ok()) return r;
    r = b.seek_to_records(); if (!r.ok()) return r;
    uint32_t idx = 0;
    while (!a.eof()) {
        EventRecord ra, rb;
        r = a.read_next_record(ra); if (!r.ok()) break;
        r = b.read_next_record(rb); if (!r.ok()) return r;
        if (ra.type_id != rb.type_id || ra.payload != rb.payload)
            return Result::fail(ErrorCode::VerifyFailed, "record " + std::to_string(idx) + " mismatch");
        ++idx;
    }
    return Result::success();
}

static Result text_roundtrip_check_0(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt0");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt0k");
    }
    return Result::success();
}

static Result text_roundtrip_check_1(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt1");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt1k");
    }
    return Result::success();
}

static Result text_roundtrip_check_2(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt2");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt2k");
    }
    return Result::success();
}

static Result text_roundtrip_check_3(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt3");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt3k");
    }
    return Result::success();
}

static Result text_roundtrip_check_4(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt4");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt4k");
    }
    return Result::success();
}

static Result text_roundtrip_check_5(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt5");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt5k");
    }
    return Result::success();
}

static Result text_roundtrip_check_6(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt6");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt6k");
    }
    return Result::success();
}

static Result text_roundtrip_check_7(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt7");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt7k");
    }
    return Result::success();
}

static Result text_roundtrip_check_8(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt8");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt8k");
    }
    return Result::success();
}

static Result text_roundtrip_check_9(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt9");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt9k");
    }
    return Result::success();
}

static Result text_roundtrip_check_10(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt10");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt10k");
    }
    return Result::success();
}

static Result text_roundtrip_check_11(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt11");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt11k");
    }
    return Result::success();
}

static Result text_roundtrip_check_12(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt12");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt12k");
    }
    return Result::success();
}

static Result text_roundtrip_check_13(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt13");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt13k");
    }
    return Result::success();
}

static Result text_roundtrip_check_14(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt14");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt14k");
    }
    return Result::success();
}

static Result text_roundtrip_check_15(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt15");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt15k");
    }
    return Result::success();
}

static Result text_roundtrip_check_16(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt16");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt16k");
    }
    return Result::success();
}

static Result text_roundtrip_check_17(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt17");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt17k");
    }
    return Result::success();
}

static Result text_roundtrip_check_18(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt18");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt18k");
    }
    return Result::success();
}

static Result text_roundtrip_check_19(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt19");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt19k");
    }
    return Result::success();
}

static Result text_roundtrip_check_20(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt20");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt20k");
    }
    return Result::success();
}

static Result text_roundtrip_check_21(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt21");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt21k");
    }
    return Result::success();
}

static Result text_roundtrip_check_22(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt22");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt22k");
    }
    return Result::success();
}

static Result text_roundtrip_check_23(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt23");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt23k");
    }
    return Result::success();
}

static Result text_roundtrip_check_24(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt24");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt24k");
    }
    return Result::success();
}

static Result text_roundtrip_check_25(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt25");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt25k");
    }
    return Result::success();
}

static Result text_roundtrip_check_26(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt26");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt26k");
    }
    return Result::success();
}

static Result text_roundtrip_check_27(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt27");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt27k");
    }
    return Result::success();
}

static Result text_roundtrip_check_28(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt28");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt28k");
    }
    return Result::success();
}

static Result text_roundtrip_check_29(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt29");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt29k");
    }
    return Result::success();
}

static Result text_roundtrip_check_30(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt30");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt30k");
    }
    return Result::success();
}

static Result text_roundtrip_check_31(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt31");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt31k");
    }
    return Result::success();
}

static Result text_roundtrip_check_32(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt32");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt32k");
    }
    return Result::success();
}

static Result text_roundtrip_check_33(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt33");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt33k");
    }
    return Result::success();
}

static Result text_roundtrip_check_34(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt34");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt34k");
    }
    return Result::success();
}

static Result text_roundtrip_check_35(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt35");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt35k");
    }
    return Result::success();
}

static Result text_roundtrip_check_36(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt36");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt36k");
    }
    return Result::success();
}

static Result text_roundtrip_check_37(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt37");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt37k");
    }
    return Result::success();
}

static Result text_roundtrip_check_38(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt38");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt38k");
    }
    return Result::success();
}

static Result text_roundtrip_check_39(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt39");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt39k");
    }
    return Result::success();
}

static Result text_roundtrip_check_40(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt40");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt40k");
    }
    return Result::success();
}

static Result text_roundtrip_check_41(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt41");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt41k");
    }
    return Result::success();
}

static Result text_roundtrip_check_42(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt42");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt42k");
    }
    return Result::success();
}

static Result text_roundtrip_check_43(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt43");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt43k");
    }
    return Result::success();
}

static Result text_roundtrip_check_44(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt44");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt44k");
    }
    return Result::success();
}

static Result text_roundtrip_check_45(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt45");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt45k");
    }
    return Result::success();
}

static Result text_roundtrip_check_46(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt46");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt46k");
    }
    return Result::success();
}

static Result text_roundtrip_check_47(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt47");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt47k");
    }
    return Result::success();
}

static Result text_roundtrip_check_48(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt48");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt48k");
    }
    return Result::success();
}

static Result text_roundtrip_check_49(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt49");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt49k");
    }
    return Result::success();
}

static Result text_roundtrip_check_50(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt50");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt50k");
    }
    return Result::success();
}

static Result text_roundtrip_check_51(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt51");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt51k");
    }
    return Result::success();
}

static Result text_roundtrip_check_52(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt52");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt52k");
    }
    return Result::success();
}

static Result text_roundtrip_check_53(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt53");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt53k");
    }
    return Result::success();
}

static Result text_roundtrip_check_54(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt54");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt54k");
    }
    return Result::success();
}

static Result text_roundtrip_check_55(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt55");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt55k");
    }
    return Result::success();
}

static Result text_roundtrip_check_56(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt56");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt56k");
    }
    return Result::success();
}

static Result text_roundtrip_check_57(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt57");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt57k");
    }
    return Result::success();
}

static Result text_roundtrip_check_58(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt58");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt58k");
    }
    return Result::success();
}

static Result text_roundtrip_check_59(uint16_t type_id, const std::vector<uint8_t>& p) {
    payload_fields::DecodedFields f;
    Result r = payload_fields::decode_all_fields(type_id, p, f);
    if (!r.ok()) return r;
    if (type_id == static_cast<uint16_t>(EventType::Counter)) {
        if (f.string_fields.count("name") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt59");
    }
    if (type_id == static_cast<uint16_t>(EventType::KeyValue)) {
        if (f.string_fields.count("key") == 0) return Result::fail(ErrorCode::InvalidPayload, "rt59k");
    }
    return Result::success();
}

static Result verify_all_roundtrip_types(const std::string& path) {
    EventLogReader reader(path);
    Result r = reader.open(); if (!r.ok()) return r;
    FileHeader h; r = reader.read_header(h); if (!r.ok()) return r;
    r = reader.seek_to_records(); if (!r.ok()) return r;
    static Result (*checks[60])(uint16_t, const std::vector<uint8_t>&) = {
        text_roundtrip_check_0,
        text_roundtrip_check_1,
        text_roundtrip_check_2,
        text_roundtrip_check_3,
        text_roundtrip_check_4,
        text_roundtrip_check_5,
        text_roundtrip_check_6,
        text_roundtrip_check_7,
        text_roundtrip_check_8,
        text_roundtrip_check_9,
        text_roundtrip_check_10,
        text_roundtrip_check_11,
        text_roundtrip_check_12,
        text_roundtrip_check_13,
        text_roundtrip_check_14,
        text_roundtrip_check_15,
        text_roundtrip_check_16,
        text_roundtrip_check_17,
        text_roundtrip_check_18,
        text_roundtrip_check_19,
        text_roundtrip_check_20,
        text_roundtrip_check_21,
        text_roundtrip_check_22,
        text_roundtrip_check_23,
        text_roundtrip_check_24,
        text_roundtrip_check_25,
        text_roundtrip_check_26,
        text_roundtrip_check_27,
        text_roundtrip_check_28,
        text_roundtrip_check_29,
        text_roundtrip_check_30,
        text_roundtrip_check_31,
        text_roundtrip_check_32,
        text_roundtrip_check_33,
        text_roundtrip_check_34,
        text_roundtrip_check_35,
        text_roundtrip_check_36,
        text_roundtrip_check_37,
        text_roundtrip_check_38,
        text_roundtrip_check_39,
        text_roundtrip_check_40,
        text_roundtrip_check_41,
        text_roundtrip_check_42,
        text_roundtrip_check_43,
        text_roundtrip_check_44,
        text_roundtrip_check_45,
        text_roundtrip_check_46,
        text_roundtrip_check_47,
        text_roundtrip_check_48,
        text_roundtrip_check_49,
        text_roundtrip_check_50,
        text_roundtrip_check_51,
        text_roundtrip_check_52,
        text_roundtrip_check_53,
        text_roundtrip_check_54,
        text_roundtrip_check_55,
        text_roundtrip_check_56,
        text_roundtrip_check_57,
        text_roundtrip_check_58,
        text_roundtrip_check_59,
    };
    uint32_t n = 0;
    while (!reader.eof()) {
        EventRecord rec;
        r = reader.read_next_record(rec);
        if (!r.ok()) { if (r.code == ErrorCode::UnexpectedEof) break; return r; }
        r = checks[n % 60](rec.type_id, rec.payload);
        if (!r.ok()) return r;
        ++n;
    }
    reader.close();
    return Result::success();
}

Result text_format_full_verify(const std::string& original, const std::string& reimported) {
    Result r = text_format_verify_roundtrip(original, reimported);
    if (!r.ok()) return r;
    return verify_all_roundtrip_types(reimported);
}

}  // namespace telltale

// Optimization pass 40 - memory iteration 1

// Optimization pass 30 - caching iteration 1
