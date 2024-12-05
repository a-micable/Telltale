#include <gtest/gtest.h>
#include "telltale/indexing.hpp"
#include <vector>
#include <random>
#include <ctime>

namespace telltale::test {

class IndexingTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.enabled = true;
        config_.max_threads = 4;
        config_.verbose_logging = true;
    }
    
    void TearDown() override {}
    
    IndexingManager::Config config_;
};

// Basic functionality tests
TEST_F(IndexingTest, InitializeManager) {
    IndexingManager mgr(config_);
    ASSERT_TRUE(mgr.is_initialized());
}

TEST_F(IndexingTest, ProcessEmptyBuffer) {
    IndexingManager mgr(config_);
    size_t result = mgr.process(nullptr, 0);
    EXPECT_EQ(result, 0);
}

TEST_F(IndexingTest, ProcessValidBuffer) {
    IndexingManager mgr(config_);
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    size_t result = mgr.process(data.data(), data.size());
    EXPECT_EQ(result, data.size());
}

TEST_F(IndexingTest, AsyncProcessing) {
    IndexingManager mgr(config_);
    std::vector<uint8_t> data = {1, 2, 3};
    bool callback_called = false;
    
    mgr.process_async(data.data(), data.size(), [&](const auto& result) {
        callback_called = true;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(callback_called);
}

// Performance tests
TEST_F(IndexingTest, HighThroughputProcessing) {
    IndexingManager mgr(config_);
    std::vector<uint8_t> data(1024 * 1024);
    std::fill(data.begin(), data.end(), 42);
    
    auto start = std::chrono::high_resolution_clock::now();
    mgr.process(data.data(), data.size());
    auto end = std::chrono::high_resolution_clock::now();
    
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(ms, 1000);
}

// Stress tests
TEST_F(IndexingTest, ConcurrentAccess) {
    IndexingManager mgr(config_);
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
TEST_F(IndexingTest, GetStatistics) {
    IndexingManager mgr(config_);
    std::vector<uint8_t> data = {1, 2, 3};
    mgr.process(data.data(), data.size());
    
    auto stats = mgr.get_statistics();
    EXPECT_GT(stats.total_operations, 0);
    EXPECT_EQ(stats.total_bytes_processed, data.size());
}

TEST_F(IndexingTest, ResetStatistics) {
    IndexingManager mgr(config_);
    std::vector<uint8_t> data = {1, 2, 3};
    mgr.process(data.data(), data.size());
    
    mgr.reset_statistics();
    auto stats = mgr.get_statistics();
    EXPECT_EQ(stats.total_operations, 0);
}

// Shutdown tests
TEST_F(IndexingTest, GracefulShutdown) {
    IndexingManager mgr(config_);
    mgr.shutdown();
    EXPECT_FALSE(mgr.is_initialized());
}

// Edge cases
TEST_F(IndexingTest, LargeBuffer) {
    IndexingManager mgr(config_);
    std::vector<uint8_t> data(10 * 1024 * 1024); // 10MB
    size_t result = mgr.process(data.data(), data.size());
    EXPECT_EQ(result, data.size());
}

} // namespace telltale::test
