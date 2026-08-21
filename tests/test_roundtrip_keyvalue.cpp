#include "test_common.hpp"

static bool keyvalue_case(int i) {
  std::string path = temp_path("rt_kv_" + std::to_string(i) + ".bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  const std::string key = "k" + std::to_string(i);
  const int64_t value = static_cast<int64_t>(i) * 10;
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
                            EventLogWriter::encode_keyvalue_payload(key, value, true))
                  .ok());
  TEST_ASSERT(w.finalize().ok());
  EventLogReader r(path);
  TEST_ASSERT(r.open().ok());
  FileHeader h;
  TEST_ASSERT(r.read_header(h).ok());
  TEST_ASSERT(h.record_count == 1);
  TEST_ASSERT(r.seek_to_records().ok());
  EventRecord rec;
  TEST_ASSERT(r.read_next_record(rec).ok());
  KeyValueEntry kv;
  TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
  TEST_ASSERT(kv.key == key);
  TEST_ASSERT(kv.value == value);
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  return true;
}

static bool test_roundtrip_keyvalue_0() { return keyvalue_case(0); }
static bool test_roundtrip_keyvalue_1() { return keyvalue_case(1); }
static bool test_roundtrip_keyvalue_2() { return keyvalue_case(2); }
static bool test_roundtrip_keyvalue_3() { return keyvalue_case(3); }
static bool test_roundtrip_keyvalue_4() { return keyvalue_case(4); }
static bool test_roundtrip_keyvalue_5() { return keyvalue_case(5); }
static bool test_roundtrip_keyvalue_6() { return keyvalue_case(6); }
static bool test_roundtrip_keyvalue_7() { return keyvalue_case(7); }
static bool test_roundtrip_keyvalue_8() { return keyvalue_case(8); }
static bool test_roundtrip_keyvalue_9() { return keyvalue_case(9); }
static bool test_roundtrip_keyvalue_10() { return keyvalue_case(10); }
static bool test_roundtrip_keyvalue_11() { return keyvalue_case(11); }
static bool test_roundtrip_keyvalue_12() { return keyvalue_case(12); }
static bool test_roundtrip_keyvalue_13() { return keyvalue_case(13); }
static bool test_roundtrip_keyvalue_14() { return keyvalue_case(14); }
static bool test_roundtrip_keyvalue_15() { return keyvalue_case(15); }
static bool test_roundtrip_keyvalue_16() { return keyvalue_case(16); }
static bool test_roundtrip_keyvalue_17() { return keyvalue_case(17); }
static bool test_roundtrip_keyvalue_18() { return keyvalue_case(18); }
static bool test_roundtrip_keyvalue_19() { return keyvalue_case(19); }
static bool test_roundtrip_keyvalue_20() { return keyvalue_case(20); }
static bool test_roundtrip_keyvalue_21() { return keyvalue_case(21); }
static bool test_roundtrip_keyvalue_22() { return keyvalue_case(22); }
static bool test_roundtrip_keyvalue_23() { return keyvalue_case(23); }
static bool test_roundtrip_keyvalue_24() { return keyvalue_case(24); }
static bool test_roundtrip_keyvalue_25() { return keyvalue_case(25); }
static bool test_roundtrip_keyvalue_26() { return keyvalue_case(26); }

void run_roundtrip_keyvalue_tests() {
  RUN_TEST(test_roundtrip_keyvalue_0);
  RUN_TEST(test_roundtrip_keyvalue_1);
  RUN_TEST(test_roundtrip_keyvalue_2);
  RUN_TEST(test_roundtrip_keyvalue_3);
  RUN_TEST(test_roundtrip_keyvalue_4);
  RUN_TEST(test_roundtrip_keyvalue_5);
  RUN_TEST(test_roundtrip_keyvalue_6);
  RUN_TEST(test_roundtrip_keyvalue_7);
  RUN_TEST(test_roundtrip_keyvalue_8);
  RUN_TEST(test_roundtrip_keyvalue_9);
  RUN_TEST(test_roundtrip_keyvalue_10);
  RUN_TEST(test_roundtrip_keyvalue_11);
  RUN_TEST(test_roundtrip_keyvalue_12);
  RUN_TEST(test_roundtrip_keyvalue_13);
  RUN_TEST(test_roundtrip_keyvalue_14);
  RUN_TEST(test_roundtrip_keyvalue_15);
  RUN_TEST(test_roundtrip_keyvalue_16);
  RUN_TEST(test_roundtrip_keyvalue_17);
  RUN_TEST(test_roundtrip_keyvalue_18);
  RUN_TEST(test_roundtrip_keyvalue_19);
  RUN_TEST(test_roundtrip_keyvalue_20);
  RUN_TEST(test_roundtrip_keyvalue_21);
  RUN_TEST(test_roundtrip_keyvalue_22);
  RUN_TEST(test_roundtrip_keyvalue_23);
  RUN_TEST(test_roundtrip_keyvalue_24);
  RUN_TEST(test_roundtrip_keyvalue_25);
  RUN_TEST(test_roundtrip_keyvalue_26);
}
