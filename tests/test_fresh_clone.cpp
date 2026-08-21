#include <sys/stat.h>

#include <fstream>
#include <iterator>

#include "gtest/gtest.h"

TEST(FreshClone, RunTestsScriptExistsAndIsExecutable) {
  struct stat st{};
  EXPECT_TRUE(stat("scripts/run_tests.sh", &st) == 0);
  EXPECT_TRUE((st.st_mode & S_IXUSR) != 0);
  std::ifstream in("scripts/run_tests.sh");
  EXPECT_TRUE(in.good());
  std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  EXPECT_TRUE(content.find("make test") != std::string::npos);
  EXPECT_TRUE(content.find("set -euo pipefail") != std::string::npos);
  return true;
}

TEST(FreshClone, MakefileDeclaresTestTarget) {
  std::ifstream in("Makefile");
  EXPECT_TRUE(in.good());
  std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  EXPECT_TRUE(content.find("test:") != std::string::npos);
  EXPECT_TRUE(content.find("build/test_telltale") != std::string::npos);
  return true;
}

void run_fresh_clone_tests() {
  RUN_TEST(test_FreshClone_RunTestsScriptExistsAndIsExecutable);
  RUN_TEST(test_FreshClone_MakefileDeclaresTestTarget);
}
