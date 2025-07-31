#include <gtest/gtest.h>

TEST(SampleTest, BasicAssertions)
{
    EXPECT_TRUE(1 + 1 == 2);
    EXPECT_FALSE(2 * 2 == 5);
    EXPECT_EQ(2 + 2, 4);
}