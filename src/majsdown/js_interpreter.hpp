#pragma once

#include <memory>
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

    void interpret(const std::string_view source) noexcept;

    [[nodiscard]] std::string_view read_tl_buffer() const noexcept;
};

} // namespace majsdown
