#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <majsdown/converter.hpp>

#include <fstream>
#include <string_view>

using namespace std::string_view_literals;

namespace {

void do_test(const std::string_view source, const std::string_view expected)
{
    std::string output_buffer;
    const bool ok = majsdown::converter{}.convert(output_buffer, source);

    REQUIRE(ok);
    REQUIRE(output_buffer == expected);
}


void make_tmp_file(const std::string_view path, const std::string_view contents)
{
    std::ofstream ofs(std::string{path});
    REQUIRE(ofs);

    ofs << contents;
    ofs.flush();

    REQUIRE(ofs);
}

} // namespace

TEST_CASE("converter ctor/dtor")
{
    majsdown::converter c;
    (void)c;
}

TEST_CASE("converter convert #0")
{
    const std::string_view source = R"(
# hello world

some markdown here... test@mail.com

**bold**
*italic*
)"sv;

    do_test(source, source);
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
    make_tmp_file("./i.js", "var i = 10;");

    const std::string_view source = R"(
@@$ majsdown_include("./i.js");
@@{i + 5}
)"sv;

    const std::string_view expected = R"(
15
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #8")
{
    make_tmp_file("./i.js", "var i = (() => 10)();");

    const std::string_view source = R"(
@@$ majsdown_include("./i.js");
@@{i + 5}
)"sv;

    const std::string_view expected = R"(
15
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #9")
{
    const std::string_view source = R"(
hello @@{ (function() { return "world"; })() }
)"sv;

    const std::string_view expected = R"(
hello world
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #10")
{
    const std::string_view source = R"(
  TEST@@{function() { return "OK"; }()}
)"sv;

    const std::string_view expected = R"(
  TESTOK
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #11")
{
    const std::string_view source = R"(
@@_{code}
```cpp
int main() { }
```
)"sv;

    const std::string_view expected = R"(
int main() { }
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #12")
{
    const std::string_view source = R"(
@@_{lang}
```cpp
int main() { }
```
)"sv;

    const std::string_view expected = R"(
cpp
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #13")
{
    const std::string_view source = R"(
@@_{lang}
```
int main() { }
```
)"sv;

    const std::string_view expected = R"(

)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #14")
{
    const std::string_view source = R"(
@@$ var i =
@@$ 10
@@$ ;
@@{i}
)"sv;

    const std::string_view expected = R"(
10
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #15")
{
    const std::string_view source = R"(
@@_{code}
```cpp
int main() { std::cout << "hello\n"; }
```
)"sv;

    const std::string_view expected = R"(
int main() { std::cout << "hello\n"; }
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #16")
{
    make_tmp_file("./test.txt", "ABC1234");

    const std::string_view source = R"(
@@$ var x = majsdown_embed("./test.txt");
@@{x}
)"sv;

    const std::string_view expected = R"(
ABC1234
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #17")
{
    make_tmp_file("./test.txt", "ABC1234\\n");

    const std::string_view source = R"(
@@$ var x = majsdown_embed("./test.txt");
@@{x}
)"sv;

    const std::string_view expected = R"(
ABC1234\n
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #18")
{
    const std::string_view source = R"(
@@$ const value = 10; // the value
@@$ const otherValue = 12; // the other value
@@$ const lastValue = 14;
@@{value + otherValue + lastValue}
)"sv;

    const std::string_view expected = R"(
36
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #19")
{
    const std::string_view source = R"(
@@$ const value = 10; // the value
@@{value}
)"sv;

    const std::string_view expected = R"(
10
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #20")
{
    const std::string_view source = R"(
@@$ const value = 10; // the value
)"sv;

    const std::string_view expected = R"(
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #21")
{
    const std::string_view source = R"(
@@$ const value = 10; // the value
)"sv;

    const std::string_view expected = R"(
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #22")
{
    const std::string_view source = R"(
@@$ const value = 10; // the value
@@$ const otherValue = 12;
@@{value + otherValue}
)"sv;

    const std::string_view expected = R"(
22
)"sv;

    do_test(source, expected);
}

TEST_CASE("converter convert #23")
{
    const std::string_view source = R"(
@@$ function wrapInCodeBlock(code, lang)
@@$ {
@@$     return "```" + lang + "\n" + code + "\n```";
@@$ }
@@$
@@$ function replaceStdNamespace(ns, code, lang)
@@$ {
@@$     return wrapInCodeBlock(
@@$         code.replace(/std::/g, `${ns}::`), lang
@@$     );
@@$ }
@@$
@@$ const namespace = 'bsl';

## Code Block Functions

@@_{replaceStdNamespace(namespace, code, lang)}
```cpp
std::string greeting = "hello world";
std::cout << greeting << std::endl;
```
)"sv;

    const std::string_view expected = R"(

## Code Block Functions

```cpp
bsl::string greeting = "hello world";
bsl::cout << greeting << bsl::endl;
```
)"sv;

    do_test(source, expected);
}


