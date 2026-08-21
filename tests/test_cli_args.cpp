#include <cstring>
#include <vector>

#include "gtest/gtest.h"
#include "telltale/cli.hpp"
#include "telltale/errors.hpp"
#include "telltale/input_validation.hpp"

using telltale::Cli;
using telltale::ErrorCode;
using telltale::input_validation_cli_output_path;
using telltale::input_validation_flag_operand;
using telltale::input_validation_parse_type_id;
using telltale::Result;

TEST(CliArgs, ParseArgsCopiesArgcArgv) {
  char a0[] = "telltale";
  char a1[] = "write";
  char a2[] = "out.tlog";
  char* argv[] = {a0, a1, a2};
  auto args = Cli::parse_args(3, argv);
  EXPECT_EQ(args.size(), 3u);
  EXPECT_EQ(args[0], "telltale");
  EXPECT_EQ(args[1], "write");
  EXPECT_EQ(args[2], "out.tlog");
  return true;
}

TEST(CliArgs, GetFlagValueMissingReturnsDefault) {
  std::vector<std::string> args = {"telltale", "write", "out.tlog"};
  EXPECT_EQ(Cli::get_flag_value(args, "--events", "20"), "20");
  return true;
}

TEST(CliArgs, GetFlagValueFindsEvents) {
  std::vector<std::string> args = {"telltale", "write", "out.tlog", "--events", "5"};
  EXPECT_EQ(Cli::get_flag_value(args, "--events", "20"), "5");
  return true;
}

TEST(CliArgs, HasFlag) {
  std::vector<std::string> args = {"telltale", "replay", "in.tlog", "--verbose"};
  EXPECT_TRUE(Cli::has_flag(args, "--verbose"));
  EXPECT_FALSE(Cli::has_flag(args, "--explain"));
  return true;
}

TEST(CliArgs, FlagOperandMissingFieldValue) {
  std::vector<std::string> args = {"telltale", "filter", "in.tlog", "out.tlog", "--field"};
  Result r = input_validation_flag_operand(args, "--field");
  EXPECT_FALSE(r.ok());
  EXPECT_EQ(r.code, ErrorCode::InvalidPayload);
  return true;
}

TEST(CliArgs, ParseTypeIdRejectsZzAcceptsHex) {
  uint16_t out = 0;
  Result bad = input_validation_parse_type_id("zz", out);
  EXPECT_FALSE(bad.ok());
  Result good = input_validation_parse_type_id("0x10", out);
  EXPECT_TRUE(good.ok());
  EXPECT_EQ(out, static_cast<uint16_t>(0x10));
  return true;
}

TEST(CliArgs, CliOutputPathEmptyFails) {
  Result r = input_validation_cli_output_path("");
  EXPECT_FALSE(r.ok());
  return true;
}

void run_cli_args_tests() {
  RUN_TEST(test_CliArgs_ParseArgsCopiesArgcArgv);
  RUN_TEST(test_CliArgs_GetFlagValueMissingReturnsDefault);
  RUN_TEST(test_CliArgs_GetFlagValueFindsEvents);
  RUN_TEST(test_CliArgs_HasFlag);
  RUN_TEST(test_CliArgs_FlagOperandMissingFieldValue);
  RUN_TEST(test_CliArgs_ParseTypeIdRejectsZzAcceptsHex);
  RUN_TEST(test_CliArgs_CliOutputPathEmptyFails);
}
