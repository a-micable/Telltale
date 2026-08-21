#include "gtest/gtest.h"
#include "telltale/network.hpp"
#include "telltale/storage_backends.hpp"

using telltale::NetworkManager;
using telltale::StorageBackendsManager;

TEST(Network, InitializeRequiresEndpoint) {
  NetworkManager mgr;
  EXPECT_FALSE(mgr.initialize("").ok());
  EXPECT_FALSE(mgr.initialized());
  EXPECT_TRUE(mgr.initialize("127.0.0.1:9").ok());
  EXPECT_TRUE(mgr.initialized());
  EXPECT_EQ(mgr.endpoint(), std::string("127.0.0.1:9"));
  EXPECT_TRUE(mgr.shutdown().ok());
  EXPECT_FALSE(mgr.initialized());
  return true;
}

TEST(StorageBackends, InitializeRequiresRoot) {
  StorageBackendsManager mgr;
  EXPECT_FALSE(mgr.initialize("").ok());
  EXPECT_TRUE(mgr.initialize("/tmp/telltale_store").ok());
  EXPECT_TRUE(mgr.initialized());
  EXPECT_TRUE(mgr.shutdown().ok());
  return true;
}

void run_manager_logging_tests() {
  RUN_TEST(test_Network_InitializeRequiresEndpoint);
  RUN_TEST(test_StorageBackends_InitializeRequiresRoot);
}
