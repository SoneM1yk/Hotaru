#pragma once

#include <string>
#include <functional>
#include <utility>

namespace hotaru {

enum class ResultType {
    Success,
    InvalidArgument,
    Unsupported,
    InitializationFailed,
};

class Result
{
public:
    Result() noexcept = default;
 
    Result(ResultType type, std::string message)
        : type_(type)
        , message_(std::move(message))
    {}

    static Result Ok() noexcept
    {
        return {};
    }

    static Result Failure(ResultType type, std::string message)
    {
        return Result{type, std::move(message)};
    }

    [[nodiscard]] bool IsSuccess() const noexcept
    {
        return type_ == ResultType::Success;
    }

    explicit operator bool() const noexcept
    {
        return IsSuccess();
    }

    [[nodiscard]] ResultType Type() const noexcept
    {
        return type_;
    }

    [[nodiscard]] const std::string& Message() const noexcept
    {
        return message_;
    }

    Result AndThen(std::function<Result()> func) 
    {
        if (IsSuccess() && func != nullptr) {
            return func();
        }
        return *this;
    }

private:
    ResultType type_ = ResultType::Success;
    std::string message_;
};

} // namespace hotaru
