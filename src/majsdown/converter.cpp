#include "converter.hpp"

#include "js_interpreter.hpp"

#include <cassert>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace majsdown {

[[nodiscard]] std::string& get_tl_buffer()
{
    thread_local std::string buffer;
    return buffer;
}

struct converter::impl
{
    [[nodiscard]] explicit impl()
    {}

    ~impl() noexcept
    {}

    [[nodiscard]] bool convert(
        std::string& output_buffer, const std::string_view source) noexcept
    {
        js_interpreter ji;

        std::size_t curr_idx = 0;

        const auto is_done = [&]() -> bool
        { return curr_idx >= source.size(); };

        const auto step_fwd = [&]() -> void { ++curr_idx; };

        const auto get_curr_char = [&]() -> char { return source[curr_idx]; };

        const auto peek = [&](const std::size_t n_steps) -> std::optional<char>
        {
            if (curr_idx + n_steps >= source.size())
            {
                return std::nullopt;
            }

            assert(curr_idx + n_steps < source.size());
            return source[curr_idx + n_steps];
        };

        const auto find_next =
            [&](const char c,
                const std::size_t start_idx) -> std::optional<std::size_t>
        {
            for (std::size_t i = start_idx; i < source.size(); ++i)
            {
                assert(i < source.size());

                if (source[i] == c)
                {
                    return {i};
                }
            }

            return std::nullopt;
        };

        const auto copy_range_to_tl_buffer =
            [&](const std::size_t start_idx, const std::size_t end_idx) -> void
        {
            for (std::size_t i = start_idx; i < end_idx; ++i)
            {
                assert(i < source.size());
                get_tl_buffer().append(1, source[i]);
            }
        };

        while (!is_done())
        {
            const char c = get_curr_char();

            if (c != '@')
            {
                output_buffer.append(1, c);
                step_fwd();

                continue;
            }

            assert(c == '@');

            {
                const std::optional<char> next1 = peek(1);

                if (!next1.has_value() || *next1 != '@')
                {
                    output_buffer.append(1, c);
                    step_fwd();

                    continue;
                }

                assert(*next1 == '@');
            }

            const std::optional<char> next2 = peek(2);

            if (!next2.has_value() || (*next2 != '$' && *next2 != '{'))
            {
                output_buffer.append(1, c);
                step_fwd();

                continue;
            }

            assert(*next2 == '$' || *next2 == '{');

            const std::size_t js_start_idx = curr_idx + 3;
            if (js_start_idx >= source.size())
            {
                std::cerr << "Unterminated '@@" << *next2
                          << "' directive (reached end of source)" << std::endl;

                return false;
            }

            assert(js_start_idx < source.size());

            if (*next2 == '$')
            {
                const std::optional<std::size_t> js_end_idx =
                    find_next('\n', js_start_idx);

                if (!js_end_idx.has_value())
                {
                    std::cerr << "Unterminated '@@" << *next2
                              << "' directive (missing newline)" << std::endl;

                    return false;
                }

                assert(js_end_idx.has_value());

                get_tl_buffer().clear();
                copy_range_to_tl_buffer(js_start_idx, *js_end_idx);

                const std::string_view null_terminated_js = get_tl_buffer();
                ji.interpret_discard(null_terminated_js);

                curr_idx = *js_end_idx + 1;
                continue;
            }

            if (*next2 == '{')
            {
                const std::optional<std::size_t> js_end_idx =
                    find_next('}', js_start_idx); // TODO: balance parens

                if (!js_end_idx.has_value())
                {
                    std::cerr << "Unterminated '@@" << *next2
                              << "' directive (missing newline)" << std::endl;

                    return false;
                }

                assert(js_end_idx.has_value());

                get_tl_buffer().clear();

                get_tl_buffer().append("majsdown_set_output(");
                copy_range_to_tl_buffer(js_start_idx, *js_end_idx);
                get_tl_buffer().append(");");

                const std::string_view null_terminated_js = get_tl_buffer();
                ji.interpret(output_buffer, null_terminated_js);

                curr_idx = *js_end_idx + 1;
                continue;
            }

            std::cerr << "Fatal conversion error" << std::endl;
            return false;
        }

        return true;
    }
};

converter::converter() : _impl{std::make_unique<impl>()}
{}

converter::~converter() = default;

bool converter::convert(
    std::string& output_buffer, const std::string_view source) noexcept
{
    return _impl->convert(output_buffer, source);
}

} // namespace majsdown
