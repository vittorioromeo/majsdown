#include "js_interpreter.hpp"
#include "majsdown/js_interpreter.hpp"

#include <quickjs-libc.h>
#include <quickjs.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
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

constexpr auto js_runtime_deleter = [](JSRuntime* ptr)
{
    JS_RunGC(ptr);
    JS_FreeRuntime(ptr);
};

constexpr auto js_context_deleter = [](JSContext* ptr) { JS_FreeContext(ptr); };

using js_runtime_uptr =
    std::unique_ptr<JSRuntime, decltype(js_runtime_deleter)>;

using js_context_uptr =
    std::unique_ptr<JSContext, decltype(js_context_deleter)>;

struct raii_js_value
{
    JSContext* _context;
    JSValue _value;

    explicit raii_js_value(JSContext* context, JSValue&& value) noexcept
        : _context{context}, _value{std::move(value)}
    {}

    raii_js_value(const raii_js_value&) = delete;
    raii_js_value& operator=(const raii_js_value&) = delete;

    raii_js_value(raii_js_value&& rhs)
        : _context{std::exchange(rhs._context, nullptr)},
          _value{std::move(rhs._value)}
    {}

    raii_js_value& operator=(raii_js_value&& rhs)
    {
        if (_context != nullptr)
        {
            JS_FreeValue(_context, _value);
        }

        _context = std::exchange(rhs._context, nullptr);
        _value = std::move(rhs._value);

        return *this;
    }

    ~raii_js_value()
    {
        if (_context != nullptr)
        {
            JS_FreeValue(_context, _value);
        }
    }
};

// ----------------------------------------------------------------------------

static raii_js_value eval_impl(
    JSContext* context, const std::string_view source) noexcept
{
    return raii_js_value{context, JS_Eval(context, source.data(), source.size(),
                                      "<evalScript>", JS_EVAL_TYPE_GLOBAL)};
}

static void output_to_tl_buffer_pointee(JSContext* context, JSValueConst* argv)
{
    const raii_js_value str_value{context, JS_ToString(context, argv[0])};
    const char* string_arg{JS_ToCString(context, str_value._value)};

    std::string* const buffer_ptr{get_tl_buffer_ptr()};
    assert(buffer_ptr != nullptr);
    buffer_ptr->append(string_arg);
}

[[nodiscard]] static std::ostream& error_diagnostic_stream(const char* type)
{
    return (*get_tl_err_stream()) << "((" << type << " ERROR)): ";
}

static void set_line_adjustment(JSContext* context, JSValueConst* argv)
{
    std::int32_t out;
    if (JS_ToInt32(context, &out, argv[0]) != 0)
    {
        error_diagnostic_stream("INTERNAL")
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
        ::majsdown::error_diagnostic_stream("IO")
            << "(" << diagnostic_line << ") Failed to open file '" << buffer
            << "'\n\n";

        return false;
    }

    const auto size = static_cast<std::streamsize>(ifs.tellg());
    ifs.seekg(0, std::ios::beg);

    buffer.clear();
    buffer.resize(size);

    if (!ifs.read(buffer.data(), size))
    {
        ::majsdown::error_diagnostic_stream("IO")
            << "(" << diagnostic_line << ") Failed to read file '" << buffer
            << "'\n\n";

        return false;
    }

    return true;
}

static void include_file(JSContext* context, JSValueConst* argv)
{
    const raii_js_value str_value{context, JS_ToString(context, argv[0])};
    const char* string_arg{JS_ToCString(context, str_value._value)};

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
    const raii_js_value str_value{context, JS_ToString(context, argv[0])};
    const char* string_arg{JS_ToCString(context, str_value._value)};

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
    js_runtime_uptr _runtime;
    js_context_uptr _context;
    std::size_t _curr_diagnostics_line;

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
                // TODO: does this need to be freed as well?
                return JS_NewString(context, FPtr(context, argv));
            }
        };

        JSContext* ctx = _context.get();

        const raii_js_value global_obj{ctx, JS_GetGlobalObject(ctx)};

        const JSValue js_func =
            JS_NewCFunction(ctx, +func, name.data(), n_args);

        JS_SetPropertyStr(ctx, global_obj._value, name.data(), js_func);
    }

    [[nodiscard]] std::optional<error> check_js_errors(const JSValue& js_value)
    {
        JSContext* ctx = _context.get();

        if (JS_IsException(js_value) || JS_IsError(ctx, js_value))
        {
            const raii_js_value js_exception{ctx, JS_GetException(ctx)};

            const raii_js_value js_stack_trace{
                ctx, JS_GetPropertyStr(ctx, js_exception._value, "stack")};

            const std::string_view js_stack_trace_sv =
                JS_ToCString(ctx, js_stack_trace._value);

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
                << JS_ToCString(ctx, js_exception._value) << "\n\n"
                << js_stack_trace_sv << "\n\n"
                << "Interpreter line: '" << js_stack_trace_line_num.value_or(1) << "'\n";

            return error{._line = js_stack_trace_line_num.value_or(1)};
        }

        return std::nullopt;
    }

public:
    [[nodiscard]] explicit impl(std::ostream& err_stream) noexcept
        : _err_stream_tl_guard{&err_stream},
          _runtime{JS_NewRuntime()},
          _context{JS_NewContext(_runtime.get())},
          _curr_diagnostics_line{0}
    {
        bind_function<&output_to_tl_buffer_pointee>("__mjsd", 1);
        bind_function<&set_line_adjustment>("__mjsd_line", 1);
        bind_function<&include_file>("majsdown_include", 1);
        bind_function<&embed_file>("majsdown_embed", 1);
    }

    [[nodiscard]] std::optional<error> interpret(
        std::string& output_buffer, const std::string_view source) noexcept
    {
        tl_guard<&get_tl_buffer_ptr> buffer_ptr_guard{&output_buffer};
        return check_js_errors(eval_impl(_context.get(), source)._value);
    }

    [[nodiscard]] std::optional<error> interpret_discard(
        const std::string_view source) noexcept
    {
        return check_js_errors(eval_impl(_context.get(), source)._value);
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

std::optional<js_interpreter::error> js_interpreter::interpret(
    std::string& output_buffer, const std::string_view source) noexcept
{
    return _impl->interpret(output_buffer, source);
}

std::optional<js_interpreter::error> js_interpreter::interpret_discard(
    const std::string_view source) noexcept
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
