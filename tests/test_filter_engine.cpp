#include "test_common.hpp"

static bool write_mixed_log(const std::string& path) {
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
                            EventLogWriter::encode_counter_payload("hits", 10, true))
                  .ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
                            EventLogWriter::encode_keyvalue_payload("alpha", 100, true))
                  .ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
                            EventLogWriter::encode_print_payload("hello", 1))
                  .ok());
  TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
                            EventLogWriter::encode_counter_payload("hits", 5, false))
                  .ok());
  TEST_ASSERT(w.finalize().ok());
  return true;
}

static bool write_simple_log(const std::string& path, int n_counters) {
  remove_file(path);
  EventLogWriter w(path);
  TEST_ASSERT(w.open().ok());
  TEST_ASSERT(w.write_header().ok());
  for (int i = 0; i < n_counters; ++i) {
    TEST_ASSERT(
        w.write_event(static_cast<uint16_t>(EventType::Counter),
                      EventLogWriter::encode_counter_payload("c" + std::to_string(i), i, true))
            .ok());
  }
  TEST_ASSERT(w.finalize().ok());
  return true;
}

static bool test_filter_type_range_counters_only() {
  std::string path = temp_path("filter_type.bin");
  TEST_ASSERT(write_mixed_log(path));

  FilterEngine engine;
  TEST_ASSERT(engine.load(path).ok());

  FilterCriteria criteria;
  criteria.type_range.enabled = true;
  criteria.type_range.min_type = static_cast<uint16_t>(EventType::Counter);
  criteria.type_range.max_type = static_cast<uint16_t>(EventType::Counter);
  engine.set_criteria(criteria);

  TEST_ASSERT(engine.run_filter().ok());
  TEST_ASSERT(engine.match_count() == 2);
  for (const auto& m : engine.matches()) {
    TEST_ASSERT(m.record.type_id == static_cast<uint16_t>(EventType::Counter));
  }
  return true;
}

static bool test_filter_write_matches() {
  std::string path = temp_path("filter_src.bin");
  std::string out = temp_path("filter_out.bin");
  TEST_ASSERT(write_mixed_log(path));
  remove_file(out);

  FilterEngine engine;
  TEST_ASSERT(engine.load(path).ok());

  FilterCriteria criteria;
  criteria.type_range.enabled = true;
  criteria.type_range.min_type = static_cast<uint16_t>(EventType::KeyValue);
  criteria.type_range.max_type = static_cast<uint16_t>(EventType::KeyValue);
  engine.set_criteria(criteria);

  TEST_ASSERT(engine.run_filter().ok());
  TEST_ASSERT(engine.match_count() == 1);
  TEST_ASSERT(engine.write_matches(out).ok());
  TEST_ASSERT(file_exists(out));

  EventLogReader reader(out);
  TEST_ASSERT(reader.open().ok());
  FileHeader hdr;
  TEST_ASSERT(reader.read_header(hdr).ok());
  EventRecord rec;
  TEST_ASSERT(reader.read_next_record(rec).ok());
  TEST_ASSERT(rec.type_id == static_cast<uint16_t>(EventType::KeyValue));
  return true;
}

static bool test_filter_payload_field_name() {
  std::string path = temp_path("filter_payload.bin");
  TEST_ASSERT(write_mixed_log(path));

  FilterEngine engine;
  TEST_ASSERT(engine.load(path).ok());

  FilterCriteria criteria;
  PayloadFieldFilter pf;
  pf.enabled = true;
  pf.field_name = "name";
  pf.string_value = "hits";
  pf.op = FilterFieldOp::Equal;
  criteria.payload_fields.push_back(pf);
  engine.set_criteria(criteria);

  TEST_ASSERT(engine.run_filter().ok());
  TEST_ASSERT(engine.match_count() == 2);
  return true;
}

static bool test_filter_parse_field_op() {
  TEST_ASSERT(FilterEngine::parse_field_op("eq") == FilterFieldOp::Equal);
  TEST_ASSERT(FilterEngine::parse_field_op("contains") == FilterFieldOp::Contains);
  TEST_ASSERT(FilterEngine::field_op_name(FilterFieldOp::StartsWith) == "startswith");
  return true;
}

static bool test_filter_cli_type_flag() {
  std::string in_path = temp_path("filter_cli_in.bin");
  std::string out_path = temp_path("filter_cli_out.bin");
  TEST_ASSERT(write_simple_log(in_path, 6));
  remove_file(out_path);
  std::vector<std::string> storage = {"telltale", "filter", in_path, out_path, "--type", "1"};
  std::vector<char*> argv;
  for (auto& s : storage) argv.push_back(s.data());
  TEST_ASSERT(Cli::run(static_cast<int>(argv.size()), argv.data()) == 0);
  TEST_ASSERT(file_exists(out_path));
  EventLogReader r(out_path);
  TEST_ASSERT(r.open().ok());
  FileHeader h;
  TEST_ASSERT(r.read_header(h).ok());
  TEST_ASSERT(h.record_count >= 1);
  return true;
}

static bool test_filter_loaded_record_count() {
  std::string path = temp_path("filter_loaded.bin");
  TEST_ASSERT(write_simple_log(path, 3));
  FilterEngine engine;
  TEST_ASSERT(engine.load(path).ok());
  FilterCriteria criteria;
  criteria.type_range.enabled = true;
  criteria.type_range.min_type = static_cast<uint16_t>(EventType::Counter);
  criteria.type_range.max_type = static_cast<uint16_t>(EventType::Counter);
  criteria.combine_op = FilterCombineOp::And;
  engine.set_criteria(criteria);
  TEST_ASSERT(engine.run_filter().ok());
  TEST_ASSERT(engine.match_count() == 3);
  TEST_ASSERT(engine.total_records_loaded() >= 3);
  return true;
}

void run_filter_engine_tests() {
  RUN_TEST(test_filter_type_range_counters_only);
  RUN_TEST(test_filter_write_matches);
  RUN_TEST(test_filter_payload_field_name);
  RUN_TEST(test_filter_parse_field_op);
  RUN_TEST(test_filter_cli_type_flag);
  RUN_TEST(test_filter_loaded_record_count);
}
