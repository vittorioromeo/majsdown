#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <majsdown/js_interpreter.hpp>

#include <iostream>
#include <sstream>

TEST_CASE("js_interpreter ctor/dtor")
{
    majsdown::js_interpreter ji{std::cerr};
    (void)ji;
}

TEST_CASE("js_interpreter interpret #0")
{
    majsdown::js_interpreter ji{std::cerr};

    std::string output_buffer;
    const bool ok = ji.interpret(output_buffer, R"(
__mjsd('hello world!');
)");

    REQUIRE(ok);
    REQUIRE(output_buffer == "hello world!");
}

TEST_CASE("js_interpreter interpret #1")
{
    majsdown::js_interpreter ji{std::cerr};

    std::string output_buffer;
    const bool ok = ji.interpret(output_buffer, R"(
var i = 10;
var j = 15;
var res = (function(){ return i + j; })();

__mjsd(res);
)");

    REQUIRE(ok);
    REQUIRE(output_buffer == "25");
}

TEST_CASE("js_interpreter interpret #2")
{
    std::ostringstream oss;
    majsdown::js_interpreter ji{oss};

    std::string output_buffer;
    const bool ok = ji.interpret(output_buffer, R"(
var i = 10;
)");

    REQUIRE(ok);
    REQUIRE(output_buffer == "");
    REQUIRE(oss.str() == "");
}

[[nodiscard]] static bool has_line_diagnostic(
    const std::string_view sv, const std::size_t i)
{
    std::ostringstream oss;
    oss << "NUM: '" << i << "'";

    return sv.find(oss.str()) != std::string_view::npos;
}

TEST_CASE("js_interpreter interpret #3")
{
    std::ostringstream oss;
    majsdown::js_interpreter ji{oss};

    std::string output_buffer;
    const bool ok = ji.interpret(output_buffer, R"(
x
)");

    REQUIRE(!ok);
    REQUIRE(output_buffer == "");
    REQUIRE(has_line_diagnostic(oss.str(), 2));
}

TEST_CASE("js_interpreter interpret #4")
{
    std::ostringstream oss;
    majsdown::js_interpreter ji{oss};

    std::string output_buffer;
    const bool ok = ji.interpret(output_buffer, R"(

x
)");

    REQUIRE(!ok);
    REQUIRE(output_buffer == "");
    REQUIRE(has_line_diagnostic(oss.str(), 3));
}

TEST_CASE("js_interpreter interpret #5")
{
    std::ostringstream oss;
    majsdown::js_interpreter ji{oss};

    std::string output_buffer;
    const bool ok = ji.interpret(output_buffer, R"(10;
//
x
)");

    REQUIRE(!ok);
    REQUIRE(output_buffer == "");
    REQUIRE(has_line_diagnostic(oss.str(), 3));
}
