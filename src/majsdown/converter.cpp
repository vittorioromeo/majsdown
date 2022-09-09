#include "converter.hpp"

#include "js_interpreter.hpp"

#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include <cassert>
#include <cstddef>

namespace majsdown {

class converter_impl
{
private:
    const std::string_view _source;
    std::string _tmp_buffer;
    std::string _js_buffer;
    js_interpreter _js_interpreter;
    std::size_t _curr_idx;
    std::size_t _curr_line;

    void copy_range_to_tmp_buffer(
        const std::size_t start_idx, const std::size_t end_idx)
    {
        assert(end_idx < _source.size());
        assert(start_idx <= end_idx);

        _tmp_buffer.append(_source.data() + start_idx, end_idx - start_idx);
    }

    [[nodiscard]] bool is_done()
    {
        return _curr_idx >= _source.size();
    }

    void step_fwd(const std::size_t n_steps)
    {
        _curr_idx += n_steps;
    }

    [[nodiscard]] char get_curr_char()
    {
        return _source[_curr_idx];
    }

    [[nodiscard]] std::optional<char> peek(const std::size_t n_steps)
    {
        if (_curr_idx + n_steps >= _source.size())
        {
            return std::nullopt;
        }

        assert(_curr_idx + n_steps < _source.size());
        return _source[_curr_idx + n_steps];
    }

    [[nodiscard]] std::optional<std::size_t> find_next(
        const char c, const std::size_t start_idx)
    {
        for (std::size_t i = start_idx; i < _source.size(); ++i)
        {
            assert(i < _source.size());

            if (_source[i] == c)
            {
                return {i};
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> find_js_end_idx(
        const std::size_t real_js_start_idx)
    {
        std::size_t n_braces = 1;

        for (std::size_t i = real_js_start_idx; i < _source.size(); ++i)
        {
            if (_source[i] == '{')
            {
                ++n_braces;
                continue;
            }

            if (_source[i] == '}')
            {
                --n_braces;
                if (n_braces == 0)
                {
                    return i;
                }
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] std::ostream& error_diagnostic()
    {
        return std::cerr << "((MJSD ERROR))(" << _curr_line << "): ";
    }

    void error_diagnostic_unterminated(
        const char disambiguator, const std::string_view reason)
    {
        error_diagnostic() << "Unterminated '@@" << disambiguator
                           << "' directive (" << reason << ")\n";
    }

public:
    [[nodiscard]] explicit converter_impl(const std::string_view source)
        : _source{source}, _curr_idx{0}, _curr_line{0}
    {
        _js_interpreter.set_current_diagnostics_line(_curr_line);
    }

    ~converter_impl() noexcept
    {}

    [[nodiscard]] bool convert(std::string& output_buffer) noexcept
    {
        while (!is_done())
        {
            const char c = get_curr_char();

            if (c != '@')
            {
                if (c == '\n')
                {
                    ++_curr_line;
                    _js_interpreter.set_current_diagnostics_line(_curr_line);
                }

                output_buffer.append(1, c);
                step_fwd(1);

                continue;
            }

            assert(c == '@');

            {
                const std::optional<char> next1 = peek(1);

                if (!next1.has_value() || *next1 != '@')
                {
                    output_buffer.append(1, c);
                    step_fwd(1);

                    continue;
                }

                assert(*next1 == '@');
            }

            const std::optional<char> next2 = peek(2);

            if (!next2.has_value() ||
                (*next2 != '$' && *next2 != '{' && *next2 != '_'))
            {
                output_buffer.append(1, c);
                step_fwd(1);

                continue;
            }

            assert(*next2 == '$' || *next2 == '{' || *next2 == '_');

            const std::size_t js_start_idx = _curr_idx + 3;
            if (js_start_idx >= _source.size())
            {
                error_diagnostic_unterminated(*next2, "reached end of source");
                return false;
            }

            assert(js_start_idx < _source.size());

            //
            // Process `@@$`
            // ----------------------------------------------------------------
            if (*next2 == '$')
            {
                const std::optional<std::size_t> js_end_idx =
                    find_next('\n', js_start_idx);

                if (!js_end_idx.has_value())
                {
                    error_diagnostic_unterminated(*next2, "missing newline");
                    return false;
                }

                assert(js_end_idx.has_value());

                _tmp_buffer.clear();
                copy_range_to_tmp_buffer(js_start_idx, *js_end_idx);

                _js_buffer.append(_tmp_buffer);
                _js_buffer.append(1, '\n');

                _curr_idx = *js_end_idx + 1;
                continue;
            }
            else if (!_js_buffer.empty())
            {
                _js_interpreter.interpret_discard(_js_buffer);
                _js_buffer.clear();
            }

            //
            // Process `@@{`
            // ----------------------------------------------------------------
            if (*next2 == '{')
            {
                const std::optional<std::size_t> js_end_idx =
                    find_js_end_idx(js_start_idx);

                if (!js_end_idx.has_value())
                {
                    error_diagnostic_unterminated(
                        *next2, "missing closing brace");

                    return false;
                }

                assert(js_end_idx.has_value());

                _tmp_buffer.clear();
                _tmp_buffer.append("majsdown_set_output(");
                copy_range_to_tmp_buffer(js_start_idx, *js_end_idx);
                _tmp_buffer.append(");");

                const std::string_view null_terminated_js = _tmp_buffer;
                _js_interpreter.interpret(output_buffer, null_terminated_js);

                _curr_idx = *js_end_idx + 1;
                continue;
            }

            //
            // Process `@@_`
            // ----------------------------------------------------------------
            if (*next2 == '_')
            {
                const std::size_t real_js_start_idx = js_start_idx + 1;

                const std::optional<std::size_t> js_end_idx =
                    find_js_end_idx(real_js_start_idx);

                if (!js_end_idx.has_value())
                {
                    error_diagnostic_unterminated(
                        *next2, "missing closing brace");

                    return false;
                }

                assert(js_end_idx.has_value());

                _curr_idx = *js_end_idx + 2;

                const std::optional<char> backtick0 = peek(0);
                const std::optional<char> backtick1 = peek(1);
                const std::optional<char> backtick2 = peek(2);

                if (backtick0 != '`' || backtick1 != '`' || backtick2 != '`')
                {
                    error_diagnostic_unterminated(
                        *next2, "expected ``` code block");

                    return false;
                }

                step_fwd(3);
                assert(_curr_idx < _source.size());

                const std::size_t lang_start_idx = _curr_idx;

                const auto lang_end_idx = [&]() -> std::optional<std::size_t>
                {
                    for (std::size_t i = lang_start_idx; i < _source.size();
                         ++i)
                    {
                        if (_source[i] == '\n')
                        {
                            return i;
                        }
                    }

                    return std::nullopt;
                }();

                if (!lang_end_idx.has_value())
                {
                    error_diagnostic_unterminated(
                        *next2, "malformed ``` code block");

                    return false;
                }

                assert(lang_end_idx.has_value());

                const std::string_view extracted_lang = _source.substr(
                    lang_start_idx, *lang_end_idx - lang_start_idx);

                const std::size_t code_start_idx = *lang_end_idx + 1;

                const auto code_end_idx = [&]() -> std::optional<std::size_t>
                {
                    for (std::size_t i = code_start_idx; i < _source.size() - 3;
                         ++i)
                    {
                        if (_source[i] == '\n' && _source[i + 1] == '`' &&
                            _source[i + 2] == '`' && _source[i + 3] == '`')
                        {
                            return i;
                        }
                    }

                    return std::nullopt;
                }();

                if (!code_end_idx.has_value())
                {
                    error_diagnostic_unterminated(
                        *next2, "open ``` code block");

                    return false;
                }

                assert(code_end_idx.has_value());

                const std::string_view extracted_code = _source.substr(
                    code_start_idx, *code_end_idx - code_start_idx);

                _tmp_buffer.clear();
                _tmp_buffer.append("(() => { const code = String.raw`");
                _tmp_buffer.append(extracted_code);
                _tmp_buffer.append("`; const lang = `");
                _tmp_buffer.append(extracted_lang);
                _tmp_buffer.append("`; return majsdown_set_output(");
                copy_range_to_tmp_buffer(real_js_start_idx, *js_end_idx);
                _tmp_buffer.append("); })()");

                const std::string_view null_terminated_js = _tmp_buffer;
                _js_interpreter.interpret(output_buffer, null_terminated_js);

                _curr_idx = *code_end_idx + 4;
                continue;
            }

            std::cerr << "((MJSD ERROR))(" << _curr_line
                      << "): Fatal conversion error\n\n";

            return false;
        }

        return true;
    }
};

converter::converter() = default;
converter::~converter() = default;

bool converter::convert(
    std::string& output_buffer, const std::string_view source) noexcept
{
    return converter_impl{source}.convert(output_buffer);
}

} // namespace majsdown
