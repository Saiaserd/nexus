#include <gtest/gtest.h>

#include "core/NumberParser.h"

using nexus::core::ParseError;
using nexus::core::parsePositiveInt;

TEST(SmokeTest, ParsesValidNumber)
{
    const auto result = parsePositiveInt("42");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST(SmokeTest, RejectsEmpty)
{
    const auto result = parsePositiveInt("");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ParseError::Empty);
}

TEST(SmokeTest, RejectsNonNumeric)
{
    EXPECT_EQ(parsePositiveInt("abc").error(), ParseError::NotANumber);
    EXPECT_EQ(parsePositiveInt("12x").error(), ParseError::NotANumber);
}

TEST(SmokeTest, RejectsZeroNegativeAndOverflow)
{
    EXPECT_EQ(parsePositiveInt("0").error(), ParseError::OutOfRange);
    EXPECT_EQ(parsePositiveInt("-5").error(), ParseError::OutOfRange);
    EXPECT_EQ(parsePositiveInt("99999999999999999999").error(), ParseError::OutOfRange);
}
