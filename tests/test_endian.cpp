#include "test_common.hpp"

static bool endian_case(int i) {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03E8 + i));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03E8 + i));
  write_u32_le(buf, static_cast<uint32_t>(0x00002710 + i));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002710 + i));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186A0 + i));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186A0 + i));
  write_i64_le(buf, static_cast<int64_t>(-1 - i));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-1 - i));
  return true;
}

static bool test_endian_roundtrip_0() { return endian_case(0); }
static bool test_endian_roundtrip_1() { return endian_case(1); }
static bool test_endian_roundtrip_2() { return endian_case(2); }
static bool test_endian_roundtrip_3() { return endian_case(3); }
static bool test_endian_roundtrip_4() { return endian_case(4); }
static bool test_endian_roundtrip_5() { return endian_case(5); }
static bool test_endian_roundtrip_6() { return endian_case(6); }
static bool test_endian_roundtrip_7() { return endian_case(7); }
static bool test_endian_roundtrip_8() { return endian_case(8); }
static bool test_endian_roundtrip_9() { return endian_case(9); }
static bool test_endian_roundtrip_10() { return endian_case(10); }
static bool test_endian_roundtrip_11() { return endian_case(11); }
static bool test_endian_roundtrip_12() { return endian_case(12); }
static bool test_endian_roundtrip_13() { return endian_case(13); }
static bool test_endian_roundtrip_14() { return endian_case(14); }
static bool test_endian_roundtrip_15() { return endian_case(15); }
static bool test_endian_roundtrip_16() { return endian_case(16); }
static bool test_endian_roundtrip_17() { return endian_case(17); }
static bool test_endian_roundtrip_18() { return endian_case(18); }
static bool test_endian_roundtrip_19() { return endian_case(19); }
static bool test_endian_roundtrip_20() { return endian_case(20); }
static bool test_endian_roundtrip_21() { return endian_case(21); }
static bool test_endian_roundtrip_22() { return endian_case(22); }
static bool test_endian_roundtrip_23() { return endian_case(23); }
static bool test_endian_roundtrip_24() { return endian_case(24); }
static bool test_endian_roundtrip_25() { return endian_case(25); }
static bool test_endian_roundtrip_26() { return endian_case(26); }
static bool test_endian_roundtrip_27() { return endian_case(27); }
static bool test_endian_roundtrip_28() { return endian_case(28); }
static bool test_endian_roundtrip_29() { return endian_case(29); }
static bool test_endian_roundtrip_30() { return endian_case(30); }
static bool test_endian_roundtrip_31() { return endian_case(31); }
static bool test_endian_roundtrip_32() { return endian_case(32); }
static bool test_endian_roundtrip_33() { return endian_case(33); }
static bool test_endian_roundtrip_34() { return endian_case(34); }
static bool test_endian_roundtrip_35() { return endian_case(35); }
static bool test_endian_roundtrip_36() { return endian_case(36); }
static bool test_endian_roundtrip_37() { return endian_case(37); }
static bool test_endian_roundtrip_38() { return endian_case(38); }
static bool test_endian_roundtrip_39() { return endian_case(39); }

void run_endian_tests() {
  RUN_TEST(test_endian_roundtrip_0);
  RUN_TEST(test_endian_roundtrip_1);
  RUN_TEST(test_endian_roundtrip_2);
  RUN_TEST(test_endian_roundtrip_3);
  RUN_TEST(test_endian_roundtrip_4);
  RUN_TEST(test_endian_roundtrip_5);
  RUN_TEST(test_endian_roundtrip_6);
  RUN_TEST(test_endian_roundtrip_7);
  RUN_TEST(test_endian_roundtrip_8);
  RUN_TEST(test_endian_roundtrip_9);
  RUN_TEST(test_endian_roundtrip_10);
  RUN_TEST(test_endian_roundtrip_11);
  RUN_TEST(test_endian_roundtrip_12);
  RUN_TEST(test_endian_roundtrip_13);
  RUN_TEST(test_endian_roundtrip_14);
  RUN_TEST(test_endian_roundtrip_15);
  RUN_TEST(test_endian_roundtrip_16);
  RUN_TEST(test_endian_roundtrip_17);
  RUN_TEST(test_endian_roundtrip_18);
  RUN_TEST(test_endian_roundtrip_19);
  RUN_TEST(test_endian_roundtrip_20);
  RUN_TEST(test_endian_roundtrip_21);
  RUN_TEST(test_endian_roundtrip_22);
  RUN_TEST(test_endian_roundtrip_23);
  RUN_TEST(test_endian_roundtrip_24);
  RUN_TEST(test_endian_roundtrip_25);
  RUN_TEST(test_endian_roundtrip_26);
  RUN_TEST(test_endian_roundtrip_27);
  RUN_TEST(test_endian_roundtrip_28);
  RUN_TEST(test_endian_roundtrip_29);
  RUN_TEST(test_endian_roundtrip_30);
  RUN_TEST(test_endian_roundtrip_31);
  RUN_TEST(test_endian_roundtrip_32);
  RUN_TEST(test_endian_roundtrip_33);
  RUN_TEST(test_endian_roundtrip_34);
  RUN_TEST(test_endian_roundtrip_35);
  RUN_TEST(test_endian_roundtrip_36);
  RUN_TEST(test_endian_roundtrip_37);
  RUN_TEST(test_endian_roundtrip_38);
  RUN_TEST(test_endian_roundtrip_39);
}
