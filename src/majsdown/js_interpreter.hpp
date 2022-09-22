#pragma once

#include <iosfwd>
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
    struct error
    {
        std::size_t _line;
    };

    [[nodiscard]] explicit js_interpreter(std::ostream& err_stream);
    ~js_interpreter();

    [[nodiscard]] std::optional<error> interpret(
        std::string& output_buffer, const std::string_view source) noexcept;

    [[nodiscard]] std::optional<error> interpret_discard(
        const std::string_view source) noexcept;

    void set_current_diagnostics_line(const std::size_t line) noexcept;

    [[nodiscard]] std::size_t
    get_current_diagnostics_line_adjustment() noexcept;
};

} // namespace majsdown
