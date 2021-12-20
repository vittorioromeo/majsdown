#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <majsdown/js_interpreter.hpp>

TEST_CASE("js_interpreter ctor/dtor")
{
    majsdown::js_interpreter ji;
    (void)ji;
}

TEST_CASE("js_interpreter interpret #0")
{
    majsdown::js_interpreter ji;

    ji.interpret(R"(
majsdown_set_output('hello world!');
)");

    REQUIRE(ji.read_tl_buffer() == "hello world!");
}

TEST_CASE("js_interpreter interpret #1")
{
    majsdown::js_interpreter ji;

    ji.interpret(R"(
var i = 10;
var j = 15;
var res = (function(){ return i + j; })();

majsdown_set_output(res);
)");

    REQUIRE(ji.read_tl_buffer() == "25");
}
