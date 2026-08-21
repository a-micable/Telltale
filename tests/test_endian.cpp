#include "test_common.hpp"

static bool test_endian_roundtrip_0() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03E8));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03E8));
  write_u32_le(buf, static_cast<uint32_t>(0x00002710));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002710));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186A0));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186A0));
  write_i64_le(buf, static_cast<int64_t>(-1));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-1));

  return true;
}

static bool test_endian_roundtrip_1() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03E9));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03E9));
  write_u32_le(buf, static_cast<uint32_t>(0x00002711));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002711));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186A1));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186A1));
  write_i64_le(buf, static_cast<int64_t>(-2));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-2));

  return true;
}

static bool test_endian_roundtrip_2() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03EA));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03EA));
  write_u32_le(buf, static_cast<uint32_t>(0x00002712));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002712));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186A2));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186A2));
  write_i64_le(buf, static_cast<int64_t>(-3));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-3));

  return true;
}

static bool test_endian_roundtrip_3() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03EB));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03EB));
  write_u32_le(buf, static_cast<uint32_t>(0x00002713));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002713));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186A3));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186A3));
  write_i64_le(buf, static_cast<int64_t>(-4));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-4));

  return true;
}

static bool test_endian_roundtrip_4() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03EC));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03EC));
  write_u32_le(buf, static_cast<uint32_t>(0x00002714));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002714));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186A4));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186A4));
  write_i64_le(buf, static_cast<int64_t>(-5));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-5));

  return true;
}

static bool test_endian_roundtrip_5() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03ED));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03ED));
  write_u32_le(buf, static_cast<uint32_t>(0x00002715));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002715));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186A5));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186A5));
  write_i64_le(buf, static_cast<int64_t>(-6));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-6));

  return true;
}

static bool test_endian_roundtrip_6() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03EE));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03EE));
  write_u32_le(buf, static_cast<uint32_t>(0x00002716));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002716));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186A6));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186A6));
  write_i64_le(buf, static_cast<int64_t>(-7));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-7));

  return true;
}

static bool test_endian_roundtrip_7() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03EF));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03EF));
  write_u32_le(buf, static_cast<uint32_t>(0x00002717));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002717));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186A7));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186A7));
  write_i64_le(buf, static_cast<int64_t>(-8));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-8));

  return true;
}

static bool test_endian_roundtrip_8() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03F0));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03F0));
  write_u32_le(buf, static_cast<uint32_t>(0x00002718));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002718));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186A8));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186A8));
  write_i64_le(buf, static_cast<int64_t>(-9));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-9));

  return true;
}

static bool test_endian_roundtrip_9() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03F1));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03F1));
  write_u32_le(buf, static_cast<uint32_t>(0x00002719));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002719));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186A9));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186A9));
  write_i64_le(buf, static_cast<int64_t>(-10));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-10));

  return true;
}

static bool test_endian_roundtrip_10() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03F2));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03F2));
  write_u32_le(buf, static_cast<uint32_t>(0x0000271A));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x0000271A));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186AA));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186AA));
  write_i64_le(buf, static_cast<int64_t>(-11));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-11));

  return true;
}

static bool test_endian_roundtrip_11() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03F3));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03F3));
  write_u32_le(buf, static_cast<uint32_t>(0x0000271B));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x0000271B));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186AB));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186AB));
  write_i64_le(buf, static_cast<int64_t>(-12));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-12));

  return true;
}

static bool test_endian_roundtrip_12() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03F4));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03F4));
  write_u32_le(buf, static_cast<uint32_t>(0x0000271C));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x0000271C));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186AC));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186AC));
  write_i64_le(buf, static_cast<int64_t>(-13));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-13));

  return true;
}

static bool test_endian_roundtrip_13() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03F5));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03F5));
  write_u32_le(buf, static_cast<uint32_t>(0x0000271D));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x0000271D));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186AD));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186AD));
  write_i64_le(buf, static_cast<int64_t>(-14));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-14));

  return true;
}

static bool test_endian_roundtrip_14() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03F6));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03F6));
  write_u32_le(buf, static_cast<uint32_t>(0x0000271E));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x0000271E));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186AE));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186AE));
  write_i64_le(buf, static_cast<int64_t>(-15));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-15));

  return true;
}

static bool test_endian_roundtrip_15() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03F7));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03F7));
  write_u32_le(buf, static_cast<uint32_t>(0x0000271F));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x0000271F));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186AF));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186AF));
  write_i64_le(buf, static_cast<int64_t>(-16));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-16));

  return true;
}

static bool test_endian_roundtrip_16() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03F8));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03F8));
  write_u32_le(buf, static_cast<uint32_t>(0x00002720));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002720));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186B0));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186B0));
  write_i64_le(buf, static_cast<int64_t>(-17));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-17));

  return true;
}

static bool test_endian_roundtrip_17() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03F9));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03F9));
  write_u32_le(buf, static_cast<uint32_t>(0x00002721));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002721));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186B1));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186B1));
  write_i64_le(buf, static_cast<int64_t>(-18));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-18));

  return true;
}

static bool test_endian_roundtrip_18() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03FA));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03FA));
  write_u32_le(buf, static_cast<uint32_t>(0x00002722));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002722));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186B2));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186B2));
  write_i64_le(buf, static_cast<int64_t>(-19));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-19));

  return true;
}

static bool test_endian_roundtrip_19() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03FB));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03FB));
  write_u32_le(buf, static_cast<uint32_t>(0x00002723));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002723));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186B3));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186B3));
  write_i64_le(buf, static_cast<int64_t>(-20));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-20));

  return true;
}

static bool test_endian_roundtrip_20() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03FC));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03FC));
  write_u32_le(buf, static_cast<uint32_t>(0x00002724));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002724));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186B4));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186B4));
  write_i64_le(buf, static_cast<int64_t>(-21));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-21));

  return true;
}

static bool test_endian_roundtrip_21() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03FD));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03FD));
  write_u32_le(buf, static_cast<uint32_t>(0x00002725));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002725));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186B5));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186B5));
  write_i64_le(buf, static_cast<int64_t>(-22));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-22));

  return true;
}

static bool test_endian_roundtrip_22() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03FE));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03FE));
  write_u32_le(buf, static_cast<uint32_t>(0x00002726));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002726));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186B6));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186B6));
  write_i64_le(buf, static_cast<int64_t>(-23));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-23));

  return true;
}

static bool test_endian_roundtrip_23() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x03FF));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x03FF));
  write_u32_le(buf, static_cast<uint32_t>(0x00002727));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002727));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186B7));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186B7));
  write_i64_le(buf, static_cast<int64_t>(-24));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-24));

  return true;
}

static bool test_endian_roundtrip_24() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x0400));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x0400));
  write_u32_le(buf, static_cast<uint32_t>(0x00002728));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002728));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186B8));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186B8));
  write_i64_le(buf, static_cast<int64_t>(-25));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-25));

  return true;
}

static bool test_endian_roundtrip_25() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x0401));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x0401));
  write_u32_le(buf, static_cast<uint32_t>(0x00002729));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002729));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186B9));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186B9));
  write_i64_le(buf, static_cast<int64_t>(-26));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-26));

  return true;
}

static bool test_endian_roundtrip_26() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x0402));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x0402));
  write_u32_le(buf, static_cast<uint32_t>(0x0000272A));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x0000272A));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186BA));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186BA));
  write_i64_le(buf, static_cast<int64_t>(-27));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-27));

  return true;
}

static bool test_endian_roundtrip_27() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x0403));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x0403));
  write_u32_le(buf, static_cast<uint32_t>(0x0000272B));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x0000272B));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186BB));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186BB));
  write_i64_le(buf, static_cast<int64_t>(-28));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-28));

  return true;
}

static bool test_endian_roundtrip_28() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x0404));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x0404));
  write_u32_le(buf, static_cast<uint32_t>(0x0000272C));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x0000272C));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186BC));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186BC));
  write_i64_le(buf, static_cast<int64_t>(-29));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-29));

  return true;
}

static bool test_endian_roundtrip_29() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x0405));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x0405));
  write_u32_le(buf, static_cast<uint32_t>(0x0000272D));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x0000272D));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186BD));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186BD));
  write_i64_le(buf, static_cast<int64_t>(-30));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-30));

  return true;
}

static bool test_endian_roundtrip_30() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x0406));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x0406));
  write_u32_le(buf, static_cast<uint32_t>(0x0000272E));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x0000272E));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186BE));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186BE));
  write_i64_le(buf, static_cast<int64_t>(-31));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-31));

  return true;
}

static bool test_endian_roundtrip_31() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x0407));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x0407));
  write_u32_le(buf, static_cast<uint32_t>(0x0000272F));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x0000272F));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186BF));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186BF));
  write_i64_le(buf, static_cast<int64_t>(-32));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-32));

  return true;
}

static bool test_endian_roundtrip_32() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x0408));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x0408));
  write_u32_le(buf, static_cast<uint32_t>(0x00002730));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002730));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186C0));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186C0));
  write_i64_le(buf, static_cast<int64_t>(-33));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-33));

  return true;
}

static bool test_endian_roundtrip_33() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x0409));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x0409));
  write_u32_le(buf, static_cast<uint32_t>(0x00002731));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002731));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186C1));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186C1));
  write_i64_le(buf, static_cast<int64_t>(-34));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-34));

  return true;
}

static bool test_endian_roundtrip_34() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x040A));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x040A));
  write_u32_le(buf, static_cast<uint32_t>(0x00002732));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002732));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186C2));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186C2));
  write_i64_le(buf, static_cast<int64_t>(-35));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-35));

  return true;
}

static bool test_endian_roundtrip_35() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x040B));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x040B));
  write_u32_le(buf, static_cast<uint32_t>(0x00002733));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002733));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186C3));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186C3));
  write_i64_le(buf, static_cast<int64_t>(-36));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-36));

  return true;
}

static bool test_endian_roundtrip_36() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x040C));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x040C));
  write_u32_le(buf, static_cast<uint32_t>(0x00002734));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002734));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186C4));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186C4));
  write_i64_le(buf, static_cast<int64_t>(-37));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-37));

  return true;
}

static bool test_endian_roundtrip_37() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x040D));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x040D));
  write_u32_le(buf, static_cast<uint32_t>(0x00002735));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002735));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186C5));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186C5));
  write_i64_le(buf, static_cast<int64_t>(-38));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-38));

  return true;
}

static bool test_endian_roundtrip_38() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x040E));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x040E));
  write_u32_le(buf, static_cast<uint32_t>(0x00002736));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002736));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186C6));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186C6));
  write_i64_le(buf, static_cast<int64_t>(-39));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-39));

  return true;
}

static bool test_endian_roundtrip_39() {
  uint8_t buf[8];
  write_u16_le(buf, static_cast<uint16_t>(0x040F));
  TEST_ASSERT(read_u16_le(buf) == static_cast<uint16_t>(0x040F));
  write_u32_le(buf, static_cast<uint32_t>(0x00002737));
  TEST_ASSERT(read_u32_le(buf) == static_cast<uint32_t>(0x00002737));
  write_u64_le(buf, static_cast<uint64_t>(0x00000000000186C7));
  TEST_ASSERT(read_u64_le(buf) == static_cast<uint64_t>(0x00000000000186C7));
  write_i64_le(buf, static_cast<int64_t>(-40));
  TEST_ASSERT(read_i64_le(buf) == static_cast<int64_t>(-40));

  return true;
}

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
