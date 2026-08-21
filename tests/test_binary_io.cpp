#include <cstring>

#include "gtest/gtest.h"
#include "telltale/binary_io.hpp"
#include "telltale/types.hpp"

using telltale::EventLogReader;
using telltale::EventLogWriter;
using telltale::EventType;
using telltale::FileHeader;
using telltale::MAGIC;
using telltale::PrintMessage;

TEST(BinaryIo, HeaderRoundtrip) {
  std::string path = temp_path("binary_io_header.bin");
  remove_file(path);
  EventLogWriter w(path);
  EXPECT_TRUE(w.open().ok());
  EXPECT_TRUE(w.write_header().ok());
  EXPECT_TRUE(w.finalize().ok());

  EventLogReader r(path);
  EXPECT_TRUE(r.open().ok());
  FileHeader h;
  EXPECT_TRUE(r.read_header(h).ok());
  EXPECT_TRUE(std::memcmp(h.magic, MAGIC, 4) == 0);
  EXPECT_EQ(h.record_count, 0u);
  return true;
}

TEST(BinaryIo, EncodeDecodePrintPayload) {
  auto payload = EventLogWriter::encode_print_payload("hello-binary-io", 2);
  PrintMessage msg;
  EXPECT_TRUE(EventLogReader::decode_print_payload(payload, msg).ok());
  EXPECT_EQ(msg.message, "hello-binary-io");
  EXPECT_EQ(msg.severity, static_cast<uint8_t>(2));
  return true;
}

void run_binary_io_tests() {
  RUN_TEST(test_BinaryIo_HeaderRoundtrip);
  RUN_TEST(test_BinaryIo_EncodeDecodePrintPayload);
}
