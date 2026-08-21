#include "test_common.hpp"

static bool test_writer_reader_header() {
  std::string path = temp_path("header.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  TEST_ASSERT(w.finalize().ok());
  EventLogReader r(path);
  TEST_ASSERT(r.open().ok());
  FileHeader h;
  TEST_ASSERT(r.read_header(h).ok());
  TEST_ASSERT(std::memcmp(h.magic, MAGIC, 4) == 0);
  TEST_ASSERT(h.record_count == 0);

  return true;
}

static bool test_crc_rejection() {
  std::string path = temp_path("bad_crc.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
                            EventLogWriter::encode_print_payload("ok", 1))
                  .ok());
  TEST_ASSERT(w.finalize().ok());
  {
    std::fstream f(path, std::ios::binary | std::ios::in | std::ios::out);
    f.seekp(-4, std::ios::end);
    uint8_t bad[] = {0xDE, 0xAD, 0xBE, 0xEF};
    f.write(reinterpret_cast<char*>(bad), 4);
  }
  Dispatcher d;
  Result r = d.replay_file(path, ReplayMode::VerifyOnly);
  TEST_ASSERT(!r.ok());
  TEST_ASSERT(r.code == ErrorCode::RecordCrcMismatch);

  return true;
}

static bool test_empty_file() {
  std::string path = temp_path("empty.bin");
  remove_file(path);
  {
    std::ofstream f(path);
  }
  EventLogReader r(path);
  TEST_ASSERT(r.open().ok());
  FileHeader h;
  Result res = r.read_header(h);
  TEST_ASSERT(!res.ok());
  TEST_ASSERT(res.code == ErrorCode::EmptyFile);

  return true;
}

static bool test_record_count_finalize() {
  std::string path = temp_path("count.bin");
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int i = 0; i < 5; ++i) {
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
                              EventLogWriter::encode_print_payload("e" + std::to_string(i), 0))
                    .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  EventLogReader r(path);
  TEST_ASSERT(r.open().ok());
  FileHeader h;
  TEST_ASSERT(r.read_header(h).ok());
  TEST_ASSERT(h.record_count == 5);

  return true;
}

static bool test_invalid_magic() {
  std::string path = temp_path("bad_magic.bin");
  remove_file(path);
  {
    std::ofstream f(path, std::ios::binary);
    f << "BAD!";
  }
  EventLogReader r(path);
  TEST_ASSERT(r.open().ok());
  FileHeader h;
  Result res = r.read_header(h);
  TEST_ASSERT(!res.ok());
  TEST_ASSERT(res.code == ErrorCode::InvalidMagic);

  return true;
}

void run_writer_tests() {
  RUN_TEST(test_writer_reader_header);
  RUN_TEST(test_crc_rejection);
  RUN_TEST(test_empty_file);
  RUN_TEST(test_record_count_finalize);
  RUN_TEST(test_invalid_magic);
}
