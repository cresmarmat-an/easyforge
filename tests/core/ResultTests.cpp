#include <easyforge/core.h>
#include <easyforge/core/Testing.h>

#include <memory>
#include <string>

using namespace easyforge;

namespace
{
    Result<int> ParseDigit(char character)
    {
        if (character < '0' || character > '9')
        {
            return Failure(std::format("'{}' is not a digit", character));
        }
        return character - '0';
    }

    Result<> Check(bool good)
    {
        if (!good)
        {
            return Failure("it was not good");
        }
        return {};
    }
}

EASYFORGE_TEST(ResultHoldsAValue)
{
    Result<int> result = ParseDigit('7');
    EASYFORGE_REQUIRE(result);
    EASYFORGE_EXPECT(result.Succeeded());
    EASYFORGE_EXPECT_EQUAL(result.Get(), 7);
    EASYFORGE_EXPECT_EQUAL(*result, 7);
    EASYFORGE_EXPECT(result.Error().empty());
}

EASYFORGE_TEST(ResultHoldsAFailure)
{
    Result<int> result = ParseDigit('x');
    EASYFORGE_EXPECT(!result);
    EASYFORGE_EXPECT_EQUAL(result.Error(), std::string("'x' is not a digit"));
    EASYFORGE_EXPECT_EQUAL(result.GetOr(-1), -1);
}

EASYFORGE_TEST(ResultWithoutValue)
{
    EASYFORGE_EXPECT(Check(true));
    EASYFORGE_EXPECT(Check(true).Error().empty());

    Result<> failed = Check(false);
    EASYFORGE_EXPECT(!failed);
    EASYFORGE_EXPECT_EQUAL(failed.Error(), std::string("it was not good"));
}

EASYFORGE_TEST(ResultMoveOnlyValue)
{
    Result<std::unique_ptr<int>> result = std::make_unique<int>(5);
    EASYFORGE_REQUIRE(result);
    std::unique_ptr<int> taken = std::move(result).Get();
    EASYFORGE_EXPECT_EQUAL(*taken, 5);
}

EASYFORGE_TEST(ResultMemberAccess)
{
    Result<std::string> result = std::string("text");
    EASYFORGE_EXPECT_EQUAL(result->size(), std::size_t { 4 });
}
