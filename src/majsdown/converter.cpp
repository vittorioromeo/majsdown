#include "converter.hpp"

#include "js_interpreter.hpp"
#include "majsdown/js_interpreter.hpp"

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <cassert>
#include <cstddef>

namespace majsdown {

class converter_state
{
public:
    js_interpreter _js_interpreter{std::cerr};
    std::string _tmp_buffer;
    std::string _js_buffer;

    void clear_buffers()
    {
        _tmp_buffer.clear();
        _js_buffer.clear();
    }
};

class converter_pass
{
private:
    converter_state& _state;
    const converter::config _cfg;
    const std::string_view _source;
    std::size_t _curr_idx;
    std::size_t _curr_line;

    [[nodiscard]] js_interpreter& get_js_interpreter() noexcept
    {
        return _state._js_interpreter;
    }

    [[nodiscard]] std::string& get_tmp_buffer() noexcept
    {
        return _state._tmp_buffer;
    }

    [[nodiscard]] std::string& get_js_buffer() noexcept
    {
        return _state._js_buffer;
    }

    [[nodiscard]] std::size_t get_adjusted_curr_line() noexcept
    {
        return _curr_line +
               _state._js_interpreter.get_current_diagnostics_line_adjustment();
    }

    void copy_range_to_tmp_buffer(
        const std::size_t start_idx, const std::size_t end_idx)
    {
        assert(end_idx < _source.size());
        assert(start_idx <= end_idx);

        get_tmp_buffer().append(
            _source.data() + start_idx, end_idx - start_idx);
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
        return std::cerr << "((MJSD ERROR))(" << get_adjusted_curr_line()
                         << "): ";
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

        get_tmp_buffer().clear();
        copy_range_to_tmp_buffer(js_start_idx, *js_end_idx);

        get_js_buffer().append(get_tmp_buffer());
        get_js_buffer().append(1, '\n');

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

        get_tmp_buffer().clear();
        get_tmp_buffer().append("__mjsd(");
        copy_range_to_tmp_buffer(js_start_idx, js_end_idx);
        get_tmp_buffer().append(");");

        if (!get_js_interpreter().interpret(
                output_buffer, get_tmp_buffer() /* null-terminated JS */))
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

        get_tmp_buffer().clear();
        copy_range_to_tmp_buffer(js_start_idx, js_end_idx);

        get_js_buffer().append(get_tmp_buffer());
        get_js_buffer().append(1, '\n');

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

        get_tmp_buffer().clear();
        get_tmp_buffer().append("(() => { const code = (String.raw`");

        // First, we must escape backticks to embed in a JS template literal
        for (const char c : extracted_code)
        {
            if (c != '`')
            {
                get_tmp_buffer().append(1, c);
                continue;
            }

            get_tmp_buffer().append("\\`");
        }

        // Then we unescape them after the `raw` call to avoid emitting slashes
        get_tmp_buffer().append("`).replace(/\\\\`/g, '`');; const lang = `");
        get_tmp_buffer().append(extracted_lang);
        get_tmp_buffer().append("`; return __mjsd(");
        copy_range_to_tmp_buffer(real_js_start_idx, js_end_idx);
        get_tmp_buffer().append("); })()");

        const std::string_view null_terminated_js = get_tmp_buffer();
        if (!get_js_interpreter().interpret(output_buffer, null_terminated_js))
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
            increment_curr_line(1);
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
        get_js_interpreter().set_current_diagnostics_line(_curr_line);
    }

    [[nodiscard]] bool convert_step(std::string& output_buffer)
    {
        const char c = get_curr_char();

        //
        // Consume JS statement buffer if possible
        // ----------------------------------------------------------------
        {
            const bool next_is_stmt =
                c == '@' && peek(1) == '@' && peek(2) == '$';

            if (!next_is_stmt && !get_js_buffer().empty())
            {
                if (!get_js_interpreter().interpret_discard(get_js_buffer()))
                {
                    return false;
                }

                get_js_buffer().clear();
            }
        }

        //
        // Process escaped '@'
        // ----------------------------------------------------------------
        if (c == '\\')
        {
            if (peek(1) == '@')
            {
                if (_cfg.skip_escaped_symbols)
                {
                    process_normal_character(output_buffer, c);
                    process_normal_character(output_buffer, '@');
                    return true;
                }

                process_normal_character(output_buffer, '@');
                step_fwd(1);
            }
            else
            {
                process_normal_character(output_buffer, c);
            }

            return true;
        }

        //
        // Process normal (non-special) characters
        // ----------------------------------------------------------------
        if (c != '@' || peek(1) != '@')
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

        assert(next2.has_value() && is_special_character(*next2));

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
            if (peek(3) == '{')
            {
                if (_cfg.skip_block_statements)
                {
                    process_normal_character(output_buffer, c);
                    return true;
                }

                return process_block_statement(js_start_idx + 1);
            }
            else
            {
                if (_cfg.skip_inline_statements)
                {
                    process_normal_character(output_buffer, c);
                    return true;
                }

                return process_inline_statement(js_start_idx);
            }
        }

        //
        // Process `@@{`
        // ----------------------------------------------------------------
        if (*next2 == '{')
        {
            if (_cfg.skip_inline_expressions)
            {
                process_normal_character(output_buffer, c);
                return true;
            }

            return process_inline_expression(output_buffer, js_start_idx);
        }

        //
        // Process `@@_`
        // ----------------------------------------------------------------
        if (*next2 == '_')
        {
            if (_cfg.skip_code_block_decorators)
            {
                process_normal_character(output_buffer, c);
                return true;
            }

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

        std::cerr << "((MJSD ERROR))(" << get_adjusted_curr_line()
                  << "): Fatal conversion error\n\n";

        return false;
    }

public:
    [[nodiscard]] explicit converter_pass(converter_state& state,
        const converter::config& cfg, const std::string_view source)
        : _state{state}, _cfg{cfg}, _source{source}, _curr_idx{0}, _curr_line{0}
    {
        get_js_interpreter().set_current_diagnostics_line(_curr_line);
    }

    ~converter_pass() = default;

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

converter::converter() : _state{std::make_unique<converter_state>()}
{}

converter::~converter() = default;

bool converter::convert(const config& cfg, std::string& output_buffer,
    const std::string_view source) noexcept
{
    _state->clear_buffers();
    return converter_pass{*_state, cfg, source}.convert(output_buffer);
}

} // namespace majsdown
