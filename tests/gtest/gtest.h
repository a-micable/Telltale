// Minimal GoogleTest-compatible shim for Telltale's hand-rolled suite.
// Static analyzers that look for gtest includes / TEST() macros can discover
// the suite; runtime still uses tests/test_common.hpp assertions.
#pragma once

#include <iostream>
#include <string>

#include "test_common.hpp"

#ifndef TEST
// Register a named test function that returns bool (true = pass).
#define TEST(suite, name) static bool test_##suite##_##name()
#endif

#ifndef EXPECT_TRUE
#define EXPECT_TRUE(cond) TEST_ASSERT(cond)
#endif

#ifndef ASSERT_TRUE
#define ASSERT_TRUE(cond) TEST_ASSERT(cond)
#endif

#ifndef EXPECT_EQ
#define EXPECT_EQ(a, b) TEST_ASSERT((a) == (b))
#endif

#ifndef ASSERT_EQ
#define ASSERT_EQ(a, b) TEST_ASSERT((a) == (b))
#endif

#ifndef EXPECT_FALSE
#define EXPECT_FALSE(cond) TEST_ASSERT(!(cond))
#endif
