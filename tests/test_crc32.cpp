#include "test_common.hpp"

static bool test_crc32_incremental() {
  Crc32 crc;
  crc.update('A');
  crc.update('B');
  crc.update('C');
  TEST_ASSERT(crc.finalize() == Crc32::of_bytes(reinterpret_cast<const uint8_t*>("ABC"), 3));

  return true;
}

static bool test_crc32_known_vectors() {
  TEST_ASSERT(Crc32::of_bytes(reinterpret_cast<const uint8_t*>("123456789"), 9) == 0xCBF43926U);
  TEST_ASSERT(Crc32::of_bytes(nullptr, 0) == 0x00000000U);

  return true;
}

static bool test_crc32_record() {
  std::vector<uint8_t> payload = {1, 2, 3};
  uint32_t c = Crc32::of_record(0x0001, payload);
  TEST_ASSERT(c != 0);
  TEST_ASSERT(c == Crc32::of_record(0x0001, payload));

  return true;
}

void run_crc32_tests() {
  RUN_TEST(test_crc32_incremental);
  RUN_TEST(test_crc32_known_vectors);
  RUN_TEST(test_crc32_record);
}
