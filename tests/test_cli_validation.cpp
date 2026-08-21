#include <cstring>
#include <vector>

#include "test_common.hpp"

static std::vector<char*> make_argv(std::vector<std::string>& storage) {
  std::vector<char*> argv;
  argv.reserve(storage.size());
  for (auto& s : storage) {
    argv.push_back(s.data());
  }
  return argv;
}

static bool test_cli_missing_args_fails() {
  std::vector<std::string> storage = {"telltale"};
  auto argv = make_argv(storage);
  TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) != 0);
  return true;
}

static bool test_cli_unknown_command_fails() {
  std::vector<std::string> storage = {"telltale", "not-a-command"};
  auto argv = make_argv(storage);
  TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) != 0);
  return true;
}

static bool test_cli_write_missing_path_fails() {
  std::vector<std::string> storage = {"telltale", "write"};
  auto argv = make_argv(storage);
  TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) != 0);
  return true;
}

static bool test_cli_replay_missing_path_fails() {
  std::vector<std::string> storage = {"telltale", "replay"};
  auto argv = make_argv(storage);
  TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) != 0);
  return true;
}

static bool test_cli_verify_missing_path_fails() {
  std::vector<std::string> storage = {"telltale", "verify"};
  auto argv = make_argv(storage);
  TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) != 0);
  return true;
}

static bool test_cli_filter_missing_args_fails() {
  std::vector<std::string> storage = {"telltale", "filter", "only_one.tlog"};
  auto argv = make_argv(storage);
  TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) != 0);
  return true;
}

static bool test_cli_export_missing_args_fails() {
  std::vector<std::string> storage = {"telltale", "export", "only_one.tlog"};
  auto argv = make_argv(storage);
  TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) != 0);
  return true;
}

static bool test_cli_import_missing_args_fails() {
  std::vector<std::string> storage = {"telltale", "import", "only_one.txt"};
  auto argv = make_argv(storage);
  TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) != 0);
  return true;
}

static bool test_cli_write_replay_verify_ok() {
  std::string path = temp_path("cli_wrv.tlog");
  remove_file(path);
  {
    std::vector<std::string> storage = {"telltale", "write", path, "--events", "5"};
    auto argv = make_argv(storage);
    TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) == 0);
  }
  TEST_ASSERT(file_exists(path));
  {
    std::vector<std::string> storage = {"telltale", "verify", path};
    auto argv = make_argv(storage);
    TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) == 0);
  }
  {
    std::vector<std::string> storage = {"telltale", "replay", path};
    auto argv = make_argv(storage);
    TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) == 0);
  }
  return true;
}

static bool test_cli_export_import_roundtrip() {
  std::string bin_path = temp_path("cli_ei.bin");
  std::string txt_path = temp_path("cli_ei.txt");
  std::string out_path = temp_path("cli_ei_out.bin");
  remove_file(bin_path);
  remove_file(txt_path);
  remove_file(out_path);

  EventLogWriter w(bin_path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
                            EventLogWriter::encode_counter_payload("hits", 3, true))
                  .ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
                            EventLogWriter::encode_keyvalue_payload("k", 9, true))
                  .ok());
  TEST_ASSERT(w.finalize().ok());

  {
    std::vector<std::string> storage = {"telltale", "export", bin_path, txt_path};
    auto argv = make_argv(storage);
    TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) == 0);
  }
  TEST_ASSERT(file_exists(txt_path));
  {
    std::vector<std::string> storage = {"telltale", "import", txt_path, out_path};
    auto argv = make_argv(storage);
    TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) == 0);
  }
  TEST_ASSERT(file_exists(out_path));
  return true;
}

static bool test_cli_filter_type_range() {
  std::string in_path = temp_path("cli_filter_in.bin");
  std::string out_path = temp_path("cli_filter_out.bin");
  remove_file(in_path);
  remove_file(out_path);
  {
    std::vector<std::string> storage = {"telltale", "write", in_path, "--events", "8"};
    auto argv = make_argv(storage);
    TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) == 0);
  }
  {
    std::vector<std::string> storage = {"telltale",   "filter", in_path,      out_path,
                                        "--type-min", "1",      "--type-max", "1"};
    auto argv = make_argv(storage);
    TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) == 0);
  }
  TEST_ASSERT(file_exists(out_path));
  return true;
}

static bool test_import_malformed_missing_equals_fails() {
  TextImporter importer;
  importer.set_strict(true);
  std::string text = std::string(TEXT_FORMAT_MAGIC_LINE) + "\n" + TEXT_RECORD_BEGIN +
                     "\n  type_id\n" + TEXT_RECORD_END + "\n";
  Result r = importer.parse(text);
  TEST_ASSERT(!r.ok());
  TEST_ASSERT(r.code == ErrorCode::InvalidPayload);
  return true;
}

static bool test_import_malformed_unclosed_record_fails() {
  TextImporter importer;
  importer.set_strict(true);
  std::string text = std::string(TEXT_FORMAT_MAGIC_LINE) + "\n" + TEXT_RECORD_BEGIN +
                     "\n  type_id=0x1\n  name=\"x\"\n  value=1\n";
  Result r = importer.parse(text);
  TEST_ASSERT(!r.ok());
  TEST_ASSERT(r.code == ErrorCode::InvalidPayload);
  return true;
}

static bool test_import_open_missing_file_fails() {
  TextImporter importer;
  std::string missing = temp_path("does_not_exist_import.txt");
  remove_file(missing);
  Result r = importer.import_file(missing, temp_path("out_missing.bin"));
  TEST_ASSERT(!r.ok());
  TEST_ASSERT(r.code == ErrorCode::OpenError);
  return true;
}

static bool test_cli_import_malformed_file_fails() {
  std::string bad = temp_path("cli_bad_import.txt");
  std::string out = temp_path("cli_bad_out.bin");
  remove_file(bad);
  remove_file(out);
  {
    std::ofstream ofs(bad);
    ofs << TEXT_FORMAT_MAGIC_LINE << "\n";
    ofs << TEXT_RECORD_BEGIN << "\n";
    ofs << "  type_id=not_a_number\n";
    ofs << TEXT_RECORD_END << "\n";
  }
  std::vector<std::string> storage = {"telltale", "import", bad, out};
  auto argv = make_argv(storage);
  TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) != 0);
  return true;
}

void run_cli_validation_tests() {
  RUN_TEST(test_cli_missing_args_fails);
  RUN_TEST(test_cli_unknown_command_fails);
  RUN_TEST(test_cli_write_missing_path_fails);
  RUN_TEST(test_cli_replay_missing_path_fails);
  RUN_TEST(test_cli_verify_missing_path_fails);
  RUN_TEST(test_cli_filter_missing_args_fails);
  RUN_TEST(test_cli_export_missing_args_fails);
  RUN_TEST(test_cli_import_missing_args_fails);
  RUN_TEST(test_cli_write_replay_verify_ok);
  RUN_TEST(test_cli_export_import_roundtrip);
  RUN_TEST(test_cli_filter_type_range);
  RUN_TEST(test_import_malformed_missing_equals_fails);
  RUN_TEST(test_import_malformed_unclosed_record_fails);
  RUN_TEST(test_import_open_missing_file_fails);
  RUN_TEST(test_cli_import_malformed_file_fails);
}
