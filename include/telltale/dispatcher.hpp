#pragma once

#include "telltale/types.hpp"
#include "telltale/errors.hpp"
#include "telltale/handler_registry.hpp"
#include "telltale/binary_io.hpp"
#include <string>
#include <vector>
#include <functional>
#include <ostream>

namespace telltale {

class ReplayContext;

class Dispatcher {
public:
    Dispatcher();
    ~Dispatcher();

    Dispatcher(const Dispatcher&) = delete;
    Dispatcher& operator=(const Dispatcher&) = delete;

    HandlerRegistry& registry() { return registry_; }
    const HandlerRegistry& registry() const { return registry_; }

    Result replay_file(const std::string& path, ReplayMode mode);
    Result replay_buffer(const uint8_t* data, size_t length, ReplayMode mode);
    Result dispatch_record(const EventRecord& record, ReplayMode mode);
    Result dispatch_payload(uint16_t type_id, const std::vector<uint8_t>& payload,
                            ReplayMode mode);

    void set_output_stream(std::ostream* out);
    void set_verbose(bool verbose);
    bool verbose() const { return verbose_; }

    uint32_t records_processed() const { return records_processed_; }
    uint32_t handlers_invoked() const { return handlers_invoked_; }
    uint32_t schema_updates_applied() const { return schema_updates_applied_; }
    uint32_t crc_errors() const { return crc_errors_; }
    uint32_t missing_handlers() const { return missing_handlers_; }

    void reset_stats();

    using ProgressCallback = std::function<void(uint32_t index, uint16_t type_id,
                                                 size_t payload_size)>;
    void set_progress_callback(ProgressCallback callback);

    struct ReplayReport {
        uint32_t total_records;
        uint32_t records_processed;
        uint32_t handlers_invoked;
        uint32_t schema_updates;
        uint32_t crc_errors;
        uint32_t missing_handlers;
        uint32_t verify_only_skipped;
        bool success;
        std::string error_message;
    };

    const ReplayReport& last_report() const { return last_report_; }

private:
    HandlerRegistry registry_;
    std::ostream* output_;
    bool verbose_;
    uint32_t records_processed_;
    uint32_t handlers_invoked_;
    uint32_t schema_updates_applied_;
    uint32_t crc_errors_;
    uint32_t missing_handlers_;
    uint32_t verify_only_skipped_;
    ProgressCallback progress_callback_;
    ReplayReport last_report_;
    ReplayContext* replay_context_;
    std::vector<std::vector<uint8_t>> pending_schema_updates_;

    Result process_schema_update(const std::vector<uint8_t>& payload, ReplayMode mode);
    Result invoke_handler(uint16_t type_id, const uint8_t* payload,
                          size_t payload_len, ReplayMode mode);
    void flush_pending_schema_updates(ReplayMode mode);
    void log_dispatch(uint16_t type_id, size_t payload_len) const;
    void finalize_report(bool success, const std::string& error = "");
    Result replay_from_reader(EventLogReader& reader, ReplayMode mode,
                              const char* source_label);
};

}  // namespace telltale
