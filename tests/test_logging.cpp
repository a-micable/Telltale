#include "gtest/gtest.h"
#include "telltale/logging.hpp"

using telltale::log;
using telltale::Logger;
using telltale::LogLevel;

TEST(Logging, FormatIncludesLevelModuleAndMessage) {
  const std::string line = Logger::format(LogLevel::Warn, "network", "hello");
  EXPECT_TRUE(line.find("[WARN]") != std::string::npos);
  EXPECT_TRUE(line.find("[network]") != std::string::npos);
  EXPECT_TRUE(line.find("hello") != std::string::npos);
  EXPECT_TRUE(line.find('T') != std::string::npos);
  return true;
}

TEST(Logging, LevelFilteringSuppressesInfoWhenWarn) {
  std::ostringstream oss;
  Logger& lg = log();
  const LogLevel prev = lg.level();
  std::ostream* prev_out = &lg.stream();
  lg.set_stream(&oss);
  lg.set_level(LogLevel::Warn);
  lg.info("mod", "should-not-appear");
  lg.warn("mod", "should-appear");
  lg.set_level(prev);
  lg.set_stream(prev_out);
  const std::string out = oss.str();
  EXPECT_TRUE(out.find("should-not-appear") == std::string::npos);
  EXPECT_TRUE(out.find("should-appear") != std::string::npos);
  EXPECT_TRUE(out.find("[WARN]") != std::string::npos);
  return true;
}

TEST(Logging, ErrorAlwaysEmitted) {
  std::ostringstream oss;
  Logger& lg = log();
  const LogLevel prev = lg.level();
  std::ostream* prev_out = &lg.stream();
  lg.set_stream(&oss);
  lg.set_level(LogLevel::Error);
  lg.error("storage_backends", "boom");
  lg.set_level(prev);
  lg.set_stream(prev_out);
  EXPECT_TRUE(oss.str().find("[ERROR]") != std::string::npos);
  EXPECT_TRUE(oss.str().find("[storage_backends]") != std::string::npos);
  return true;
}

void run_logging_tests() {
  RUN_TEST(test_Logging_FormatIncludesLevelModuleAndMessage);
  RUN_TEST(test_Logging_LevelFilteringSuppressesInfoWhenWarn);
  RUN_TEST(test_Logging_ErrorAlwaysEmitted);
}
