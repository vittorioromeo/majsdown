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

struct converter::impl
{
    std::string _tmp_buffer;

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
            assert(end_idx < source.size());
            assert(start_idx <= end_idx);

            _tmp_buffer.append(source.data() + start_idx, end_idx - start_idx);
        };

        const auto find_js_end_idx = [&](const std::size_t real_js_start_idx)
            -> std::optional<std::size_t>
        {
            std::size_t n_braces = 1;

            for (std::size_t i = real_js_start_idx; i < source.size(); ++i)
            {
                if (source[i] == '{')
                {
                    ++n_braces;
                    continue;
                }

                if (source[i] == '}')
                {
                    --n_braces;
                    if (n_braces == 0)
                    {
                        return i;
                    }
                }
            }

            return std::nullopt;
        };

        std::string js_buffer;

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

            if (!next2.has_value() ||
                (*next2 != '$' && *next2 != '{' && *next2 != '_'))
            {
                output_buffer.append(1, c);
                step_fwd();

                continue;
            }

            assert(*next2 == '$' || *next2 == '{' || *next2 == '_');

            const std::size_t js_start_idx = curr_idx + 3;
            if (js_start_idx >= source.size())
            {
                std::cerr << "Unterminated '@@" << *next2
                          << "' directive (reached end of source)\n";

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
                              << "' directive (missing newline)\n";

                    return false;
                }

                assert(js_end_idx.has_value());

                _tmp_buffer.clear();
                copy_range_to_tl_buffer(js_start_idx, *js_end_idx);

                js_buffer.append(_tmp_buffer);
                // const std::string_view null_terminated_js = _tmp_buffer;
                // ji.interpret_discard(null_terminated_js);

                curr_idx = *js_end_idx + 1;
                continue;
            }
            else if (!js_buffer.empty())
            {
                ji.interpret_discard(js_buffer);
                js_buffer.clear();
            }

            if (*next2 == '{')
            {
                const std::optional<std::size_t> js_end_idx =
                    find_js_end_idx(js_start_idx);

                if (!js_end_idx.has_value())
                {
                    std::cerr << "Unterminated '@@" << *next2
                              << "' directive (missing closing brace)\n";

                    return false;
                }

                assert(js_end_idx.has_value());

                _tmp_buffer.clear();
                _tmp_buffer.append("majsdown_set_output(");
                copy_range_to_tl_buffer(js_start_idx, *js_end_idx);
                _tmp_buffer.append(");");

                const std::string_view null_terminated_js = _tmp_buffer;
                ji.interpret(output_buffer, null_terminated_js);

                curr_idx = *js_end_idx + 1;
                continue;
            }

            if (*next2 == '_')
            {
                const std::size_t real_js_start_idx = js_start_idx + 1;

                const std::optional<std::size_t> js_end_idx =
                    find_js_end_idx(real_js_start_idx);

                if (!js_end_idx.has_value())
                {
                    std::cerr << "Unterminated '@@" << *next2
                              << "' directive (missing closing brace)\n";

                    return false;
                }

                assert(js_end_idx.has_value());

                curr_idx = *js_end_idx + 2;

                const std::optional<char> backtick0 = peek(0);
                const std::optional<char> backtick1 = peek(1);
                const std::optional<char> backtick2 = peek(2);

                if (backtick0 != '`' || backtick1 != '`' || backtick2 != '`')
                {
                    std::cerr << "Unterminated '@@" << *next2
                              << "' directive (expected ``` code block)\n";

                    return false;
                }

                step_fwd();
                step_fwd();
                step_fwd();
                assert(curr_idx < source.size());

                const std::size_t lang_start_idx = curr_idx;

                const auto lang_end_idx = [&]() -> std::optional<std::size_t>
                {
                    for (std::size_t i = lang_start_idx; i < source.size(); ++i)
                    {
                        if (source[i] == '\n')
                        {
                            return i;
                        }
                    }

                    return std::nullopt;
                }();

                if (!lang_end_idx.has_value())
                {
                    std::cerr << "Unterminated '@@" << *next2
                              << "' directive (malformed ``` code block)\n";

                    return false;
                }

                assert(lang_end_idx.has_value());

                const std::string_view extracted_lang = source.substr(
                    lang_start_idx, *lang_end_idx - lang_start_idx);

                const std::size_t code_start_idx = *lang_end_idx + 1;

                const auto code_end_idx = [&]() -> std::optional<std::size_t>
                {
                    for (std::size_t i = code_start_idx; i < source.size() - 3;
                         ++i)
                    {
                        if (source[i] == '\n' && source[i + 1] == '`' &&
                            source[i + 2] == '`' && source[i + 3] == '`')
                        {
                            return i;
                        }
                    }

                    return std::nullopt;
                }();

                if (!code_end_idx.has_value())
                {
                    std::cerr << "Unterminated '@@" << *next2
                              << "' directive (open ``` code block)\n";

                    return false;
                }

                assert(code_end_idx.has_value());

                const std::string_view extracted_code = source.substr(
                    code_start_idx, *code_end_idx - code_start_idx);

                _tmp_buffer.clear();
                _tmp_buffer.append("(() => { ");
                _tmp_buffer.append(" const code = String.raw`");
                _tmp_buffer.append(extracted_code);
                _tmp_buffer.append("`; const lang = `");
                _tmp_buffer.append(extracted_lang);
                _tmp_buffer.append("`; return majsdown_set_output(");
                copy_range_to_tl_buffer(real_js_start_idx, *js_end_idx);
                _tmp_buffer.append("); })()");

                const std::string_view null_terminated_js = _tmp_buffer;
                ji.interpret(output_buffer, null_terminated_js);

                curr_idx = *code_end_idx + 4;
                continue;
            }

            std::cerr << "Fatal conversion error\n";
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
