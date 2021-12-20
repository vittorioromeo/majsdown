#include "js_interpreter.hpp"

#include <mujs.h>

#include <cassert>
#include <memory>
#include <string>
#include <string_view>

namespace majsdown {

[[nodiscard]] std::string*& get_tl_buffer_ptr() noexcept
{
    thread_local std::string* buffer_ptr;
    return buffer_ptr;
}

void output_to_tl_buffer_pointee(js_State* state)
{
    std::string* const buffer_ptr{get_tl_buffer_ptr()};
    assert(buffer_ptr != nullptr);

    const char* string_arg{js_tostring(state, 1)};

    buffer_ptr->append(string_arg);

    js_pushundefined(state);
}

struct js_interpreter::impl
{
    js_State* _state;

    [[nodiscard]] explicit impl() noexcept
        : _state{js_newstate(nullptr, nullptr, JS_STRICT)}
    {
        bind_function(&output_to_tl_buffer_pointee, "majsdown_set_output", 1);
    }

    ~impl() noexcept
    {
        js_freestate(_state);
    }

    void bind_function(const js_CFunction function, const std::string_view name,
        const int n_args) noexcept
    {
        js_newcfunction(_state, function, name.data(), n_args);
        js_setglobal(_state, name.data());
    }

    void interpret(
        std::string& output_buffer, const std::string_view source) noexcept
    {
        get_tl_buffer_ptr() = &output_buffer;
        js_dostring(_state, source.data());
    }

    void interpret_discard(const std::string_view source) noexcept
    {
        js_dostring(_state, source.data());
    }
};

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
