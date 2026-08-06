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

void run_event_roundtrip_tests() {
  RUN_TEST(test_roundtrip_counter);
  RUN_TEST(test_roundtrip_keyvalue);
  RUN_TEST(test_roundtrip_timestamp);
  RUN_TEST(test_roundtrip_checksum);
  RUN_TEST(test_roundtrip_batch);
  RUN_TEST(test_roundtrip_reset);
  RUN_TEST(test_roundtrip_print);
  RUN_TEST(test_roundtrip_stats);
}
