#include "gtest/gtest.h"
#include "telltale/logging.hpp"

using telltale::log;
using telltale::Logger;
using telltale::LogLevel;

TEST(Logging, FormatIsKeyValueStructured) {
  const std::string line = Logger::format(LogLevel::Warn, "network", "hello");
  EXPECT_TRUE(line.find("ts=") != std::string::npos);
  EXPECT_TRUE(line.find("level=WARN") != std::string::npos);
  EXPECT_TRUE(line.find("module=network") != std::string::npos);
  EXPECT_TRUE(line.find("msg=hello") != std::string::npos);
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
  EXPECT_TRUE(out.find("msg=should-appear") != std::string::npos);
  EXPECT_TRUE(out.find("level=WARN") != std::string::npos);
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
  EXPECT_TRUE(oss.str().find("level=ERROR") != std::string::npos);
  EXPECT_TRUE(oss.str().find("module=storage_backends") != std::string::npos);
  EXPECT_TRUE(oss.str().find("msg=boom") != std::string::npos);
  return true;
}

void run_logging_tests() {
  RUN_TEST(test_Logging_FormatIsKeyValueStructured);
  RUN_TEST(test_Logging_LevelFilteringSuppressesInfoWhenWarn);
  RUN_TEST(test_Logging_ErrorAlwaysEmitted);
}
