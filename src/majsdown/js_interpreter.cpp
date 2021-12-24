#include "js_interpreter.hpp"

#include <quickjs-libc.h>
#include <quickjs.h>

#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace majsdown {

[[nodiscard]] static std::string*& get_tl_buffer_ptr() noexcept
{
    thread_local std::string* buffer_ptr{nullptr};
    return buffer_ptr;
}

[[nodiscard]] static JSContext*& get_tl_js_context() noexcept
{
    thread_local JSContext* js_context_ptr{nullptr};
    return js_context_ptr;
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

static void output_to_tl_buffer_pointee(JSContext* context, JSValueConst* argv)
{
    const JSValue strValue = JS_ToString(context, argv[0]);
    const char* string_arg{JS_ToCString(context, strValue)};

    std::string* const buffer_ptr{get_tl_buffer_ptr()};
    assert(buffer_ptr != nullptr);
    buffer_ptr->append(string_arg);
}

static void include_file(JSContext* context, JSValueConst* argv)
{
    const JSValue strValue = JS_ToString(context, argv[0]);
    const char* string_arg{JS_ToCString(context, strValue)};

    thread_local std::string tmp_buffer;

    tmp_buffer.clear();
    tmp_buffer.append(string_arg);

    std::ifstream ifs(tmp_buffer, std::ios::binary | std::ios::ate);
    if (!ifs)
    {
        std::cerr << "Failed to open file '" << tmp_buffer << "'\n";
        return;
    }

    const auto size = static_cast<std::streamsize>(ifs.tellg());
    ifs.seekg(0, std::ios::beg);

    tmp_buffer.clear();
    tmp_buffer.resize(size);

    if (!ifs.read(tmp_buffer.data(), size))
    {
        std::cerr << "Failed to read file '" << tmp_buffer << "'\n";
        return;
    }

    JSContext* const js_context_ptr = get_tl_js_context();
    assert(js_context_ptr != nullptr);

    JS_Eval(context, tmp_buffer.data(), tmp_buffer.size(), "<evalScript>",
        JS_EVAL_TYPE_GLOBAL);
}

// ----------------------------------------------------------------------------

struct js_interpreter::impl
{
    JSRuntime* _runtime;
    JSContext* _context;

    [[nodiscard]] explicit impl() noexcept
        : _runtime{JS_NewRuntime()}, _context{JS_NewContext(_runtime)}
    {
        // js_init_module_std(_context, "std");
        // js_init_module_os(_context, "os");

        bind_function<&output_to_tl_buffer_pointee>("majsdown_set_output", 1);
        bind_function<&include_file>("majsdown_include", 1);
    }

    ~impl() noexcept
    {
        JS_RunGC(_runtime);

        JS_FreeContext(_context);
        JS_FreeRuntime(_runtime);
    }

    template <auto FPtr>
    void bind_function(const std::string_view name, const int n_args) noexcept
    {
        auto func = [](JSContext* context, JSValueConst this_val, int argc,
                        JSValueConst* argv) -> JSValue
        {
            (void)this_val;
            (void)argc;

            FPtr(context, argv);
            return JS_UNDEFINED;
        };

        JSValue global_obj = JS_GetGlobalObject(_context);

        JS_SetPropertyStr(_context, global_obj, name.data(),
            JS_NewCFunction(_context, +func, name.data(), n_args));

        JS_FreeValue(_context, global_obj);
    }

    void interpret(
        std::string& output_buffer, const std::string_view source) noexcept
    {
        tl_guard<&get_tl_js_context> js_context_guard{_context};
        tl_guard<&get_tl_buffer_ptr> buffer_ptr_guard{&output_buffer};

        JS_Eval(_context, source.data(), source.size(), "<evalScript>",
            JS_EVAL_TYPE_GLOBAL);
    }

    void interpret_discard(const std::string_view source) noexcept
    {
        tl_guard<&get_tl_js_context> js_context_guard{_context};

        JS_Eval(_context, source.data(), source.size(), "<evalScript>",
            JS_EVAL_TYPE_GLOBAL);
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

} // namespace majsdown
