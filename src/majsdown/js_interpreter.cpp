#include "js_interpreter.hpp"

#include <mujs.h>

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

[[nodiscard]] static js_State*& get_tl_js_state() noexcept
{
    thread_local js_State* js_state_ptr{nullptr};
    return js_state_ptr;
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

static void output_to_tl_buffer_pointee(js_State* state)
{
    std::string* const buffer_ptr{get_tl_buffer_ptr()};
    assert(buffer_ptr != nullptr);

    const char* string_arg{js_tostring(state, 1)};
    buffer_ptr->append(string_arg);

    js_pushundefined(state);
}

static void include_file(js_State* state)
{
    thread_local std::string tmp_buffer;

    const char* string_arg{js_tostring(state, 1)};
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

    js_State* const js_state_ptr = get_tl_js_state();
    assert(js_state_ptr != nullptr);

    js_dostring(js_state_ptr, tmp_buffer.data());
    js_pushundefined(state);
}

// ----------------------------------------------------------------------------

struct js_interpreter::impl
{
    js_State* _state;

    [[nodiscard]] explicit impl() noexcept
        : _state{js_newstate(nullptr, nullptr, JS_STRICT)}
    {
        bind_function(&output_to_tl_buffer_pointee, "majsdown_set_output", 1);
        bind_function(&include_file, "majsdown_include", 1);
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
        tl_guard<&get_tl_js_state> js_state_guard{_state};
        tl_guard<&get_tl_buffer_ptr> buffer_ptr_guard{&output_buffer};

        js_dostring(_state, source.data());
    }

    void interpret_discard(const std::string_view source) noexcept
    {
        tl_guard<&get_tl_js_state> js_state_guard{_state};

        js_dostring(_state, source.data());
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
