#include <sys/stat.h>

#include "gtest/gtest.h"

// Confirms the near-duplicate Manager fillers called out by score reports stay gone.
TEST(FillerCleanup, QueryDistributedIndexingSourcesRemoved) {
  struct stat st{};
  EXPECT_TRUE(stat("src/query.cpp", &st) != 0);
  EXPECT_TRUE(stat("src/distributed.cpp", &st) != 0);
  EXPECT_TRUE(stat("src/indexing.cpp", &st) != 0);
  EXPECT_TRUE(stat("src/stream_io.cpp", &st) != 0);
  EXPECT_TRUE(stat("src/compression.cpp", &st) != 0);
  EXPECT_TRUE(stat("src/recovery.cpp", &st) != 0);
  EXPECT_TRUE(stat("src/wal.cpp", &st) != 0);
  return true;
}

void run_filler_cleanup_tests() {
  RUN_TEST(test_FillerCleanup_QueryDistributedIndexingSourcesRemoved);
}
