#include "test_common.hpp"

static bool test_handler_registry_basic() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  TEST_ASSERT(
      reg.register_handler(
             0x100, resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinNoOp), nullptr),
             &ctx, static_cast<uint16_t>(HandlerId::BuiltinNoOp))
          .ok());
  TEST_ASSERT(reg.has_handler(0x100));
  TEST_ASSERT(reg.deregister_handler(0x100).ok());
  TEST_ASSERT(!reg.has_handler(0x100));

  return true;
}

static bool test_max_registrations() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  for (uint16_t i = 0x1000; i < 0x1000 + 100; ++i) {
    Result r = reg.register_handler(
        i, resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinNoOp), nullptr), &ctx,
        static_cast<uint16_t>(HandlerId::BuiltinNoOp));
    TEST_ASSERT(r.ok());
  }
  TEST_ASSERT(reg.active_handler_count() >= 100);

  return true;
}

static bool test_generate_sample_log() {
  std::string path = temp_path("sample.bin");
  remove_file(path);
  TEST_ASSERT(generate_sample_log(path, 15, false) == 0);
  TEST_ASSERT(file_exists(path));
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

  return true;
}

static bool test_keyvalue_no_overwrite() {
  std::string path = temp_path("kv_noow.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
                            EventLogWriter::encode_keyvalue_payload("k", 10, true))
                  .ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
                            EventLogWriter::encode_keyvalue_payload("k", 99, false))
                  .ok());
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

  return true;
}

static bool test_registry_resize_0() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 0);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_1() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 1);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_2() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 2);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_3() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 3);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_4() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 4);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_5() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 5);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_6() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 6);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_7() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 7);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_8() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 8);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_9() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 9);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_10() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 10);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_11() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 11);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_12() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 12);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_13() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 13);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_14() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 14);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_15() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 15);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_16() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 16);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_17() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 17);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_18() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 18);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_19() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 19);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_20() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 20);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_21() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 21);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_22() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 22);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_23() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 23);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_24() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 24);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_25() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 25);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_26() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 26);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_27() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 27);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_28() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 28);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

static bool test_registry_resize_29() {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  uint16_t tid = static_cast<uint16_t>(0x2000 + 29);
  TEST_ASSERT(reg.register_handler(tid,
                                   resolve_builtin_handler(
                                       static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                   &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(reg.has_handler(tid));
  TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

  return true;
}

void run_registry_tests() {
  RUN_TEST(test_handler_registry_basic);
  RUN_TEST(test_max_registrations);
  RUN_TEST(test_generate_sample_log);
  RUN_TEST(test_keyvalue_no_overwrite);
  RUN_TEST(test_registry_resize_0);
  RUN_TEST(test_registry_resize_1);
  RUN_TEST(test_registry_resize_2);
  RUN_TEST(test_registry_resize_3);
  RUN_TEST(test_registry_resize_4);
  RUN_TEST(test_registry_resize_5);
  RUN_TEST(test_registry_resize_6);
  RUN_TEST(test_registry_resize_7);
  RUN_TEST(test_registry_resize_8);
  RUN_TEST(test_registry_resize_9);
  RUN_TEST(test_registry_resize_10);
  RUN_TEST(test_registry_resize_11);
  RUN_TEST(test_registry_resize_12);
  RUN_TEST(test_registry_resize_13);
  RUN_TEST(test_registry_resize_14);
  RUN_TEST(test_registry_resize_15);
  RUN_TEST(test_registry_resize_16);
  RUN_TEST(test_registry_resize_17);
  RUN_TEST(test_registry_resize_18);
  RUN_TEST(test_registry_resize_19);
  RUN_TEST(test_registry_resize_20);
  RUN_TEST(test_registry_resize_21);
  RUN_TEST(test_registry_resize_22);
  RUN_TEST(test_registry_resize_23);
  RUN_TEST(test_registry_resize_24);
  RUN_TEST(test_registry_resize_25);
  RUN_TEST(test_registry_resize_26);
  RUN_TEST(test_registry_resize_27);
  RUN_TEST(test_registry_resize_28);
  RUN_TEST(test_registry_resize_29);
}
