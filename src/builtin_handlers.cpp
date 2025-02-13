#include "telltale/builtin_handlers.hpp"
#include "telltale/binary_io.hpp"
#include "telltale/crc32.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>

namespace telltale {

static ReplayContext* g_primary_context = nullptr;

ReplayState::ReplayState()
    : event_sequence(0), reset_count(0), last_computed_crc(0), checksum_verified(false) {}

void ReplayState::reset_all() {
    counters.clear();
    key_values.clear();
    timestamps.clear();
    print_log.clear();
    event_sequence = 0;
    reset_count = 0;
    last_computed_crc = 0;
    checksum_verified = false;
}

void ReplayState::reset_counters() { counters.clear(); }
void ReplayState::reset_key_values() { key_values.clear(); }
void ReplayState::reset_timestamps() { timestamps.clear(); }
void ReplayState::reset_print_log() { print_log.clear(); }

int64_t ReplayState::get_counter(const std::string& name) const {
    auto it = counters.find(name);
    return it != counters.end() ? it->second : 0;
}

int64_t ReplayState::get_key_value(const std::string& key) const {
    auto it = key_values.find(key);
    return it != key_values.end() ? it->second : 0;
}

void ReplayState::increment_counter(const std::string& name, int64_t delta) {
    counters[name] += delta;
    ++event_sequence;
}

void ReplayState::set_counter_absolute(const std::string& name, int64_t value) {
    counters[name] = value;
    ++event_sequence;
}

void ReplayState::set_key_value(const std::string& key, int64_t value, bool overwrite) {
    if (!overwrite && key_values.count(key) > 0) {
        return;
    }
    key_values[key] = value;
    ++event_sequence;
}

void ReplayState::add_timestamp(const std::string& label, uint64_t epoch_millis) {
    timestamps.emplace_back(label, epoch_millis);
    ++event_sequence;
}

void ReplayState::add_print(const std::string& message) {
    print_log.push_back(message);
    ++event_sequence;
}

uint32_t ReplayState::compute_state_crc(uint32_t scope_flags) const {
    Crc32 crc;
    if (scope_flags & 0x01) {
        for (const auto& kv : counters) {
            crc.update(reinterpret_cast<const uint8_t*>(kv.first.data()), kv.first.size());
            uint8_t buf[8];
            write_i64_le(buf, kv.second);
            crc.update(buf, 8);
        }
    }
    if (scope_flags & 0x02) {
        for (const auto& kv : key_values) {
            crc.update(reinterpret_cast<const uint8_t*>(kv.first.data()), kv.first.size());
            uint8_t buf[8];
            write_i64_le(buf, kv.second);
            crc.update(buf, 8);
        }
    }
    if (scope_flags & 0x04) {
        for (const auto& ts : timestamps) {
            crc.update(reinterpret_cast<const uint8_t*>(ts.first.data()), ts.first.size());
            uint8_t buf[8];
            write_u64_le(buf, ts.second);
            crc.update(buf, 8);
        }
    }
    return crc.finalize();
}

void ReplayState::print_stats(std::ostream& out, uint8_t output_flags,
                               const std::string& prefix) const {
    std::string pfx = prefix.empty() ? "" : prefix + ": ";
    if (output_flags & 0x01) {
        out << pfx << "=== Counters (" << counters.size() << ") ===" << std::endl;
        for (const auto& kv : counters) {
            out << pfx << "  " << kv.first << " = " << kv.second << std::endl;
        }
    }
    if (output_flags & 0x02) {
        out << pfx << "=== Key-Values (" << key_values.size() << ") ===" << std::endl;
        for (const auto& kv : key_values) {
            out << pfx << "  " << kv.first << " -> " << kv.second << std::endl;
        }
    }
    if (output_flags & 0x04) {
        out << pfx << "=== Timestamps (" << timestamps.size() << ") ===" << std::endl;
        for (const auto& ts : timestamps) {
            out << pfx << "  " << ts.first << " @ " << ts.second << " ms" << std::endl;
        }
    }
    if (output_flags & 0x08) {
        out << pfx << "=== Summary ===" << std::endl;
        out << pfx << "  events=" << event_sequence
            << " resets=" << reset_count
            << " prints=" << print_log.size() << std::endl;
    }
}

ReplayContext::ReplayContext()
    : output_(nullptr), null_buf_(), default_output_(&null_buf_),
      verbose_(false), dispatch_depth_(0),
      total_events_handled_(0), registry_(nullptr),
      inline_dispatch_(nullptr), inline_dispatch_ctx_(nullptr) {}

ReplayContext::~ReplayContext() = default;

void ReplayContext::set_inline_dispatcher(InlineDispatchFn fn, void* ctx) {
    inline_dispatch_ = fn;
    inline_dispatch_ctx_ = ctx;
}

Result ReplayContext::dispatch_inline(uint16_t type_id, const uint8_t* payload, size_t len) {
    if (inline_dispatch_) {
        inline_dispatch_(type_id, payload, len, inline_dispatch_ctx_);
        return Result::success();
    }
    if (registry_) {
        const HandlerEntry* entry = registry_->lookup(type_id);
        if (entry && entry->is_valid()) {
            entry->function(type_id, payload, len, entry->context);
            return Result::success();
        }
    }
    return Result::fail(ErrorCode::HandlerNotFound, "No inline dispatcher available");
}

static ReplayContext* get_context(void* ctx) {
    return static_cast<ReplayContext*>(ctx);
}

static const char* severity_name(uint8_t sev) {
    switch (sev) {
        case 0: return "DEBUG";
        case 1: return "INFO";
        case 2: return "WARN";
        case 3: return "ERROR";
        default: return "UNKNOWN";
    }
}

void handle_counter(uint16_t /*type_id*/, const uint8_t* payload, size_t payload_len,
                    void* context) {
    ReplayContext* ctx = get_context(context);
    if (!ctx) return;
    std::vector<uint8_t> vec(payload, payload + payload_len);
    CounterDelta delta;
    Result r = EventLogReader::decode_counter_payload(vec, delta);
    if (!r.ok()) {
        if (ctx->verbose()) {
            ctx->output() << "[counter] decode error: " << r.message << std::endl;
        }
        return;
    }
    if (delta.use_absolute) {
        ctx->state().set_counter_absolute(delta.name, delta.absolute);
        if (ctx->verbose()) {
            ctx->output() << "[counter] set " << delta.name << " = " << delta.absolute << std::endl;
        }
    } else {
        ctx->state().increment_counter(delta.name, delta.delta);
        if (ctx->verbose()) {
            ctx->output() << "[counter] " << delta.name << " += " << delta.delta
                          << " (now " << ctx->state().get_counter(delta.name) << ")" << std::endl;
        }
    }
}

void handle_keyvalue(uint16_t /*type_id*/, const uint8_t* payload, size_t payload_len,
                     void* context) {
    ReplayContext* ctx = get_context(context);
    if (!ctx) return;
    std::vector<uint8_t> vec(payload, payload + payload_len);
    KeyValueEntry entry;
    Result r = EventLogReader::decode_keyvalue_payload(vec, entry);
    if (!r.ok()) {
        if (ctx->verbose()) {
            ctx->output() << "[keyvalue] decode error: " << r.message << std::endl;
        }
        return;
    }
    ctx->state().set_key_value(entry.key, entry.value, entry.overwrite);
    if (ctx->verbose()) {
        ctx->output() << "[keyvalue] " << entry.key << " = " << entry.value << std::endl;
    }
}

void handle_timestamp(uint16_t /*type_id*/, const uint8_t* payload, size_t payload_len,
                      void* context) {
    ReplayContext* ctx = get_context(context);
    if (!ctx) return;
    std::vector<uint8_t> vec(payload, payload + payload_len);
    TimestampMarker marker;
    Result r = EventLogReader::decode_timestamp_payload(vec, marker);
    if (!r.ok()) {
        if (ctx->verbose()) {
            ctx->output() << "[timestamp] decode error: " << r.message << std::endl;
        }
        return;
    }
    uint64_t millis = marker.epoch_millis;
    if (!marker.use_explicit_time) {
        auto now = std::chrono::system_clock::now();
        millis = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count());
    }
    ctx->state().add_timestamp(marker.label, millis);
    if (ctx->verbose()) {
        ctx->output() << "[timestamp] " << marker.label << " @ " << millis << " ms" << std::endl;
    }
}

void handle_checksum(uint16_t /*type_id*/, const uint8_t* payload, size_t payload_len,
                     void* context) {
    ReplayContext* ctx = get_context(context);
    if (!ctx) return;
    std::vector<uint8_t> vec(payload, payload + payload_len);
    ChecksumSpec spec;
    Result r = EventLogReader::decode_checksum_payload(vec, spec);
    if (!r.ok()) {
        if (ctx->verbose()) {
            ctx->output() << "[checksum] decode error: " << r.message << std::endl;
        }
        return;
    }
    uint32_t computed = ctx->state().compute_state_crc(spec.scope_flags);
    ctx->state().last_computed_crc = computed;
    bool match = (computed == spec.expected_crc);
    ctx->state().checksum_verified = match;
    if (ctx->verbose()) {
        ctx->output() << "[checksum] " << spec.label << ": expected=0x" << std::hex
                      << spec.expected_crc << " computed=0x" << computed << std::dec
                      << (match ? " OK" : " MISMATCH") << std::endl;
    }
}

void handle_batch(uint16_t /*type_id*/, const uint8_t* payload, size_t payload_len,
                  void* context) {
    ReplayContext* ctx = get_context(context);
    if (!ctx) return;
    std::vector<uint8_t> vec(payload, payload + payload_len);
    BatchHeader header;
    Result r = EventLogReader::decode_batch_header(vec, header);
    if (!r.ok()) {
        if (ctx->verbose()) {
            ctx->output() << "[batch] decode error: " << r.message << std::endl;
        }
        return;
    }
    size_t offset = 4;
    int depth = ctx->dispatch_depth();
    ctx->set_dispatch_depth(depth + 1);
    if (ctx->verbose()) {
        ctx->output() << "[batch] processing " << header.sub_event_count
                      << " sub-events (depth=" << depth + 1 << ")" << std::endl;
    }
    for (uint32_t i = 0; i < header.sub_event_count; ++i) {
        if (offset + 6 > vec.size()) break;
        uint16_t sub_type = read_u16_le(vec.data() + offset);
        uint32_t sub_len = read_u32_le(vec.data() + offset + 2);
        offset += 6;
        if (offset + sub_len > vec.size()) break;
        ctx->dispatch_inline(sub_type, vec.data() + offset, sub_len);
        offset += sub_len;
    }
    ctx->set_dispatch_depth(depth);
}

void handle_reset(uint16_t /*type_id*/, const uint8_t* payload, size_t payload_len,
                  void* context) {
    ReplayContext* ctx = get_context(context);
    if (!ctx) return;
    std::vector<uint8_t> vec(payload, payload + payload_len);
    uint8_t scope = 0xFF;
    Result r = EventLogReader::decode_reset_payload(vec, scope);
    if (!r.ok()) return;
    if (scope == 0xFF) {
        ctx->state().reset_all();
    } else {
        if (scope & 0x01) ctx->state().reset_counters();
        if (scope & 0x02) ctx->state().reset_key_values();
        if (scope & 0x04) ctx->state().reset_timestamps();
        if (scope & 0x08) ctx->state().reset_print_log();
    }
    ++ctx->state().reset_count;
    if (ctx->verbose()) {
        ctx->output() << "[reset] scope=0x" << std::hex << static_cast<int>(scope)
                      << std::dec << std::endl;
    }
}

void handle_print(uint16_t /*type_id*/, const uint8_t* payload, size_t payload_len,
                  void* context) {
    ReplayContext* ctx = get_context(context);
    if (!ctx) return;
    std::vector<uint8_t> vec(payload, payload + payload_len);
    PrintMessage msg;
    Result r = EventLogReader::decode_print_payload(vec, msg);
    if (!r.ok()) return;
    ctx->state().add_print(msg.message);
    ctx->output() << "[" << severity_name(msg.severity) << "] " << msg.message << std::endl;
}

void handle_stats(uint16_t /*type_id*/, const uint8_t* payload, size_t payload_len,
                  void* context) {
    ReplayContext* ctx = get_context(context);
    if (!ctx) return;
    std::vector<uint8_t> vec(payload, payload + payload_len);
    StatsRequest req;
    Result r = EventLogReader::decode_stats_payload(vec, req);
    if (!r.ok()) return;
    uint8_t flags = req.output_flags;
    if (flags == 0) flags = 0x0F;
    ctx->state().print_stats(ctx->output(), flags, req.prefix);
}

void handle_noop(uint16_t type_id, const uint8_t* /*payload*/, size_t /*payload_len*/,
                 void* context) {
    ReplayContext* ctx = get_context(context);
    if (ctx && ctx->verbose()) {
        ctx->output() << "[noop] type 0x" << std::hex << type_id << std::dec << std::endl;
    }
}

void register_builtin_handlers(HandlerRegistry& registry, ReplayContext* context) {
    g_primary_context = context;
    if (context) {
        context->set_registry(&registry);
    }
    registry.register_handler(
        static_cast<uint16_t>(EventType::Counter), handle_counter, context,
        static_cast<uint16_t>(HandlerId::BuiltinCounter));
    registry.register_handler(
        static_cast<uint16_t>(EventType::KeyValue), handle_keyvalue, context,
        static_cast<uint16_t>(HandlerId::BuiltinKeyValue));
    registry.register_handler(
        static_cast<uint16_t>(EventType::Timestamp), handle_timestamp, context,
        static_cast<uint16_t>(HandlerId::BuiltinTimestamp));
    registry.register_handler(
        static_cast<uint16_t>(EventType::Checksum), handle_checksum, context,
        static_cast<uint16_t>(HandlerId::BuiltinChecksum));
    registry.register_handler(
        static_cast<uint16_t>(EventType::Batch), handle_batch, context,
        static_cast<uint16_t>(HandlerId::BuiltinBatch));
    registry.register_handler(
        static_cast<uint16_t>(EventType::Reset), handle_reset, context,
        static_cast<uint16_t>(HandlerId::BuiltinReset));
    registry.register_handler(
        static_cast<uint16_t>(EventType::Print), handle_print, context,
        static_cast<uint16_t>(HandlerId::BuiltinPrint));
    registry.register_handler(
        static_cast<uint16_t>(EventType::Stats), handle_stats, context,
        static_cast<uint16_t>(HandlerId::BuiltinStats));
}

HandlerEntry::HandlerFn resolve_builtin_handler(uint16_t handler_id,
                                                 ReplayContext** out_context) {
    if (out_context) {
        *out_context = g_primary_context;
    }
    switch (handler_id) {
        case static_cast<uint16_t>(HandlerId::BuiltinCounter):   return handle_counter;
        case static_cast<uint16_t>(HandlerId::BuiltinKeyValue):  return handle_keyvalue;
        case static_cast<uint16_t>(HandlerId::BuiltinTimestamp): return handle_timestamp;
        case static_cast<uint16_t>(HandlerId::BuiltinChecksum):  return handle_checksum;
        case static_cast<uint16_t>(HandlerId::BuiltinBatch):     return handle_batch;
        case static_cast<uint16_t>(HandlerId::BuiltinReset):     return handle_reset;
        case static_cast<uint16_t>(HandlerId::BuiltinPrint):     return handle_print;
        case static_cast<uint16_t>(HandlerId::BuiltinStats):     return handle_stats;
        case static_cast<uint16_t>(HandlerId::BuiltinNoOp):      return handle_noop;
        default: return nullptr;
    }
}

}  // namespace telltale

// Optimization pass 40 - memory iteration 1
