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

    [[nodiscard]] bool interpret(
        std::string& output_buffer, const std::string_view source) noexcept;

    [[nodiscard]] bool interpret_discard(
        const std::string_view source) noexcept;

    void set_current_diagnostics_line(const std::size_t line) noexcept;
};

} // namespace majsdown
