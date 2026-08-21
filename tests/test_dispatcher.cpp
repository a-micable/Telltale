#include "test_common.hpp"

static bool test_roundtrip_counter() {
  std::string path = temp_path("counter.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
                            EventLogWriter::encode_counter_payload("hits", 10, true))
                  .ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
                            EventLogWriter::encode_counter_payload("hits", 5, false))
                  .ok());
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

  return true;
}

static bool test_roundtrip_keyvalue() {
  std::string path = temp_path("kv.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
                            EventLogWriter::encode_keyvalue_payload("alpha", 100, true))
                  .ok());
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

  return true;
}

static bool test_roundtrip_timestamp() {
  std::string path = temp_path("ts.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Timestamp),
                            EventLogWriter::encode_timestamp_payload("t1", 1234567890ULL, true))
                  .ok());
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

  return true;
}

static bool test_roundtrip_checksum() {
  std::string path = temp_path("cs.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
                            EventLogWriter::encode_counter_payload("x", 42, true))
                  .ok());
  ReplayContext ctx;
  uint32_t crc = ctx.state().compute_state_crc(0x01);
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Checksum),
                            EventLogWriter::encode_checksum_payload("c", crc, 0x01))
                  .ok());
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

  return true;
}

static bool test_roundtrip_batch() {
  std::string path = temp_path("batch.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  std::vector<std::vector<uint8_t>> subs;
  subs.push_back(EventLogWriter::encode_counter_payload("b", 1, false));
  subs.push_back(EventLogWriter::encode_print_payload("in batch", 1));
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Batch),
                            EventLogWriter::encode_batch_payload(subs))
                  .ok());
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

  return true;
}

static bool test_roundtrip_reset() {
  std::string path = temp_path("reset.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
                            EventLogWriter::encode_counter_payload("r", 99, true))
                  .ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Reset),
                            EventLogWriter::encode_reset_payload(0x01))
                  .ok());
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

  return true;
}

static bool test_roundtrip_print() {
  std::string path = temp_path("print.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
                            EventLogWriter::encode_print_payload("hello telltale", 1))
                  .ok());
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

  return true;
}

static bool test_roundtrip_stats() {
  std::string path = temp_path("stats.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
                            EventLogWriter::encode_counter_payload("s", 1, true))
                  .ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Stats),
                            EventLogWriter::encode_stats_payload(0x0F, "test"))
                  .ok());
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

  return true;
}

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

static bool test_schema_codec() {
  auto payload = SchemaUpdateCodec::encode(static_cast<uint8_t>(SchemaUpdateFlag::Register), 0x1234,
                                           static_cast<uint16_t>(HandlerId::BuiltinCounter), true);
  SchemaUpdatePayload decoded;
  TEST_ASSERT(SchemaUpdateCodec::decode(payload, decoded).ok());
  TEST_ASSERT(decoded.type_id == 0x1234);
  TEST_ASSERT(decoded.has_handler_id);

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

static bool test_state_crc_0() {
  ReplayState st;
  for (int j = 0; j < 1; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (0 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 0 == 0);
  }

  return true;
}

static bool test_state_crc_1() {
  ReplayState st;
  for (int j = 0; j < 2; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (1 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 1 == 0);
  }

  return true;
}

static bool test_state_crc_2() {
  ReplayState st;
  for (int j = 0; j < 3; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (2 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 2 == 0);
  }

  return true;
}

static bool test_state_crc_3() {
  ReplayState st;
  for (int j = 0; j < 4; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (3 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 3 == 0);
  }

  return true;
}

static bool test_state_crc_4() {
  ReplayState st;
  for (int j = 0; j < 5; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (4 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 4 == 0);
  }

  return true;
}

static bool test_state_crc_5() {
  ReplayState st;
  for (int j = 0; j < 6; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (5 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 5 == 0);
  }

  return true;
}

static bool test_state_crc_6() {
  ReplayState st;
  for (int j = 0; j < 7; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (6 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 6 == 0);
  }

  return true;
}

static bool test_state_crc_7() {
  ReplayState st;
  for (int j = 0; j < 8; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (7 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 7 == 0);
  }

  return true;
}

static bool test_state_crc_8() {
  ReplayState st;
  for (int j = 0; j < 9; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (8 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 8 == 0);
  }

  return true;
}

static bool test_state_crc_9() {
  ReplayState st;
  for (int j = 0; j < 10; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (9 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 9 == 0);
  }

  return true;
}

static bool test_state_crc_10() {
  ReplayState st;
  for (int j = 0; j < 11; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (10 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 10 == 0);
  }

  return true;
}

static bool test_state_crc_11() {
  ReplayState st;
  for (int j = 0; j < 12; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (11 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 11 == 0);
  }

  return true;
}

static bool test_state_crc_12() {
  ReplayState st;
  for (int j = 0; j < 13; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (12 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 12 == 0);
  }

  return true;
}

static bool test_state_crc_13() {
  ReplayState st;
  for (int j = 0; j < 14; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (13 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 13 == 0);
  }

  return true;
}

static bool test_state_crc_14() {
  ReplayState st;
  for (int j = 0; j < 15; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (14 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 14 == 0);
  }

  return true;
}

static bool test_state_crc_15() {
  ReplayState st;
  for (int j = 0; j < 16; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (15 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 15 == 0);
  }

  return true;
}

static bool test_state_crc_16() {
  ReplayState st;
  for (int j = 0; j < 17; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (16 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 16 == 0);
  }

  return true;
}

static bool test_state_crc_17() {
  ReplayState st;
  for (int j = 0; j < 18; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (17 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 17 == 0);
  }

  return true;
}

static bool test_state_crc_18() {
  ReplayState st;
  for (int j = 0; j < 19; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (18 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 18 == 0);
  }

  return true;
}

static bool test_state_crc_19() {
  ReplayState st;
  for (int j = 0; j < 20; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (19 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 19 == 0);
  }

  return true;
}

static bool test_state_crc_20() {
  ReplayState st;
  for (int j = 0; j < 1; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (20 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 20 == 0);
  }

  return true;
}

static bool test_state_crc_21() {
  ReplayState st;
  for (int j = 0; j < 2; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (21 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 21 == 0);
  }

  return true;
}

static bool test_state_crc_22() {
  ReplayState st;
  for (int j = 0; j < 3; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (22 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 22 == 0);
  }

  return true;
}

static bool test_state_crc_23() {
  ReplayState st;
  for (int j = 0; j < 4; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (23 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 23 == 0);
  }

  return true;
}

static bool test_state_crc_24() {
  ReplayState st;
  for (int j = 0; j < 5; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (24 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 24 == 0);
  }

  return true;
}

static bool test_state_crc_25() {
  ReplayState st;
  for (int j = 0; j < 6; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (25 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 25 == 0);
  }

  return true;
}

static bool test_state_crc_26() {
  ReplayState st;
  for (int j = 0; j < 7; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (26 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 26 == 0);
  }

  return true;
}

static bool test_state_crc_27() {
  ReplayState st;
  for (int j = 0; j < 8; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (27 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 27 == 0);
  }

  return true;
}

static bool test_state_crc_28() {
  ReplayState st;
  for (int j = 0; j < 9; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (28 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 28 == 0);
  }

  return true;
}

static bool test_state_crc_29() {
  ReplayState st;
  for (int j = 0; j < 10; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (29 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 29 == 0);
  }

  return true;
}

static bool test_state_crc_30() {
  ReplayState st;
  for (int j = 0; j < 11; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (30 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 30 == 0);
  }

  return true;
}

static bool test_state_crc_31() {
  ReplayState st;
  for (int j = 0; j < 12; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (31 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 31 == 0);
  }

  return true;
}

static bool test_state_crc_32() {
  ReplayState st;
  for (int j = 0; j < 13; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (32 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 32 == 0);
  }

  return true;
}

static bool test_state_crc_33() {
  ReplayState st;
  for (int j = 0; j < 14; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (33 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 33 == 0);
  }

  return true;
}

static bool test_state_crc_34() {
  ReplayState st;
  for (int j = 0; j < 15; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (34 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 34 == 0);
  }

  return true;
}

static bool test_state_crc_35() {
  ReplayState st;
  for (int j = 0; j < 16; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (35 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 35 == 0);
  }

  return true;
}

static bool test_state_crc_36() {
  ReplayState st;
  for (int j = 0; j < 17; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (36 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 36 == 0);
  }

  return true;
}

static bool test_state_crc_37() {
  ReplayState st;
  for (int j = 0; j < 18; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (37 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 37 == 0);
  }

  return true;
}

static bool test_state_crc_38() {
  ReplayState st;
  for (int j = 0; j < 19; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (38 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 38 == 0);
  }

  return true;
}

static bool test_state_crc_39() {
  ReplayState st;
  for (int j = 0; j < 20; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (39 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 39 == 0);
  }

  return true;
}

static bool test_state_crc_40() {
  ReplayState st;
  for (int j = 0; j < 1; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (40 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 40 == 0);
  }

  return true;
}

static bool test_state_crc_41() {
  ReplayState st;
  for (int j = 0; j < 2; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (41 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 41 == 0);
  }

  return true;
}

static bool test_state_crc_42() {
  ReplayState st;
  for (int j = 0; j < 3; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (42 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 42 == 0);
  }

  return true;
}

static bool test_state_crc_43() {
  ReplayState st;
  for (int j = 0; j < 4; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (43 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 43 == 0);
  }

  return true;
}

static bool test_state_crc_44() {
  ReplayState st;
  for (int j = 0; j < 5; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (44 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 44 == 0);
  }

  return true;
}

static bool test_state_crc_45() {
  ReplayState st;
  for (int j = 0; j < 6; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (45 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 45 == 0);
  }

  return true;
}

static bool test_state_crc_46() {
  ReplayState st;
  for (int j = 0; j < 7; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (46 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 46 == 0);
  }

  return true;
}

static bool test_state_crc_47() {
  ReplayState st;
  for (int j = 0; j < 8; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (47 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 47 == 0);
  }

  return true;
}

static bool test_state_crc_48() {
  ReplayState st;
  for (int j = 0; j < 9; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (48 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 48 == 0);
  }

  return true;
}

static bool test_state_crc_49() {
  ReplayState st;
  for (int j = 0; j < 10; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (49 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 49 == 0);
  }

  return true;
}

static bool test_state_crc_50() {
  ReplayState st;
  for (int j = 0; j < 11; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (50 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 50 == 0);
  }

  return true;
}

static bool test_state_crc_51() {
  ReplayState st;
  for (int j = 0; j < 12; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (51 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 51 == 0);
  }

  return true;
}

static bool test_state_crc_52() {
  ReplayState st;
  for (int j = 0; j < 13; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (52 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 52 == 0);
  }

  return true;
}

static bool test_state_crc_53() {
  ReplayState st;
  for (int j = 0; j < 14; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (53 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 53 == 0);
  }

  return true;
}

static bool test_state_crc_54() {
  ReplayState st;
  for (int j = 0; j < 15; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (54 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 54 == 0);
  }

  return true;
}

static bool test_state_crc_55() {
  ReplayState st;
  for (int j = 0; j < 16; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (55 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 55 == 0);
  }

  return true;
}

static bool test_state_crc_56() {
  ReplayState st;
  for (int j = 0; j < 17; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (56 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 56 == 0);
  }

  return true;
}

static bool test_state_crc_57() {
  ReplayState st;
  for (int j = 0; j < 18; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (57 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 57 == 0);
  }

  return true;
}

static bool test_state_crc_58() {
  ReplayState st;
  for (int j = 0; j < 19; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (58 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 58 == 0);
  }

  return true;
}

static bool test_state_crc_59() {
  ReplayState st;
  for (int j = 0; j < 20; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (59 > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || 59 == 0);
  }

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

static bool test_multi_event_log_0() {
  std::string path = temp_path("multi_0.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 5; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(5));

  return true;
}

static bool test_multi_event_log_1() {
  std::string path = temp_path("multi_1.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 6; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(6));

  return true;
}

static bool test_multi_event_log_2() {
  std::string path = temp_path("multi_2.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 7; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(7));

  return true;
}

static bool test_multi_event_log_3() {
  std::string path = temp_path("multi_3.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 8; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(8));

  return true;
}

static bool test_multi_event_log_4() {
  std::string path = temp_path("multi_4.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 9; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(9));

  return true;
}

static bool test_multi_event_log_5() {
  std::string path = temp_path("multi_5.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 10; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(10));

  return true;
}

static bool test_multi_event_log_6() {
  std::string path = temp_path("multi_6.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 11; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(11));

  return true;
}

static bool test_multi_event_log_7() {
  std::string path = temp_path("multi_7.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 12; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(12));

  return true;
}

static bool test_multi_event_log_8() {
  std::string path = temp_path("multi_8.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 13; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(13));

  return true;
}

static bool test_multi_event_log_9() {
  std::string path = temp_path("multi_9.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 14; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(14));

  return true;
}

static bool test_multi_event_log_10() {
  std::string path = temp_path("multi_10.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 5; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(5));

  return true;
}

static bool test_multi_event_log_11() {
  std::string path = temp_path("multi_11.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 6; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(6));

  return true;
}

static bool test_multi_event_log_12() {
  std::string path = temp_path("multi_12.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 7; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(7));

  return true;
}

static bool test_multi_event_log_13() {
  std::string path = temp_path("multi_13.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 8; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(8));

  return true;
}

static bool test_multi_event_log_14() {
  std::string path = temp_path("multi_14.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 9; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(9));

  return true;
}

static bool test_multi_event_log_15() {
  std::string path = temp_path("multi_15.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 10; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(10));

  return true;
}

static bool test_multi_event_log_16() {
  std::string path = temp_path("multi_16.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 11; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(11));

  return true;
}

static bool test_multi_event_log_17() {
  std::string path = temp_path("multi_17.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 12; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(12));

  return true;
}

static bool test_multi_event_log_18() {
  std::string path = temp_path("multi_18.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 13; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(13));

  return true;
}

static bool test_multi_event_log_19() {
  std::string path = temp_path("multi_19.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 14; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(14));

  return true;
}

static bool test_multi_event_log_20() {
  std::string path = temp_path("multi_20.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 5; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(5));

  return true;
}

static bool test_multi_event_log_21() {
  std::string path = temp_path("multi_21.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 6; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(6));

  return true;
}

static bool test_multi_event_log_22() {
  std::string path = temp_path("multi_22.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 7; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(7));

  return true;
}

static bool test_multi_event_log_23() {
  std::string path = temp_path("multi_23.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 8; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(8));

  return true;
}

static bool test_multi_event_log_24() {
  std::string path = temp_path("multi_24.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 9; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(9));

  return true;
}

static bool test_multi_event_log_25() {
  std::string path = temp_path("multi_25.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 10; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(10));

  return true;
}

static bool test_multi_event_log_26() {
  std::string path = temp_path("multi_26.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 11; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(11));

  return true;
}

static bool test_multi_event_log_27() {
  std::string path = temp_path("multi_27.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 12; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(12));

  return true;
}

static bool test_multi_event_log_28() {
  std::string path = temp_path("multi_28.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 13; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(13));

  return true;
}

static bool test_multi_event_log_29() {
  std::string path = temp_path("multi_29.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 14; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(14));

  return true;
}

static bool test_multi_event_log_30() {
  std::string path = temp_path("multi_30.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 5; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(5));

  return true;
}

static bool test_multi_event_log_31() {
  std::string path = temp_path("multi_31.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 6; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(6));

  return true;
}

static bool test_multi_event_log_32() {
  std::string path = temp_path("multi_32.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 7; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(7));

  return true;
}

static bool test_multi_event_log_33() {
  std::string path = temp_path("multi_33.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 8; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(8));

  return true;
}

static bool test_multi_event_log_34() {
  std::string path = temp_path("multi_34.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 9; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(9));

  return true;
}

static bool test_multi_event_log_35() {
  std::string path = temp_path("multi_35.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 10; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(10));

  return true;
}

static bool test_multi_event_log_36() {
  std::string path = temp_path("multi_36.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 11; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(11));

  return true;
}

static bool test_multi_event_log_37() {
  std::string path = temp_path("multi_37.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 12; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(12));

  return true;
}

static bool test_multi_event_log_38() {
  std::string path = temp_path("multi_38.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 13; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(13));

  return true;
}

static bool test_multi_event_log_39() {
  std::string path = temp_path("multi_39.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int e = 0; e < 14; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(14));

  return true;
}

void run_dispatcher_tests() {
  RUN_TEST(test_roundtrip_counter);
  RUN_TEST(test_roundtrip_keyvalue);
  RUN_TEST(test_roundtrip_timestamp);
  RUN_TEST(test_roundtrip_checksum);
  RUN_TEST(test_roundtrip_batch);
  RUN_TEST(test_roundtrip_reset);
  RUN_TEST(test_roundtrip_print);
  RUN_TEST(test_roundtrip_stats);
  RUN_TEST(test_schema_register);
  RUN_TEST(test_schema_deregister);
  RUN_TEST(test_schema_replace);
  RUN_TEST(test_schema_only_file);
  RUN_TEST(test_verify_mode);
  RUN_TEST(test_handler_registry_basic);
  RUN_TEST(test_schema_codec);
  RUN_TEST(test_max_registrations);
  RUN_TEST(test_generate_sample_log);
  RUN_TEST(test_keyvalue_no_overwrite);
  RUN_TEST(test_state_crc_0);
  RUN_TEST(test_state_crc_1);
  RUN_TEST(test_state_crc_2);
  RUN_TEST(test_state_crc_3);
  RUN_TEST(test_state_crc_4);
  RUN_TEST(test_state_crc_5);
  RUN_TEST(test_state_crc_6);
  RUN_TEST(test_state_crc_7);
  RUN_TEST(test_state_crc_8);
  RUN_TEST(test_state_crc_9);
  RUN_TEST(test_state_crc_10);
  RUN_TEST(test_state_crc_11);
  RUN_TEST(test_state_crc_12);
  RUN_TEST(test_state_crc_13);
  RUN_TEST(test_state_crc_14);
  RUN_TEST(test_state_crc_15);
  RUN_TEST(test_state_crc_16);
  RUN_TEST(test_state_crc_17);
  RUN_TEST(test_state_crc_18);
  RUN_TEST(test_state_crc_19);
  RUN_TEST(test_state_crc_20);
  RUN_TEST(test_state_crc_21);
  RUN_TEST(test_state_crc_22);
  RUN_TEST(test_state_crc_23);
  RUN_TEST(test_state_crc_24);
  RUN_TEST(test_state_crc_25);
  RUN_TEST(test_state_crc_26);
  RUN_TEST(test_state_crc_27);
  RUN_TEST(test_state_crc_28);
  RUN_TEST(test_state_crc_29);
  RUN_TEST(test_state_crc_30);
  RUN_TEST(test_state_crc_31);
  RUN_TEST(test_state_crc_32);
  RUN_TEST(test_state_crc_33);
  RUN_TEST(test_state_crc_34);
  RUN_TEST(test_state_crc_35);
  RUN_TEST(test_state_crc_36);
  RUN_TEST(test_state_crc_37);
  RUN_TEST(test_state_crc_38);
  RUN_TEST(test_state_crc_39);
  RUN_TEST(test_state_crc_40);
  RUN_TEST(test_state_crc_41);
  RUN_TEST(test_state_crc_42);
  RUN_TEST(test_state_crc_43);
  RUN_TEST(test_state_crc_44);
  RUN_TEST(test_state_crc_45);
  RUN_TEST(test_state_crc_46);
  RUN_TEST(test_state_crc_47);
  RUN_TEST(test_state_crc_48);
  RUN_TEST(test_state_crc_49);
  RUN_TEST(test_state_crc_50);
  RUN_TEST(test_state_crc_51);
  RUN_TEST(test_state_crc_52);
  RUN_TEST(test_state_crc_53);
  RUN_TEST(test_state_crc_54);
  RUN_TEST(test_state_crc_55);
  RUN_TEST(test_state_crc_56);
  RUN_TEST(test_state_crc_57);
  RUN_TEST(test_state_crc_58);
  RUN_TEST(test_state_crc_59);
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
  RUN_TEST(test_multi_event_log_0);
  RUN_TEST(test_multi_event_log_1);
  RUN_TEST(test_multi_event_log_2);
  RUN_TEST(test_multi_event_log_3);
  RUN_TEST(test_multi_event_log_4);
  RUN_TEST(test_multi_event_log_5);
  RUN_TEST(test_multi_event_log_6);
  RUN_TEST(test_multi_event_log_7);
  RUN_TEST(test_multi_event_log_8);
  RUN_TEST(test_multi_event_log_9);
  RUN_TEST(test_multi_event_log_10);
  RUN_TEST(test_multi_event_log_11);
  RUN_TEST(test_multi_event_log_12);
  RUN_TEST(test_multi_event_log_13);
  RUN_TEST(test_multi_event_log_14);
  RUN_TEST(test_multi_event_log_15);
  RUN_TEST(test_multi_event_log_16);
  RUN_TEST(test_multi_event_log_17);
  RUN_TEST(test_multi_event_log_18);
  RUN_TEST(test_multi_event_log_19);
  RUN_TEST(test_multi_event_log_20);
  RUN_TEST(test_multi_event_log_21);
  RUN_TEST(test_multi_event_log_22);
  RUN_TEST(test_multi_event_log_23);
  RUN_TEST(test_multi_event_log_24);
  RUN_TEST(test_multi_event_log_25);
  RUN_TEST(test_multi_event_log_26);
  RUN_TEST(test_multi_event_log_27);
  RUN_TEST(test_multi_event_log_28);
  RUN_TEST(test_multi_event_log_29);
  RUN_TEST(test_multi_event_log_30);
  RUN_TEST(test_multi_event_log_31);
  RUN_TEST(test_multi_event_log_32);
  RUN_TEST(test_multi_event_log_33);
  RUN_TEST(test_multi_event_log_34);
  RUN_TEST(test_multi_event_log_35);
  RUN_TEST(test_multi_event_log_36);
  RUN_TEST(test_multi_event_log_37);
  RUN_TEST(test_multi_event_log_38);
  RUN_TEST(test_multi_event_log_39);
}
