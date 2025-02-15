#include "telltale/dispatcher.hpp"
#include "telltale/schema_update.hpp"
#include "telltale/builtin_handlers.hpp"
#include <iostream>

namespace telltale {

static std::pair<HandlerEntry::HandlerFn, void*> builtin_resolver(uint16_t handler_id) {
    ReplayContext* ctx = nullptr;
    HandlerEntry::HandlerFn fn = resolve_builtin_handler(handler_id, &ctx);
    return {fn, static_cast<void*>(ctx)};
}

Dispatcher::Dispatcher()
    : output_(&std::cout),
      verbose_(false),
      records_processed_(0),
      handlers_invoked_(0),
      schema_updates_applied_(0),
      crc_errors_(0),
      missing_handlers_(0),
      verify_only_skipped_(0),
      replay_context_(nullptr),
      current_replay_mode_(ReplayMode::Execute),
      cached_handler_entry_(nullptr),
      cached_handler_type_(0xFFFF) {
    replay_context_ = new ReplayContext();
    replay_context_->set_output(output_);
    replay_context_->set_registry(&registry_);
    replay_context_->set_inline_dispatcher(&Dispatcher::inline_dispatch_bridge, this);
    registry_.set_handler_resolver(builtin_resolver);
    register_builtin_handlers(registry_, replay_context_);
}

Dispatcher::~Dispatcher() {
    delete replay_context_;
    replay_context_ = nullptr;
}

void Dispatcher::set_output_stream(std::ostream* out) {
    output_ = out ? out : &std::cout;
    replay_context_->set_output(output_);
}

void Dispatcher::set_verbose(bool verbose) {
    verbose_ = verbose;
    replay_context_->set_verbose(verbose);
}

void Dispatcher::reset_stats() {
    records_processed_ = 0;
    handlers_invoked_ = 0;
    schema_updates_applied_ = 0;
    crc_errors_ = 0;
    missing_handlers_ = 0;
    verify_only_skipped_ = 0;
}

void Dispatcher::set_progress_callback(ProgressCallback callback) {
    progress_callback_ = std::move(callback);
}

void Dispatcher::log_dispatch(uint16_t type_id, size_t payload_len) const {
    if (verbose_ && output_) {
        *output_ << "[dispatch] type=0x" << std::hex << type_id << std::dec
                 << " (" << event_type_name(type_id) << ")"
                 << " payload=" << payload_len << " bytes" << std::endl;
    }
}

void Dispatcher::finalize_report(bool success, const std::string& error) {
    last_report_.total_records = records_processed_;
    last_report_.records_processed = records_processed_;
    last_report_.handlers_invoked = handlers_invoked_;
    last_report_.schema_updates = schema_updates_applied_;
    last_report_.crc_errors = crc_errors_;
    last_report_.missing_handlers = missing_handlers_;
    last_report_.verify_only_skipped = verify_only_skipped_;
    last_report_.success = success;
    last_report_.error_message = error;
}

void Dispatcher::inline_dispatch_bridge(uint16_t type_id, const uint8_t* payload,
                                       size_t len, void* ctx) {
    Dispatcher* dispatcher = static_cast<Dispatcher*>(ctx);
    std::vector<uint8_t> payload_copy(payload, payload + len);
    dispatcher->dispatch_payload(type_id, payload_copy, dispatcher->current_replay_mode_);
}

void Dispatcher::flush_pending_schema_updates(ReplayMode mode) {
    for (const auto& pending : pending_schema_updates_) {
        SchemaUpdatePayload update;
        Result dr = SchemaUpdateCodec::decode(pending, update);
        if (!dr.ok()) {
            continue;
        }
        if (mode == ReplayMode::VerifyOnly) {
            ++schema_updates_applied_;
            ++verify_only_skipped_;
            continue;
        }
        registry_.apply_schema_update(update);
        ++schema_updates_applied_;
    }
    pending_schema_updates_.clear();
}

Result Dispatcher::process_schema_update(const std::vector<uint8_t>& payload,
                                          ReplayMode mode) {
    SchemaUpdatePayload update;
    Result dr = SchemaUpdateCodec::decode(payload, update);
    if (!dr.ok()) {
        return dr;
    }

    if (mode == ReplayMode::VerifyOnly) {
        ++schema_updates_applied_;
        ++verify_only_skipped_;
        if (verbose_ && output_) {
            *output_ << "[verify] schema update: "
                     << SchemaUpdateCodec::flags_to_string(update.flags)
                     << " type=0x" << std::hex << update.type_id << std::dec;
            if (update.has_handler_id) {
                *output_ << " handler=" << handler_id_name(update.handler_id);
            }
            *output_ << std::endl;
        }
        return Result::success();
    }

    Result ar = registry_.apply_schema_update(update);
    if (!ar.ok()) {
        return ar;
    }
    ++schema_updates_applied_;
    if (verbose_ && output_) {
        *output_ << "[schema] applied " << SchemaUpdateCodec::flags_to_string(update.flags)
                 << " for type 0x" << std::hex << update.type_id << std::dec << std::endl;
    }
    return Result::success();
}

Result Dispatcher::invoke_handler(uint16_t type_id, const uint8_t* payload,
                                   size_t payload_len, ReplayMode mode) {
    if (mode == ReplayMode::VerifyOnly) {
        const HandlerEntry* entry = registry_.lookup(type_id);
        if (entry == nullptr) {
            ++missing_handlers_;
            registry_.invoke_no_handler(type_id);
        } else {
            ++verify_only_skipped_;
        }
        return Result::success();
    }

    const HandlerEntry* entry = nullptr;
    if (type_id == cached_handler_type_ && cached_handler_entry_ != nullptr) {
        entry = cached_handler_entry_;
    } else {
        entry = registry_.lookup(type_id);
        cached_handler_type_ = type_id;
        cached_handler_entry_ = entry;
    }
    if (entry == nullptr) {
        ++missing_handlers_;
        registry_.invoke_no_handler(type_id);
        if (verbose_ && output_) {
            *output_ << "[warn] no handler for type 0x" << std::hex << type_id
                     << std::dec << std::endl;
        }
        return Result::success();
    }

    entry->function(type_id, payload, payload_len, entry->context);
    ++handlers_invoked_;
    replay_context_->increment_events_handled();
    return Result::success();
}

Result Dispatcher::dispatch_payload(uint16_t type_id,
                                     const std::vector<uint8_t>& payload,
                                     ReplayMode mode) {
    current_replay_mode_ = mode;
    log_dispatch(type_id, payload.size());

    if (type_id == SCHEMA_UPDATE_TYPE) {
        return process_schema_update(payload, mode);
    }

    return invoke_handler(type_id, payload.data(), payload.size(), mode);
}

Result Dispatcher::dispatch_record(const EventRecord& record, ReplayMode mode) {
    ++records_processed_;
    if (progress_callback_) {
        progress_callback_(records_processed_, record.type_id, record.payload.size());
    }
    return dispatch_payload(record.type_id, record.payload, mode);
}

Result Dispatcher::replay_from_reader(EventLogReader& reader, ReplayMode mode,
                                      const char* source_label) {
    reset_stats();
    replay_context_->state().reset_all();

    Result r = reader.open();
    if (!r.ok()) {
        finalize_report(false, r.message);
        return r;
    }

    FileHeader header;
    r = reader.read_header(header);
    if (!r.ok()) {
        finalize_report(false, r.message);
        return r;
    }

    r = reader.seek_to_records();
    if (!r.ok()) {
        finalize_report(false, r.message);
        return r;
    }

    if (verbose_ && output_) {
        *output_ << "Replaying " << source_label << " ("
                 << header.record_count << " records)" << std::endl;
    }

    uint32_t index = 0;
    while (!reader.eof()) {
        EventRecord record;
        r = reader.read_next_record(record);
        if (!r.ok()) {
            if (r.code == ErrorCode::UnexpectedEof) {
                break;
            }
            if (r.code == ErrorCode::RecordCrcMismatch) {
                ++crc_errors_;
            }
            finalize_report(false, r.message);
            return r;
        }
        r = dispatch_record(record, mode);
        if (!r.ok()) {
            finalize_report(false, r.message);
            return r;
        }
        ++index;
    }

    if (index != header.record_count) {
        std::string msg = "Record count mismatch: header says " +
                          std::to_string(header.record_count) + ", read " +
                          std::to_string(index);
        finalize_report(false, msg);
        return Result::fail(ErrorCode::VerifyFailed, msg);
    }

    reader.close();
    finalize_report(true);
    return Result::success();
}

Result Dispatcher::replay_file(const std::string& path, ReplayMode mode) {
    EventLogReader reader(path);
    return replay_from_reader(reader, mode, path.c_str());
}

Result Dispatcher::replay_buffer(const uint8_t* data, size_t length, ReplayMode mode) {
    EventLogReader reader(data, length);
    return replay_from_reader(reader, mode, "buffer");
}

}  // namespace telltale

// Optimization pass 40 - memory iteration 1

// Optimization pass 30 - caching iteration 1

// Optimization pass 25 - indexing iteration 1
