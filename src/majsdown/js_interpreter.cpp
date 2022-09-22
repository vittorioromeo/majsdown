#include "js_interpreter.hpp"

#include <optional>
#include <quickjs-libc.h>
#include <quickjs.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <cassert>
#include <cstdint>

namespace majsdown {

[[nodiscard]] static std::string*& get_tl_buffer_ptr() noexcept
{
    thread_local std::string* buffer_ptr{nullptr};
    return buffer_ptr;
}

[[nodiscard]] static std::size_t& get_tl_diagnostics_line() noexcept
{
    thread_local std::size_t diagnostics_line{0};
    return diagnostics_line;
}

[[nodiscard]] static std::size_t& get_tl_diagnostics_line_adjustment() noexcept
{
    thread_local std::size_t diagnostics_line_adjustment{0};
    return diagnostics_line_adjustment;
}

[[nodiscard]] static std::ostream*& get_tl_err_stream() noexcept
{
    thread_local std::ostream* err_stream{nullptr};
    return err_stream;
}

// ----------------------------------------------------------------------------

template <auto FPtr>
struct tl_guard
{
    using type = std::remove_reference_t<decltype(FPtr())>;
    const type _prev;

    explicit tl_guard(const type& obj) : _prev{std::exchange(FPtr(), obj)}
    {}

    ~tl_guard()
    {
        FPtr() = std::move(_prev);
    }
};

// ----------------------------------------------------------------------------

static JSValue eval_impl(
    JSContext* context, const std::string_view source) noexcept
{
    return JS_Eval(context, source.data(), source.size(), "<evalScript>",
        JS_EVAL_TYPE_GLOBAL);
}

static void output_to_tl_buffer_pointee(JSContext* context, JSValueConst* argv)
{
    const JSValue str_value{JS_ToString(context, argv[0])};
    const char* string_arg{JS_ToCString(context, str_value)};

    std::string* const buffer_ptr{get_tl_buffer_ptr()};
    assert(buffer_ptr != nullptr);
    buffer_ptr->append(string_arg);
}

[[nodiscard]] static std::ostream& error_diagnostic_stream(
    const char* type, const std::size_t line)
{
    return (*get_tl_err_stream())
           << "((" << type << " ERROR))(" << (line + 1) << "): ";
}

static void set_line_adjustment(JSContext* context, JSValueConst* argv)
{
    std::int32_t out;
    if (JS_ToInt32(context, &out, argv[0]) != 0)
    {
        error_diagnostic_stream("INTERNAL ERROR", 0)
            << "Error in 'set_line_adjustment'\n\n";

        return;
    }

    get_tl_diagnostics_line_adjustment() = out;
}

[[nodiscard]] static bool read_file_in_buffer(
    const std::string_view path, std::string& buffer)
{
    const std::size_t diagnostic_line =
        get_tl_diagnostics_line() + get_tl_diagnostics_line_adjustment();

    std::ifstream ifs(path.data(), std::ios::binary | std::ios::ate);
    if (!ifs)
    {
        ::majsdown::error_diagnostic_stream("IO", diagnostic_line)
            << "Failed to open file '" << buffer << "'\n\n";

        return false;
    }

    const auto size = static_cast<std::streamsize>(ifs.tellg());
    ifs.seekg(0, std::ios::beg);

    buffer.clear();
    buffer.resize(size);

    if (!ifs.read(buffer.data(), size))
    {
        ::majsdown::error_diagnostic_stream("IO", diagnostic_line)
            << "Failed to read file '" << buffer << "'\n\n";

        return false;
    }

    return true;
}

static void include_file(JSContext* context, JSValueConst* argv)
{
    const JSValue str_value = JS_ToString(context, argv[0]);
    const char* string_arg{JS_ToCString(context, str_value)};

    thread_local std::string tmp_buffer;

    tmp_buffer.clear();
    tmp_buffer.append(string_arg);

    if (!read_file_in_buffer(tmp_buffer, tmp_buffer))
    {
        return;
    }

    eval_impl(context, tmp_buffer);
}

[[nodiscard]] static const char* embed_file(
    JSContext* context, JSValueConst* argv)
{
    const JSValue str_value = JS_ToString(context, argv[0]);
    const char* string_arg{JS_ToCString(context, str_value)};

    thread_local std::string tmp_buffer;

    tmp_buffer.clear();
    tmp_buffer.append(string_arg);

    if (!read_file_in_buffer(tmp_buffer, tmp_buffer))
    {
        return "((MJSD ERROR)): Failure reading file to be embedded";
    }

    return tmp_buffer.data();
}

// ----------------------------------------------------------------------------

struct js_interpreter::impl
{
private:
    tl_guard<&get_tl_err_stream> _err_stream_tl_guard;
    JSRuntime* _runtime;
    JSContext* _context;
    std::size_t _curr_diagnostics_line;

    [[nodiscard]] std::ostream& error_diagnostic_stream(const char* type)
    {
        return ::majsdown::error_diagnostic_stream(type,
            _curr_diagnostics_line + get_tl_diagnostics_line_adjustment());
    }

    template <auto FPtr>
    void bind_function(const std::string_view name, const int n_args) noexcept
    {
        auto func = [](JSContext* context, JSValueConst this_val, int argc,
                        JSValueConst* argv) -> JSValue
        {
            (void)this_val;
            (void)argc;

            if constexpr (std::is_void_v<decltype(FPtr(context, argv))>)
            {
                FPtr(context, argv);
                return JS_UNDEFINED;
            }
            else
            {
                return JS_NewString(context, FPtr(context, argv));
            }
        };

        const JSValue global_obj{JS_GetGlobalObject(_context)};

        JS_SetPropertyStr(_context, global_obj, name.data(),
            JS_NewCFunction(_context, +func, name.data(), n_args));

        JS_FreeValue(_context, global_obj);
    }

    [[nodiscard]] bool check_js_errors(const JSValue& js_value)
    {
        if (JS_IsException(js_value) || JS_IsError(_context, js_value))
        {
            const JSValue js_exception = JS_GetException(_context);

            const JSValue js_stack_trace =
                JS_GetPropertyStr(_context, js_exception, "stack");

            const std::string_view js_stack_trace_sv =
                JS_ToCString(_context, js_stack_trace);

            const auto js_stack_trace_line_num =
                [&]() -> std::optional<std::size_t>
            {
                using namespace std::string_view_literals;

                const auto needle = "(<evalScript>:"sv;
                const auto n_begin = js_stack_trace_sv.find(needle);

                if (n_begin == std::string_view::npos)
                {
                    return std::nullopt;
                }

                const auto n_end = js_stack_trace_sv.find(")", n_begin);

                if (n_end == std::string_view::npos)
                {
                    return std::nullopt;
                }

                const auto extr_begin = n_begin + needle.size();
                const auto extr_len = n_end - extr_begin;

                const std::string_view extr =
                    js_stack_trace_sv.substr(extr_begin, extr_len);

                return std::stoi(std::string(extr));
            }();

            // TODO: deal with number extraction for nicer diagnostics
            error_diagnostic_stream("JS")
                << JS_ToCString(_context, js_exception) << "\n\n"
                << js_stack_trace_sv << "\n\n"
                << "NUM: '" << js_stack_trace_line_num.value_or(999) << "'\n";

            JS_FreeValue(
                _context, js_stack_trace); // TODO: RAII, and fix other places,
                                           // grep for JSValue and JS_Eval
            JS_FreeValue(_context, js_exception);

            return false;
        }

        return true;
    }

public:
    [[nodiscard]] explicit impl(std::ostream& err_stream) noexcept
        : _err_stream_tl_guard{&err_stream},
          _runtime{JS_NewRuntime()},
          _context{JS_NewContext(_runtime)},
          _curr_diagnostics_line{0}
    {
        bind_function<&output_to_tl_buffer_pointee>("__mjsd", 1);
        bind_function<&set_line_adjustment>("__mjsd_line", 1);
        bind_function<&include_file>("majsdown_include", 1);
        bind_function<&embed_file>("majsdown_embed", 1);
    }

    ~impl() noexcept
    {
        JS_RunGC(_runtime);

        JS_FreeContext(_context);
        JS_FreeRuntime(_runtime);
    }

    [[nodiscard]] bool interpret(
        std::string& output_buffer, const std::string_view source) noexcept
    {
        tl_guard<&get_tl_buffer_ptr> buffer_ptr_guard{&output_buffer};

        const JSValue result = eval_impl(_context, source);
        const bool rc = check_js_errors(result);
        JS_FreeValue(_context, result);
        return rc;
    }

    [[nodiscard]] bool interpret_discard(const std::string_view source) noexcept
    {
        const JSValue result = eval_impl(_context, source);
        const bool rc = check_js_errors(result);
        JS_FreeValue(_context, result);
        return rc;
    }

    void set_current_diagnostics_line(const std::size_t line) noexcept
    {
        get_tl_diagnostics_line() = _curr_diagnostics_line = line;
    }

    [[nodiscard]] std::size_t get_current_diagnostics_line_adjustment() noexcept
    {
        return get_tl_diagnostics_line_adjustment();
    }
};

// ----------------------------------------------------------------------------

js_interpreter::js_interpreter(std::ostream& err_stream)
    : _impl{std::make_unique<impl>(err_stream)}
{}

js_interpreter::~js_interpreter() = default;

bool js_interpreter::interpret(
    std::string& output_buffer, const std::string_view source) noexcept
{
    return _impl->interpret(output_buffer, source);
}

bool js_interpreter::interpret_discard(const std::string_view source) noexcept
{
    return _impl->interpret_discard(source);
}

void js_interpreter::set_current_diagnostics_line(
    const std::size_t line) noexcept
{
    _impl->set_current_diagnostics_line(line);
}

[[nodiscard]] std::size_t
js_interpreter::get_current_diagnostics_line_adjustment() noexcept
{
    return _impl->get_current_diagnostics_line_adjustment();
}

} // namespace majsdown
