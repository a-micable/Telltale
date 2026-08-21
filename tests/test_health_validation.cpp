#include "gtest/gtest.h"
#include "telltale/health.hpp"
#include "telltale/validation.hpp"

using telltale::build_health_report;
using telltale::format_health_report_json;
using telltale::validate_event_type_id;
using telltale::validate_input_path;
using telltale::validate_output_path;
using telltale::validate_payload_size;
using telltale::validate_schema_handler_id;

TEST(Validation, RejectsEmptyPathsAndZeroIds) {
  EXPECT_FALSE(validate_output_path("").ok());
  EXPECT_FALSE(validate_input_path("").ok());
  EXPECT_TRUE(validate_output_path("out.tlog").ok());
  EXPECT_TRUE(validate_input_path("in.tlog").ok());
  EXPECT_FALSE(validate_event_type_id(0).ok());
  EXPECT_TRUE(validate_event_type_id(1).ok());
  EXPECT_FALSE(validate_schema_handler_id(0).ok());
  EXPECT_TRUE(validate_payload_size(16).ok());
  return true;
}

TEST(Health, ReportIsOkJson) {
  auto report = build_health_report();
  EXPECT_EQ(report.status, std::string("ok"));
  EXPECT_TRUE(report.logging_ready);
  const std::string json = format_health_report_json(report);
  EXPECT_TRUE(json.find("\"status\":\"ok\"") != std::string::npos);
  EXPECT_TRUE(json.find("\"logging_ready\":true") != std::string::npos);
  EXPECT_TRUE(json.find("logging_framework") != std::string::npos);
  EXPECT_TRUE(json.find("telltale_structured_logger") != std::string::npos);
  return true;
}

TEST(Health, CliHealthSubcommand) {
  std::vector<std::string> storage = {"telltale", "health"};
  std::vector<char*> argv;
  for (auto& s : storage) argv.push_back(s.data());
  EXPECT_TRUE(Cli::run(static_cast<int>(argv.size()), argv.data()) == 0);
  return true;
}

void run_health_validation_tests() {
  RUN_TEST(test_Validation_RejectsEmptyPathsAndZeroIds);
  RUN_TEST(test_Health_ReportIsOkJson);
  RUN_TEST(test_Health_CliHealthSubcommand);
}
