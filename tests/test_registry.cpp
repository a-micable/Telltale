#include "gtest/gtest.h"
#include "test_common.hpp"

TEST(Registry, HandlerRegistryBasic) {
  HandlerRegistry reg;
  ReplayContext ctx;
  reg.set_handler_resolver([&ctx](uint16_t hid) {
    ReplayContext* p = &ctx;
    return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
  });
  EXPECT_TRUE(
      reg.register_handler(
             0x100, resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinNoOp), nullptr),
             &ctx, static_cast<uint16_t>(HandlerId::BuiltinNoOp))
          .ok());
  EXPECT_TRUE(reg.has_handler(0x100));
  EXPECT_TRUE(reg.deregister_handler(0x100).ok());
  EXPECT_TRUE(!reg.has_handler(0x100));
  return true;
}

TEST(Registry, MaxRegistrations) {
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
    EXPECT_TRUE(r.ok());
  }
  EXPECT_TRUE(reg.active_handler_count() >= 100);
  return true;
}

TEST(Registry, GenerateSampleLog) {
  std::string path = temp_path("sample.bin");
  remove_file(path);
  EXPECT_TRUE(generate_sample_log(path, 15, false) == 0);
  EXPECT_TRUE(file_exists(path));
  Dispatcher d;
  EXPECT_TRUE(d.replay_file(path, ReplayMode::Execute).ok());
  return true;
}

TEST(Registry, KeyValueNoOverwrite) {
  std::string path = temp_path("kv_noow.bin");
  remove_file(path);
  EventLogWriter w(path);
  EXPECT_TRUE(w.open().ok());
  EXPECT_TRUE(w.write_header().ok());
  EXPECT_TRUE(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
                            EventLogWriter::encode_keyvalue_payload("k", 10, true))
                  .ok());
  EXPECT_TRUE(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
                            EventLogWriter::encode_keyvalue_payload("k", 99, false))
                  .ok());
  EXPECT_TRUE(w.finalize().ok());
  Dispatcher d;
  EXPECT_TRUE(d.replay_file(path, ReplayMode::Execute).ok());
  return true;
}

TEST(Registry, HighTypeIdTableResize) {
  // One parameterized case covers the former generator-inflated resize_0..29 clones.
  for (uint16_t offset = 0; offset < 30; ++offset) {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
      ReplayContext* p = &ctx;
      return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + offset);
    EXPECT_TRUE(reg.register_handler(tid,
                                     resolve_builtin_handler(
                                         static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
                                     &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter))
                    .ok());
    EXPECT_TRUE(reg.has_handler(tid));
    EXPECT_TRUE(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);
  }
  return true;
}

void run_registry_tests() {
  RUN_TEST(test_Registry_HandlerRegistryBasic);
  RUN_TEST(test_Registry_MaxRegistrations);
  RUN_TEST(test_Registry_GenerateSampleLog);
  RUN_TEST(test_Registry_KeyValueNoOverwrite);
  RUN_TEST(test_Registry_HighTypeIdTableResize);
}
