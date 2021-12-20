#include "converter.hpp"

#include <cassert>
#include <memory>
#include <string>
#include <string_view>

namespace majsdown {

struct converter::impl
{
    [[nodiscard]] explicit impl()
    {}

    ~impl() noexcept
    {}

    [[nodiscard]] bool convert(
        std::string& output_buffer, const std::string_view source) noexcept
    {
        output_buffer.append(source);
        return true;
    }
};

converter::converter() : _impl{std::make_unique<impl>()}
{}

converter::~converter() = default;

bool converter::convert(
    std::string& output_buffer, const std::string_view source) noexcept
{
    return _impl->convert(output_buffer, source);
}

} // namespace majsdown
