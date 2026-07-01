#include "telltale/handler_registry.hpp"
#include "telltale/schema_update.hpp"
#include <algorithm>

namespace telltale {

HandlerRegistry::HandlerRegistry()
    : table_(INITIAL_HANDLER_TABLE_SIZE, nullptr),
      slot_live_(INITIAL_HANDLER_TABLE_SIZE, false),
      registration_count_(0),
      max_type_id_(0),
      handler_resolver_(nullptr) {}

HandlerRegistry::~HandlerRegistry() {
    for (size_t i = 0; i < table_.size(); ++i) {
        if (slot_live_[i] && table_[i] != nullptr) {
            delete table_[i];
        }
    }
}

Result HandlerRegistry::validate_type_id(uint16_t type_id) const {
    if (type_id == SCHEMA_UPDATE_TYPE) {
        return Result::fail(ErrorCode::InvalidTypeId,
            "Cannot register handler for schema update type");
    }
    return Result::success();
}

Result HandlerRegistry::validate_handler_fn(HandlerEntry::HandlerFn fn) const {
    if (fn == nullptr) {
        return Result::fail(ErrorCode::InvalidHandlerId, "Handler function is null");
    }
    return Result::success();
}

void HandlerRegistry::update_max_type_id(uint16_t type_id) {
    if (type_id > max_type_id_) {
        max_type_id_ = type_id;
    }
}

Result HandlerRegistry::ensure_capacity(uint16_t type_id) {
    if (type_id >= table_.size()) {
        size_t new_size = std::max(table_.size() * 2, static_cast<size_t>(type_id) + 1);
        if (new_size > MAX_HANDLER_TABLE_SIZE) {
            new_size = MAX_HANDLER_TABLE_SIZE;
        }
        if (type_id >= new_size) {
            return Result::fail(ErrorCode::HandlerTableFull,
                "Type ID " + std::to_string(type_id) + " exceeds table capacity");
        }
        return resize_table(new_size);
    }
    return Result::success();
}

Result HandlerRegistry::resize_table(size_t new_size) {
    if (new_size > MAX_HANDLER_TABLE_SIZE) {
        return Result::fail(ErrorCode::HandlerTableFull,
            "Requested table size exceeds maximum");
    }
    size_t old_size = table_.size();
    table_.resize(new_size, nullptr);
    slot_live_.resize(new_size, false);
    for (size_t i = old_size; i < new_size; ++i) {
        table_[i] = nullptr;
        slot_live_[i] = false;
    }
    return Result::success();
}

Result HandlerRegistry::register_handler(uint16_t type_id, HandlerEntry::HandlerFn fn,
                                          void* context, uint16_t handler_id) {
    Result vr = validate_type_id(type_id);
    if (!vr.ok()) return vr;
    vr = validate_handler_fn(fn);
    if (!vr.ok()) return vr;
    vr = ensure_capacity(type_id);
    if (!vr.ok()) return vr;

    HandlerEntry* existing = table_[type_id];
    if (existing != nullptr && slot_live_[type_id] && existing->is_valid()) {
        return Result::fail(ErrorCode::HandlerAlreadyRegistered,
            "Handler already registered for type " + std::to_string(type_id));
    }

    if (slot_live_[type_id] && existing != nullptr) {
        delete existing;
    }
    table_[type_id] = new HandlerEntry(fn, context, handler_id);
    slot_live_[type_id] = true;
    ++registration_count_;
    update_max_type_id(type_id);
    return Result::success();
}

Result HandlerRegistry::deregister_handler(uint16_t type_id) {
    Result vr = validate_type_id(type_id);
    if (!vr.ok()) return vr;
    if (type_id >= table_.size()) {
        return Result::fail(ErrorCode::HandlerNotFound,
            "No handler registered for type " + std::to_string(type_id));
    }

    HandlerEntry* entry = table_[type_id];
    if (entry != nullptr) {
        if (entry->is_valid()) {
            --registration_count_;
        }
        delete entry;
        slot_live_[type_id] = false;
    }
    return Result::success();
}

Result HandlerRegistry::replace_handler(uint16_t type_id, HandlerEntry::HandlerFn fn,
                                         void* context, uint16_t handler_id) {
    Result vr = validate_type_id(type_id);
    if (!vr.ok()) return vr;
    vr = validate_handler_fn(fn);
    if (!vr.ok()) return vr;
    vr = ensure_capacity(type_id);
    if (!vr.ok()) return vr;

    HandlerEntry* existing = table_[type_id];
    bool was_active = existing != nullptr && slot_live_[type_id] && existing->is_valid();
    if (slot_live_[type_id] && existing != nullptr) {
        delete existing;
    }
    table_[type_id] = new HandlerEntry(fn, context, handler_id);
    slot_live_[type_id] = true;
    if (!was_active) {
        ++registration_count_;
    }
    update_max_type_id(type_id);
    return Result::success();
}

Result HandlerRegistry::apply_schema_update(const SchemaUpdatePayload& update) {
    if (!SchemaUpdateCodec::is_valid_flags(update.flags)) {
        return Result::fail(ErrorCode::InvalidSchemaUpdate, "Invalid schema update flags");
    }

    if (update.flags & static_cast<uint8_t>(SchemaUpdateFlag::ResizeTable)) {
        if (!update.has_table_size) {
            return Result::fail(ErrorCode::InvalidSchemaUpdate,
                "Resize requires table size");
        }
        return resize_table(update.table_size);
    }

    if (update.flags & static_cast<uint8_t>(SchemaUpdateFlag::Deregister)) {
        return deregister_handler(update.type_id);
    }

    if (update.flags & static_cast<uint8_t>(SchemaUpdateFlag::Replace)) {
        if (!update.has_handler_id) {
            return Result::fail(ErrorCode::InvalidSchemaUpdate,
                "Replace requires handler ID");
        }
        if (!handler_resolver_) {
            return Result::fail(ErrorCode::InternalError, "No handler resolver configured");
        }
        auto resolved = handler_resolver_(update.handler_id);
        if (resolved.first == nullptr) {
            return Result::fail(ErrorCode::InvalidHandlerId,
                "Unknown handler ID: " + std::to_string(update.handler_id));
        }
        return replace_handler(update.type_id, resolved.first, resolved.second,
                               update.handler_id);
    }

    if (update.flags & static_cast<uint8_t>(SchemaUpdateFlag::Register)) {
        if (!update.has_handler_id) {
            return Result::fail(ErrorCode::InvalidSchemaUpdate,
                "Register requires handler ID");
        }
        if (!handler_resolver_) {
            return Result::fail(ErrorCode::InternalError, "No handler resolver configured");
        }
        auto resolved = handler_resolver_(update.handler_id);
        if (resolved.first == nullptr) {
            return Result::fail(ErrorCode::InvalidHandlerId,
                "Unknown handler ID: " + std::to_string(update.handler_id));
        }
        return register_handler(update.type_id, resolved.first, resolved.second,
                                update.handler_id);
    }

    return Result::fail(ErrorCode::InvalidSchemaUpdate, "No operation specified in flags");
}

const HandlerEntry* HandlerRegistry::lookup(uint16_t type_id) const {
    if (type_id >= table_.size()) {
        return nullptr;
    }
    if (!slot_live_[type_id]) {
        return nullptr;
    }
    const HandlerEntry* entry = table_[type_id];
    if (entry == nullptr || !entry->is_valid()) {
        return nullptr;
    }
    return entry;
}

HandlerEntry* HandlerRegistry::lookup(uint16_t type_id) {
    if (type_id >= table_.size()) {
        return nullptr;
    }
    if (!slot_live_[type_id]) {
        return nullptr;
    }
    HandlerEntry* entry = table_[type_id];
    if (entry == nullptr || !entry->is_valid()) {
        return nullptr;
    }
    return entry;
}

bool HandlerRegistry::has_handler(uint16_t type_id) const {
    return lookup(type_id) != nullptr;
}

size_t HandlerRegistry::active_handler_count() const {
    size_t count = 0;
    for (size_t i = 0; i < table_.size(); ++i) {
        if (slot_live_[i] && table_[i] != nullptr && table_[i]->is_valid()) {
            ++count;
        }
    }
    return count;
}

std::vector<uint16_t> HandlerRegistry::registered_type_ids() const {
    std::vector<uint16_t> ids;
    for (size_t i = 0; i < table_.size(); ++i) {
        if (slot_live_[i] && table_[i] != nullptr && table_[i]->is_valid()) {
            ids.push_back(static_cast<uint16_t>(i));
        }
    }
    return ids;
}

std::vector<uint16_t> HandlerRegistry::active_type_ids() const {
    return registered_type_ids();
}

void HandlerRegistry::clear_all() {
    for (size_t i = 0; i < table_.size(); ++i) {
        if (slot_live_[i] && table_[i] != nullptr) {
            delete table_[i];
        }
        table_[i] = nullptr;
        slot_live_[i] = false;
    }
    registration_count_ = 0;
    max_type_id_ = 0;
}

void HandlerRegistry::reset_to_defaults() {
    clear_all();
}

std::vector<HandlerRegistry::RegistrationInfo>
HandlerRegistry::list_registrations() const {
    std::vector<RegistrationInfo> result;
    for (size_t i = 0; i < table_.size(); ++i) {
        if (slot_live_[i] && table_[i] != nullptr && table_[i]->is_valid()) {
            RegistrationInfo info;
            info.type_id = static_cast<uint16_t>(i);
            info.handler_id = table_[i]->handler_id;
            info.active = table_[i]->active;
            result.push_back(info);
        }
    }
    return result;
}

void HandlerRegistry::set_no_handler_callback(std::function<void(uint16_t)> callback) {
    no_handler_callback_ = std::move(callback);
}

void HandlerRegistry::invoke_no_handler(uint16_t type_id) const {
    if (no_handler_callback_) {
        no_handler_callback_(type_id);
    }
}

void HandlerRegistry::set_handler_resolver(HandlerResolverFn resolver) {
    handler_resolver_ = resolver;
}

}  // namespace telltale
