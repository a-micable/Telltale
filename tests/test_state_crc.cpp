#include "test_common.hpp"

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
