#include <hotaru/result.h>

#include <gtest/gtest.h>

#include <memory>

TEST(BackendResultTest, FailureCarriesAnError)
{
    const hotaru::Status result = hotaru::Failure(
        hotaru::ErrorCode::InvalidArgument, "bad argument");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, hotaru::ErrorCode::InvalidArgument);
    EXPECT_EQ(result.error().message, "bad argument");
}

TEST(BackendResultTest, ResultReturnsTheCreatedValue)
{
    hotaru::Result<std::unique_ptr<int>> value = std::make_unique<int>(42);

    ASSERT_TRUE(value.has_value());
    ASSERT_NE(*value, nullptr);
    EXPECT_EQ(**value, 42);
}
