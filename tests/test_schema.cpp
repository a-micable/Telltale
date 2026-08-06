#include "test_common.hpp"

static bool test_schema_register() {
  std::string path = temp_path("schema_reg.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  TEST_ASSERT(w.write_schema_update(static_cast<uint8_t>(SchemaUpdateFlag::Register), 0x0200,
                                    static_cast<uint16_t>(HandlerId::BuiltinPrint))
                  .ok());
  TEST_ASSERT(w.write_event(0x0200, EventLogWriter::encode_print_payload("registered", 1)).ok());
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

  return true;
}

static bool test_schema_deregister() {
  std::string path = temp_path("schema_dereg.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  TEST_ASSERT(w.write_schema_update(static_cast<uint8_t>(SchemaUpdateFlag::Register), 0x0300,
                                    static_cast<uint16_t>(HandlerId::BuiltinNoOp))
                  .ok());
  TEST_ASSERT(
      w.write_schema_update(static_cast<uint8_t>(SchemaUpdateFlag::Deregister), 0x0300, 0).ok());
  TEST_ASSERT(w.write_event(0x0300, EventLogWriter::encode_print_payload("no handler", 1)).ok());
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.missing_handlers() >= 1);

  return true;
}

static bool test_schema_replace() {
  std::string path = temp_path("schema_replace.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  TEST_ASSERT(w.write_schema_update(static_cast<uint8_t>(SchemaUpdateFlag::Register), 0x0400,
                                    static_cast<uint16_t>(HandlerId::BuiltinNoOp))
                  .ok());
  TEST_ASSERT(w.write_schema_update(static_cast<uint8_t>(SchemaUpdateFlag::Replace), 0x0400,
                                    static_cast<uint16_t>(HandlerId::BuiltinPrint))
                  .ok());
  TEST_ASSERT(
      w.write_event(0x0400, EventLogWriter::encode_print_payload("replaced handler", 1)).ok());
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

  return true;
}

static bool test_schema_only_file() {
  std::string path = temp_path("schema_only.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  TEST_ASSERT(w.write_schema_update(static_cast<uint8_t>(SchemaUpdateFlag::Register), 0x0500,
                                    static_cast<uint16_t>(HandlerId::BuiltinCounter))
                  .ok());
  TEST_ASSERT(
      w.write_schema_update(static_cast<uint8_t>(SchemaUpdateFlag::Deregister), 0x0500, 0).ok());
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::VerifyOnly).ok());
  TEST_ASSERT(d.schema_updates_applied() == 2);

  return true;
}

static bool test_verify_mode() {
  std::string path = temp_path("verify.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
                            EventLogWriter::encode_counter_payload("v", 1, true))
                  .ok());
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::VerifyOnly).ok());
  TEST_ASSERT(d.handlers_invoked() == 0);

  return true;
}

static bool test_schema_codec() {
  auto payload = SchemaUpdateCodec::encode(static_cast<uint8_t>(SchemaUpdateFlag::Register), 0x1234,
                                           static_cast<uint16_t>(HandlerId::BuiltinCounter), true);
  SchemaUpdatePayload decoded;
  TEST_ASSERT(SchemaUpdateCodec::decode(payload, decoded).ok());
  TEST_ASSERT(decoded.type_id == 0x1234);
  TEST_ASSERT(decoded.has_handler_id);

  return true;
}

void run_schema_tests() {
  RUN_TEST(test_schema_register);
  RUN_TEST(test_schema_deregister);
  RUN_TEST(test_schema_replace);
  RUN_TEST(test_schema_only_file);
  RUN_TEST(test_verify_mode);
  RUN_TEST(test_schema_codec);
}
