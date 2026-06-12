#pragma once

#include "telltale/types.hpp"
#include "telltale/errors.hpp"
#include "telltale/handler_registry.hpp"
#include <string>
#include <map>
#include <vector>
#include <ostream>
#include <cstdint>
#include <mutex>

#include <streambuf>

namespace telltale {

class NullStreamBuf : public std::streambuf {
protected:
    int overflow(int c) override { return c; }
};

struct ReplayState {
    std::map<std::string, int64_t> counters;
    std::map<std::string, int64_t> key_values;
    std::vector<std::pair<std::string, uint64_t>> timestamps;
    std::vector<std::string> print_log;
    uint64_t event_sequence;
    uint64_t reset_count;
    uint32_t last_computed_crc;
    bool checksum_verified;

    ReplayState();

    void reset_all();
    void reset_counters();
    void reset_key_values();
    void reset_timestamps();
    void reset_print_log();

    int64_t get_counter(const std::string& name) const;
    int64_t get_key_value(const std::string& key) const;

    void increment_counter(const std::string& name, int64_t delta);
    void set_counter_absolute(const std::string& name, int64_t value);
    void set_key_value(const std::string& key, int64_t value, bool overwrite);

    void add_timestamp(const std::string& label, uint64_t epoch_millis);
    void add_print(const std::string& message);

    uint32_t compute_state_crc(uint32_t scope_flags) const;
    void print_stats(std::ostream& out, uint8_t output_flags,
                     const std::string& prefix) const;

    size_t counter_count() const { return counters.size(); }
    size_t key_value_count() const { return key_values.size(); }
    size_t timestamp_count() const { return timestamps.size(); }
};

class ReplayContext {
public:
    ReplayContext();
    ~ReplayContext();

    ReplayState& state() { return state_; }
    const ReplayState& state() const { return state_; }

    void set_output(std::ostream* out) { output_ = out; }
    std::ostream& output() { return output_ ? *output_ : default_output_; }

    void set_verbose(bool v) { verbose_ = v; }
    bool verbose() const { return verbose_; }

    void set_dispatch_depth(int depth) { dispatch_depth_ = depth; }
    int dispatch_depth() const { return dispatch_depth_; }

    uint64_t total_events_handled() const { return total_events_handled_; }
    void increment_events_handled() { ++total_events_handled_; }

    class HandlerRegistry* registry() { return registry_; }
    void set_registry(class HandlerRegistry* reg) { registry_ = reg; }

    using InlineDispatchFn = void (*)(uint16_t, const uint8_t*, size_t, void*);
    void set_inline_dispatcher(InlineDispatchFn fn, void* ctx);

    Result dispatch_inline(uint16_t type_id, const uint8_t* payload, size_t len);

private:
    ReplayState state_;
    std::ostream* output_;
    NullStreamBuf null_buf_;
    std::ostream default_output_;
    bool verbose_;
    int dispatch_depth_;
    uint64_t total_events_handled_;
    class HandlerRegistry* registry_;
    InlineDispatchFn inline_dispatch_;
    void* inline_dispatch_ctx_;
};

void register_builtin_handlers(HandlerRegistry& registry, ReplayContext* context);
HandlerEntry::HandlerFn resolve_builtin_handler(uint16_t handler_id, ReplayContext** out_context);

}  // namespace telltale
