#include "test_common.hpp"

static bool state_crc_case(int i) {
  ReplayState st;
  for (int j = 0; j < i + 1; ++j) {
    st.increment_counter("c" + std::to_string(j), j + 1);
    st.set_key_value("k" + std::to_string(j), j * 100, true);
  }
  uint32_t crc1 = st.compute_state_crc(0x03);
  uint32_t crc2 = st.compute_state_crc(0x03);
  TEST_ASSERT(crc1 == crc2);
  if (i > 0) {
    st.increment_counter("extra", 1);
    uint32_t crc3 = st.compute_state_crc(0x01);
    TEST_ASSERT(crc3 != crc1 || i == 0);
  }
  return true;
}

static bool test_state_crc_0() { return state_crc_case(0); }
static bool test_state_crc_1() { return state_crc_case(1); }
static bool test_state_crc_2() { return state_crc_case(2); }
static bool test_state_crc_3() { return state_crc_case(3); }
static bool test_state_crc_4() { return state_crc_case(4); }
static bool test_state_crc_5() { return state_crc_case(5); }
static bool test_state_crc_6() { return state_crc_case(6); }
static bool test_state_crc_7() { return state_crc_case(7); }
static bool test_state_crc_8() { return state_crc_case(8); }
static bool test_state_crc_9() { return state_crc_case(9); }
static bool test_state_crc_10() { return state_crc_case(10); }
static bool test_state_crc_11() { return state_crc_case(11); }
static bool test_state_crc_12() { return state_crc_case(12); }
static bool test_state_crc_13() { return state_crc_case(13); }
static bool test_state_crc_14() { return state_crc_case(14); }
static bool test_state_crc_15() { return state_crc_case(15); }
static bool test_state_crc_16() { return state_crc_case(16); }
static bool test_state_crc_17() { return state_crc_case(17); }
static bool test_state_crc_18() { return state_crc_case(18); }
static bool test_state_crc_19() { return state_crc_case(19); }
static bool test_state_crc_20() { return state_crc_case(20); }
static bool test_state_crc_21() { return state_crc_case(21); }
static bool test_state_crc_22() { return state_crc_case(22); }
static bool test_state_crc_23() { return state_crc_case(23); }
static bool test_state_crc_24() { return state_crc_case(24); }
static bool test_state_crc_25() { return state_crc_case(25); }
static bool test_state_crc_26() { return state_crc_case(26); }
static bool test_state_crc_27() { return state_crc_case(27); }
static bool test_state_crc_28() { return state_crc_case(28); }
static bool test_state_crc_29() { return state_crc_case(29); }
static bool test_state_crc_30() { return state_crc_case(30); }
static bool test_state_crc_31() { return state_crc_case(31); }
static bool test_state_crc_32() { return state_crc_case(32); }
static bool test_state_crc_33() { return state_crc_case(33); }
static bool test_state_crc_34() { return state_crc_case(34); }
static bool test_state_crc_35() { return state_crc_case(35); }
static bool test_state_crc_36() { return state_crc_case(36); }
static bool test_state_crc_37() { return state_crc_case(37); }
static bool test_state_crc_38() { return state_crc_case(38); }
static bool test_state_crc_39() { return state_crc_case(39); }
static bool test_state_crc_40() { return state_crc_case(40); }
static bool test_state_crc_41() { return state_crc_case(41); }
static bool test_state_crc_42() { return state_crc_case(42); }
static bool test_state_crc_43() { return state_crc_case(43); }
static bool test_state_crc_44() { return state_crc_case(44); }
static bool test_state_crc_45() { return state_crc_case(45); }
static bool test_state_crc_46() { return state_crc_case(46); }
static bool test_state_crc_47() { return state_crc_case(47); }
static bool test_state_crc_48() { return state_crc_case(48); }
static bool test_state_crc_49() { return state_crc_case(49); }
static bool test_state_crc_50() { return state_crc_case(50); }
static bool test_state_crc_51() { return state_crc_case(51); }
static bool test_state_crc_52() { return state_crc_case(52); }
static bool test_state_crc_53() { return state_crc_case(53); }
static bool test_state_crc_54() { return state_crc_case(54); }
static bool test_state_crc_55() { return state_crc_case(55); }
static bool test_state_crc_56() { return state_crc_case(56); }
static bool test_state_crc_57() { return state_crc_case(57); }
static bool test_state_crc_58() { return state_crc_case(58); }
static bool test_state_crc_59() { return state_crc_case(59); }

void run_state_crc_tests() {
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
}
