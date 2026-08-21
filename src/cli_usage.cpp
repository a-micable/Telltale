#include "telltale/cli.hpp"
#include "telltale/logging.hpp"

namespace telltale {

namespace {

void cli_out(const std::string& message) { log().info("cli", message); }

}  // namespace

void Cli::print_banner() { cli_out("Telltale Event Log Tool v1.0"); }

void Cli::print_usage() {
  cli_out("Usage: telltale <command> [options]");
  cli_out("");
  cli_out("Commands:");
  cli_out("  write   Generate a sample event log file");
  cli_out("  replay  Replay an event log file with handler execution");
  cli_out("  verify  Verify CRC32 and schema without executing handlers");
  cli_out("  filter  Filter records from a log into a new log file");
  cli_out("  diff    Compare two log files and print a diff report");
  cli_out("  compact Merge multiple log files into one output file");
  cli_out("  export  Export a binary log to plain-text format");
  cli_out("  import  Import a plain-text log back to binary format");
  cli_out("  health  Print JSON health status for automation");
  cli_out("  help    Show this help message");
}

void Cli::print_write_usage() {
  cli_out("Usage: telltale write <output-file> [options]");
  cli_out("Options:");
  cli_out("  --events N    Number of events to generate (default: 20)");
  cli_out("  --verbose     Enable verbose output");
}

void Cli::print_replay_usage() {
  cli_out("Usage: telltale replay <input-file> [options]");
  cli_out("Options:");
  cli_out("  --verbose     Enable verbose handler output");
}

void Cli::print_verify_usage() {
  cli_out("Usage: telltale verify <input-file> [options]");
  cli_out("Options:");
  cli_out("  --verbose     Enable verbose verification output");
}

void Cli::print_filter_usage() {
  cli_out("Usage: telltale filter <input-file> <output-file> [options]");
  cli_out("Options:");
  cli_out("  --type ID         Match exact type ID (hex or decimal)");
  cli_out("  --type-min ID     Minimum type ID (inclusive)");
  cli_out("  --type-max ID     Maximum type ID (inclusive)");
  cli_out("  --time-min MS     Minimum timestamp (epoch millis)");
  cli_out("  --time-max MS     Maximum timestamp (epoch millis)");
  cli_out("  --field NAME OP VALUE   Payload field filter (op: eq,ne,gt,lt,contains)");
  cli_out("  --combine and|or  Combine field filters (default: and)");
  cli_out("  --no-schema       Exclude schema update events");
  cli_out("  --explain         Print filter statistics");
  cli_out("  --verbose         Verbose output");
}

void Cli::print_diff_usage() {
  cli_out("Usage: telltale diff <left-file> <right-file> [options]");
  cli_out("Options:");
  cli_out("  --verbose     Show equal records in addition to changes");
}

void Cli::print_compact_usage() {
  cli_out("Usage: telltale compact <output-file> <input-file> [more-inputs...] [options]");
  cli_out("Options:");
  cli_out("  --no-dedupe-reset   Keep consecutive duplicate reset events");
  cli_out("  --verify-output     Verify output file after compaction");
  cli_out("  --verbose           Verbose output");
}

void Cli::print_export_usage() {
  cli_out("Usage: telltale export <input-file> <output-file> [options]");
  cli_out("Options:");
  cli_out("  --raw-hex     Include raw hex payload for unknown types");
  cli_out("  --verbose     Verbose output");
}

void Cli::print_import_usage() {
  cli_out("Usage: telltale import <input-text-file> <output-binary-file> [options]");
  cli_out("Options:");
  cli_out("  --lenient     Continue on parse errors where possible");
  cli_out("  --verbose     Verbose output");
}

}  // namespace telltale
