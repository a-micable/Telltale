#include "test_common.hpp"

static bool counter_case(int i) {
  std::string path = temp_path("rt_counter_" + std::to_string(i) + ".bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  const std::string name = "n" + std::to_string(i);
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
                            EventLogWriter::encode_counter_payload(name, i, true))
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
  CounterDelta cd;
  TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
  TEST_ASSERT(cd.name == name);
  TEST_ASSERT(cd.absolute == i);
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  return true;
}

static bool test_roundtrip_counter_0() { return counter_case(0); }
static bool test_roundtrip_counter_1() { return counter_case(1); }
static bool test_roundtrip_counter_2() { return counter_case(2); }
static bool test_roundtrip_counter_3() { return counter_case(3); }
static bool test_roundtrip_counter_4() { return counter_case(4); }
static bool test_roundtrip_counter_5() { return counter_case(5); }
static bool test_roundtrip_counter_6() { return counter_case(6); }
static bool test_roundtrip_counter_7() { return counter_case(7); }
static bool test_roundtrip_counter_8() { return counter_case(8); }
static bool test_roundtrip_counter_9() { return counter_case(9); }
static bool test_roundtrip_counter_10() { return counter_case(10); }
static bool test_roundtrip_counter_11() { return counter_case(11); }
static bool test_roundtrip_counter_12() { return counter_case(12); }
static bool test_roundtrip_counter_13() { return counter_case(13); }
static bool test_roundtrip_counter_14() { return counter_case(14); }
static bool test_roundtrip_counter_15() { return counter_case(15); }
static bool test_roundtrip_counter_16() { return counter_case(16); }
static bool test_roundtrip_counter_17() { return counter_case(17); }
static bool test_roundtrip_counter_18() { return counter_case(18); }
static bool test_roundtrip_counter_19() { return counter_case(19); }
static bool test_roundtrip_counter_20() { return counter_case(20); }
static bool test_roundtrip_counter_21() { return counter_case(21); }
static bool test_roundtrip_counter_22() { return counter_case(22); }
static bool test_roundtrip_counter_23() { return counter_case(23); }
static bool test_roundtrip_counter_24() { return counter_case(24); }
static bool test_roundtrip_counter_25() { return counter_case(25); }
static bool test_roundtrip_counter_26() { return counter_case(26); }

void run_roundtrip_counter_tests() {
  RUN_TEST(test_roundtrip_counter_0);
  RUN_TEST(test_roundtrip_counter_1);
  RUN_TEST(test_roundtrip_counter_2);
  RUN_TEST(test_roundtrip_counter_3);
  RUN_TEST(test_roundtrip_counter_4);
  RUN_TEST(test_roundtrip_counter_5);
  RUN_TEST(test_roundtrip_counter_6);
  RUN_TEST(test_roundtrip_counter_7);
  RUN_TEST(test_roundtrip_counter_8);
  RUN_TEST(test_roundtrip_counter_9);
  RUN_TEST(test_roundtrip_counter_10);
  RUN_TEST(test_roundtrip_counter_11);
  RUN_TEST(test_roundtrip_counter_12);
  RUN_TEST(test_roundtrip_counter_13);
  RUN_TEST(test_roundtrip_counter_14);
  RUN_TEST(test_roundtrip_counter_15);
  RUN_TEST(test_roundtrip_counter_16);
  RUN_TEST(test_roundtrip_counter_17);
  RUN_TEST(test_roundtrip_counter_18);
  RUN_TEST(test_roundtrip_counter_19);
  RUN_TEST(test_roundtrip_counter_20);
  RUN_TEST(test_roundtrip_counter_21);
  RUN_TEST(test_roundtrip_counter_22);
  RUN_TEST(test_roundtrip_counter_23);
  RUN_TEST(test_roundtrip_counter_24);
  RUN_TEST(test_roundtrip_counter_25);
  RUN_TEST(test_roundtrip_counter_26);
}
