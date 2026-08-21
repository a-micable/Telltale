#include <sstream>
#include <string>
#include <vector>

#include "telltale/binary_io.hpp"
#include "telltale/builtin_handlers.hpp"
#include "telltale/cli.hpp"
#include "telltale/dispatcher.hpp"
#include "telltale/error_tracking.hpp"
#include "telltale/logging.hpp"
#include "telltale/metrics.hpp"
#include "telltale/schema_update.hpp"
#include "telltale/types.hpp"

namespace telltale {

namespace {

void cli_out(const std::string& message) { log().info("cli", message); }

void cli_err(const std::string& message) {
  error_tracking().record(ErrorCode::InvalidPayload, message, "cli");
  log().error("cli", message);
}

}  // namespace

int generate_sample_log(const std::string& output_path, size_t event_count, bool verbose) {
  EventLogWriter writer(output_path);
  Result r = writer.open();
  if (!r.ok()) {
    cli_err(std::string("Error: ") + r.message);
    return 1;
  }
  r = writer.write_header();
  if (!r.ok()) {
    cli_err(std::string("Error: ") + r.message);
    return 1;
  }

  if (verbose) {
    cli_out(std::string("Writing sample log to ") + output_path);
  }

  writer.write_event(static_cast<uint16_t>(EventType::Print),
                     EventLogWriter::encode_print_payload("Telltale sample log started", 1));

  writer.write_event(static_cast<uint16_t>(EventType::Counter),
                     EventLogWriter::encode_counter_payload("requests", 0, true));
  writer.write_event(static_cast<uint16_t>(EventType::Counter),
                     EventLogWriter::encode_counter_payload("errors", 0, true));
  writer.write_event(static_cast<uint16_t>(EventType::Counter),
                     EventLogWriter::encode_counter_payload("requests", 1, false));
  writer.write_event(static_cast<uint16_t>(EventType::Counter),
                     EventLogWriter::encode_counter_payload("requests", 5, false));

  writer.write_event(static_cast<uint16_t>(EventType::KeyValue),
                     EventLogWriter::encode_keyvalue_payload("host", 1, true));
  writer.write_event(static_cast<uint16_t>(EventType::KeyValue),
                     EventLogWriter::encode_keyvalue_payload("port", 8080, true));
  writer.write_event(static_cast<uint16_t>(EventType::KeyValue),
                     EventLogWriter::encode_keyvalue_payload("version", 42, true));

  writer.write_event(static_cast<uint16_t>(EventType::Timestamp),
                     EventLogWriter::encode_timestamp_payload("start", 1700000000000ULL, true));
  writer.write_event(static_cast<uint16_t>(EventType::Timestamp),
                     EventLogWriter::encode_timestamp_payload("checkpoint", 0, false));

  std::vector<std::vector<uint8_t>> batch_subs;
  batch_subs.push_back(EventLogWriter::encode_counter_payload("batch_items", 1, false));
  batch_subs.push_back(EventLogWriter::encode_counter_payload("batch_items", 1, false));
  batch_subs.push_back(EventLogWriter::encode_counter_payload("batch_items", 1, false));
  batch_subs.push_back(EventLogWriter::encode_print_payload("batch sub-event", 1));
  writer.write_event(static_cast<uint16_t>(EventType::Batch),
                     EventLogWriter::encode_batch_payload(batch_subs));

  {
    ReplayContext tmp;
    uint32_t crc = tmp.state().compute_state_crc(0x03);
    writer.write_event(static_cast<uint16_t>(EventType::Checksum),
                       EventLogWriter::encode_checksum_payload("state", crc, 0x03));
  }

  writer.write_event(static_cast<uint16_t>(EventType::Print),
                     EventLogWriter::encode_print_payload("Processing complete", 1));

  writer.write_event(static_cast<uint16_t>(EventType::Stats),
                     EventLogWriter::encode_stats_payload(0x0F, "sample"));

  writer.write_schema_update(static_cast<uint8_t>(SchemaUpdateFlag::Register), 0x0100,
                             static_cast<uint16_t>(HandlerId::BuiltinNoOp));

  writer.write_event(0x0100,
                     EventLogWriter::encode_print_payload("Custom type via schema update", 2));

  writer.write_schema_update(static_cast<uint8_t>(SchemaUpdateFlag::Deregister), 0x0100, 0);

  size_t extra = event_count > 12 ? event_count - 12 : 0;
  for (size_t i = 0; i < extra; ++i) {
    writer.write_event(
        static_cast<uint16_t>(EventType::Counter),
        EventLogWriter::encode_counter_payload("extra_" + std::to_string(i), 1, false));
  }

  r = writer.finalize();
  if (!r.ok()) {
    cli_err(std::string("Error finalizing: ") + r.message);
    return 1;
  }

  metrics().inc_records_written(writer.record_count());
  if (verbose) {
    cli_out(std::string("Wrote ") + std::to_string(writer.record_count()) + " records");
  }
  return 0;
}

}  // namespace telltale
