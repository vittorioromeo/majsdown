#pragma once

#include <string_view>

namespace majsdown {

class converter
{
public:
    [[nodiscard]] explicit converter();
    ~converter();

    [[nodiscard]] bool convert(
        std::string& output_buffer, const std::string_view source) noexcept;
};

} // namespace majsdown
