#pragma once

#include <memory>
#include <string_view>

namespace majsdown {

class converter
{
private:
    struct impl;

    std::unique_ptr<impl> _impl;

public:
    [[nodiscard]] explicit converter();
    ~converter();

    [[nodiscard]] bool convert(
        std::string& output_buffer, const std::string_view source) noexcept;
};

} // namespace majsdown
