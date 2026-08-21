#include "test_common.hpp"

static bool multi_event_case(int i) {
  std::string path = temp_path("multi_" + std::to_string(i) + ".bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  const int count = 5 + i;
  for (int e = 0; e < count; ++e) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("e" + std::to_string(e), 1, false))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  Dispatcher d;
  TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
  TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(count));
  return true;
}

static bool test_multi_event_log_0() { return multi_event_case(0); }
static bool test_multi_event_log_1() { return multi_event_case(1); }
static bool test_multi_event_log_2() { return multi_event_case(2); }
static bool test_multi_event_log_3() { return multi_event_case(3); }
static bool test_multi_event_log_4() { return multi_event_case(4); }
static bool test_multi_event_log_5() { return multi_event_case(5); }
static bool test_multi_event_log_6() { return multi_event_case(6); }
static bool test_multi_event_log_7() { return multi_event_case(7); }
static bool test_multi_event_log_8() { return multi_event_case(8); }
static bool test_multi_event_log_9() { return multi_event_case(9); }
static bool test_multi_event_log_10() { return multi_event_case(10); }
static bool test_multi_event_log_11() { return multi_event_case(11); }
static bool test_multi_event_log_12() { return multi_event_case(12); }
static bool test_multi_event_log_13() { return multi_event_case(13); }
static bool test_multi_event_log_14() { return multi_event_case(14); }
static bool test_multi_event_log_15() { return multi_event_case(15); }
static bool test_multi_event_log_16() { return multi_event_case(16); }
static bool test_multi_event_log_17() { return multi_event_case(17); }
static bool test_multi_event_log_18() { return multi_event_case(18); }
static bool test_multi_event_log_19() { return multi_event_case(19); }
static bool test_multi_event_log_20() { return multi_event_case(20); }
static bool test_multi_event_log_21() { return multi_event_case(21); }
static bool test_multi_event_log_22() { return multi_event_case(22); }
static bool test_multi_event_log_23() { return multi_event_case(23); }
static bool test_multi_event_log_24() { return multi_event_case(24); }
static bool test_multi_event_log_25() { return multi_event_case(25); }
static bool test_multi_event_log_26() { return multi_event_case(26); }
static bool test_multi_event_log_27() { return multi_event_case(27); }
static bool test_multi_event_log_28() { return multi_event_case(28); }
static bool test_multi_event_log_29() { return multi_event_case(29); }
static bool test_multi_event_log_30() { return multi_event_case(30); }
static bool test_multi_event_log_31() { return multi_event_case(31); }
static bool test_multi_event_log_32() { return multi_event_case(32); }
static bool test_multi_event_log_33() { return multi_event_case(33); }
static bool test_multi_event_log_34() { return multi_event_case(34); }
static bool test_multi_event_log_35() { return multi_event_case(35); }
static bool test_multi_event_log_36() { return multi_event_case(36); }
static bool test_multi_event_log_37() { return multi_event_case(37); }
static bool test_multi_event_log_38() { return multi_event_case(38); }
static bool test_multi_event_log_39() { return multi_event_case(39); }

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
