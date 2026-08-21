#pragma once

#include <string>
#include <vector>

#include "telltale/errors.hpp"

namespace telltale {

class Cli {
 public:
  static int run(int argc, char* argv[]);

 private:
  static int cmd_write(const std::vector<std::string>& args);
  static int cmd_replay(const std::vector<std::string>& args);
  static int cmd_verify(const std::vector<std::string>& args);
  static int cmd_filter(const std::vector<std::string>& args);
  static int cmd_diff(const std::vector<std::string>& args);
  static int cmd_compact(const std::vector<std::string>& args);
  static int cmd_export(const std::vector<std::string>& args);
  static int cmd_import(const std::vector<std::string>& args);
  static int cmd_help(const std::vector<std::string>& args);

  static void print_usage();
  static void print_write_usage();
  static void print_replay_usage();
  static void print_verify_usage();
  static void print_filter_usage();
  static void print_diff_usage();
  static void print_compact_usage();
  static void print_export_usage();
  static void print_import_usage();

  static std::vector<std::string> parse_args(int argc, char* argv[]);
  static bool has_flag(const std::vector<std::string>& args, const std::string& flag);
  static std::string get_flag_value(const std::vector<std::string>& args, const std::string& flag,
                                    const std::string& default_val);
  static std::vector<std::string> collect_positional(const std::vector<std::string>& args,
                                                     size_t start_index);
};

int generate_sample_log(const std::string& output_path, size_t event_count, bool verbose);

}  // namespace telltale
