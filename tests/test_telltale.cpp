#include "telltale/binary_io.hpp"
#include "telltale/dispatcher.hpp"
#include "telltale/handler_registry.hpp"
#include "telltale/schema_update.hpp"
#include "telltale/builtin_handlers.hpp"
#include "telltale/crc32.hpp"
#include "telltale/cli.hpp"
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <functional>
#include <sstream>
#include <sys/stat.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;
static std::string g_current_test;

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        std::cerr << "FAIL: " << g_current_test << " - assertion failed: " #cond \
                  << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        ++g_tests_failed; \
        return false; \
    } \
} while(0)

#define RUN_TEST(fn) do { \
    g_current_test = #fn; \
    ++g_tests_run; \
    std::cout << "  " << #fn << " ... "; \
    if (fn()) { \
        std::cout << "OK" << std::endl; \
        ++g_tests_passed; \
    } else { \
        std::cout << "FAILED" << std::endl; \
    } \
} while(0)

static bool file_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

static void remove_file(const std::string& path) {
    std::remove(path.c_str());
}

static std::string temp_path(const std::string& name) {
    return std::string("/tmp/telltale_test_") + name;
}

using namespace telltale;


static bool test_crc32_known_vectors() {
    TEST_ASSERT(Crc32::of_bytes(reinterpret_cast<const uint8_t*>("123456789"), 9) == 0xCBF43926U);
    TEST_ASSERT(Crc32::of_bytes(nullptr, 0) == 0x00000000U);

    return true;
}

static bool test_crc32_incremental() {
    Crc32 crc;
    crc.update('A'); crc.update('B'); crc.update('C');
    TEST_ASSERT(crc.finalize() == Crc32::of_bytes(reinterpret_cast<const uint8_t*>("ABC"), 3));

    return true;
}

static bool test_crc32_record() {
    std::vector<uint8_t> payload = {1,2,3};
    uint32_t c = Crc32::of_record(0x0001, payload);
    TEST_ASSERT(c != 0);
    TEST_ASSERT(c == Crc32::of_record(0x0001, payload));

    return true;
}

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

static bool test_roundtrip_counter() {
    std::string path = temp_path("counter.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("hits", 10, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("hits", 5, false)).ok());
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_roundtrip_keyvalue() {
    std::string path = temp_path("kv.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("alpha", 100, true)).ok());
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_roundtrip_timestamp() {
    std::string path = temp_path("ts.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Timestamp),
        EventLogWriter::encode_timestamp_payload("t1", 1234567890ULL, true)).ok());
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_roundtrip_checksum() {
    std::string path = temp_path("cs.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("x", 42, true)).ok());
    ReplayContext ctx;
    uint32_t crc = ctx.state().compute_state_crc(0x01);
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Checksum),
        EventLogWriter::encode_checksum_payload("c", crc, 0x01)).ok());
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_roundtrip_batch() {
    std::string path = temp_path("batch.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    std::vector<std::vector<uint8_t>> subs;
    subs.push_back(EventLogWriter::encode_counter_payload("b", 1, false));
    subs.push_back(EventLogWriter::encode_print_payload("in batch", 1));
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Batch),
        EventLogWriter::encode_batch_payload(subs)).ok());
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_roundtrip_reset() {
    std::string path = temp_path("reset.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("r", 99, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Reset),
        EventLogWriter::encode_reset_payload(0x01)).ok());
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_roundtrip_print() {
    std::string path = temp_path("print.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("hello telltale", 1)).ok());
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_roundtrip_stats() {
    std::string path = temp_path("stats.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("s", 1, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Stats),
        EventLogWriter::encode_stats_payload(0x0F, "test")).ok());
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_schema_register() {
    std::string path = temp_path("schema_reg.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_schema_update(static_cast<uint8_t>(SchemaUpdateFlag::Register),
        0x0200, static_cast<uint16_t>(HandlerId::BuiltinPrint)).ok());
    TEST_ASSERT(w.write_event(0x0200,
        EventLogWriter::encode_print_payload("registered", 1)).ok());
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_schema_deregister() {
    std::string path = temp_path("schema_dereg.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_schema_update(static_cast<uint8_t>(SchemaUpdateFlag::Register),
        0x0300, static_cast<uint16_t>(HandlerId::BuiltinNoOp)).ok());
    TEST_ASSERT(w.write_schema_update(static_cast<uint8_t>(SchemaUpdateFlag::Deregister),
        0x0300, 0).ok());
    TEST_ASSERT(w.write_event(0x0300,
        EventLogWriter::encode_print_payload("no handler", 1)).ok());
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.missing_handlers() >= 1);

    return true;
}

static bool test_schema_replace() {
    std::string path = temp_path("schema_replace.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_schema_update(static_cast<uint8_t>(SchemaUpdateFlag::Register),
        0x0400, static_cast<uint16_t>(HandlerId::BuiltinNoOp)).ok());
    TEST_ASSERT(w.write_schema_update(static_cast<uint8_t>(SchemaUpdateFlag::Replace),
        0x0400, static_cast<uint16_t>(HandlerId::BuiltinPrint)).ok());
    TEST_ASSERT(w.write_event(0x0400,
        EventLogWriter::encode_print_payload("replaced handler", 1)).ok());
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_crc_rejection() {
    std::string path = temp_path("bad_crc.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("ok", 1)).ok());
    TEST_ASSERT(w.finalize().ok());
    { std::fstream f(path, std::ios::binary|std::ios::in|std::ios::out);
      f.seekp(-4, std::ios::end);
      uint8_t bad[]={0xDE,0xAD,0xBE,0xEF}; f.write(reinterpret_cast<char*>(bad),4); }
    Dispatcher d;
    Result r = d.replay_file(path, ReplayMode::VerifyOnly);
    TEST_ASSERT(!r.ok());
    TEST_ASSERT(r.code == ErrorCode::RecordCrcMismatch);

    return true;
}

static bool test_empty_file() {
    std::string path = temp_path("empty.bin");
    remove_file(path);
    { std::ofstream f(path); }
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    Result res = r.read_header(h);
    TEST_ASSERT(!res.ok());
    TEST_ASSERT(res.code == ErrorCode::EmptyFile);

    return true;
}

static bool test_schema_only_file() {
    std::string path = temp_path("schema_only.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_schema_update(static_cast<uint8_t>(SchemaUpdateFlag::Register),
        0x0500, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(w.write_schema_update(static_cast<uint8_t>(SchemaUpdateFlag::Deregister),
        0x0500, 0).ok());
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::VerifyOnly).ok());
    TEST_ASSERT(d.schema_updates_applied() == 2);

    return true;
}

static bool test_verify_mode() {
    std::string path = temp_path("verify.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("v", 1, true)).ok());
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::VerifyOnly).ok());
    TEST_ASSERT(d.handlers_invoked() == 0);

    return true;
}

static bool test_handler_registry_basic() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    TEST_ASSERT(reg.register_handler(0x100,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinNoOp), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinNoOp)).ok());
    TEST_ASSERT(reg.has_handler(0x100));
    TEST_ASSERT(reg.deregister_handler(0x100).ok());
    TEST_ASSERT(!reg.has_handler(0x100));

    return true;
}

static bool test_schema_codec() {
    auto payload = SchemaUpdateCodec::encode(
        static_cast<uint8_t>(SchemaUpdateFlag::Register), 0x1234,
        static_cast<uint16_t>(HandlerId::BuiltinCounter), true);
    SchemaUpdatePayload decoded;
    TEST_ASSERT(SchemaUpdateCodec::decode(payload, decoded).ok());
    TEST_ASSERT(decoded.type_id == 0x1234);
    TEST_ASSERT(decoded.has_handler_id);

    return true;
}

static bool test_max_registrations() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    for (uint16_t i = 0x1000; i < 0x1000 + 100; ++i) {
        Result r = reg.register_handler(i,
            resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinNoOp), nullptr),
            &ctx, static_cast<uint16_t>(HandlerId::BuiltinNoOp));
        TEST_ASSERT(r.ok());
    }
    TEST_ASSERT(reg.active_handler_count() >= 100);

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
            EventLogWriter::encode_print_payload("e"+std::to_string(i), 0)).ok());
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
    { std::ofstream f(path, std::ios::binary); f << "BAD!"; }
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    Result res = r.read_header(h);
    TEST_ASSERT(!res.ok());
    TEST_ASSERT(res.code == ErrorCode::InvalidMagic);

    return true;
}

static bool test_generate_sample_log() {
    std::string path = temp_path("sample.bin");
    remove_file(path);
    TEST_ASSERT(generate_sample_log(path, 15, false) == 0);
    TEST_ASSERT(file_exists(path));
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_keyvalue_no_overwrite() {
    std::string path = temp_path("kv_noow.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k", 10, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k", 99, false)).ok());
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_0() {
    std::string path = temp_path("rt_0.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n0", 0, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k0", 0, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg0", static_cast<uint8_t>(0))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n0");
    TEST_ASSERT(cd.absolute == 0);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k0");
    TEST_ASSERT(kv.value == 0);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg0");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_1() {
    std::string path = temp_path("rt_1.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n1", 1, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k1", 10, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg1", static_cast<uint8_t>(1))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n1");
    TEST_ASSERT(cd.absolute == 1);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k1");
    TEST_ASSERT(kv.value == 10);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg1");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_2() {
    std::string path = temp_path("rt_2.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n2", 2, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k2", 20, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg2", static_cast<uint8_t>(2))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n2");
    TEST_ASSERT(cd.absolute == 2);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k2");
    TEST_ASSERT(kv.value == 20);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg2");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_3() {
    std::string path = temp_path("rt_3.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n3", 3, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k3", 30, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg3", static_cast<uint8_t>(3))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n3");
    TEST_ASSERT(cd.absolute == 3);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k3");
    TEST_ASSERT(kv.value == 30);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg3");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_4() {
    std::string path = temp_path("rt_4.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n4", 4, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k4", 40, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg4", static_cast<uint8_t>(0))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n4");
    TEST_ASSERT(cd.absolute == 4);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k4");
    TEST_ASSERT(kv.value == 40);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg4");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_5() {
    std::string path = temp_path("rt_5.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n5", 5, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k5", 50, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg5", static_cast<uint8_t>(1))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n5");
    TEST_ASSERT(cd.absolute == 5);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k5");
    TEST_ASSERT(kv.value == 50);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg5");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_6() {
    std::string path = temp_path("rt_6.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n6", 6, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k6", 60, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg6", static_cast<uint8_t>(2))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n6");
    TEST_ASSERT(cd.absolute == 6);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k6");
    TEST_ASSERT(kv.value == 60);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg6");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_7() {
    std::string path = temp_path("rt_7.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n7", 7, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k7", 70, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg7", static_cast<uint8_t>(3))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n7");
    TEST_ASSERT(cd.absolute == 7);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k7");
    TEST_ASSERT(kv.value == 70);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg7");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_8() {
    std::string path = temp_path("rt_8.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n8", 8, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k8", 80, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg8", static_cast<uint8_t>(0))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n8");
    TEST_ASSERT(cd.absolute == 8);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k8");
    TEST_ASSERT(kv.value == 80);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg8");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_9() {
    std::string path = temp_path("rt_9.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n9", 9, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k9", 90, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg9", static_cast<uint8_t>(1))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n9");
    TEST_ASSERT(cd.absolute == 9);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k9");
    TEST_ASSERT(kv.value == 90);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg9");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_10() {
    std::string path = temp_path("rt_10.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n10", 10, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k10", 100, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg10", static_cast<uint8_t>(2))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n10");
    TEST_ASSERT(cd.absolute == 10);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k10");
    TEST_ASSERT(kv.value == 100);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg10");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_11() {
    std::string path = temp_path("rt_11.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n11", 11, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k11", 110, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg11", static_cast<uint8_t>(3))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n11");
    TEST_ASSERT(cd.absolute == 11);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k11");
    TEST_ASSERT(kv.value == 110);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg11");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_12() {
    std::string path = temp_path("rt_12.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n12", 12, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k12", 120, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg12", static_cast<uint8_t>(0))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n12");
    TEST_ASSERT(cd.absolute == 12);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k12");
    TEST_ASSERT(kv.value == 120);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg12");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_13() {
    std::string path = temp_path("rt_13.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n13", 13, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k13", 130, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg13", static_cast<uint8_t>(1))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n13");
    TEST_ASSERT(cd.absolute == 13);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k13");
    TEST_ASSERT(kv.value == 130);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg13");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_14() {
    std::string path = temp_path("rt_14.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n14", 14, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k14", 140, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg14", static_cast<uint8_t>(2))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n14");
    TEST_ASSERT(cd.absolute == 14);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k14");
    TEST_ASSERT(kv.value == 140);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg14");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_15() {
    std::string path = temp_path("rt_15.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n15", 15, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k15", 150, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg15", static_cast<uint8_t>(3))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n15");
    TEST_ASSERT(cd.absolute == 15);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k15");
    TEST_ASSERT(kv.value == 150);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg15");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_16() {
    std::string path = temp_path("rt_16.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n16", 16, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k16", 160, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg16", static_cast<uint8_t>(0))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n16");
    TEST_ASSERT(cd.absolute == 16);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k16");
    TEST_ASSERT(kv.value == 160);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg16");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_17() {
    std::string path = temp_path("rt_17.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n17", 17, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k17", 170, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg17", static_cast<uint8_t>(1))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n17");
    TEST_ASSERT(cd.absolute == 17);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k17");
    TEST_ASSERT(kv.value == 170);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg17");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_18() {
    std::string path = temp_path("rt_18.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n18", 18, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k18", 180, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg18", static_cast<uint8_t>(2))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n18");
    TEST_ASSERT(cd.absolute == 18);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k18");
    TEST_ASSERT(kv.value == 180);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg18");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_19() {
    std::string path = temp_path("rt_19.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n19", 19, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k19", 190, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg19", static_cast<uint8_t>(3))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n19");
    TEST_ASSERT(cd.absolute == 19);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k19");
    TEST_ASSERT(kv.value == 190);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg19");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_20() {
    std::string path = temp_path("rt_20.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n20", 20, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k20", 200, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg20", static_cast<uint8_t>(0))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n20");
    TEST_ASSERT(cd.absolute == 20);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k20");
    TEST_ASSERT(kv.value == 200);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg20");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_21() {
    std::string path = temp_path("rt_21.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n21", 21, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k21", 210, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg21", static_cast<uint8_t>(1))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n21");
    TEST_ASSERT(cd.absolute == 21);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k21");
    TEST_ASSERT(kv.value == 210);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg21");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_22() {
    std::string path = temp_path("rt_22.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n22", 22, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k22", 220, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg22", static_cast<uint8_t>(2))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n22");
    TEST_ASSERT(cd.absolute == 22);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k22");
    TEST_ASSERT(kv.value == 220);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg22");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_23() {
    std::string path = temp_path("rt_23.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n23", 23, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k23", 230, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg23", static_cast<uint8_t>(3))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n23");
    TEST_ASSERT(cd.absolute == 23);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k23");
    TEST_ASSERT(kv.value == 230);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg23");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_24() {
    std::string path = temp_path("rt_24.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n24", 24, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k24", 240, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg24", static_cast<uint8_t>(0))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n24");
    TEST_ASSERT(cd.absolute == 24);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k24");
    TEST_ASSERT(kv.value == 240);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg24");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_25() {
    std::string path = temp_path("rt_25.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n25", 25, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k25", 250, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg25", static_cast<uint8_t>(1))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n25");
    TEST_ASSERT(cd.absolute == 25);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k25");
    TEST_ASSERT(kv.value == 250);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg25");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_26() {
    std::string path = temp_path("rt_26.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n26", 26, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k26", 260, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg26", static_cast<uint8_t>(2))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n26");
    TEST_ASSERT(cd.absolute == 26);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k26");
    TEST_ASSERT(kv.value == 260);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg26");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_27() {
    std::string path = temp_path("rt_27.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n27", 27, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k27", 270, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg27", static_cast<uint8_t>(3))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n27");
    TEST_ASSERT(cd.absolute == 27);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k27");
    TEST_ASSERT(kv.value == 270);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg27");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_28() {
    std::string path = temp_path("rt_28.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n28", 28, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k28", 280, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg28", static_cast<uint8_t>(0))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n28");
    TEST_ASSERT(cd.absolute == 28);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k28");
    TEST_ASSERT(kv.value == 280);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg28");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_29() {
    std::string path = temp_path("rt_29.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n29", 29, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k29", 290, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg29", static_cast<uint8_t>(1))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n29");
    TEST_ASSERT(cd.absolute == 29);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k29");
    TEST_ASSERT(kv.value == 290);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg29");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_30() {
    std::string path = temp_path("rt_30.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n30", 30, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k30", 300, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg30", static_cast<uint8_t>(2))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n30");
    TEST_ASSERT(cd.absolute == 30);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k30");
    TEST_ASSERT(kv.value == 300);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg30");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_31() {
    std::string path = temp_path("rt_31.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n31", 31, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k31", 310, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg31", static_cast<uint8_t>(3))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n31");
    TEST_ASSERT(cd.absolute == 31);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k31");
    TEST_ASSERT(kv.value == 310);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg31");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_32() {
    std::string path = temp_path("rt_32.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n32", 32, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k32", 320, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg32", static_cast<uint8_t>(0))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n32");
    TEST_ASSERT(cd.absolute == 32);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k32");
    TEST_ASSERT(kv.value == 320);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg32");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_33() {
    std::string path = temp_path("rt_33.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n33", 33, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k33", 330, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg33", static_cast<uint8_t>(1))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n33");
    TEST_ASSERT(cd.absolute == 33);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k33");
    TEST_ASSERT(kv.value == 330);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg33");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_34() {
    std::string path = temp_path("rt_34.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n34", 34, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k34", 340, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg34", static_cast<uint8_t>(2))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n34");
    TEST_ASSERT(cd.absolute == 34);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k34");
    TEST_ASSERT(kv.value == 340);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg34");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_35() {
    std::string path = temp_path("rt_35.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n35", 35, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k35", 350, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg35", static_cast<uint8_t>(3))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n35");
    TEST_ASSERT(cd.absolute == 35);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k35");
    TEST_ASSERT(kv.value == 350);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg35");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_36() {
    std::string path = temp_path("rt_36.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n36", 36, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k36", 360, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg36", static_cast<uint8_t>(0))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n36");
    TEST_ASSERT(cd.absolute == 36);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k36");
    TEST_ASSERT(kv.value == 360);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg36");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_37() {
    std::string path = temp_path("rt_37.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n37", 37, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k37", 370, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg37", static_cast<uint8_t>(1))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n37");
    TEST_ASSERT(cd.absolute == 37);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k37");
    TEST_ASSERT(kv.value == 370);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg37");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_38() {
    std::string path = temp_path("rt_38.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n38", 38, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k38", 380, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg38", static_cast<uint8_t>(2))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n38");
    TEST_ASSERT(cd.absolute == 38);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k38");
    TEST_ASSERT(kv.value == 380);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg38");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_39() {
    std::string path = temp_path("rt_39.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n39", 39, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k39", 390, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg39", static_cast<uint8_t>(3))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n39");
    TEST_ASSERT(cd.absolute == 39);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k39");
    TEST_ASSERT(kv.value == 390);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg39");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_40() {
    std::string path = temp_path("rt_40.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n40", 40, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k40", 400, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg40", static_cast<uint8_t>(0))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n40");
    TEST_ASSERT(cd.absolute == 40);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k40");
    TEST_ASSERT(kv.value == 400);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg40");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_41() {
    std::string path = temp_path("rt_41.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n41", 41, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k41", 410, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg41", static_cast<uint8_t>(1))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n41");
    TEST_ASSERT(cd.absolute == 41);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k41");
    TEST_ASSERT(kv.value == 410);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg41");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_42() {
    std::string path = temp_path("rt_42.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n42", 42, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k42", 420, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg42", static_cast<uint8_t>(2))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n42");
    TEST_ASSERT(cd.absolute == 42);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k42");
    TEST_ASSERT(kv.value == 420);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg42");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_43() {
    std::string path = temp_path("rt_43.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n43", 43, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k43", 430, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg43", static_cast<uint8_t>(3))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n43");
    TEST_ASSERT(cd.absolute == 43);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k43");
    TEST_ASSERT(kv.value == 430);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg43");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_44() {
    std::string path = temp_path("rt_44.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n44", 44, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k44", 440, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg44", static_cast<uint8_t>(0))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n44");
    TEST_ASSERT(cd.absolute == 44);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k44");
    TEST_ASSERT(kv.value == 440);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg44");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_45() {
    std::string path = temp_path("rt_45.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n45", 45, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k45", 450, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg45", static_cast<uint8_t>(1))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n45");
    TEST_ASSERT(cd.absolute == 45);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k45");
    TEST_ASSERT(kv.value == 450);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg45");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_46() {
    std::string path = temp_path("rt_46.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n46", 46, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k46", 460, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg46", static_cast<uint8_t>(2))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n46");
    TEST_ASSERT(cd.absolute == 46);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k46");
    TEST_ASSERT(kv.value == 460);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg46");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_47() {
    std::string path = temp_path("rt_47.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n47", 47, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k47", 470, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg47", static_cast<uint8_t>(3))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n47");
    TEST_ASSERT(cd.absolute == 47);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k47");
    TEST_ASSERT(kv.value == 470);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg47");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_48() {
    std::string path = temp_path("rt_48.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n48", 48, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k48", 480, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg48", static_cast<uint8_t>(0))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n48");
    TEST_ASSERT(cd.absolute == 48);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k48");
    TEST_ASSERT(kv.value == 480);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg48");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_49() {
    std::string path = temp_path("rt_49.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n49", 49, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k49", 490, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg49", static_cast<uint8_t>(1))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n49");
    TEST_ASSERT(cd.absolute == 49);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k49");
    TEST_ASSERT(kv.value == 490);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg49");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_50() {
    std::string path = temp_path("rt_50.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n50", 50, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k50", 500, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg50", static_cast<uint8_t>(2))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n50");
    TEST_ASSERT(cd.absolute == 50);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k50");
    TEST_ASSERT(kv.value == 500);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg50");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_51() {
    std::string path = temp_path("rt_51.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n51", 51, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k51", 510, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg51", static_cast<uint8_t>(3))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n51");
    TEST_ASSERT(cd.absolute == 51);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k51");
    TEST_ASSERT(kv.value == 510);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg51");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_52() {
    std::string path = temp_path("rt_52.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n52", 52, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k52", 520, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg52", static_cast<uint8_t>(0))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n52");
    TEST_ASSERT(cd.absolute == 52);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k52");
    TEST_ASSERT(kv.value == 520);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg52");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_53() {
    std::string path = temp_path("rt_53.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n53", 53, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k53", 530, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg53", static_cast<uint8_t>(1))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n53");
    TEST_ASSERT(cd.absolute == 53);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k53");
    TEST_ASSERT(kv.value == 530);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg53");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_54() {
    std::string path = temp_path("rt_54.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n54", 54, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k54", 540, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg54", static_cast<uint8_t>(2))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n54");
    TEST_ASSERT(cd.absolute == 54);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k54");
    TEST_ASSERT(kv.value == 540);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg54");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_55() {
    std::string path = temp_path("rt_55.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n55", 55, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k55", 550, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg55", static_cast<uint8_t>(3))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n55");
    TEST_ASSERT(cd.absolute == 55);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k55");
    TEST_ASSERT(kv.value == 550);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg55");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_56() {
    std::string path = temp_path("rt_56.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n56", 56, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k56", 560, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg56", static_cast<uint8_t>(0))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n56");
    TEST_ASSERT(cd.absolute == 56);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k56");
    TEST_ASSERT(kv.value == 560);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg56");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_57() {
    std::string path = temp_path("rt_57.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n57", 57, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k57", 570, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg57", static_cast<uint8_t>(1))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n57");
    TEST_ASSERT(cd.absolute == 57);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k57");
    TEST_ASSERT(kv.value == 570);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg57");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_58() {
    std::string path = temp_path("rt_58.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n58", 58, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k58", 580, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg58", static_cast<uint8_t>(2))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n58");
    TEST_ASSERT(cd.absolute == 58);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k58");
    TEST_ASSERT(kv.value == 580);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg58");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_59() {
    std::string path = temp_path("rt_59.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n59", 59, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k59", 590, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg59", static_cast<uint8_t>(3))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n59");
    TEST_ASSERT(cd.absolute == 59);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k59");
    TEST_ASSERT(kv.value == 590);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg59");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_60() {
    std::string path = temp_path("rt_60.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n60", 60, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k60", 600, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg60", static_cast<uint8_t>(0))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n60");
    TEST_ASSERT(cd.absolute == 60);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k60");
    TEST_ASSERT(kv.value == 600);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg60");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_61() {
    std::string path = temp_path("rt_61.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n61", 61, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k61", 610, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg61", static_cast<uint8_t>(1))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n61");
    TEST_ASSERT(cd.absolute == 61);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k61");
    TEST_ASSERT(kv.value == 610);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg61");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_62() {
    std::string path = temp_path("rt_62.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n62", 62, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k62", 620, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg62", static_cast<uint8_t>(2))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n62");
    TEST_ASSERT(cd.absolute == 62);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k62");
    TEST_ASSERT(kv.value == 620);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg62");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_63() {
    std::string path = temp_path("rt_63.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n63", 63, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k63", 630, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg63", static_cast<uint8_t>(3))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n63");
    TEST_ASSERT(cd.absolute == 63);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k63");
    TEST_ASSERT(kv.value == 630);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg63");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_64() {
    std::string path = temp_path("rt_64.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n64", 64, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k64", 640, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg64", static_cast<uint8_t>(0))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n64");
    TEST_ASSERT(cd.absolute == 64);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k64");
    TEST_ASSERT(kv.value == 640);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg64");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_65() {
    std::string path = temp_path("rt_65.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n65", 65, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k65", 650, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg65", static_cast<uint8_t>(1))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n65");
    TEST_ASSERT(cd.absolute == 65);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k65");
    TEST_ASSERT(kv.value == 650);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg65");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_66() {
    std::string path = temp_path("rt_66.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n66", 66, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k66", 660, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg66", static_cast<uint8_t>(2))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n66");
    TEST_ASSERT(cd.absolute == 66);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k66");
    TEST_ASSERT(kv.value == 660);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg66");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_67() {
    std::string path = temp_path("rt_67.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n67", 67, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k67", 670, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg67", static_cast<uint8_t>(3))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n67");
    TEST_ASSERT(cd.absolute == 67);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k67");
    TEST_ASSERT(kv.value == 670);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg67");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_68() {
    std::string path = temp_path("rt_68.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n68", 68, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k68", 680, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg68", static_cast<uint8_t>(0))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n68");
    TEST_ASSERT(cd.absolute == 68);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k68");
    TEST_ASSERT(kv.value == 680);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg68");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_69() {
    std::string path = temp_path("rt_69.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n69", 69, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k69", 690, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg69", static_cast<uint8_t>(1))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n69");
    TEST_ASSERT(cd.absolute == 69);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k69");
    TEST_ASSERT(kv.value == 690);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg69");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_70() {
    std::string path = temp_path("rt_70.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n70", 70, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k70", 700, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg70", static_cast<uint8_t>(2))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n70");
    TEST_ASSERT(cd.absolute == 70);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k70");
    TEST_ASSERT(kv.value == 700);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg70");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_71() {
    std::string path = temp_path("rt_71.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n71", 71, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k71", 710, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg71", static_cast<uint8_t>(3))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n71");
    TEST_ASSERT(cd.absolute == 71);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k71");
    TEST_ASSERT(kv.value == 710);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg71");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_72() {
    std::string path = temp_path("rt_72.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n72", 72, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k72", 720, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg72", static_cast<uint8_t>(0))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n72");
    TEST_ASSERT(cd.absolute == 72);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k72");
    TEST_ASSERT(kv.value == 720);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg72");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_73() {
    std::string path = temp_path("rt_73.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n73", 73, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k73", 730, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg73", static_cast<uint8_t>(1))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n73");
    TEST_ASSERT(cd.absolute == 73);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k73");
    TEST_ASSERT(kv.value == 730);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg73");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_74() {
    std::string path = temp_path("rt_74.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n74", 74, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k74", 740, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg74", static_cast<uint8_t>(2))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n74");
    TEST_ASSERT(cd.absolute == 74);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k74");
    TEST_ASSERT(kv.value == 740);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg74");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_75() {
    std::string path = temp_path("rt_75.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n75", 75, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k75", 750, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg75", static_cast<uint8_t>(3))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n75");
    TEST_ASSERT(cd.absolute == 75);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k75");
    TEST_ASSERT(kv.value == 750);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg75");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_76() {
    std::string path = temp_path("rt_76.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n76", 76, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k76", 760, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg76", static_cast<uint8_t>(0))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n76");
    TEST_ASSERT(cd.absolute == 76);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k76");
    TEST_ASSERT(kv.value == 760);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg76");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_77() {
    std::string path = temp_path("rt_77.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n77", 77, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k77", 770, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg77", static_cast<uint8_t>(1))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n77");
    TEST_ASSERT(cd.absolute == 77);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k77");
    TEST_ASSERT(kv.value == 770);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg77");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_78() {
    std::string path = temp_path("rt_78.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n78", 78, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k78", 780, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg78", static_cast<uint8_t>(2))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n78");
    TEST_ASSERT(cd.absolute == 78);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k78");
    TEST_ASSERT(kv.value == 780);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg78");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_payload_roundtrip_79() {
    std::string path = temp_path("rt_79.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("n79", 79, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::KeyValue),
        EventLogWriter::encode_keyvalue_payload("k79", 790, true)).ok());
    TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Print),
        EventLogWriter::encode_print_payload("msg79", static_cast<uint8_t>(3))).ok());
    TEST_ASSERT(w.finalize().ok());
    EventLogReader r(path);
    TEST_ASSERT(r.open().ok());
    FileHeader h;
    TEST_ASSERT(r.read_header(h).ok());
    TEST_ASSERT(h.record_count == 3);
    TEST_ASSERT(r.seek_to_records().ok());
    EventRecord rec;
    TEST_ASSERT(r.read_next_record(rec).ok());
    CounterDelta cd;
    TEST_ASSERT(EventLogReader::decode_counter_payload(rec.payload, cd).ok());
    TEST_ASSERT(cd.name == "n79");
    TEST_ASSERT(cd.absolute == 79);
    TEST_ASSERT(r.read_next_record(rec).ok());
    KeyValueEntry kv;
    TEST_ASSERT(EventLogReader::decode_keyvalue_payload(rec.payload, kv).ok());
    TEST_ASSERT(kv.key == "k79");
    TEST_ASSERT(kv.value == 790);
    TEST_ASSERT(r.read_next_record(rec).ok());
    PrintMessage pm;
    TEST_ASSERT(EventLogReader::decode_print_payload(rec.payload, pm).ok());
    TEST_ASSERT(pm.message == "msg79");
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());

    return true;
}

static bool test_state_crc_0() {
    ReplayState st;
    for (int j = 0; j < 1; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (0 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 0 == 0);
    }

    return true;
}

static bool test_state_crc_1() {
    ReplayState st;
    for (int j = 0; j < 2; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (1 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 1 == 0);
    }

    return true;
}

static bool test_state_crc_2() {
    ReplayState st;
    for (int j = 0; j < 3; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (2 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 2 == 0);
    }

    return true;
}

static bool test_state_crc_3() {
    ReplayState st;
    for (int j = 0; j < 4; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (3 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 3 == 0);
    }

    return true;
}

static bool test_state_crc_4() {
    ReplayState st;
    for (int j = 0; j < 5; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (4 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 4 == 0);
    }

    return true;
}

static bool test_state_crc_5() {
    ReplayState st;
    for (int j = 0; j < 6; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (5 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 5 == 0);
    }

    return true;
}

static bool test_state_crc_6() {
    ReplayState st;
    for (int j = 0; j < 7; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (6 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 6 == 0);
    }

    return true;
}

static bool test_state_crc_7() {
    ReplayState st;
    for (int j = 0; j < 8; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (7 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 7 == 0);
    }

    return true;
}

static bool test_state_crc_8() {
    ReplayState st;
    for (int j = 0; j < 9; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (8 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 8 == 0);
    }

    return true;
}

static bool test_state_crc_9() {
    ReplayState st;
    for (int j = 0; j < 10; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (9 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 9 == 0);
    }

    return true;
}

static bool test_state_crc_10() {
    ReplayState st;
    for (int j = 0; j < 11; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (10 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 10 == 0);
    }

    return true;
}

static bool test_state_crc_11() {
    ReplayState st;
    for (int j = 0; j < 12; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (11 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 11 == 0);
    }

    return true;
}

static bool test_state_crc_12() {
    ReplayState st;
    for (int j = 0; j < 13; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (12 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 12 == 0);
    }

    return true;
}

static bool test_state_crc_13() {
    ReplayState st;
    for (int j = 0; j < 14; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (13 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 13 == 0);
    }

    return true;
}

static bool test_state_crc_14() {
    ReplayState st;
    for (int j = 0; j < 15; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (14 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 14 == 0);
    }

    return true;
}

static bool test_state_crc_15() {
    ReplayState st;
    for (int j = 0; j < 16; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (15 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 15 == 0);
    }

    return true;
}

static bool test_state_crc_16() {
    ReplayState st;
    for (int j = 0; j < 17; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (16 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 16 == 0);
    }

    return true;
}

static bool test_state_crc_17() {
    ReplayState st;
    for (int j = 0; j < 18; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (17 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 17 == 0);
    }

    return true;
}

static bool test_state_crc_18() {
    ReplayState st;
    for (int j = 0; j < 19; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (18 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 18 == 0);
    }

    return true;
}

static bool test_state_crc_19() {
    ReplayState st;
    for (int j = 0; j < 20; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (19 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 19 == 0);
    }

    return true;
}

static bool test_state_crc_20() {
    ReplayState st;
    for (int j = 0; j < 1; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (20 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 20 == 0);
    }

    return true;
}

static bool test_state_crc_21() {
    ReplayState st;
    for (int j = 0; j < 2; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (21 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 21 == 0);
    }

    return true;
}

static bool test_state_crc_22() {
    ReplayState st;
    for (int j = 0; j < 3; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (22 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 22 == 0);
    }

    return true;
}

static bool test_state_crc_23() {
    ReplayState st;
    for (int j = 0; j < 4; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (23 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 23 == 0);
    }

    return true;
}

static bool test_state_crc_24() {
    ReplayState st;
    for (int j = 0; j < 5; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (24 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 24 == 0);
    }

    return true;
}

static bool test_state_crc_25() {
    ReplayState st;
    for (int j = 0; j < 6; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (25 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 25 == 0);
    }

    return true;
}

static bool test_state_crc_26() {
    ReplayState st;
    for (int j = 0; j < 7; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (26 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 26 == 0);
    }

    return true;
}

static bool test_state_crc_27() {
    ReplayState st;
    for (int j = 0; j < 8; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (27 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 27 == 0);
    }

    return true;
}

static bool test_state_crc_28() {
    ReplayState st;
    for (int j = 0; j < 9; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (28 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 28 == 0);
    }

    return true;
}

static bool test_state_crc_29() {
    ReplayState st;
    for (int j = 0; j < 10; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (29 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 29 == 0);
    }

    return true;
}

static bool test_state_crc_30() {
    ReplayState st;
    for (int j = 0; j < 11; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (30 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 30 == 0);
    }

    return true;
}

static bool test_state_crc_31() {
    ReplayState st;
    for (int j = 0; j < 12; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (31 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 31 == 0);
    }

    return true;
}

static bool test_state_crc_32() {
    ReplayState st;
    for (int j = 0; j < 13; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (32 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 32 == 0);
    }

    return true;
}

static bool test_state_crc_33() {
    ReplayState st;
    for (int j = 0; j < 14; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (33 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 33 == 0);
    }

    return true;
}

static bool test_state_crc_34() {
    ReplayState st;
    for (int j = 0; j < 15; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (34 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 34 == 0);
    }

    return true;
}

static bool test_state_crc_35() {
    ReplayState st;
    for (int j = 0; j < 16; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (35 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 35 == 0);
    }

    return true;
}

static bool test_state_crc_36() {
    ReplayState st;
    for (int j = 0; j < 17; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (36 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 36 == 0);
    }

    return true;
}

static bool test_state_crc_37() {
    ReplayState st;
    for (int j = 0; j < 18; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (37 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 37 == 0);
    }

    return true;
}

static bool test_state_crc_38() {
    ReplayState st;
    for (int j = 0; j < 19; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (38 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 38 == 0);
    }

    return true;
}

static bool test_state_crc_39() {
    ReplayState st;
    for (int j = 0; j < 20; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (39 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 39 == 0);
    }

    return true;
}

static bool test_state_crc_40() {
    ReplayState st;
    for (int j = 0; j < 1; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (40 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 40 == 0);
    }

    return true;
}

static bool test_state_crc_41() {
    ReplayState st;
    for (int j = 0; j < 2; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (41 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 41 == 0);
    }

    return true;
}

static bool test_state_crc_42() {
    ReplayState st;
    for (int j = 0; j < 3; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (42 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 42 == 0);
    }

    return true;
}

static bool test_state_crc_43() {
    ReplayState st;
    for (int j = 0; j < 4; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (43 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 43 == 0);
    }

    return true;
}

static bool test_state_crc_44() {
    ReplayState st;
    for (int j = 0; j < 5; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (44 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 44 == 0);
    }

    return true;
}

static bool test_state_crc_45() {
    ReplayState st;
    for (int j = 0; j < 6; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (45 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 45 == 0);
    }

    return true;
}

static bool test_state_crc_46() {
    ReplayState st;
    for (int j = 0; j < 7; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (46 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 46 == 0);
    }

    return true;
}

static bool test_state_crc_47() {
    ReplayState st;
    for (int j = 0; j < 8; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (47 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 47 == 0);
    }

    return true;
}

static bool test_state_crc_48() {
    ReplayState st;
    for (int j = 0; j < 9; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (48 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 48 == 0);
    }

    return true;
}

static bool test_state_crc_49() {
    ReplayState st;
    for (int j = 0; j < 10; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (49 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 49 == 0);
    }

    return true;
}

static bool test_state_crc_50() {
    ReplayState st;
    for (int j = 0; j < 11; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (50 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 50 == 0);
    }

    return true;
}

static bool test_state_crc_51() {
    ReplayState st;
    for (int j = 0; j < 12; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (51 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 51 == 0);
    }

    return true;
}

static bool test_state_crc_52() {
    ReplayState st;
    for (int j = 0; j < 13; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (52 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 52 == 0);
    }

    return true;
}

static bool test_state_crc_53() {
    ReplayState st;
    for (int j = 0; j < 14; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (53 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 53 == 0);
    }

    return true;
}

static bool test_state_crc_54() {
    ReplayState st;
    for (int j = 0; j < 15; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (54 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 54 == 0);
    }

    return true;
}

static bool test_state_crc_55() {
    ReplayState st;
    for (int j = 0; j < 16; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (55 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 55 == 0);
    }

    return true;
}

static bool test_state_crc_56() {
    ReplayState st;
    for (int j = 0; j < 17; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (56 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 56 == 0);
    }

    return true;
}

static bool test_state_crc_57() {
    ReplayState st;
    for (int j = 0; j < 18; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (57 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 57 == 0);
    }

    return true;
}

static bool test_state_crc_58() {
    ReplayState st;
    for (int j = 0; j < 19; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (58 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 58 == 0);
    }

    return true;
}

static bool test_state_crc_59() {
    ReplayState st;
    for (int j = 0; j < 20; ++j) {
        st.increment_counter("c"+std::to_string(j), j+1);
        st.set_key_value("k"+std::to_string(j), j*100, true);
    }
    uint32_t crc1 = st.compute_state_crc(0x03);
    uint32_t crc2 = st.compute_state_crc(0x03);
    TEST_ASSERT(crc1 == crc2);
    if (59 > 0) {
        st.increment_counter("extra", 1);
        uint32_t crc3 = st.compute_state_crc(0x01);
        TEST_ASSERT(crc3 != crc1 || 59 == 0);
    }

    return true;
}

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

static bool test_registry_resize_0() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 0);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_1() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 1);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_2() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 2);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_3() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 3);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_4() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 4);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_5() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 5);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_6() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 6);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_7() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 7);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_8() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 8);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_9() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 9);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_10() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 10);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_11() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 11);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_12() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 12);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_13() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 13);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_14() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 14);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_15() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 15);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_16() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 16);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_17() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 17);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_18() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 18);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_19() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 19);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_20() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 20);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_21() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 21);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_22() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 22);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_23() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 23);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_24() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 24);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_25() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 25);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_26() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 26);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_27() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 27);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_28() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 28);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_registry_resize_29() {
    HandlerRegistry reg;
    ReplayContext ctx;
    reg.set_handler_resolver([&ctx](uint16_t hid) {
        ReplayContext* p = &ctx;
        return std::make_pair(resolve_builtin_handler(hid, &p), static_cast<void*>(&ctx));
    });
    uint16_t tid = static_cast<uint16_t>(0x2000 + 29);
    TEST_ASSERT(reg.register_handler(tid,
        resolve_builtin_handler(static_cast<uint16_t>(HandlerId::BuiltinCounter), nullptr),
        &ctx, static_cast<uint16_t>(HandlerId::BuiltinCounter)).ok());
    TEST_ASSERT(reg.has_handler(tid));
    TEST_ASSERT(reg.table_size() > static_cast<size_t>(tid) || reg.table_size() >= 256);

    return true;
}

static bool test_multi_event_log_0() {
    std::string path = temp_path("multi_0.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 5; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(5));

    return true;
}

static bool test_multi_event_log_1() {
    std::string path = temp_path("multi_1.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 6; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(6));

    return true;
}

static bool test_multi_event_log_2() {
    std::string path = temp_path("multi_2.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 7; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(7));

    return true;
}

static bool test_multi_event_log_3() {
    std::string path = temp_path("multi_3.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 8; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(8));

    return true;
}

static bool test_multi_event_log_4() {
    std::string path = temp_path("multi_4.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 9; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(9));

    return true;
}

static bool test_multi_event_log_5() {
    std::string path = temp_path("multi_5.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 10; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(10));

    return true;
}

static bool test_multi_event_log_6() {
    std::string path = temp_path("multi_6.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 11; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(11));

    return true;
}

static bool test_multi_event_log_7() {
    std::string path = temp_path("multi_7.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 12; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(12));

    return true;
}

static bool test_multi_event_log_8() {
    std::string path = temp_path("multi_8.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 13; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(13));

    return true;
}

static bool test_multi_event_log_9() {
    std::string path = temp_path("multi_9.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 14; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(14));

    return true;
}

static bool test_multi_event_log_10() {
    std::string path = temp_path("multi_10.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 5; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(5));

    return true;
}

static bool test_multi_event_log_11() {
    std::string path = temp_path("multi_11.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 6; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(6));

    return true;
}

static bool test_multi_event_log_12() {
    std::string path = temp_path("multi_12.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 7; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(7));

    return true;
}

static bool test_multi_event_log_13() {
    std::string path = temp_path("multi_13.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 8; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(8));

    return true;
}

static bool test_multi_event_log_14() {
    std::string path = temp_path("multi_14.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 9; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(9));

    return true;
}

static bool test_multi_event_log_15() {
    std::string path = temp_path("multi_15.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 10; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(10));

    return true;
}

static bool test_multi_event_log_16() {
    std::string path = temp_path("multi_16.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 11; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(11));

    return true;
}

static bool test_multi_event_log_17() {
    std::string path = temp_path("multi_17.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 12; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(12));

    return true;
}

static bool test_multi_event_log_18() {
    std::string path = temp_path("multi_18.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 13; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(13));

    return true;
}

static bool test_multi_event_log_19() {
    std::string path = temp_path("multi_19.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 14; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(14));

    return true;
}

static bool test_multi_event_log_20() {
    std::string path = temp_path("multi_20.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 5; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(5));

    return true;
}

static bool test_multi_event_log_21() {
    std::string path = temp_path("multi_21.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 6; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(6));

    return true;
}

static bool test_multi_event_log_22() {
    std::string path = temp_path("multi_22.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 7; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(7));

    return true;
}

static bool test_multi_event_log_23() {
    std::string path = temp_path("multi_23.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 8; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(8));

    return true;
}

static bool test_multi_event_log_24() {
    std::string path = temp_path("multi_24.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 9; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(9));

    return true;
}

static bool test_multi_event_log_25() {
    std::string path = temp_path("multi_25.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 10; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(10));

    return true;
}

static bool test_multi_event_log_26() {
    std::string path = temp_path("multi_26.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 11; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(11));

    return true;
}

static bool test_multi_event_log_27() {
    std::string path = temp_path("multi_27.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 12; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(12));

    return true;
}

static bool test_multi_event_log_28() {
    std::string path = temp_path("multi_28.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 13; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(13));

    return true;
}

static bool test_multi_event_log_29() {
    std::string path = temp_path("multi_29.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 14; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(14));

    return true;
}

static bool test_multi_event_log_30() {
    std::string path = temp_path("multi_30.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 5; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(5));

    return true;
}

static bool test_multi_event_log_31() {
    std::string path = temp_path("multi_31.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 6; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(6));

    return true;
}

static bool test_multi_event_log_32() {
    std::string path = temp_path("multi_32.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 7; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(7));

    return true;
}

static bool test_multi_event_log_33() {
    std::string path = temp_path("multi_33.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 8; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(8));

    return true;
}

static bool test_multi_event_log_34() {
    std::string path = temp_path("multi_34.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 9; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(9));

    return true;
}

static bool test_multi_event_log_35() {
    std::string path = temp_path("multi_35.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 10; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(10));

    return true;
}

static bool test_multi_event_log_36() {
    std::string path = temp_path("multi_36.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 11; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(11));

    return true;
}

static bool test_multi_event_log_37() {
    std::string path = temp_path("multi_37.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 12; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(12));

    return true;
}

static bool test_multi_event_log_38() {
    std::string path = temp_path("multi_38.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 13; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(13));

    return true;
}

static bool test_multi_event_log_39() {
    std::string path = temp_path("multi_39.bin");
    remove_file(path);
    EventLogWriter w(path);
    TEST_ASSERT(w.open().ok());
    TEST_ASSERT(w.write_header().ok());
    for (int e = 0; e < 14; ++e) {
        TEST_ASSERT(w.write_event(static_cast<uint16_t>(EventType::Counter),
            EventLogWriter::encode_counter_payload("e"+std::to_string(e), 1, false)).ok());
    }
    TEST_ASSERT(w.finalize().ok());
    Dispatcher d;
    TEST_ASSERT(d.replay_file(path, ReplayMode::Execute).ok());
    TEST_ASSERT(d.records_processed() == static_cast<uint32_t>(14));

    return true;
}

int main() {
    std::cout << "Telltale Test Suite" << std::endl;
    std::cout << "===================" << std::endl;
    RUN_TEST(test_crc32_known_vectors);
    RUN_TEST(test_crc32_incremental);
    RUN_TEST(test_crc32_record);
    RUN_TEST(test_writer_reader_header);
    RUN_TEST(test_roundtrip_counter);
    RUN_TEST(test_roundtrip_keyvalue);
    RUN_TEST(test_roundtrip_timestamp);
    RUN_TEST(test_roundtrip_checksum);
    RUN_TEST(test_roundtrip_batch);
    RUN_TEST(test_roundtrip_reset);
    RUN_TEST(test_roundtrip_print);
    RUN_TEST(test_roundtrip_stats);
    RUN_TEST(test_schema_register);
    RUN_TEST(test_schema_deregister);
    RUN_TEST(test_schema_replace);
    RUN_TEST(test_crc_rejection);
    RUN_TEST(test_empty_file);
    RUN_TEST(test_schema_only_file);
    RUN_TEST(test_verify_mode);
    RUN_TEST(test_handler_registry_basic);
    RUN_TEST(test_schema_codec);
    RUN_TEST(test_max_registrations);
    RUN_TEST(test_record_count_finalize);
    RUN_TEST(test_invalid_magic);
    RUN_TEST(test_generate_sample_log);
    RUN_TEST(test_keyvalue_no_overwrite);
    RUN_TEST(test_payload_roundtrip_0);
    RUN_TEST(test_payload_roundtrip_1);
    RUN_TEST(test_payload_roundtrip_2);
    RUN_TEST(test_payload_roundtrip_3);
    RUN_TEST(test_payload_roundtrip_4);
    RUN_TEST(test_payload_roundtrip_5);
    RUN_TEST(test_payload_roundtrip_6);
    RUN_TEST(test_payload_roundtrip_7);
    RUN_TEST(test_payload_roundtrip_8);
    RUN_TEST(test_payload_roundtrip_9);
    RUN_TEST(test_payload_roundtrip_10);
    RUN_TEST(test_payload_roundtrip_11);
    RUN_TEST(test_payload_roundtrip_12);
    RUN_TEST(test_payload_roundtrip_13);
    RUN_TEST(test_payload_roundtrip_14);
    RUN_TEST(test_payload_roundtrip_15);
    RUN_TEST(test_payload_roundtrip_16);
    RUN_TEST(test_payload_roundtrip_17);
    RUN_TEST(test_payload_roundtrip_18);
    RUN_TEST(test_payload_roundtrip_19);
    RUN_TEST(test_payload_roundtrip_20);
    RUN_TEST(test_payload_roundtrip_21);
    RUN_TEST(test_payload_roundtrip_22);
    RUN_TEST(test_payload_roundtrip_23);
    RUN_TEST(test_payload_roundtrip_24);
    RUN_TEST(test_payload_roundtrip_25);
    RUN_TEST(test_payload_roundtrip_26);
    RUN_TEST(test_payload_roundtrip_27);
    RUN_TEST(test_payload_roundtrip_28);
    RUN_TEST(test_payload_roundtrip_29);
    RUN_TEST(test_payload_roundtrip_30);
    RUN_TEST(test_payload_roundtrip_31);
    RUN_TEST(test_payload_roundtrip_32);
    RUN_TEST(test_payload_roundtrip_33);
    RUN_TEST(test_payload_roundtrip_34);
    RUN_TEST(test_payload_roundtrip_35);
    RUN_TEST(test_payload_roundtrip_36);
    RUN_TEST(test_payload_roundtrip_37);
    RUN_TEST(test_payload_roundtrip_38);
    RUN_TEST(test_payload_roundtrip_39);
    RUN_TEST(test_payload_roundtrip_40);
    RUN_TEST(test_payload_roundtrip_41);
    RUN_TEST(test_payload_roundtrip_42);
    RUN_TEST(test_payload_roundtrip_43);
    RUN_TEST(test_payload_roundtrip_44);
    RUN_TEST(test_payload_roundtrip_45);
    RUN_TEST(test_payload_roundtrip_46);
    RUN_TEST(test_payload_roundtrip_47);
    RUN_TEST(test_payload_roundtrip_48);
    RUN_TEST(test_payload_roundtrip_49);
    RUN_TEST(test_payload_roundtrip_50);
    RUN_TEST(test_payload_roundtrip_51);
    RUN_TEST(test_payload_roundtrip_52);
    RUN_TEST(test_payload_roundtrip_53);
    RUN_TEST(test_payload_roundtrip_54);
    RUN_TEST(test_payload_roundtrip_55);
    RUN_TEST(test_payload_roundtrip_56);
    RUN_TEST(test_payload_roundtrip_57);
    RUN_TEST(test_payload_roundtrip_58);
    RUN_TEST(test_payload_roundtrip_59);
    RUN_TEST(test_payload_roundtrip_60);
    RUN_TEST(test_payload_roundtrip_61);
    RUN_TEST(test_payload_roundtrip_62);
    RUN_TEST(test_payload_roundtrip_63);
    RUN_TEST(test_payload_roundtrip_64);
    RUN_TEST(test_payload_roundtrip_65);
    RUN_TEST(test_payload_roundtrip_66);
    RUN_TEST(test_payload_roundtrip_67);
    RUN_TEST(test_payload_roundtrip_68);
    RUN_TEST(test_payload_roundtrip_69);
    RUN_TEST(test_payload_roundtrip_70);
    RUN_TEST(test_payload_roundtrip_71);
    RUN_TEST(test_payload_roundtrip_72);
    RUN_TEST(test_payload_roundtrip_73);
    RUN_TEST(test_payload_roundtrip_74);
    RUN_TEST(test_payload_roundtrip_75);
    RUN_TEST(test_payload_roundtrip_76);
    RUN_TEST(test_payload_roundtrip_77);
    RUN_TEST(test_payload_roundtrip_78);
    RUN_TEST(test_payload_roundtrip_79);
    RUN_TEST(test_state_crc_0);
    RUN_TEST(test_state_crc_1);
    RUN_TEST(test_state_crc_2);
    RUN_TEST(test_state_crc_3);
    RUN_TEST(test_state_crc_4);
    RUN_TEST(test_state_crc_5);
    RUN_TEST(test_state_crc_6);
    RUN_TEST(test_state_crc_7);
    RUN_TEST(test_state_crc_8);
    RUN_TEST(test_state_crc_9);
    RUN_TEST(test_state_crc_10);
    RUN_TEST(test_state_crc_11);
    RUN_TEST(test_state_crc_12);
    RUN_TEST(test_state_crc_13);
    RUN_TEST(test_state_crc_14);
    RUN_TEST(test_state_crc_15);
    RUN_TEST(test_state_crc_16);
    RUN_TEST(test_state_crc_17);
    RUN_TEST(test_state_crc_18);
    RUN_TEST(test_state_crc_19);
    RUN_TEST(test_state_crc_20);
    RUN_TEST(test_state_crc_21);
    RUN_TEST(test_state_crc_22);
    RUN_TEST(test_state_crc_23);
    RUN_TEST(test_state_crc_24);
    RUN_TEST(test_state_crc_25);
    RUN_TEST(test_state_crc_26);
    RUN_TEST(test_state_crc_27);
    RUN_TEST(test_state_crc_28);
    RUN_TEST(test_state_crc_29);
    RUN_TEST(test_state_crc_30);
    RUN_TEST(test_state_crc_31);
    RUN_TEST(test_state_crc_32);
    RUN_TEST(test_state_crc_33);
    RUN_TEST(test_state_crc_34);
    RUN_TEST(test_state_crc_35);
    RUN_TEST(test_state_crc_36);
    RUN_TEST(test_state_crc_37);
    RUN_TEST(test_state_crc_38);
    RUN_TEST(test_state_crc_39);
    RUN_TEST(test_state_crc_40);
    RUN_TEST(test_state_crc_41);
    RUN_TEST(test_state_crc_42);
    RUN_TEST(test_state_crc_43);
    RUN_TEST(test_state_crc_44);
    RUN_TEST(test_state_crc_45);
    RUN_TEST(test_state_crc_46);
    RUN_TEST(test_state_crc_47);
    RUN_TEST(test_state_crc_48);
    RUN_TEST(test_state_crc_49);
    RUN_TEST(test_state_crc_50);
    RUN_TEST(test_state_crc_51);
    RUN_TEST(test_state_crc_52);
    RUN_TEST(test_state_crc_53);
    RUN_TEST(test_state_crc_54);
    RUN_TEST(test_state_crc_55);
    RUN_TEST(test_state_crc_56);
    RUN_TEST(test_state_crc_57);
    RUN_TEST(test_state_crc_58);
    RUN_TEST(test_state_crc_59);
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
    RUN_TEST(test_registry_resize_0);
    RUN_TEST(test_registry_resize_1);
    RUN_TEST(test_registry_resize_2);
    RUN_TEST(test_registry_resize_3);
    RUN_TEST(test_registry_resize_4);
    RUN_TEST(test_registry_resize_5);
    RUN_TEST(test_registry_resize_6);
    RUN_TEST(test_registry_resize_7);
    RUN_TEST(test_registry_resize_8);
    RUN_TEST(test_registry_resize_9);
    RUN_TEST(test_registry_resize_10);
    RUN_TEST(test_registry_resize_11);
    RUN_TEST(test_registry_resize_12);
    RUN_TEST(test_registry_resize_13);
    RUN_TEST(test_registry_resize_14);
    RUN_TEST(test_registry_resize_15);
    RUN_TEST(test_registry_resize_16);
    RUN_TEST(test_registry_resize_17);
    RUN_TEST(test_registry_resize_18);
    RUN_TEST(test_registry_resize_19);
    RUN_TEST(test_registry_resize_20);
    RUN_TEST(test_registry_resize_21);
    RUN_TEST(test_registry_resize_22);
    RUN_TEST(test_registry_resize_23);
    RUN_TEST(test_registry_resize_24);
    RUN_TEST(test_registry_resize_25);
    RUN_TEST(test_registry_resize_26);
    RUN_TEST(test_registry_resize_27);
    RUN_TEST(test_registry_resize_28);
    RUN_TEST(test_registry_resize_29);
    RUN_TEST(test_multi_event_log_0);
    RUN_TEST(test_multi_event_log_1);
    RUN_TEST(test_multi_event_log_2);
    RUN_TEST(test_multi_event_log_3);
    RUN_TEST(test_multi_event_log_4);
    RUN_TEST(test_multi_event_log_5);
    RUN_TEST(test_multi_event_log_6);
    RUN_TEST(test_multi_event_log_7);
    RUN_TEST(test_multi_event_log_8);
    RUN_TEST(test_multi_event_log_9);
    RUN_TEST(test_multi_event_log_10);
    RUN_TEST(test_multi_event_log_11);
    RUN_TEST(test_multi_event_log_12);
    RUN_TEST(test_multi_event_log_13);
    RUN_TEST(test_multi_event_log_14);
    RUN_TEST(test_multi_event_log_15);
    RUN_TEST(test_multi_event_log_16);
    RUN_TEST(test_multi_event_log_17);
    RUN_TEST(test_multi_event_log_18);
    RUN_TEST(test_multi_event_log_19);
    RUN_TEST(test_multi_event_log_20);
    RUN_TEST(test_multi_event_log_21);
    RUN_TEST(test_multi_event_log_22);
    RUN_TEST(test_multi_event_log_23);
    RUN_TEST(test_multi_event_log_24);
    RUN_TEST(test_multi_event_log_25);
    RUN_TEST(test_multi_event_log_26);
    RUN_TEST(test_multi_event_log_27);
    RUN_TEST(test_multi_event_log_28);
    RUN_TEST(test_multi_event_log_29);
    RUN_TEST(test_multi_event_log_30);
    RUN_TEST(test_multi_event_log_31);
    RUN_TEST(test_multi_event_log_32);
    RUN_TEST(test_multi_event_log_33);
    RUN_TEST(test_multi_event_log_34);
    RUN_TEST(test_multi_event_log_35);
    RUN_TEST(test_multi_event_log_36);
    RUN_TEST(test_multi_event_log_37);
    RUN_TEST(test_multi_event_log_38);
    RUN_TEST(test_multi_event_log_39);
    std::cout << std::endl;
    std::cout << "Results: " << g_tests_passed << "/" << g_tests_run << " passed";
    if (g_tests_failed > 0) std::cout << ", " << g_tests_failed << " FAILED";
    std::cout << std::endl;
    return g_tests_failed > 0 ? 1 : 0;
}
