#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "telltale/errors.hpp"
#include "telltale/types.hpp"

namespace telltale {

class HandlerRegistry {
 public:
  HandlerRegistry();
  ~HandlerRegistry();

  HandlerRegistry(const HandlerRegistry&) = delete;
  HandlerRegistry& operator=(const HandlerRegistry&) = delete;

  Result register_handler(uint16_t type_id, HandlerEntry::HandlerFn fn, void* context,
                          uint16_t handler_id);
  Result deregister_handler(uint16_t type_id);
  Result replace_handler(uint16_t type_id, HandlerEntry::HandlerFn fn, void* context,
                         uint16_t handler_id);
  Result apply_schema_update(const SchemaUpdatePayload& update);

  const HandlerEntry* lookup(uint16_t type_id) const;
  HandlerEntry* lookup(uint16_t type_id);
  bool has_handler(uint16_t type_id) const;

  size_t table_size() const { return table_.size(); }
  size_t active_handler_count() const;
  size_t registration_count() const { return registration_count_; }

  std::vector<uint16_t> registered_type_ids() const;
  std::vector<uint16_t> active_type_ids() const;

  void clear_all();
  void reset_to_defaults();

  Result ensure_capacity(uint16_t type_id);
  Result resize_table(size_t new_size);

  uint16_t max_type_id() const { return max_type_id_; }

  struct RegistrationInfo {
    uint16_t type_id;
    uint16_t handler_id;
    bool active;
  };
  std::vector<RegistrationInfo> list_registrations() const;

  void set_no_handler_callback(std::function<void(uint16_t)> callback);
  void invoke_no_handler(uint16_t type_id) const;

  using HandlerResolverFn =
      std::function<std::pair<HandlerEntry::HandlerFn, void*>(uint16_t handler_id)>;
  void set_handler_resolver(HandlerResolverFn resolver);

 private:
  std::vector<HandlerEntry*> table_;
  std::vector<bool> slot_live_;
  size_t registration_count_;
  uint16_t max_type_id_;
  std::function<void(uint16_t)> no_handler_callback_;
  HandlerResolverFn handler_resolver_;

  Result validate_type_id(uint16_t type_id) const;
  Result validate_handler_fn(HandlerEntry::HandlerFn fn) const;
  void update_max_type_id(uint16_t type_id);
};

}  // namespace telltale
