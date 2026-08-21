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

void run_filter_engine_tests() {
    RUN_TEST(test_filter_type_range_counters_only);
    RUN_TEST(test_filter_write_matches);
    RUN_TEST(test_filter_payload_field_name);
    RUN_TEST(test_filter_parse_field_op);
}
