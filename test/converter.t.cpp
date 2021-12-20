#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <majsdown/converter.hpp>

#include <string_view>

using namespace std::string_view_literals;

TEST_CASE("converter ctor/dtor")
{
    majsdown::converter c;
    (void)c;
}

TEST_CASE("converter convert #0")
{
    majsdown::converter c;

    const std::string_view source = R"(
# hello world

some markdown here... test@mail.com

**bold**
*italic*
)"sv;

    std::string output_buffer;
    const bool ok = c.convert(output_buffer, source);

    REQUIRE(ok);
    REQUIRE(output_buffer == source);
}
