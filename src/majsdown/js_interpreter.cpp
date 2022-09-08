#include "js_interpreter.hpp"

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

[[nodiscard]] static std::ostream& error_diagnostic(
    const char* type, const std::size_t line)
{
    return std::cerr << "((" << type << " ERROR))(" << line << "): ";
}

[[nodiscard]] static bool read_file_in_buffer(
    const std::string_view path, std::string& buffer)
{
    const auto get_diagnostic_line = [&]() -> std::string
    {
        const std::size_t diagnostics_line = get_tl_diagnostics_line();
        return diagnostics_line == 0 ? "?" : std::to_string(diagnostics_line);
    };

    std::ifstream ifs(path.data(), std::ios::binary | std::ios::ate);
    if (!ifs)
    {
        std::cerr << "((IO ERROR))(" << get_diagnostic_line()
                  << "): Failed to open file '" << buffer << "'\n\n";

        return false;
    }

    const auto size = static_cast<std::streamsize>(ifs.tellg());
    ifs.seekg(0, std::ios::beg);

    buffer.clear();
    buffer.resize(size);

    if (!ifs.read(buffer.data(), size))
    {
        std::cerr << "((IO ERROR))(" << get_diagnostic_line()
                  << "): Failed to read file '" << buffer << "'\n\n";

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
    JSRuntime* _runtime;
    JSContext* _context;
    std::size_t _curr_diagnostics_line;

    [[nodiscard]] std::ostream& error_diagnostic(const char* type)
    {
        return ::majsdown::error_diagnostic(type, _curr_diagnostics_line);
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

    [[nodiscard]] JSValue check_js_errors(const JSValue& js_value)
    {
        if (JS_IsException(js_value) || JS_IsError(_context, js_value))
        {
            error_diagnostic("JS")
                << JS_ToCString(_context, JS_GetException(_context)) << "\n\n";

            return JS_UNDEFINED;
        }

        return js_value;
    }

public:
    [[nodiscard]] explicit impl() noexcept
        : _runtime{JS_NewRuntime()},
          _context{JS_NewContext(_runtime)},
          _curr_diagnostics_line{0}
    {
        // js_init_module_std(_context, "std");
        // js_init_module_os(_context, "os");

        bind_function<&output_to_tl_buffer_pointee>("majsdown_set_output", 1);
        bind_function<&include_file>("majsdown_include", 1);
        bind_function<&embed_file>("majsdown_embed", 1);
    }

    ~impl() noexcept
    {
        JS_RunGC(_runtime);

        JS_FreeContext(_context);
        JS_FreeRuntime(_runtime);
    }

    JSValue interpret(
        std::string& output_buffer, const std::string_view source) noexcept
    {
        tl_guard<&get_tl_buffer_ptr> buffer_ptr_guard{&output_buffer};
        return check_js_errors(eval_impl(_context, source));
    }

    JSValue interpret_discard(const std::string_view source) noexcept
    {
        return check_js_errors(eval_impl(_context, source));
    }

    void set_current_diagnostics_line(const std::size_t line) noexcept
    {
        get_tl_diagnostics_line() = _curr_diagnostics_line = line;
    }
};

// ----------------------------------------------------------------------------

js_interpreter::js_interpreter() : _impl{std::make_unique<impl>()}
{}

js_interpreter::~js_interpreter() = default;

void js_interpreter::interpret(
    std::string& output_buffer, const std::string_view source) noexcept
{
    _impl->interpret(output_buffer, source);
}

void js_interpreter::interpret_discard(const std::string_view source) noexcept
{
    _impl->interpret_discard(source);
}

void js_interpreter::set_current_diagnostics_line(
    const std::size_t line) noexcept
{
    _impl->set_current_diagnostics_line(line);
}

} // namespace majsdown
