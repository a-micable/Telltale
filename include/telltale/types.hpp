#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <ostream>
#include <istream>
#include <fstream>
#include <chrono>
#include <limits>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <stdexcept>
#include <optional>
#include <array>
#include <unordered_map>
#include <set>
#include <deque>
#include <numeric>

namespace telltale {

constexpr char MAGIC[4] = {'T', 'L', 'T', 'L'};
constexpr uint16_t FORMAT_VERSION = 1;
constexpr uint16_t SCHEMA_UPDATE_TYPE = 0xFFFF;
constexpr size_t HEADER_SIZE = 14;
constexpr size_t RECORD_HEADER_SIZE = 6;
constexpr size_t RECORD_CRC_SIZE = 4;
constexpr size_t MAX_PAYLOAD_SIZE = 16 * 1024 * 1024;
constexpr size_t MAX_HANDLER_TABLE_SIZE = 65536;
constexpr size_t INITIAL_HANDLER_TABLE_SIZE = 256;
constexpr size_t MAX_NAME_LENGTH = 4096;
constexpr size_t MAX_BATCH_SUB_EVENTS = 10000;
constexpr size_t MAX_PRINT_LENGTH = 65536;
constexpr size_t MAX_KEY_LENGTH = 4096;
constexpr size_t MAX_COUNTERS = 100000;
constexpr size_t MAX_KEY_VALUES = 100000;
constexpr size_t MAX_TIMESTAMPS = 100000;

enum class EventType : uint16_t {
    Counter     = 0x0001,
    KeyValue    = 0x0002,
    Timestamp   = 0x0003,
    Checksum    = 0x0004,
    Batch       = 0x0005,
    Reset       = 0x0006,
    Print       = 0x0007,
    Stats       = 0x0008,
    SchemaUpdate = 0xFFFF
};

enum class SchemaUpdateFlag : uint8_t {
    Register   = 0x01,
    Deregister = 0x02,
    Replace    = 0x04,
    ResizeTable = 0x08
};

enum class HandlerId : uint16_t {
    None              = 0x0000,
    BuiltinCounter    = 0x0001,
    BuiltinKeyValue   = 0x0002,
    BuiltinTimestamp  = 0x0003,
    BuiltinChecksum   = 0x0004,
    BuiltinBatch      = 0x0005,
    BuiltinReset      = 0x0006,
    BuiltinPrint      = 0x0007,
    BuiltinStats      = 0x0008,
    BuiltinNoOp       = 0x00FF
};

enum class ReplayMode {
    Execute,
    VerifyOnly
};

struct EventRecord {
    uint16_t type_id;
    std::vector<uint8_t> payload;
    uint32_t crc32;
};

struct SchemaUpdatePayload {
    uint8_t flags;
    uint16_t type_id;
    uint16_t handler_id;
    bool has_handler_id;
    uint32_t table_size;
    bool has_table_size;
};

struct HandlerEntry {
    using HandlerFn = void (*)(uint16_t type_id,
                               const uint8_t* payload,
                               size_t payload_len,
                               void* context);

    HandlerFn function;
    void* context;
    uint16_t handler_id;
    bool active;

    HandlerEntry()
        : function(nullptr), context(nullptr), handler_id(0), active(false) {}

    HandlerEntry(HandlerFn fn, void* ctx, uint16_t id)
        : function(fn), context(ctx), handler_id(id), active(fn != nullptr) {}

    bool is_valid() const {
        return active && function != nullptr;
    }
};

struct FileHeader {
    char magic[4];
    uint16_t version;
    uint32_t record_count;
    uint32_t header_crc;

    FileHeader()
        : version(0), record_count(0), header_crc(0) {
        std::memcpy(magic, MAGIC, 4);
    }
};

struct CounterDelta {
    std::string name;
    int64_t delta;
    int64_t absolute;
    bool use_absolute;
};

struct KeyValueEntry {
    std::string key;
    int64_t value;
    bool overwrite;
};

struct TimestampMarker {
    std::string label;
    uint64_t epoch_millis;
    bool use_explicit_time;
};

struct ChecksumSpec {
    std::string label;
    uint32_t expected_crc;
    uint32_t scope_flags;
};

struct BatchHeader {
    uint32_t sub_event_count;
};

struct PrintMessage {
    std::string message;
    uint8_t severity;
};

struct StatsRequest {
    uint8_t output_flags;
    std::string prefix;
};

inline const char* event_type_name(uint16_t type_id) {
    switch (type_id) {
        case static_cast<uint16_t>(EventType::Counter):     return "Counter";
        case static_cast<uint16_t>(EventType::KeyValue):    return "KeyValue";
        case static_cast<uint16_t>(EventType::Timestamp):   return "Timestamp";
        case static_cast<uint16_t>(EventType::Checksum):    return "Checksum";
        case static_cast<uint16_t>(EventType::Batch):       return "Batch";
        case static_cast<uint16_t>(EventType::Reset):       return "Reset";
        case static_cast<uint16_t>(EventType::Print):       return "Print";
        case static_cast<uint16_t>(EventType::Stats):       return "Stats";
        case static_cast<uint16_t>(EventType::SchemaUpdate): return "SchemaUpdate";
        default: return "Unknown";
    }
}

inline const char* handler_id_name(uint16_t handler_id) {
    switch (handler_id) {
        case static_cast<uint16_t>(HandlerId::BuiltinCounter):   return "BuiltinCounter";
        case static_cast<uint16_t>(HandlerId::BuiltinKeyValue):  return "BuiltinKeyValue";
        case static_cast<uint16_t>(HandlerId::BuiltinTimestamp): return "BuiltinTimestamp";
        case static_cast<uint16_t>(HandlerId::BuiltinChecksum):    return "BuiltinChecksum";
        case static_cast<uint16_t>(HandlerId::BuiltinBatch):     return "BuiltinBatch";
        case static_cast<uint16_t>(HandlerId::BuiltinReset):     return "BuiltinReset";
        case static_cast<uint16_t>(HandlerId::BuiltinPrint):     return "BuiltinPrint";
        case static_cast<uint16_t>(HandlerId::BuiltinStats):     return "BuiltinStats";
        case static_cast<uint16_t>(HandlerId::BuiltinNoOp):      return "BuiltinNoOp";
        default: return "Unknown";
    }
}

inline uint16_t read_u16_le(const uint8_t* data) {
    return static_cast<uint16_t>(data[0])
         | (static_cast<uint16_t>(data[1]) << 8);
}

inline uint32_t read_u32_le(const uint8_t* data) {
    return static_cast<uint32_t>(data[0])
         | (static_cast<uint32_t>(data[1]) << 8)
         | (static_cast<uint32_t>(data[2]) << 16)
         | (static_cast<uint32_t>(data[3]) << 24);
}

inline uint64_t read_u64_le(const uint8_t* data) {
    return static_cast<uint64_t>(data[0])
         | (static_cast<uint64_t>(data[1]) << 8)
         | (static_cast<uint64_t>(data[2]) << 16)
         | (static_cast<uint64_t>(data[3]) << 24)
         | (static_cast<uint64_t>(data[4]) << 32)
         | (static_cast<uint64_t>(data[5]) << 40)
         | (static_cast<uint64_t>(data[6]) << 48)
         | (static_cast<uint64_t>(data[7]) << 56);
}

inline int64_t read_i64_le(const uint8_t* data) {
    return static_cast<int64_t>(read_u64_le(data));
}

inline void write_u16_le(uint8_t* data, uint16_t value) {
    data[0] = static_cast<uint8_t>(value & 0xFF);
    data[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

inline void write_u32_le(uint8_t* data, uint32_t value) {
    data[0] = static_cast<uint8_t>(value & 0xFF);
    data[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    data[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

inline void write_u64_le(uint8_t* data, uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        data[i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
    }
}

inline void write_i64_le(uint8_t* data, int64_t value) {
    write_u64_le(data, static_cast<uint64_t>(value));
}

}  // namespace telltale
