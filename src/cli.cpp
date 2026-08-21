#include "telltale/cli.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include "telltale/binary_io.hpp"
#include "telltale/builtin_handlers.hpp"
#include "telltale/compaction_engine.hpp"
#include "telltale/crc32.hpp"
#include "telltale/diff_engine.hpp"
#include "telltale/dispatcher.hpp"
#include "telltale/error_tracking.hpp"
#include "telltale/filter_engine.hpp"
#include "telltale/health.hpp"
#include "telltale/input_validation.hpp"
#include "telltale/logging.hpp"
#include "telltale/metrics.hpp"
#include "telltale/schema_update.hpp"
#include "telltale/text_format.hpp"
#include "telltale/validation.hpp"

namespace telltale {

namespace {

void cli_out(const std::string& message) { log().info("cli", message); }

void cli_err(const std::string& message) {
  error_tracking().record(ErrorCode::InvalidPayload, message, "cli");
  log().error("cli", message);
}

void report_validation_failure(const Result& r) {
  metrics().inc_validation_failures();
  error_tracking().record(r.code, r.message, "validation");
  log().error("validation", r.message);
}

}  // namespace

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

int Cli::cmd_write(const std::vector<std::string>& args) {
  if (args.size() < 3) {
    print_write_usage();
    return 1;
  }
  std::string output = args[2];
  Result vr = input_validation_cli_output_path(output);
  if (!vr.ok()) {
    report_validation_failure(vr);
    return 1;
  }
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
  Result vr = input_validation_cli_input_path(input);
  if (!vr.ok()) {
    report_validation_failure(vr);
    return 1;
  }
  bool verbose = has_flag(args, "--verbose");

  Dispatcher dispatcher;
  dispatcher.set_verbose(verbose);
  Result r = dispatcher.replay_file(input, ReplayMode::Execute);
  if (!r.ok()) {
    cli_err(std::string("Replay failed: ") + r.message);
    return 1;
  }

  const auto& report = dispatcher.last_report();
  std::ostringstream oss;
  oss << "Replay complete: " << report.records_processed << " records, " << report.handlers_invoked
      << " handlers invoked, " << report.schema_updates << " schema updates";
  cli_out(oss.str());
  return 0;
}

int Cli::cmd_verify(const std::vector<std::string>& args) {
  if (args.size() < 3) {
    print_verify_usage();
    return 1;
  }
  std::string input = args[2];
  Result vr = input_validation_cli_input_path(input);
  if (!vr.ok()) {
    report_validation_failure(vr);
    return 1;
  }
  bool verbose = has_flag(args, "--verbose");

  Dispatcher dispatcher;
  dispatcher.set_verbose(verbose);
  Result r = dispatcher.replay_file(input, ReplayMode::VerifyOnly);
  if (!r.ok()) {
    cli_err(std::string("Verify failed: ") + r.message);
    return 1;
  }

  const auto& report = dispatcher.last_report();
  std::ostringstream oss;
  oss << "Verify OK: " << report.records_processed << " records, " << report.schema_updates
      << " schema updates validated, " << report.missing_handlers << " unhandled types";
  cli_out(oss.str());
  return 0;
}

int Cli::cmd_filter(const std::vector<std::string>& args) {
  if (args.size() < 4) {
    print_filter_usage();
    return 1;
  }
  std::string input = args[2];
  std::string output = args[3];
  Result vr = input_validation_cli_input_path(input);
  if (!vr.ok()) {
    report_validation_failure(vr);
    return 1;
  }
  vr = input_validation_cli_output_path(output);
  if (!vr.ok()) {
    report_validation_failure(vr);
    return 1;
  }
  vr = input_validation_flag_operand(args, "--field");
  if (!vr.ok()) {
    report_validation_failure(vr);
    return 1;
  }
  vr = input_validation_flag_operand(args, "--type");
  if (!vr.ok()) {
    report_validation_failure(vr);
    return 1;
  }
  bool verbose = has_flag(args, "--verbose");
  bool explain = has_flag(args, "--explain");

  FilterEngine engine;
  Result r = engine.load(input);
  if (!r.ok()) {
    cli_err(std::string("Filter load failed: ") + r.message);
    return 1;
  }
  FilterCriteria criteria = FilterEngine::parse_criteria_from_args(args);
  engine.set_criteria(criteria);
  r = engine.run_filter();
  if (!r.ok()) {
    cli_err(std::string("Filter failed: ") + r.message);
    return 1;
  }
  r = engine.write_matches(output);
  if (!r.ok()) {
    cli_err(std::string("Filter write failed: ") + r.message);
    return 1;
  }
  if (verbose || explain) {
    cli_out(filter_engine_explain(engine));
  }
  std::ostringstream oss;
  oss << "Filter: " << engine.match_count() << " of " << engine.total_records_loaded()
      << " records written to " << output;
  cli_out(oss.str());
  return 0;
}

int Cli::cmd_diff(const std::vector<std::string>& args) {
  if (args.size() < 4) {
    print_diff_usage();
    return 1;
  }
  std::string left = args[2];
  std::string right = args[3];
  Result vr = input_validation_cli_input_path(left);
  if (!vr.ok()) {
    report_validation_failure(vr);
    return 1;
  }
  vr = input_validation_cli_input_path(right);
  if (!vr.ok()) {
    report_validation_failure(vr);
    return 1;
  }
  bool verbose = has_flag(args, "--verbose");

  DiffEngine engine;
  Result r = engine.load_left(left);
  if (!r.ok()) {
    cli_err(std::string("Diff load left failed: ") + r.message);
    return 1;
  }
  r = engine.load_right(right);
  if (!r.ok()) {
    cli_err(std::string("Diff load right failed: ") + r.message);
    return 1;
  }
  r = engine.compare();
  if (!r.ok()) {
    cli_err(std::string("Diff compare failed: ") + r.message);
    return 1;
  }
  cli_out(engine.format_report(verbose));
  cli_out(diff_engine_summary_line(engine));
  return 0;
}

int Cli::cmd_compact(const std::vector<std::string>& args) {
  auto positional = collect_positional(args, 2);
  if (positional.size() < 2) {
    print_compact_usage();
    return 1;
  }
  std::string output = positional[0];
  Result vr = input_validation_cli_output_path(output);
  if (!vr.ok()) {
    report_validation_failure(vr);
    return 1;
  }
  bool verbose = has_flag(args, "--verbose");
  bool verify_output = has_flag(args, "--verify-output");

  CompactionEngine engine;
  CompactionOptions opts;
  opts.dedupe_consecutive_resets = !has_flag(args, "--no-dedupe-reset");
  engine.set_options(opts);

  for (size_t i = 1; i < positional.size(); ++i) {
    Result ir = input_validation_cli_input_path(positional[i]);
    if (!ir.ok()) {
      report_validation_failure(ir);
      return 1;
    }
    Result r = engine.add_input(positional[i], static_cast<uint32_t>(i));
    if (!r.ok()) {
      cli_err(std::string("Compact add input failed: ") + r.message);
      return 1;
    }
  }

  Result r = engine.compact(output);
  if (!r.ok()) {
    cli_err(std::string("Compact failed: ") + r.message);
    return 1;
  }

  if (verify_output) {
    r = compaction_verify_output(output, engine.stats());
    if (!r.ok()) {
      cli_err(std::string("Output verification failed: ") + r.message);
      return 1;
    }
  }

  const auto& stats = engine.stats();
  metrics().inc_records_written(stats.output_records);
  if (verbose) {
    std::ostringstream oss;
    oss << "Compacted " << stats.input_files << " files, " << stats.input_records_total
        << " input records -> " << stats.output_records << " output records, "
        << stats.resets_deduped << " resets deduped";
    cli_out(oss.str());
  } else {
    std::ostringstream oss;
    oss << "Compact OK: " << stats.output_records << " records -> " << output;
    cli_out(oss.str());
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
  Result vr = input_validation_cli_input_path(input);
  if (!vr.ok()) {
    report_validation_failure(vr);
    return 1;
  }
  vr = input_validation_cli_output_path(output);
  if (!vr.ok()) {
    report_validation_failure(vr);
    return 1;
  }
  bool verbose = has_flag(args, "--verbose");

  TextExporter exporter;
  exporter.set_include_raw_hex(has_flag(args, "--raw-hex"));
  exporter.set_verbose(verbose);
  Result r = exporter.export_file(input, output);
  if (!r.ok()) {
    cli_err(std::string("Export failed: ") + r.message);
    return 1;
  }
  std::ostringstream oss;
  oss << "Exported " << exporter.records_exported() << " records to " << output;
  cli_out(oss.str());
  return 0;
}

int Cli::cmd_import(const std::vector<std::string>& args) {
  if (args.size() < 4) {
    print_import_usage();
    return 1;
  }
  std::string input = args[2];
  std::string output = args[3];
  Result vr = input_validation_cli_input_path(input);
  if (!vr.ok()) {
    report_validation_failure(vr);
    return 1;
  }
  vr = input_validation_cli_output_path(output);
  if (!vr.ok()) {
    report_validation_failure(vr);
    return 1;
  }
  bool verbose = has_flag(args, "--verbose");

  TextImporter importer;
  importer.set_strict(!has_flag(args, "--lenient"));
  Result r = importer.import_file(input, output);
  if (!r.ok()) {
    cli_err(std::string("Import failed: ") + r.message);
    return 1;
  }
  if (verbose) {
    std::ostringstream oss;
    oss << "Imported " << importer.records().size() << " records";
    if (importer.parse_errors() > 0) {
      oss << " (" << importer.parse_errors() << " parse warnings)";
    }
    cli_out(oss.str());
  }
  std::ostringstream oss;
  oss << "Import OK: " << importer.records().size() << " records -> " << output;
  cli_out(oss.str());
  return 0;
}

int Cli::cmd_health(const std::vector<std::string>& /*args*/) {
  HealthReport report = build_health_report();
  // Machine-readable JSON via the logging stream (no raw std::cout).
  log().emit_raw(format_health_report_json(report));
  return report.status == "ok" ? 0 : 1;
}

int Cli::run(int argc, char* argv[]) {
  log().set_stream(&std::cout);
  log().configure_from_env();
  metrics().inc_commands();

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
  if (cmd == "health") return cmd_health(args);
  if (cmd == "help" || cmd == "--help" || cmd == "-h") return cmd_help(args);

  cli_err(std::string("Unknown command: ") + cmd);
  print_usage();
  return 1;
}

}  // namespace telltale
