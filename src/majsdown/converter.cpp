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

    struct find_result
    {
        std::size_t _idx;
        std::size_t _n_newlines;
    };

    [[nodiscard]] std::optional<find_result> find_js_end_idx(
        const std::size_t real_js_start_idx)
    {
        std::size_t n_newlines = 0;
        std::size_t n_braces = 1;

        for (std::size_t i = real_js_start_idx; i < _source.size(); ++i)
        {
            if (_source[i] == '\n')
            {
                ++n_newlines;
                continue;
            }

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
                    return find_result{._idx = i, ._n_newlines = n_newlines};
                }
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] std::optional<find_result> find_js_end_idx_block_statement(
        const std::size_t real_js_start_idx)
    {
        std::size_t n_newlines = 0;

        for (std::size_t i = real_js_start_idx; i < _source.size() - 2; ++i)
        {
            if (_source[i] == '\n')
            {
                ++n_newlines;
                continue;
            }

            if (_source[i] == '}' && _source[i + 1] == '$')
            {
                return find_result{._idx = i, ._n_newlines = n_newlines};
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] std::ostream& error_diagnostic_stream()
    {
        return std::cerr << "((MJSD ERROR))(" << _curr_line << "): ";
    }

    void error_diagnostic_directive(
        const std::string_view& disambiguator, const std::string_view& reason)
    {
        error_diagnostic_stream() << "Error in '@@" << disambiguator
                                  << "' directive (" << reason << ")\n\n";
    }

    void error_diagnostic_directive(
        const char disambiguator, const std::string_view& reason)
    {
        error_diagnostic_directive(std::string_view{&disambiguator, 1}, reason);
    }

    [[nodiscard]] bool process_inline_statement(const std::size_t js_start_idx)
    {
        const std::optional<std::size_t> js_end_idx =
            find_next('\n', js_start_idx);

        if (!js_end_idx.has_value())
        {
            error_diagnostic_directive('$', "missing newline");
            return false;
        }

        assert(js_end_idx.has_value());

        _tmp_buffer.clear();
        copy_range_to_tmp_buffer(js_start_idx, *js_end_idx);

        _js_buffer.append(_tmp_buffer);
        _js_buffer.append(1, '\n');

        increment_curr_line(1);

        _curr_idx = *js_end_idx + 1 /* newline */;
        return true;
    }

    [[nodiscard]] bool process_inline_expression(
        std::string& output_buffer, const std::size_t js_start_idx)
    {
        const std::optional<find_result> js_end_idx_result =
            find_js_end_idx(js_start_idx);

        if (!js_end_idx_result.has_value())
        {
            error_diagnostic_directive('{', "missing closing brace");
            return false;
        }

        assert(js_end_idx_result.has_value());
        const auto& [js_end_idx, n_newlines] = *js_end_idx_result;
        increment_curr_line(n_newlines);

        if (js_end_idx == js_start_idx)
        {
            error_diagnostic_directive('{', "empty directive");
            return false;
        }

        _tmp_buffer.clear();
        _tmp_buffer.append("__mjsd(");
        copy_range_to_tmp_buffer(js_start_idx, js_end_idx);
        _tmp_buffer.append(");");

        if (!_js_interpreter.interpret(
                output_buffer, _tmp_buffer /* null-terminated JS */))
        {
            return false;
        }

        _curr_idx = js_end_idx + 1 /* newline */;
        return true;
    }

    [[nodiscard]] bool process_block_statement(const std::size_t js_start_idx)
    {
        const std::optional<find_result> js_end_idx_result =
            find_js_end_idx_block_statement(js_start_idx);

        if (!js_end_idx_result.has_value())
        {
            error_diagnostic_directive("${", "missing closing brace");
            return false;
        }

        assert(js_end_idx_result.has_value());
        const auto& [js_end_idx, n_newlines] = *js_end_idx_result;
        increment_curr_line(n_newlines);

        if (js_end_idx == js_start_idx)
        {
            error_diagnostic_directive("${", "empty directive");
            return false;
        }

        _tmp_buffer.clear();
        copy_range_to_tmp_buffer(js_start_idx, js_end_idx);

        _js_buffer.append(_tmp_buffer);
        _js_buffer.append(1, '\n');

        _curr_idx = js_end_idx + 1 /* newline */ + 2 /* }$ */;
        return true;
    }

    [[nodiscard]] bool process_code_block_decorator(
        std::string& output_buffer, const std::size_t js_start_idx)
    {
        const std::size_t real_js_start_idx = js_start_idx + 1 /* { */;

        const std::optional<find_result> js_end_idx_result =
            find_js_end_idx(real_js_start_idx);

        if (!js_end_idx_result.has_value())
        {
            error_diagnostic_directive('_', "missing closing brace");
            return false;
        }

        assert(js_end_idx_result.has_value());
        const auto& [js_end_idx, n_newlines] = *js_end_idx_result;
        increment_curr_line(n_newlines);

        if (_source[js_end_idx + 1] != '_')
        {
            error_diagnostic_directive('_', "missing closing underscore");
            return false;
        }

        _curr_idx = js_end_idx + 2 /* }_ */ + 1 /* newline */;

        const std::size_t n_backticks = [&]
        {
            std::size_t result = 0;

            while (peek(result) == '`')
            {
                ++result;
            }

            return result;
        }();

        if (n_backticks == 0)
        {
            error_diagnostic_directive('_', "expected ``` code block");
            return false;
        }

        step_fwd(n_backticks);
        assert(_curr_idx < _source.size());

        const std::size_t lang_start_idx = _curr_idx;

        const auto lang_end_idx = [&]() -> std::optional<std::size_t>
        {
            for (std::size_t i = lang_start_idx; i < _source.size(); ++i)
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
            error_diagnostic_directive('_', "malformed ``` code block");
            return false;
        }

        assert(lang_end_idx.has_value());

        const std::string_view extracted_lang =
            _source.substr(lang_start_idx, *lang_end_idx - lang_start_idx);

        const std::size_t code_start_idx = *lang_end_idx + 1;

        const auto code_end_idx = [&]() -> std::optional<std::size_t>
        {
            for (std::size_t i = code_start_idx;
                 i < _source.size() - n_backticks; ++i)
            {
                if (_source[i] != '\n')
                {
                    continue;
                }

                std::size_t found = 0;
                for (std::size_t j = 0; j <= n_backticks; ++j)
                {
                    if (_source[i + j] == '`')
                    {
                        ++found;
                    }
                }

                if (found == n_backticks)
                {
                    return i;
                }
            }

            return std::nullopt;
        }();

        if (!code_end_idx.has_value())
        {
            error_diagnostic_directive('_', "open ``` code block");
            return false;
        }

        assert(code_end_idx.has_value());

        const std::string_view extracted_code =
            _source.substr(code_start_idx, *code_end_idx - code_start_idx);

        _tmp_buffer.clear();
        _tmp_buffer.append("(() => { const code = (String.raw`");

        // First, we must escape backticks to embed in a JS template literal
        for (const char c : extracted_code)
        {
            if (c != '`')
            {
                _tmp_buffer.append(1, c);
                continue;
            }

            _tmp_buffer.append("\\`");
        }

        // Then we unescape them after the `raw` call to avoid emitting slashes
        _tmp_buffer.append("`).replace(/\\\\`/g, '`');; const lang = `");
        _tmp_buffer.append(extracted_lang);
        _tmp_buffer.append("`; return __mjsd(");
        copy_range_to_tmp_buffer(real_js_start_idx, js_end_idx);
        _tmp_buffer.append("); })()");

        const std::string_view null_terminated_js = _tmp_buffer;
        if (!_js_interpreter.interpret(output_buffer, null_terminated_js))
        {
            return false;
        }

        _curr_idx = *code_end_idx + n_backticks + 1;
        return true;
    }

    void process_normal_character(std::string& output_buffer, const char c)
    {
        if (c == '\n')
        {
            ++_curr_line;
            _js_interpreter.set_current_diagnostics_line(_curr_line);
        }

        output_buffer.append(1, c);
        step_fwd(1);
    }

    [[nodiscard]] bool is_special_character(const char c)
    {
        return c == '$' || c == '{' || c == '_';
    }

    void increment_curr_line(const std::size_t n)
    {
        _curr_line += n;
        _js_interpreter.set_current_diagnostics_line(_curr_line);
    }

    [[nodiscard]] bool convert_step(std::string& output_buffer)
    {
        const char c = get_curr_char();

        //
        // Consume JS statement buffer if possible
        // ----------------------------------------------------------------
        {
            const std::optional<char> next1 = peek(1);
            const std::optional<char> next2 = peek(2);

            const bool next_is_stmt =
                c == '@' && *next1 == '@' && *next2 == '$';

            if (!next_is_stmt && !_js_buffer.empty())
            {
                if (!_js_interpreter.interpret_discard(_js_buffer))
                {
                    return false;
                }

                _js_buffer.clear();
            }
        }

        //
        // Process normal (non-special) characters
        // ----------------------------------------------------------------
        if (c != '@')
        {
            process_normal_character(output_buffer, c);
            return true;
        }

        assert(c == '@');

        if (const std::optional<char> next1 = peek(1);
            !next1.has_value() || *next1 != '@')
        {
            process_normal_character(output_buffer, c);
            return true;
        }

        const std::optional<char> next2 = peek(2);
        if (!next2.has_value() || !is_special_character(*next2))
        {
            process_normal_character(output_buffer, c);
            return true;
        }

        assert(is_special_character(*next2));

        const std::size_t js_start_idx = _curr_idx + 3;
        if (js_start_idx >= _source.size())
        {
            error_diagnostic_directive(*next2, "reached end of source");
            return false;
        }

        assert(js_start_idx < _source.size());

        //
        // Process `@@$` and `@@${`
        // ----------------------------------------------------------------
        if (*next2 == '$')
        {
            if (const std::optional<char> next3 = peek(3); *next3 == '{')
            {
                if (!process_block_statement(js_start_idx + 1))
                {
                    return false;
                }
            }
            else if (!process_inline_statement(js_start_idx))
            {
                return false;
            }

            return true;
        }

        //
        // Process `@@{`
        // ----------------------------------------------------------------
        if (*next2 == '{')
        {
            if (!process_inline_expression(output_buffer, js_start_idx))
            {
                return false;
            }

            return true;
        }

        //
        // Process `@@_`
        // ----------------------------------------------------------------
        if (*next2 == '_')
        {
            const std::optional<char> next3 = peek(3);
            if (!next3.has_value() || *next3 != '{')
            {
                error_diagnostic_directive('_', "missing '{'");
                return false;
            }

            if (!process_code_block_decorator(output_buffer, js_start_idx))
            {
                return false;
            }

            return true;
        }

        std::cerr << "((MJSD ERROR))(" << _curr_line
                  << "): Fatal conversion error\n\n";

        return false;
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
            if (!convert_step(output_buffer))
            {
                return false;
            }
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
