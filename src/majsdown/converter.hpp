#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace majsdown {

class converter_state;

class converter
{
private:
    std::unique_ptr<converter_state> _state;

public:
    struct config
    {
        bool skip_escaped_symbols = false;
        bool skip_inline_expressions = false;
        bool skip_inline_statements = false;
        bool skip_block_statements = false;
        bool skip_code_block_decorators = false;
    };

    [[nodiscard]] explicit converter();
    ~converter();

    [[nodiscard]] bool convert(const config& cfg, std::string& output_buffer,
        const std::string_view source) noexcept;
};

} // namespace majsdown
