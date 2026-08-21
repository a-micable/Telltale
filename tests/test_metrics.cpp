#include <gtest/gtest.h>

#include <ctime>
#include <random>
#include <vector>

#include "telltale/metrics.hpp"

namespace telltale::test {

class MetricsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.enabled = true;
    config_.max_threads = 4;
    config_.verbose_logging = true;
  }

  void TearDown() override {}

  MetricsManager::Config config_;
};

// Basic functionality tests
TEST_F(MetricsTest, InitializeManager) {
  MetricsManager mgr(config_);
  ASSERT_TRUE(mgr.is_initialized());
}

TEST_F(MetricsTest, ProcessEmptyBuffer) {
  MetricsManager mgr(config_);
  size_t result = mgr.process(nullptr, 0);
  EXPECT_EQ(result, 0);
}

TEST_F(MetricsTest, ProcessValidBuffer) {
  MetricsManager mgr(config_);
  std::vector<uint8_t> data = {1, 2, 3, 4, 5};
  size_t result = mgr.process(data.data(), data.size());
  EXPECT_EQ(result, data.size());
}

TEST_F(MetricsTest, AsyncProcessing) {
  MetricsManager mgr(config_);
  std::vector<uint8_t> data = {1, 2, 3};
  bool callback_called = false;

  mgr.process_async(data.data(), data.size(), [&](const auto& result) { callback_called = true; });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_TRUE(callback_called);
}

// Performance tests
TEST_F(MetricsTest, HighThroughputProcessing) {
  MetricsManager mgr(config_);
  std::vector<uint8_t> data(1024 * 1024);
  std::fill(data.begin(), data.end(), 42);

  auto start = std::chrono::high_resolution_clock::now();
  mgr.process(data.data(), data.size());
  auto end = std::chrono::high_resolution_clock::now();

  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  EXPECT_LT(ms, 1000);
}

// Stress tests
TEST_F(MetricsTest, ConcurrentAccess) {
  MetricsManager mgr(config_);
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
TEST_F(MetricsTest, GetStatistics) {
  MetricsManager mgr(config_);
  std::vector<uint8_t> data = {1, 2, 3};
  mgr.process(data.data(), data.size());

  auto stats = mgr.get_statistics();
  EXPECT_GT(stats.total_operations, 0);
  EXPECT_EQ(stats.total_bytes_processed, data.size());
}

TEST_F(MetricsTest, ResetStatistics) {
  MetricsManager mgr(config_);
  std::vector<uint8_t> data = {1, 2, 3};
  mgr.process(data.data(), data.size());

  mgr.reset_statistics();
  auto stats = mgr.get_statistics();
  EXPECT_EQ(stats.total_operations, 0);
}

// Shutdown tests
TEST_F(MetricsTest, GracefulShutdown) {
  MetricsManager mgr(config_);
  mgr.shutdown();
  EXPECT_FALSE(mgr.is_initialized());
}

// Edge cases
TEST_F(MetricsTest, LargeBuffer) {
  MetricsManager mgr(config_);
  std::vector<uint8_t> data(10 * 1024 * 1024);  // 10MB
  size_t result = mgr.process(data.data(), data.size());
  EXPECT_EQ(result, data.size());
}

}  // namespace telltale::test
