#include "test_common.hpp"

static bool print_case(int i) {
  std::string path = temp_path("rt_print_" + std::to_string(i) + ".bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  const std::string msg = "msg" + std::to_string(i);
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
                            EventLogWriter::encode_print_payload(msg, static_cast<uint8_t>(i % 8)))
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
  PrintMessage pm;
  TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
  TEST_ASSERT(pm.message == msg);
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  return true;
}

static bool test_roundtrip_print_0() { return print_case(0); }
static bool test_roundtrip_print_1() { return print_case(1); }
static bool test_roundtrip_print_2() { return print_case(2); }
static bool test_roundtrip_print_3() { return print_case(3); }
static bool test_roundtrip_print_4() { return print_case(4); }
static bool test_roundtrip_print_5() { return print_case(5); }
static bool test_roundtrip_print_6() { return print_case(6); }
static bool test_roundtrip_print_7() { return print_case(7); }
static bool test_roundtrip_print_8() { return print_case(8); }
static bool test_roundtrip_print_9() { return print_case(9); }
static bool test_roundtrip_print_10() { return print_case(10); }
static bool test_roundtrip_print_11() { return print_case(11); }
static bool test_roundtrip_print_12() { return print_case(12); }
static bool test_roundtrip_print_13() { return print_case(13); }
static bool test_roundtrip_print_14() { return print_case(14); }
static bool test_roundtrip_print_15() { return print_case(15); }
static bool test_roundtrip_print_16() { return print_case(16); }
static bool test_roundtrip_print_17() { return print_case(17); }
static bool test_roundtrip_print_18() { return print_case(18); }
static bool test_roundtrip_print_19() { return print_case(19); }
static bool test_roundtrip_print_20() { return print_case(20); }
static bool test_roundtrip_print_21() { return print_case(21); }
static bool test_roundtrip_print_22() { return print_case(22); }
static bool test_roundtrip_print_23() { return print_case(23); }
static bool test_roundtrip_print_24() { return print_case(24); }
static bool test_roundtrip_print_25() { return print_case(25); }

void run_roundtrip_print_tests() {
  RUN_TEST(test_roundtrip_print_0);
  RUN_TEST(test_roundtrip_print_1);
  RUN_TEST(test_roundtrip_print_2);
  RUN_TEST(test_roundtrip_print_3);
  RUN_TEST(test_roundtrip_print_4);
  RUN_TEST(test_roundtrip_print_5);
  RUN_TEST(test_roundtrip_print_6);
  RUN_TEST(test_roundtrip_print_7);
  RUN_TEST(test_roundtrip_print_8);
  RUN_TEST(test_roundtrip_print_9);
  RUN_TEST(test_roundtrip_print_10);
  RUN_TEST(test_roundtrip_print_11);
  RUN_TEST(test_roundtrip_print_12);
  RUN_TEST(test_roundtrip_print_13);
  RUN_TEST(test_roundtrip_print_14);
  RUN_TEST(test_roundtrip_print_15);
  RUN_TEST(test_roundtrip_print_16);
  RUN_TEST(test_roundtrip_print_17);
  RUN_TEST(test_roundtrip_print_18);
  RUN_TEST(test_roundtrip_print_19);
  RUN_TEST(test_roundtrip_print_20);
  RUN_TEST(test_roundtrip_print_21);
  RUN_TEST(test_roundtrip_print_22);
  RUN_TEST(test_roundtrip_print_23);
  RUN_TEST(test_roundtrip_print_24);
  RUN_TEST(test_roundtrip_print_25);
}
