#include "telltale/cli.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

#include "telltale/binary_io.hpp"
#include "telltale/builtin_handlers.hpp"
#include "telltale/compaction_engine.hpp"
#include "telltale/crc32.hpp"
#include "telltale/diff_engine.hpp"
#include "telltale/dispatcher.hpp"
#include "telltale/filter_engine.hpp"
#include "telltale/logging.hpp"
#include "telltale/schema_update.hpp"
#include "telltale/text_format.hpp"

namespace telltale {

static void print_banner() {
  log().info("cli", "Telltale Event Log Tool v1.0");
  std::cout << "Telltale Event Log Tool v1.0" << std::endl;
}

std::vector<std::string> Cli::parse_args(int argc, char* argv[]) {
  std::vector<std::string> args;
  for (int i = 0; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }
  return args;
}

bool Cli::has_flag(const std::vector<std::string>& args, const std::string& flag) {
  return std::any_of(args.begin(), args.end(), [&](const std::string& a) { return a == flag; });
}

std::string Cli::get_flag_value(const std::vector<std::string>& args, const std::string& flag,
                                const std::string& default_val) {
  for (size_t i = 0; i + 1 < args.size(); ++i) {
    if (args[i] == flag) {
      return args[i + 1];
    }
  }
  return default_val;
}

void Cli::print_usage() {
  std::cout << "Usage: telltale <command> [options]" << std::endl;
  std::cout << std::endl;
  std::cout << "Commands:" << std::endl;
  std::cout << "  write   Generate a sample event log file" << std::endl;
  std::cout << "  replay  Replay an event log file with handler execution" << std::endl;
  std::cout << "  verify  Verify CRC32 and schema without executing handlers" << std::endl;
  std::cout << "  filter  Filter records from a log into a new log file" << std::endl;
  std::cout << "  diff    Compare two log files and print a diff report" << std::endl;
  std::cout << "  compact Merge multiple log files into one output file" << std::endl;
  std::cout << "  export  Export a binary log to plain-text format" << std::endl;
  std::cout << "  import  Import a plain-text log back to binary format" << std::endl;
  std::cout << "  help    Show this help message" << std::endl;
}

void Cli::print_write_usage() {
  std::cout << "Usage: telltale write <output-file> [options]" << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  --events N    Number of events to generate (default: 20)" << std::endl;
  std::cout << "  --verbose     Enable verbose output" << std::endl;
}

void Cli::print_replay_usage() {
  std::cout << "Usage: telltale replay <input-file> [options]" << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  --verbose     Enable verbose handler output" << std::endl;
}

void Cli::print_verify_usage() {
  std::cout << "Usage: telltale verify <input-file> [options]" << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  --verbose     Enable verbose verification output" << std::endl;
}

void Cli::print_filter_usage() {
  std::cout << "Usage: telltale filter <input-file> <output-file> [options]" << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  --type ID         Match exact type ID (hex or decimal)" << std::endl;
  std::cout << "  --type-min ID     Minimum type ID (inclusive)" << std::endl;
  std::cout << "  --type-max ID     Maximum type ID (inclusive)" << std::endl;
  std::cout << "  --time-min MS     Minimum timestamp (epoch millis)" << std::endl;
  std::cout << "  --time-max MS     Maximum timestamp (epoch millis)" << std::endl;
  std::cout << "  --field NAME OP VALUE   Payload field filter (op: eq,ne,gt,lt,contains)"
            << std::endl;
  std::cout << "  --combine and|or  Combine field filters (default: and)" << std::endl;
  std::cout << "  --no-schema       Exclude schema update events" << std::endl;
  std::cout << "  --explain         Print filter statistics" << std::endl;
  std::cout << "  --verbose         Verbose output" << std::endl;
}

void Cli::print_diff_usage() {
  std::cout << "Usage: telltale diff <left-file> <right-file> [options]" << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  --verbose     Show equal records in addition to changes" << std::endl;
}

void Cli::print_compact_usage() {
  std::cout << "Usage: telltale compact <output-file> <input-file> [more-inputs...] [options]"
            << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  --no-dedupe-reset   Keep consecutive duplicate reset events" << std::endl;
  std::cout << "  --verify-output     Verify output file after compaction" << std::endl;
  std::cout << "  --verbose           Verbose output" << std::endl;
}

void Cli::print_export_usage() {
  std::cout << "Usage: telltale export <input-file> <output-file> [options]" << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  --raw-hex     Include raw hex payload for unknown types" << std::endl;
  std::cout << "  --verbose     Verbose output" << std::endl;
}

void Cli::print_import_usage() {
  std::cout << "Usage: telltale import <input-text-file> <output-binary-file> [options]"
            << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  --lenient     Continue on parse errors where possible" << std::endl;
  std::cout << "  --verbose     Verbose output" << std::endl;
}

std::vector<std::string> Cli::collect_positional(const std::vector<std::string>& args,
                                                 size_t start_index) {
  std::vector<std::string> result;
  for (size_t i = start_index; i < args.size(); ++i) {
    if (!args[i].empty() && args[i][0] == '-') {
      if (args[i] == "--verbose" || args[i] == "--explain" || args[i] == "--no-schema" ||
          args[i] == "--raw-hex" || args[i] == "--lenient" || args[i] == "--no-dedupe-reset" ||
          args[i] == "--verify-output") {
        continue;
      }
      if (args[i] == "--type" || args[i] == "--type-min" || args[i] == "--type-max" ||
          args[i] == "--time-min" || args[i] == "--time-max" || args[i] == "--field" ||
          args[i] == "--combine" || args[i] == "--events") {
        ++i;
        continue;
      }
      break;
    }
    result.push_back(args[i]);
  }
  return result;
}

int Cli::cmd_help(const std::vector<std::string>& /*args*/) {
  print_usage();
  return 0;
}

int generate_sample_log(const std::string& output_path, size_t event_count, bool verbose) {
  EventLogWriter writer(output_path);
  Result r = writer.open();
  if (!r.ok()) {
    std::cerr << "Error: " << r.message << std::endl;
    return 1;
  }
  r = writer.write_header();
  if (!r.ok()) {
    std::cerr << "Error: " << r.message << std::endl;
    return 1;
  }

  if (verbose) {
    std::cout << "Writing sample log to " << output_path << std::endl;
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
    std::cerr << "Error finalizing: " << r.message << std::endl;
    return 1;
  }

  if (verbose) {
    std::cout << "Wrote " << writer.record_count() << " records" << std::endl;
  }
  return 0;
}

int Cli::cmd_write(const std::vector<std::string>& args) {
  if (args.size() < 3) {
    print_write_usage();
    return 1;
  }
  std::string output = args[2];
  size_t event_count = 20;
  std::string ec = get_flag_value(args, "--events", "20");
  event_count = static_cast<size_t>(std::strtoul(ec.c_str(), nullptr, 10));
  bool verbose = has_flag(args, "--verbose");
  return generate_sample_log(output, event_count, verbose);
}

int Cli::cmd_replay(const std::vector<std::string>& args) {
  if (args.size() < 3) {
    print_replay_usage();
    return 1;
  }
  std::string input = args[2];
  bool verbose = has_flag(args, "--verbose");

  Dispatcher dispatcher;
  dispatcher.set_verbose(verbose);
  Result r = dispatcher.replay_file(input, ReplayMode::Execute);
  if (!r.ok()) {
    std::cerr << "Replay failed: " << r.message << std::endl;
    return 1;
  }

  const auto& report = dispatcher.last_report();
  std::cout << "Replay complete: " << report.records_processed << " records, "
            << report.handlers_invoked << " handlers invoked, " << report.schema_updates
            << " schema updates" << std::endl;
  return 0;
}

int Cli::cmd_verify(const std::vector<std::string>& args) {
  if (args.size() < 3) {
    print_verify_usage();
    return 1;
  }
  std::string input = args[2];
  bool verbose = has_flag(args, "--verbose");

  Dispatcher dispatcher;
  dispatcher.set_verbose(verbose);
  Result r = dispatcher.replay_file(input, ReplayMode::VerifyOnly);
  if (!r.ok()) {
    std::cerr << "Verify failed: " << r.message << std::endl;
    return 1;
  }

  const auto& report = dispatcher.last_report();
  std::cout << "Verify OK: " << report.records_processed << " records, " << report.schema_updates
            << " schema updates validated, " << report.missing_handlers << " unhandled types"
            << std::endl;
  return 0;
}

int Cli::cmd_filter(const std::vector<std::string>& args) {
  if (args.size() < 4) {
    print_filter_usage();
    return 1;
  }
  std::string input = args[2];
  std::string output = args[3];
  bool verbose = has_flag(args, "--verbose");
  bool explain = has_flag(args, "--explain");

  FilterEngine engine;
  Result r = engine.load(input);
  if (!r.ok()) {
    std::cerr << "Filter load failed: " << r.message << std::endl;
    return 1;
  }
  FilterCriteria criteria = FilterEngine::parse_criteria_from_args(args);
  engine.set_criteria(criteria);
  r = engine.run_filter();
  if (!r.ok()) {
    std::cerr << "Filter failed: " << r.message << std::endl;
    return 1;
  }
  r = engine.write_matches(output);
  if (!r.ok()) {
    std::cerr << "Filter write failed: " << r.message << std::endl;
    return 1;
  }
  if (verbose || explain) {
    std::cout << filter_engine_explain(engine) << std::endl;
  }
  std::cout << "Filter: " << engine.match_count() << " of " << engine.total_records_loaded()
            << " records written to " << output << std::endl;
  return 0;
}

int Cli::cmd_diff(const std::vector<std::string>& args) {
  if (args.size() < 4) {
    print_diff_usage();
    return 1;
  }
  std::string left = args[2];
  std::string right = args[3];
  bool verbose = has_flag(args, "--verbose");

  DiffEngine engine;
  Result r = engine.load_left(left);
  if (!r.ok()) {
    std::cerr << "Diff load left failed: " << r.message << std::endl;
    return 1;
  }
  r = engine.load_right(right);
  if (!r.ok()) {
    std::cerr << "Diff load right failed: " << r.message << std::endl;
    return 1;
  }
  r = engine.compare();
  if (!r.ok()) {
    std::cerr << "Diff compare failed: " << r.message << std::endl;
    return 1;
  }
  std::cout << engine.format_report(verbose) << std::endl;
  std::cout << diff_engine_summary_line(engine) << std::endl;
  return 0;
}

int Cli::cmd_compact(const std::vector<std::string>& args) {
  auto positional = collect_positional(args, 2);
  if (positional.size() < 2) {
    print_compact_usage();
    return 1;
  }
  std::string output = positional[0];
  bool verbose = has_flag(args, "--verbose");
  bool verify_output = has_flag(args, "--verify-output");

  CompactionEngine engine;
  CompactionOptions opts;
  opts.dedupe_consecutive_resets = !has_flag(args, "--no-dedupe-reset");
  engine.set_options(opts);

  for (size_t i = 1; i < positional.size(); ++i) {
    Result r = engine.add_input(positional[i], static_cast<uint32_t>(i));
    if (!r.ok()) {
      std::cerr << "Compact add input failed: " << r.message << std::endl;
      return 1;
    }
  }

  Result r = engine.compact(output);
  if (!r.ok()) {
    std::cerr << "Compact failed: " << r.message << std::endl;
    return 1;
  }

  if (verify_output) {
    r = compaction_verify_output(output, engine.stats());
    if (!r.ok()) {
      std::cerr << "Output verification failed: " << r.message << std::endl;
      return 1;
    }
  }

  const auto& stats = engine.stats();
  if (verbose) {
    std::cout << "Compacted " << stats.input_files << " files, " << stats.input_records_total
              << " input records -> " << stats.output_records << " output records, "
              << stats.resets_deduped << " resets deduped" << std::endl;
  } else {
    std::cout << "Compact OK: " << stats.output_records << " records -> " << output << std::endl;
  }
  return 0;
}

int Cli::cmd_export(const std::vector<std::string>& args) {
  if (args.size() < 4) {
    print_export_usage();
    return 1;
  }
  std::string input = args[2];
  std::string output = args[3];
  bool verbose = has_flag(args, "--verbose");

  TextExporter exporter;
  exporter.set_include_raw_hex(has_flag(args, "--raw-hex"));
  exporter.set_verbose(verbose);
  Result r = exporter.export_file(input, output);
  if (!r.ok()) {
    std::cerr << "Export failed: " << r.message << std::endl;
    return 1;
  }
  std::cout << "Exported " << exporter.records_exported() << " records to " << output << std::endl;
  return 0;
}

int Cli::cmd_import(const std::vector<std::string>& args) {
  if (args.size() < 4) {
    print_import_usage();
    return 1;
  }
  std::string input = args[2];
  std::string output = args[3];
  bool verbose = has_flag(args, "--verbose");

  TextImporter importer;
  importer.set_strict(!has_flag(args, "--lenient"));
  Result r = importer.import_file(input, output);
  if (!r.ok()) {
    std::cerr << "Import failed: " << r.message << std::endl;
    return 1;
  }
  if (verbose) {
    std::cout << "Imported " << importer.records().size() << " records";
    if (importer.parse_errors() > 0) {
      std::cout << " (" << importer.parse_errors() << " parse warnings)";
    }
    std::cout << std::endl;
  }
  std::cout << "Import OK: " << importer.records().size() << " records -> " << output << std::endl;
  return 0;
}

int Cli::run(int argc, char* argv[]) {
  if (argc < 2) {
    print_banner();
    print_usage();
    return 1;
  }

  auto args = parse_args(argc, argv);
  const std::string& cmd = args[1];

  if (cmd == "write") return cmd_write(args);
  if (cmd == "replay") return cmd_replay(args);
  if (cmd == "verify") return cmd_verify(args);
  if (cmd == "filter") return cmd_filter(args);
  if (cmd == "diff") return cmd_diff(args);
  if (cmd == "compact") return cmd_compact(args);
  if (cmd == "export") return cmd_export(args);
  if (cmd == "import") return cmd_import(args);
  if (cmd == "help" || cmd == "--help" || cmd == "-h") return cmd_help(args);

  std::cerr << "Unknown command: " << cmd << std::endl;
  print_usage();
  return 1;
}

}  // namespace telltale

// Optimization pass 40 - memory iteration 1

// Optimization pass 30 - caching iteration 1

// Optimization pass 25 - indexing iteration 1
