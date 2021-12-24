#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <majsdown/converter.hpp>

#include <fstream>
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

void do_test(const std::string_view source, const std::string_view expected)
{
    std::string output_buffer;
    const bool ok = majsdown::converter{}.convert(output_buffer, source);

    REQUIRE(ok);
    REQUIRE(output_buffer == expected);
}

TEST_CASE("converter convert #1")
{
    const std::string_view source = R"(
@@{"hello world"}
)"sv;

    const std::string_view expected = R"(
hello world
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #2")
{
    const std::string_view source = R"(
@@{10 + 5}
)"sv;

    const std::string_view expected = R"(
15
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #3")
{
    const std::string_view source = R"(
@@$ var i = 10;
@@{i + 5}
)"sv;

    const std::string_view expected = R"(
15
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #4")
{
    const std::string_view source = R"(
hello
@@$ var i = 10;
world
@@{i + 5}
!
)"sv;

    const std::string_view expected = R"(
hello
world
15
!
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #5")
{
    const std::string_view source = R"(
hello @@{1 + 5} world
)"sv;

    const std::string_view expected = R"(
hello 6 world
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #6")
{
    const std::string_view source = R"(
hello @@{"world"}
)"sv;

    const std::string_view expected = R"(
hello world
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #7")
{
    {
        std::ofstream ofs("./i.js");
        REQUIRE(ofs);

        ofs << "var i = 10;";
        ofs.flush();

        REQUIRE(ofs);
    }

    const std::string_view source = R"(
@@$ majsdown_include("./i.js");
@@{i + 5}
)"sv;

    const std::string_view expected = R"(
15
)"sv;

    do_test(source, expected);
}
