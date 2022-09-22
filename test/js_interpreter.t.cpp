#include <string_view>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <majsdown/js_interpreter.hpp>

#include <iostream>
#include <sstream>

[[nodiscard]] static bool is_ok(
    const std::optional<majsdown::js_interpreter::error>& opt)
{
    return !opt.has_value();
}

TEST_CASE("js_interpreter ctor/dtor")
{
    majsdown::js_interpreter ji{std::cerr};
    (void)ji;
}

TEST_CASE("js_interpreter interpret #0")
{
    majsdown::js_interpreter ji{std::cerr};

    std::string output_buffer;
    const bool ok = is_ok(ji.interpret(output_buffer, R"(
__mjsd('hello world!');
)"));

    REQUIRE(ok);
    REQUIRE(output_buffer == "hello world!");
}

TEST_CASE("js_interpreter interpret #1")
{
    majsdown::js_interpreter ji{std::cerr};

    std::string output_buffer;
    const bool ok = is_ok(ji.interpret(output_buffer, R"(
var i = 10;
var j = 15;
var res = (function(){ return i + j; })();

__mjsd(res);
)"));

    REQUIRE(ok);
    REQUIRE(output_buffer == "25");
}

TEST_CASE("js_interpreter interpret #2")
{
    std::ostringstream oss;
    majsdown::js_interpreter ji{oss};

    std::string output_buffer;
    const bool ok = is_ok(ji.interpret(output_buffer, R"(
var i = 10;
)"));

    REQUIRE(ok);
    REQUIRE(output_buffer == "");
    REQUIRE(oss.str() == "");
}

[[nodiscard]] static bool diagnostic_contains(
    const std::string_view sv, const std::string_view needle)
{
    if (sv.find(needle) == std::string_view::npos)
    {
        std::cerr << "OUTPUT:\n" << sv << '\n';
        return false;
    }

    return true;
}

[[nodiscard]] static bool has_line_diagnostic(
    const std::string_view sv, const std::size_t i)
{
    thread_local std::ostringstream oss;
    oss << "NUM: '" << i << "'";

    return diagnostic_contains(sv, oss.str());
}

[[nodiscard]] static bool has_line_diagnostic(
    const std::ostringstream& oss, const std::size_t i)
{
    return has_line_diagnostic(oss.str(), i);
}

[[nodiscard]] static bool diagnostic_contains(
    const std::ostringstream& oss, const std::string_view needle)
{
    return diagnostic_contains(oss.str(), needle);
}

TEST_CASE("js_interpreter interpret #3")
{
    std::ostringstream oss;
    majsdown::js_interpreter ji{oss};

    std::string output_buffer;
    const bool ok = is_ok(ji.interpret(output_buffer, R"(
x
)"));

    REQUIRE(!ok);
    REQUIRE(output_buffer == "");
    REQUIRE(has_line_diagnostic(oss, 2));
}

TEST_CASE("js_interpreter interpret #4")
{
    std::ostringstream oss;
    majsdown::js_interpreter ji{oss};

    std::string output_buffer;
    const bool ok = is_ok(ji.interpret(output_buffer, R"(

x
)"));

    REQUIRE(!ok);
    REQUIRE(output_buffer == "");
    REQUIRE(has_line_diagnostic(oss, 3));
}

TEST_CASE("js_interpreter interpret #5")
{
    std::ostringstream oss;
    majsdown::js_interpreter ji{oss};

    std::string output_buffer;
    const bool ok = is_ok(ji.interpret(output_buffer, R"(10;
//
x
)"));

    REQUIRE(!ok);
    REQUIRE(output_buffer == "");
    REQUIRE(has_line_diagnostic(oss, 3));
}

TEST_CASE("js_interpreter interpret #6")
{
    std::ostringstream oss;
    majsdown::js_interpreter ji{oss};

    std::string output_buffer;
    const bool ok = is_ok(ji.interpret(output_buffer, R"(
function foo() { bar(); }
foo();
)"));

    REQUIRE(!ok);
    REQUIRE(output_buffer == "");
    REQUIRE(has_line_diagnostic(oss, 3));
    REQUIRE(diagnostic_contains(oss, "foo"));
    REQUIRE(diagnostic_contains(oss, "bar"));
}

TEST_CASE("js_interpreter interpret #7")
{
    std::ostringstream oss;
    majsdown::js_interpreter ji{oss};

    std::string output_buffer;
    const bool ok = is_ok(ji.interpret(output_buffer, R"(1;
2;
3;
4;
XXX
6;
7;
)"));

    REQUIRE(!ok);
    REQUIRE(output_buffer == "");
    REQUIRE(has_line_diagnostic(oss, 5));
}
