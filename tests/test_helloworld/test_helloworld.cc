#include <gtest/gtest.h>

// Function under test
int HelloValue() {
          return 42;
}

// A simple test case
TEST(HelloWorldTest, ReturnsExpectedValue) {
          EXPECT_EQ(HelloValue(), 42);
}