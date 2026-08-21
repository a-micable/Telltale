#include "telltale/diff_engine.hpp"
#include "test_common.hpp"

static bool write_kv_log(const std::string& path, const std::string& key, int64_t value) {
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
                            EventLogWriter::encode_keyvalue_payload(key, value, true))
                  .ok());
  TEST_ASSERT(w.finalize().ok());
  return true;
}

static bool test_diff_identical_logs() {
  std::string left = temp_path("diff_same_l.bin");
  std::string right = temp_path("diff_same_r.bin");
  TEST_ASSERT(write_kv_log(left, "k", 1));
  TEST_ASSERT(write_kv_log(right, "k", 1));
  DiffEngine engine;
  TEST_ASSERT(engine.load_left(left).ok());
  TEST_ASSERT(engine.load_right(right).ok());
  TEST_ASSERT(engine.compare().ok());
  const DiffSummary& s = engine.summary();
  TEST_ASSERT(s.left_count == 1);
  TEST_ASSERT(s.right_count == 1);
  TEST_ASSERT(s.equal_count == 1);
  TEST_ASSERT(s.insert_count == 0);
  TEST_ASSERT(s.delete_count == 0);
  return true;
}

static bool test_diff_detects_modify() {
  std::string left = temp_path("diff_mod_l.bin");
  std::string right = temp_path("diff_mod_r.bin");
  TEST_ASSERT(write_kv_log(left, "k", 1));
  TEST_ASSERT(write_kv_log(right, "k", 2));
  DiffEngine engine;
  TEST_ASSERT(engine.load_left(left).ok());
  TEST_ASSERT(engine.load_right(right).ok());
  TEST_ASSERT(engine.compare().ok());
  const DiffSummary& s = engine.summary();
  TEST_ASSERT(s.modify_count + s.insert_count + s.delete_count >= 1);
  TEST_ASSERT(!engine.changes().empty() || s.equal_count == 0 || s.modify_count >= 1);
  return true;
}

static bool test_diff_missing_right_fails() {
  std::string left = temp_path("diff_miss_l.bin");
  TEST_ASSERT(write_kv_log(left, "k", 1));
  DiffEngine engine;
  TEST_ASSERT(engine.load_left(left).ok());
  Result r = engine.load_right(temp_path("diff_does_not_exist.bin"));
  TEST_ASSERT(!r.ok());
  return true;
}

static bool test_diff_cli_subcommand() {
  std::string left = temp_path("diff_cli_l.bin");
  std::string right = temp_path("diff_cli_r.bin");
  TEST_ASSERT(write_kv_log(left, "alpha", 10));
  TEST_ASSERT(write_kv_log(right, "alpha", 10));
  std::vector<std::string> storage = {"telltale", "diff", left, right};
  std::vector<char*> argv;
  for (auto& s : storage) argv.push_back(s.data());
  TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) == 0);
  return true;
}

static bool test_diff_cli_missing_args_fails() {
  std::vector<std::string> storage = {"telltale", "diff", "only_one.bin"};
  std::vector<char*> argv;
  for (auto& s : storage) argv.push_back(s.data());
  TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) != 0);
  return true;
}

void run_diff_engine_tests() {
  RUN_TEST(test_diff_identical_logs);
  RUN_TEST(test_diff_detects_modify);
  RUN_TEST(test_diff_missing_right_fails);
  RUN_TEST(test_diff_cli_subcommand);
  RUN_TEST(test_diff_cli_missing_args_fails);
}
