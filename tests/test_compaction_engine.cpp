#include "telltale/compaction_engine.hpp"
#include "test_common.hpp"

static bool write_counter_log(const std::string& path, const std::string& name, int64_t value) {
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
                            EventLogWriter::encode_counter_payload(name, value, true))
                  .ok());
  TEST_ASSERT(w.finalize().ok());
  return true;
}

static bool test_compact_single_input() {
  std::string in_path = temp_path("compact_in.bin");
  std::string out_path = temp_path("compact_out.bin");
  TEST_ASSERT(write_counter_log(in_path, "hits", 7));
  remove_file(out_path);
  CompactionEngine engine;
  TEST_ASSERT(engine.add_input(in_path, 0).ok());
  TEST_ASSERT(engine.compact(out_path).ok());
  TEST_ASSERT(file_exists(out_path));
  const CompactionStats& st = engine.stats();
  TEST_ASSERT(st.input_files >= 1);
  TEST_ASSERT(st.output_records >= 1);
  return true;
}

static bool test_compact_two_inputs() {
  std::string a = temp_path("compact_a.bin");
  std::string b = temp_path("compact_b.bin");
  std::string out = temp_path("compact_ab.bin");
  TEST_ASSERT(write_counter_log(a, "a", 1));
  TEST_ASSERT(write_counter_log(b, "b", 2));
  remove_file(out);
  CompactionEngine engine;
  TEST_ASSERT(engine.add_input(a, 0).ok());
  TEST_ASSERT(engine.add_input(b, 1).ok());
  TEST_ASSERT(engine.compact(out).ok());
  TEST_ASSERT(file_exists(out));
  TEST_ASSERT(engine.stats().input_files == 2);
  TEST_ASSERT(engine.stats().output_records >= 2);
  return true;
}

static bool test_compact_clear_inputs() {
  std::string a = temp_path("compact_clear.bin");
  TEST_ASSERT(write_counter_log(a, "x", 1));
  CompactionEngine engine;
  TEST_ASSERT(engine.add_input(a, 0).ok());
  engine.clear_inputs();
  Result r = engine.compact(temp_path("compact_empty_out.bin"));
  TEST_ASSERT(!r.ok() || engine.stats().input_files == 0);
  return true;
}

static bool test_compact_cli_subcommand() {
  std::string in_path = temp_path("compact_cli_in.bin");
  std::string out_path = temp_path("compact_cli_out.bin");
  TEST_ASSERT(write_counter_log(in_path, "cli", 3));
  remove_file(out_path);
  std::vector<std::string> storage = {"telltale", "compact", out_path, in_path};
  std::vector<char*> argv;
  for (auto& s : storage) argv.push_back(s.data());
  TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) == 0);
  TEST_ASSERT(file_exists(out_path));
  return true;
}

static bool test_compact_cli_missing_args_fails() {
  std::vector<std::string> storage = {"telltale", "compact", "only_one.bin"};
  std::vector<char*> argv;
  for (auto& s : storage) argv.push_back(s.data());
  TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) != 0);
  return true;
}

void run_compaction_engine_tests() {
  RUN_TEST(test_compact_single_input);
  RUN_TEST(test_compact_two_inputs);
  RUN_TEST(test_compact_clear_inputs);
  RUN_TEST(test_compact_cli_subcommand);
  RUN_TEST(test_compact_cli_missing_args_fails);
}
