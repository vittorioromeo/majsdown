#pragma once

#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>

namespace majsdown {

class converter
{
private:
    class state;
    class pass;

    std::unique_ptr<state> _state;

public:
    struct config
    {
        bool skip_escaped_symbols = false;
        bool skip_inline_expressions = false;
        bool skip_inline_statements = false;
        bool skip_block_statements = false;
        bool skip_code_block_decorators = false;
    };

    [[nodiscard]] explicit converter(std::ostream& err_stream);
    ~converter();

    [[nodiscard]] bool convert(const config& cfg, std::string& output_buffer,
        const std::string_view source) noexcept;
};

} // namespace majsdown
