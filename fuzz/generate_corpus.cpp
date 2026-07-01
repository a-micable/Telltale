#include "telltale/binary_io.hpp"
#include "telltale/schema_update.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using namespace telltale;

static bool write_log(const std::string& path,
                      const std::vector<std::pair<uint16_t, std::vector<uint8_t>>>& records) {
    EventLogWriter writer(path);
    if (!writer.open().ok()) return false;
    if (!writer.write_header().ok()) return false;
    for (const auto& rec : records) {
        if (!writer.write_event(rec.first, rec.second).ok()) return false;
    }
    return writer.finalize().ok();
}

static std::vector<uint8_t> encode_batch_with_types(
    const std::vector<std::pair<uint16_t, std::vector<uint8_t>>>& subs) {
    std::vector<uint8_t> result(4);
    write_u32_le(result.data(), static_cast<uint32_t>(subs.size()));
    for (const auto& sub : subs) {
        size_t offset = result.size();
        result.resize(offset + 6 + sub.second.size());
        write_u16_le(result.data() + offset, sub.first);
        write_u32_le(result.data() + offset + 2,
                      static_cast<uint32_t>(sub.second.size()));
        if (!sub.second.empty()) {
            std::memcpy(result.data() + offset + 6, sub.second.data(), sub.second.size());
        }
    }
    return result;
}

static std::vector<uint8_t> schema_payload(uint8_t flags, uint16_t type_id,
                                           uint16_t handler_id) {
    return SchemaUpdateCodec::encode(flags, type_id, handler_id,
                                     SchemaUpdateCodec::requires_handler_id(flags));
}

int main() {
    const std::string corpus_dir = "fuzz/corpus";
    int failures = 0;

    // 1. Minimal log: header + one counter event.
    {
        std::vector<std::pair<uint16_t, std::vector<uint8_t>>> records;
        records.emplace_back(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("hits", 1, true));
        if (!write_log(corpus_dir + "/01_minimal_counter.bin", records)) {
            ++failures;
        }
    }

    // 2. All eight built-in event types in sequence.
    {
        std::vector<std::pair<uint16_t, std::vector<uint8_t>>> records;
        records.emplace_back(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("c", 1, true));
        records.emplace_back(static_cast<uint16_t>(EventType::KeyValue),
            EventLogWriter::encode_keyvalue_payload("k", 42, true));
        records.emplace_back(static_cast<uint16_t>(EventType::Timestamp),
            EventLogWriter::encode_timestamp_payload("t", 1700000000ULL, true));
        records.emplace_back(static_cast<uint16_t>(EventType::Checksum),
            EventLogWriter::encode_checksum_payload("chk", 0, 0x01));
        std::vector<std::pair<uint16_t, std::vector<uint8_t>>> batch_subs;
        batch_subs.emplace_back(static_cast<uint16_t>(EventType::Print),
            EventLogWriter::encode_print_payload("nested", 1));
        records.emplace_back(static_cast<uint16_t>(EventType::Batch),
            encode_batch_with_types(batch_subs));
        records.emplace_back(static_cast<uint16_t>(EventType::Reset),
            EventLogWriter::encode_reset_payload(0x01));
        records.emplace_back(static_cast<uint16_t>(EventType::Print),
            EventLogWriter::encode_print_payload("hello", 1));
        records.emplace_back(static_cast<uint16_t>(EventType::Stats),
            EventLogWriter::encode_stats_payload(0x0F, "pfx"));
        if (!write_log(corpus_dir + "/02_all_builtin_types.bin", records)) {
            ++failures;
        }
    }

    // 3. Schema register + event of registered type.
    {
        constexpr uint16_t kType = 0x0200;
        std::vector<std::pair<uint16_t, std::vector<uint8_t>>> records;
        records.emplace_back(SCHEMA_UPDATE_TYPE,
            schema_payload(static_cast<uint8_t>(SchemaUpdateFlag::Register), kType,
                           static_cast<uint16_t>(HandlerId::BuiltinPrint)));
        records.emplace_back(kType, EventLogWriter::encode_print_payload("registered", 1));
        if (!write_log(corpus_dir + "/03_schema_register.bin", records)) {
            ++failures;
        }
    }

    // 4. Schema deregister + event of same type.
    {
        constexpr uint16_t kType = 0x0300;
        std::vector<std::pair<uint16_t, std::vector<uint8_t>>> records;
        records.emplace_back(SCHEMA_UPDATE_TYPE,
            schema_payload(static_cast<uint8_t>(SchemaUpdateFlag::Register), kType,
                           static_cast<uint16_t>(HandlerId::BuiltinNoOp)));
        records.emplace_back(SCHEMA_UPDATE_TYPE,
            schema_payload(static_cast<uint8_t>(SchemaUpdateFlag::Deregister), kType, 0));
        records.emplace_back(kType, EventLogWriter::encode_print_payload("orphan", 1));
        if (!write_log(corpus_dir + "/04_schema_deregister.bin", records)) {
            ++failures;
        }
    }

    // 5. Two consecutive schema updates targeting the same type ID.
    {
        constexpr uint16_t kType = 0x0500;
        std::vector<std::pair<uint16_t, std::vector<uint8_t>>> records;
        records.emplace_back(SCHEMA_UPDATE_TYPE,
            schema_payload(static_cast<uint8_t>(SchemaUpdateFlag::Register), kType,
                           static_cast<uint16_t>(HandlerId::BuiltinNoOp)));
        records.emplace_back(SCHEMA_UPDATE_TYPE,
            schema_payload(static_cast<uint8_t>(SchemaUpdateFlag::Deregister), kType, 0));
        records.emplace_back(SCHEMA_UPDATE_TYPE,
            schema_payload(static_cast<uint8_t>(SchemaUpdateFlag::Deregister), kType, 0));
        if (!write_log(corpus_dir + "/05_double_deregister.bin", records)) {
            ++failures;
        }
    }

    // 6. Batch with nested sub-events including a schema update.
    {
        constexpr uint16_t kType = 0x0600;
        std::vector<std::pair<uint16_t, std::vector<uint8_t>>> batch_subs;
        batch_subs.emplace_back(SCHEMA_UPDATE_TYPE,
            schema_payload(static_cast<uint8_t>(SchemaUpdateFlag::Register), kType,
                           static_cast<uint16_t>(HandlerId::BuiltinPrint)));
        batch_subs.emplace_back(kType,
            EventLogWriter::encode_print_payload("from batch", 1));
        std::vector<std::pair<uint16_t, std::vector<uint8_t>>> records;
        records.emplace_back(static_cast<uint16_t>(EventType::Batch),
            encode_batch_with_types(batch_subs));
        if (!write_log(corpus_dir + "/06_batch_nested_schema.bin", records)) {
            ++failures;
        }
    }

    // 7. Nested batch: register, warm cache, deregister, reuse cached handler (UAF).
    {
        constexpr uint16_t kType = 0x0700;
        std::vector<std::pair<uint16_t, std::vector<uint8_t>>> batch_subs;
        batch_subs.emplace_back(SCHEMA_UPDATE_TYPE,
            schema_payload(static_cast<uint8_t>(SchemaUpdateFlag::Register), kType,
                           static_cast<uint16_t>(HandlerId::BuiltinPrint)));
        batch_subs.emplace_back(kType,
            EventLogWriter::encode_print_payload("warm cache", 1));
        batch_subs.emplace_back(SCHEMA_UPDATE_TYPE,
            schema_payload(static_cast<uint8_t>(SchemaUpdateFlag::Deregister), kType, 0));
        batch_subs.emplace_back(kType,
            EventLogWriter::encode_print_payload("stale cache", 1));
        std::vector<std::pair<uint16_t, std::vector<uint8_t>>> records;
        records.emplace_back(static_cast<uint16_t>(EventType::Batch),
            encode_batch_with_types(batch_subs));
        if (!write_log(corpus_dir + "/07_batch_cache_uaf.bin", records)) {
            ++failures;
        }
    }

    if (failures != 0) {
        std::fprintf(stderr, "corpus generation failed (%d files)\n", failures);
        return 1;
    }
    return 0;
}
