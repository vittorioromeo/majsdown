#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace majsdown {

class js_interpreter
{
private:
    struct impl;

    std::unique_ptr<impl> _impl;

public:
    [[nodiscard]] explicit js_interpreter();
    ~js_interpreter();

    void interpret(
        std::string& output_buffer, const std::string_view source) noexcept;

    void interpret_discard(const std::string_view source) noexcept;
};

} // namespace majsdown
