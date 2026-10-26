#pragma once

#include <expected>
#include <string>
#include <utility>

namespace hotaru {

enum class ErrorCode
{
    InvalidArgument,
    Unsupported,
    InitializationFailed,
    NotReady,
};

struct Error
{
    ErrorCode code = ErrorCode::NotReady;
    std::string message;
};

template <typename T>
using Result = std::expected<T, Error>;

using Status = Result<void>;

[[nodiscard]] inline std::unexpected<Error> Failure(
    ErrorCode code, std::string message)
{
    return std::unexpected(Error{code, std::move(message)});
}

} // namespace hotaru
