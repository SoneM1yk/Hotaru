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
    InvalidState,
    RuntimeError,
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

[[nodiscard]] inline std::unexpected<Error> Failure(
    Error error)
{
    return std::unexpected(error);
}

#define TRY(EXPR)                                                           \
    ({                                                                      \
        auto&& _res = (EXPR);                                               \
        if (!_res) {                                                        \
            return std::unexpected(std::move(_res.error()));                \
        }                                                                   \
        std::move(*_res);                                                   \
    })

#define TRY_STATUS(EXPR)                                            \
    do {                                                            \
        auto&& _res = (EXPR);                                       \
        if (!_res) {                                                \
            return Failure(std::move(_res.error()));                \
        }                                                           \
    } while(0)

} // namespace hotaru
