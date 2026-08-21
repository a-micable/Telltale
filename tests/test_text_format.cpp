#include "test_common.hpp"

static bool write_sample_for_text(const std::string& path) {
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
                            EventLogWriter::encode_counter_payload("hits", 10, true))
                  .ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
                            EventLogWriter::encode_keyvalue_payload("alpha", 42, true))
                  .ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
                            EventLogWriter::encode_print_payload("hello \"world\"", 1))
                  .ok());
  TEST_ASSERT(w.finalize().ok());
  return true;
}

static bool test_text_escape_roundtrip_helpers() {
  TEST_ASSERT(TextExporter::escape_string("a\"b\\c") == "a\\\"b\\\\c");
  TEST_ASSERT(TextExporter::escape_string("line\n") == "line\\n");
  TEST_ASSERT(TextExporter::hex_encode({0x0A, 0xFF}) == "0a ff");
  return true;
}

static bool test_text_export_to_string() {
  std::string path = temp_path("text_export.bin");
  TEST_ASSERT(write_sample_for_text(path));

  TextExporter exporter;
  std::string out;
  TEST_ASSERT(exporter.export_to_string(path, out).ok());
  TEST_ASSERT(out.find(TEXT_FORMAT_MAGIC_LINE) != std::string::npos);
  TEST_ASSERT(out.find(TEXT_RECORD_BEGIN) != std::string::npos);
  TEST_ASSERT(out.find("hits") != std::string::npos);
  TEST_ASSERT(exporter.records_exported() == 3);
  return true;
}

static bool test_text_export_import_roundtrip() {
  std::string bin_path = temp_path("text_rt.bin");
  std::string txt_path = temp_path("text_rt.txt");
  std::string out_bin = temp_path("text_rt_out.bin");
  TEST_ASSERT(write_sample_for_text(bin_path));
  remove_file(txt_path);
  remove_file(out_bin);

  TextExporter exporter;
  TEST_ASSERT(exporter.export_file(bin_path, txt_path).ok());
  TEST_ASSERT(file_exists(txt_path));

  TextImporter importer;
  TEST_ASSERT(importer.import_file(txt_path, out_bin).ok());
  TEST_ASSERT(file_exists(out_bin));
  TEST_ASSERT(importer.records().size() == 3);

  TEST_ASSERT(text_format_full_verify(bin_path, out_bin).ok());
  return true;
}

static bool test_text_import_rejects_bad_type_id() {
  TextImporter importer;
  importer.set_strict(true);
  Result r = importer.parse(std::string(TEXT_FORMAT_MAGIC_LINE) + "\n" + TEXT_RECORD_BEGIN +
                            "\n  type_id=not_a_number\n" + TEXT_RECORD_END + "\n");
  TEST_ASSERT(!r.ok() || importer.parse_errors() > 0);
  return true;
}

void run_text_format_tests() {
  RUN_TEST(test_text_escape_roundtrip_helpers);
  RUN_TEST(test_text_export_to_string);
  RUN_TEST(test_text_export_import_roundtrip);
  RUN_TEST(test_text_import_rejects_bad_type_id);
}
