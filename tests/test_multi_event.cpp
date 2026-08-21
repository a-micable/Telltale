#include "test_common.hpp"

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

void run_multi_event_tests() {
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
