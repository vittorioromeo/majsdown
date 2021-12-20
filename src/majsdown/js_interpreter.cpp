#include "js_interpreter.hpp"

#include <mujs.h>

#include <memory>
#include <string_view>
#include <string>

namespace majsdown {

std::string& get_tl_buffer()
{
    thread_local std::string buffer;
    return buffer;
}

void output_to_tl_buffer(js_State* state)
{
    std::string& buffer{get_tl_buffer()};

    const char* string_arg{js_tostring(state, 1)};

    buffer.clear();
    buffer.append(string_arg);

    js_pushundefined(state);
}

struct js_interpreter::impl
{
    js_State* _state;

    [[nodiscard]] explicit impl()
        : _state{js_newstate(nullptr, nullptr, JS_STRICT)}
    {
        bind_function(&output_to_tl_buffer, "majsdown_set_output", 1);
    }

    ~impl()
    {
        js_freestate(_state);
    }

    void bind_function(const js_CFunction function, const std::string_view name,
        const int n_args)
    {
        js_newcfunction(_state, function, name.data(), n_args);
        js_setglobal(_state, name.data());
    }

    void interpret(const std::string_view source)
    {
        js_dostring(_state, source.data());
    }
};

js_interpreter::js_interpreter() : _impl{std::make_unique<impl>()}
{}

js_interpreter::~js_interpreter() = default;

void js_interpreter::interpret(const std::string_view source) noexcept
{
    _impl->interpret(source);
}

std::string_view js_interpreter::read_tl_buffer() const noexcept
{
    return {get_tl_buffer()};
}

} // namespace majsdown
