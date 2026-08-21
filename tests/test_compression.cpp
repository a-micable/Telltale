#include <gtest/gtest.h>

#include <ctime>
#include <random>
#include <vector>

#include "telltale/compression.hpp"

namespace telltale::test {

class CompressionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.enabled = true;
    config_.max_threads = 4;
    config_.verbose_logging = true;
  }

  void TearDown() override {}

  CompressionManager::Config config_;
};

// Basic functionality tests
TEST_F(CompressionTest, InitializeManager) {
  CompressionManager mgr(config_);
  ASSERT_TRUE(mgr.is_initialized());
}

TEST_F(CompressionTest, ProcessEmptyBuffer) {
  CompressionManager mgr(config_);
  size_t result = mgr.process(nullptr, 0);
  EXPECT_EQ(result, 0);
}

TEST_F(CompressionTest, ProcessValidBuffer) {
  CompressionManager mgr(config_);
  std::vector<uint8_t> data = {1, 2, 3, 4, 5};
  size_t result = mgr.process(data.data(), data.size());
  EXPECT_EQ(result, data.size());
}

TEST_F(CompressionTest, AsyncProcessing) {
  CompressionManager mgr(config_);
  std::vector<uint8_t> data = {1, 2, 3};
  bool callback_called = false;

  mgr.process_async(data.data(), data.size(), [&](const auto& result) { callback_called = true; });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_TRUE(callback_called);
}

// Performance tests
TEST_F(CompressionTest, HighThroughputProcessing) {
  CompressionManager mgr(config_);
  std::vector<uint8_t> data(1024 * 1024);
  std::fill(data.begin(), data.end(), 42);

  auto start = std::chrono::high_resolution_clock::now();
  mgr.process(data.data(), data.size());
  auto end = std::chrono::high_resolution_clock::now();

  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  EXPECT_LT(ms, 1000);
}

// Stress tests
TEST_F(CompressionTest, ConcurrentAccess) {
  CompressionManager mgr(config_);
  std::vector<std::thread> threads;

  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&mgr]() {
      std::vector<uint8_t> data(1000);
      std::fill(data.begin(), data.end(), i);
      mgr.process(data.data(), data.size());
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  auto stats = mgr.get_statistics();
  EXPECT_GT(stats.total_operations, 0);
}

// Statistics tests
TEST_F(CompressionTest, GetStatistics) {
  CompressionManager mgr(config_);
  std::vector<uint8_t> data = {1, 2, 3};
  mgr.process(data.data(), data.size());

  auto stats = mgr.get_statistics();
  EXPECT_GT(stats.total_operations, 0);
  EXPECT_EQ(stats.total_bytes_processed, data.size());
}

TEST_F(CompressionTest, ResetStatistics) {
  CompressionManager mgr(config_);
  std::vector<uint8_t> data = {1, 2, 3};
  mgr.process(data.data(), data.size());

  mgr.reset_statistics();
  auto stats = mgr.get_statistics();
  EXPECT_EQ(stats.total_operations, 0);
}

// Shutdown tests
TEST_F(CompressionTest, GracefulShutdown) {
  CompressionManager mgr(config_);
  mgr.shutdown();
  EXPECT_FALSE(mgr.is_initialized());
}

// Edge cases
TEST_F(CompressionTest, LargeBuffer) {
  CompressionManager mgr(config_);
  std::vector<uint8_t> data(10 * 1024 * 1024);  // 10MB
  size_t result = mgr.process(data.data(), data.size());
  EXPECT_EQ(result, data.size());
}

}  // namespace telltale::test
