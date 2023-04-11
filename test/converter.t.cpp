#include <sstream>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <majsdown/converter.hpp>

#include <array>
#include <fstream>
#include <string_view>

#include <cassert>

using namespace std::string_view_literals;

namespace {

[[nodiscard]] std::string& get_thread_local_buf(const std::size_t i)
{
    assert(i < 2);

    thread_local auto result = []
    {
        std::array<std::string, 2> bufs;

        for (std::string& buf : bufs)
        {
            buf.reserve(512);
        }

        return bufs;
    }();

    return result[i];
}

std::string& do_test_impl(majsdown::converter& cnvtr, const std::size_t buf_idx,
    const std::string_view source, const std::string_view expected,
    const majsdown::converter::config& cfg)
{
    std::string& output_buffer = get_thread_local_buf(buf_idx);
    output_buffer.clear();

    const bool ok = cnvtr.convert(cfg, output_buffer, source);

    REQUIRE(ok);
    REQUIRE(output_buffer == expected);

    return output_buffer;
}

void do_test_one_pass_error(const std::string_view source,
    const majsdown::converter::config& cfg = {},
    std::ostream& err_stream = std::cerr)
{
    majsdown::converter cnvtr{err_stream};

    std::string& output_buffer = get_thread_local_buf(0);
    output_buffer.clear();

    const bool ok = cnvtr.convert(cfg, output_buffer, source);

    // TODO:
    // REQUIRE(ok);
    // REQUIRE(output_buffer == "");
}

void do_test_one_pass(const std::string_view source,
    const std::string_view expected,
    const majsdown::converter::config& cfg = {},
    std::ostream& err_stream = std::cerr)
{
    majsdown::converter cnvtr{err_stream};
    do_test_impl(cnvtr, 0, source, expected, cfg);
}

void do_test_two_pass(const std::string_view source,
    const std::string_view expected_fst, const std::string_view expected_snd,
    const majsdown::converter::config& cfg_fst,
    const majsdown::converter::config& cfg_snd = {},
    std::ostream& err_stream = std::cerr)
{
    majsdown::converter cnvtr{err_stream};

    const std::string& out_fst =
        do_test_impl(cnvtr, 0, source, expected_fst, cfg_fst);

    do_test_impl(cnvtr, 1, out_fst, expected_snd, cfg_snd);
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
    majsdown::converter c{std::cerr};
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

    do_test_one_pass(source, source);
}

TEST_CASE("converter convert #1")
{
    const std::string_view source = R"(
@@{"hello world"}
)"sv;

    const std::string_view expected = R"(
hello world
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #2")
{
    const std::string_view source = R"(
@@{10 + 5}
)"sv;

    const std::string_view expected = R"(
15
)"sv;

    do_test_one_pass(source, expected);
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

    do_test_one_pass(source, expected);
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

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #5")
{
    const std::string_view source = R"(
hello @@{1 + 5} world
)"sv;

    const std::string_view expected = R"(
hello 6 world
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #6")
{
    const std::string_view source = R"(
hello @@{"world"}
)"sv;

    const std::string_view expected = R"(
hello world
)"sv;

    do_test_one_pass(source, expected);
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

    do_test_one_pass(source, expected);
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

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #9")
{
    const std::string_view source = R"(
hello @@{ (function() { return "world"; })() }
)"sv;

    const std::string_view expected = R"(
hello world
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #10")
{
    const std::string_view source = R"(
  TEST@@{function() { return "OK"; }()}
)"sv;

    const std::string_view expected = R"(
  TESTOK
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #11")
{
    const std::string_view source = R"(
@@_{code}_
```cpp
int main() { }
```
)"sv;

    const std::string_view expected = R"(
int main() { }
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #12")
{
    const std::string_view source = R"(
@@_{lang}_
```cpp
int main() { }
```
)"sv;

    const std::string_view expected = R"(
cpp
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #13")
{
    const std::string_view source = R"(
@@_{lang}_
```
int main() { }
```
)"sv;

    const std::string_view expected = R"(

)"sv;

    do_test_one_pass(source, expected);
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

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #15")
{
    const std::string_view source = R"(
@@_{code}_
```cpp
int main() { std::cout << "hello\n"; }
```
)"sv;

    const std::string_view expected = R"(
int main() { std::cout << "hello\n"; }
)"sv;

    do_test_one_pass(source, expected);
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

    do_test_one_pass(source, expected);
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

    do_test_one_pass(source, expected);
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

    do_test_one_pass(source, expected);
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

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #20")
{
    const std::string_view source = R"(
@@$ const value = 10; // the value
)"sv;

    const std::string_view expected = R"(
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #21")
{
    const std::string_view source = R"(
@@$ const value = 10; // the value
)"sv;

    const std::string_view expected = R"(
)"sv;

    do_test_one_pass(source, expected);
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

    do_test_one_pass(source, expected);
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

@@_{replaceStdNamespace(namespace, code, lang)}_
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

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #24")
{
    make_tmp_file("./i.js", "var i = (() => 10)();");
    make_tmp_file("./j.js", "majsdown_include(\"./i.js\");");

    const std::string_view source = R"(
@@$ majsdown_include("./j.js");
@@{i + 5}
)"sv;

    const std::string_view expected = R"(
15
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #25")
{
    make_tmp_file("./i.js", "var i = (() => 10)();");
    make_tmp_file("./j.js",
        "majsdown_include(\"./i.js\"); majsdown_include(\"./i.js\");");
    make_tmp_file("./k.js",
        "majsdown_include(\"./j.js\"); majsdown_include(\"./j.js\");");

    const std::string_view source = R"(
@@$ majsdown_include("./j.js");
@@{i + 5}
)"sv;

    const std::string_view expected = R"(
15
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #26")
{
    const std::string_view source = R"(
@@_{code}_
`````cpp
int main() { }
`````
)"sv;

    const std::string_view expected = R"(
int main() { }
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #27")
{
    const std::string_view source = R"(
@@_{lang}_
`````cpp
int main() { }
`````
)"sv;

    const std::string_view expected = R"(
cpp
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #28")
{
    const std::string_view source = R"(
@@_{lang}_
`````
int main() { }
`````
)"sv;

    const std::string_view expected = R"(

)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #29")
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

@@_{replaceStdNamespace(namespace, code, lang)}_
`````cpp
std::string greeting = "hello world";
std::cout << greeting << std::endl;
`````
)"sv;

    const std::string_view expected = R"(

## Code Block Functions

```cpp
bsl::string greeting = "hello world";
bsl::cout << greeting << bsl::endl;
```
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #30")
{
    const std::string_view source = R"(
@@_{lang}_
`````markdown
```cpp
int main() { }
```
`````
)"sv;

    const std::string_view expected = R"(
markdown
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #31")
{
    const std::string_view source = R"(
@@_{code}_
`````markdown
```cpp
int main() { }
```
`````
)"sv;

    const std::string_view expected = R"(
```cpp
int main() { }
```
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #32")
{
    const std::string_view source = R"(
@@${ var i = 10; }$
@@{i + 5}
)"sv;

    const std::string_view expected = R"(
15
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #33")
{
    const std::string_view source = R"(
@@${
var i = 10;
}$
@@{i + 5}
)"sv;

    const std::string_view expected = R"(
15
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #33")
{
    const std::string_view source = R"(
@@${
var i = 10;
i += 5;
}$
@@{i + 5}
)"sv;

    const std::string_view expected = R"(
20
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #34")
{
    const std::string_view source = R"(
@@${
function p() { return 10; }
}$
@@{p() + 10}
)"sv;

    const std::string_view expected = R"(
20
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #35")
{
    const std::string_view source = R"(
@@${
function p() { return 10; }
}$
@@${
function g() { return 10; }
}$
@@{p() + g()}
)"sv;

    const std::string_view expected = R"(
20
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #36")
{
    const std::string_view source = R"(
@
)"sv;

    const std::string_view expected = R"(
@
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #37")
{
    const std::string_view source = R"(
@@
)"sv;

    const std::string_view expected = R"(
@@
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #38")
{
    const std::string_view source = R"(
@@$
)"sv;

    const std::string_view expected = R"(
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #39")
{
    const std::string_view source = R"(
\@@$
)"sv;

    const std::string_view expected = R"(
@@$
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #40")
{
    const std::string_view source = R"(
@\@$
)"sv;

    const std::string_view expected = R"(
@@$
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #41")
{
    const std::string_view source = R"(
@\@
)"sv;

    const std::string_view expected = R"(
@@
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #42")
{
    const std::string_view source = R"(
\@
)"sv;

    const std::string_view expected = R"(
@
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #43")
{
    const std::string_view source = R"(
\
)"sv;

    const std::string_view expected = R"(
\
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #44")
{
    const std::string_view source = R"(
\\
)"sv;

    const std::string_view expected = R"(
\\
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #45")
{
    const std::string_view source = R"(
\x
)"sv;

    const std::string_view expected = R"(
\x
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #46")
{
    const std::string_view source = R"(
```markdown
\@@$ function test() { }
```

The `\@@$` sequence is a statement.
)"sv;

    const std::string_view expected = R"(
```markdown
@@$ function test() { }
```

The `@@$` sequence is a statement.
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #47")
{
    const std::string_view source = R"(
```markdown
The result is \@@{10 + 5}.
```

The `\@@{expr}` sequence is an expression.
)"sv;

    const std::string_view expected = R"(
```markdown
The result is @@{10 + 5}.
```

The `@@{expr}` sequence is an expression.
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #48")
{
    const std::string_view source = R"(
``````markdown
\@@_{godboltify(code)}_
```cpp
#include <iostream>

int main()
{
    std::cout << "I should be using <format>...\n";
    return 0;
}
```
``````
)"sv;

    const std::string_view expected = R"(
``````markdown
@@_{godboltify(code)}_
```cpp
#include <iostream>

int main()
{
    std::cout << "I should be using <format>...\n";
    return 0;
}
```
``````
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #49")
{
    const std::string_view source = R"(
@@_{code}_
```cpp
int @@{"main"}() { }
```
)"sv;

    const std::string_view expected = R"(
int @@{"main"}() { }
)"sv;

    do_test_one_pass(source, expected);
}

TEST_CASE("converter convert #50")
{
    const std::string_view source = R"(
@@{"sup"}
)"sv;

    const majsdown::converter::config cfg_fst{
        .skip_escaped_symbols = false,
        .skip_inline_expressions = true,
        .skip_inline_statements = false,
        .skip_block_statements = false,
        .skip_code_block_decorators = false //
    };

    const std::string_view expected_fst = R"(
@@{"sup"}
)"sv;

    const std::string_view expected_snd = R"(
sup
)"sv;

    do_test_two_pass(source, expected_fst, expected_snd, cfg_fst, {});
}

TEST_CASE("converter convert #51")
{
    const std::string_view source = R"(
@@_{code}_
```cpp
foo
```
)"sv;

    const majsdown::converter::config cfg_fst{
        .skip_escaped_symbols = false,
        .skip_inline_expressions = false,
        .skip_inline_statements = false,
        .skip_block_statements = false,
        .skip_code_block_decorators = true //
    };

    const std::string_view expected_fst = R"(
@@_{code}_
```cpp
foo
```
)"sv;

    const std::string_view expected_snd = R"(
foo
)"sv;

    do_test_two_pass(source, expected_fst, expected_snd, cfg_fst, {});
}

TEST_CASE("converter convert #52")
{
    const std::string_view source = R"(
@@_{code}_
```cpp
int @@{"main"}() { }
```
)"sv;

    const majsdown::converter::config cfg_fst{
        .skip_escaped_symbols = false,
        .skip_inline_expressions = false,
        .skip_inline_statements = false,
        .skip_block_statements = false,
        .skip_code_block_decorators = true //
    };

    const std::string_view expected_fst = R"(
@@_{code}_
```cpp
int main() { }
```
)"sv;

    const std::string_view expected_snd = R"(
int main() { }
)"sv;

    do_test_two_pass(source, expected_fst, expected_snd, cfg_fst, {});
}

TEST_CASE("converter convert #53")
{
    make_tmp_file("./x.cpp", "int main() { }");

    const std::string_view source = R"(
@@_{code}_
```cpp
@@{majsdown_embed("./x.cpp")}
```
)"sv;

    const majsdown::converter::config cfg_fst{
        .skip_escaped_symbols = false,
        .skip_inline_expressions = false,
        .skip_inline_statements = false,
        .skip_block_statements = false,
        .skip_code_block_decorators = true //
    };

    const std::string_view expected_fst = R"(
@@_{code}_
```cpp
int main() { }
```
)"sv;

    const std::string_view expected_snd = R"(
int main() { }
)"sv;

    do_test_two_pass(source, expected_fst, expected_snd, cfg_fst, {});
}

TEST_CASE("converter convert #54")
{
    const std::string_view source = R"(
@@$ function id(x) { return x; }

@@_{id(code)}_
```cpp
int @@{"main"}() { }
```
)"sv;

    const majsdown::converter::config cfg_fst{
        .skip_escaped_symbols = false,
        .skip_inline_expressions = false,
        .skip_inline_statements = false,
        .skip_block_statements = false,
        .skip_code_block_decorators = true //
    };

    const std::string_view expected_fst = R"(

@@_{id(code)}_
```cpp
int main() { }
```
)"sv;

    const std::string_view expected_snd = R"(

int main() { }
)"sv;

    do_test_two_pass(source, expected_fst, expected_snd, cfg_fst, {});
}

TEST_CASE("converter convert #55")
{
    const std::string_view source = R"(
\@@$
)"sv;

    const majsdown::converter::config cfg_fst{
        .skip_escaped_symbols = true,
        .skip_inline_expressions = false,
        .skip_inline_statements = true,
        .skip_block_statements = false,
        .skip_code_block_decorators = false //
    };

    const std::string_view expected_fst = R"(
\@@$
)"sv;

    const std::string_view expected_snd = R"(
@@$
)"sv;

    do_test_two_pass(source, expected_fst, expected_snd, cfg_fst, {});
}

TEST_CASE("converter convert #56")
{
    const std::string_view source = R"(
@@$ function id(x) { return x; }

``````markdown
\@@_{id(code)}_
```cpp
int main() { }
```
``````
)"sv;

    const majsdown::converter::config cfg_fst{
        .skip_escaped_symbols = true,
        .skip_inline_expressions = false,
        .skip_inline_statements = false,
        .skip_block_statements = false,
        .skip_code_block_decorators = true //
    };

    const std::string_view expected_fst = R"(

``````markdown
\@@_{id(code)}_
```cpp
int main() { }
```
``````
)"sv;

    const std::string_view expected_snd = R"(

``````markdown
@@_{id(code)}_
```cpp
int main() { }
```
``````
)"sv;

    do_test_two_pass(source, expected_fst, expected_snd, cfg_fst, {});
}

TEST_CASE("converter convert #57")
{
    const std::string_view source = R"(
\@@$
)"sv;

    const majsdown::converter::config cfg_fst{
        .skip_escaped_symbols = true,
        .skip_inline_expressions = false,
        .skip_inline_statements = true,
        .skip_block_statements = false,
        .skip_code_block_decorators = false //
    };

    const std::string_view expected_fst = R"(
\@@$
)"sv;

    const std::string_view expected_snd = R"(
@@$
)"sv;

    do_test_two_pass(source, expected_fst, expected_snd, cfg_fst, {});
}

TEST_CASE("converter convert #58")
{
    const std::string_view source = R"(
\@@$

```cpp
"\n"
```
)"sv;

    const majsdown::converter::config cfg_fst{
        .skip_escaped_symbols = true,
        .skip_inline_expressions = false,
        .skip_inline_statements = true,
        .skip_block_statements = false,
        .skip_code_block_decorators = false //
    };

    const std::string_view expected_fst = R"(
\@@$

```cpp
"\n"
```
)"sv;

    const std::string_view expected_snd = R"(
@@$

```cpp
"\n"
```
)"sv;

    do_test_two_pass(source, expected_fst, expected_snd, cfg_fst, {});
}

TEST_CASE("converter convert #59")
{
    const std::string_view source = R"xxx(
``````markdown {style="font-size: 155%"}
\@@${
function godboltLink(code)
{
    return "[on godbolt](" + String.raw`https://godbolt.org/clientstate/
        ${Base64.encode(godboltJson(code))}` + ")";
}
}$

\@@_{godboltify(code)}_
```cpp
int main()
{
    // ...
}
```
``````
)xxx"sv;

    const majsdown::converter::config cfg_fst{
        .skip_escaped_symbols = true,
        .skip_inline_expressions = false,
        .skip_inline_statements = false,
        .skip_block_statements = false,
        .skip_code_block_decorators = true //
    };

    const std::string_view expected_fst = R"xxx(
``````markdown {style="font-size: 155%"}
\@@${
function godboltLink(code)
{
    return "[on godbolt](" + String.raw`https://godbolt.org/clientstate/
        ${Base64.encode(godboltJson(code))}` + ")";
}
}$

\@@_{godboltify(code)}_
```cpp
int main()
{
    // ...
}
```
``````
)xxx"sv;

    const std::string_view expected_snd = R"xxx(
``````markdown {style="font-size: 155%"}
@@${
function godboltLink(code)
{
    return "[on godbolt](" + String.raw`https://godbolt.org/clientstate/
        ${Base64.encode(godboltJson(code))}` + ")";
}
}$

@@_{godboltify(code)}_
```cpp
int main()
{
    // ...
}
```
``````
)xxx"sv;

    do_test_two_pass(source, expected_fst, expected_snd, cfg_fst, {});
}

// TODO: repeated with interpreter test

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

[[nodiscard]] static bool has_js_line_diagnostic(
    const std::string_view sv, const std::size_t i)
{
    thread_local std::ostringstream oss;
    oss << "Interpreter line: '" << i << "'";

    return diagnostic_contains(sv, oss.str());
}

[[nodiscard]] static bool has_final_line_diagnostic(
    const std::string_view sv, const std::size_t i)
{
    thread_local std::ostringstream oss;
    oss << "((MJSD ERROR))(" << i << "):";

    return diagnostic_contains(sv, oss.str());
}

[[nodiscard]] static bool has_js_line_diagnostic(
    const std::ostringstream& oss, const std::size_t i)
{
    return has_js_line_diagnostic(oss.str(), i);
}

[[nodiscard]] static bool has_final_line_diagnostic(
    const std::ostringstream& oss, const std::size_t i)
{
    return has_final_line_diagnostic(oss.str(), i);
}

TEST_CASE("converter convert #60")
{
    const std::string_view source = R"(
@@$ var i = 10;
@@{i + 5}
)"sv;

    const std::string_view expected = R"(
15
)"sv;

    std::ostringstream oss;
    do_test_one_pass(source, expected, {}, oss);

    REQUIRE(oss.str() == "");
}

TEST_CASE("converter convert #61")
{
    const std::string_view source = R"(
@@$ var i = j;

)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 1));
    REQUIRE(has_final_line_diagnostic(oss, 2));
}

TEST_CASE("converter convert #62")
{
    const std::string_view source = R"(
a
@@$ var i = j;

)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 1));
    REQUIRE(has_final_line_diagnostic(oss, 3));
}

TEST_CASE("converter convert #63")
{
    const std::string_view source = R"(
a
##
@@$ var i = j;

)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 1));
    REQUIRE(has_final_line_diagnostic(oss, 4));
}

TEST_CASE("converter convert #64")
{
    const std::string_view source = R"(
a
##
@@{10}
@@$ var i = j;

)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 1));
    REQUIRE(has_final_line_diagnostic(oss, 5));
}

TEST_CASE("converter convert #65")
{
    const std::string_view source = R"(
@@{j};

)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 1));
    REQUIRE(has_final_line_diagnostic(oss, 2));
}

TEST_CASE("converter convert #66")
{
    const std::string_view source = R"(
a
@@{j};

)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 1));
    REQUIRE(has_final_line_diagnostic(oss, 3));
}

TEST_CASE("converter convert #67")
{
    const std::string_view source = R"(
a
##
@@{j};

)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 1));
    REQUIRE(has_final_line_diagnostic(oss, 4));
}

TEST_CASE("converter convert #68")
{
    const std::string_view source = R"(
a
##
@@{10}
@@{j};

)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 1));
}

TEST_CASE("converter convert #69")
{
    const std::string_view source = R"(
a
##
@@$ var i = j;
@@$
@@$

)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 1));
}

TEST_CASE("converter convert #70")
{
    const std::string_view source = R"(
a
##
@@$
@@$ var i = j;
@@$

)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 2));
}

TEST_CASE("converter convert #71")
{
    const std::string_view source = R"(
a
##
@@$
@@$
@@$ var i = j;

)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 3));
}

TEST_CASE("converter convert #72")
{
    const std::string_view source = R"(
a
##
@@$ var i = j;

@@$

)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 1));
}

TEST_CASE("converter convert #73")
{
    const std::string_view source = R"(
a
##
@@$

@@$ var i = j;

)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 1));
}

TEST_CASE("converter convert #74")
{
    const std::string_view source = R"(
a
##
@@$
@@$
b

@@$
@@$ var i = j;
@@$
c

)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 2));
    REQUIRE(has_final_line_diagnostic(oss, 9));
}

TEST_CASE("converter convert #75")
{
    const std::string_view source = R"(
a
##
@@${var i = j;}$

)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 1));
}

TEST_CASE("converter convert #76")
{
    const std::string_view source = R"(
a
##
@@${
10;
var i = j;
}$

)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 3));
}

TEST_CASE("converter convert #77")
{
    const std::string_view source = R"(
@@$ x
)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 1));
    REQUIRE(has_final_line_diagnostic(oss, 2));
}

TEST_CASE("converter convert #78")
{
    const std::string_view source = R"(
@@{x}
)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 1));
    REQUIRE(has_final_line_diagnostic(oss, 2));
}

TEST_CASE("converter convert #79")
{
    const std::string_view source = R"(
@@$ var y = 0;

@@$ x
)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 1));
    REQUIRE(has_final_line_diagnostic(oss, 4));
}

TEST_CASE("converter convert #80")
{
    const std::string_view source = R"(
aaa @@{x}
)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 1));
    REQUIRE(has_final_line_diagnostic(oss, 2));
}

TEST_CASE("converter convert #81")
{
    const std::string_view source = R"(
aaa @@{x} aaa
)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 1));
    REQUIRE(has_final_line_diagnostic(oss, 2));
}

TEST_CASE("converter convert #82")
{
    const std::string_view source = R"(
aaa @@{x} aaaa
aaa

@@{10}
)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 1));
    REQUIRE(has_final_line_diagnostic(oss, 2));
}

TEST_CASE("converter convert #83")
{
    const std::string_view source = R"(
aaa @@{5} aaaa @@{x}
aaa

@@{10}
)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 1));
    REQUIRE(has_final_line_diagnostic(oss, 2));
}

TEST_CASE("converter convert #84")
{
    const std::string_view source = R"(
aaa @@{x} aaaa @@{5}
aaa

@@{10}
)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 1));
    REQUIRE(has_final_line_diagnostic(oss, 2));
}

TEST_CASE("converter convert #85")
{
    const std::string_view source = R"(
@@$
@@$ x
@@$
)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 2));
    REQUIRE(has_final_line_diagnostic(oss, 3));
}

TEST_CASE("converter convert #86")
{
    const std::string_view source = R"(
@@$ __mjsd_line(-2);
@@$ x
@@$
)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 2));
    REQUIRE(has_final_line_diagnostic(oss, 1));
}


TEST_CASE("converter convert #87")
{
    const std::string_view source = R"(
@@${
x
}$
)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 2));
    REQUIRE(has_final_line_diagnostic(oss, 3));
}

TEST_CASE("converter convert #88")
{
    const std::string_view source = R"(
@@${ __mjsd_line(-2);
x
}$
)"sv;

    std::ostringstream oss;
    do_test_one_pass_error(source, {}, oss);

    REQUIRE(has_js_line_diagnostic(oss, 2));
    REQUIRE(has_final_line_diagnostic(oss, 1));
}
