#include <majsdown/converter.hpp>

#include <iostream>
#include <string>

int main()
{
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string input_and_final_buffer;
    input_and_final_buffer.reserve(128000);

    std::string line_and_output_buffer;
    line_and_output_buffer.reserve(512);

    while (std::getline(std::cin, line_and_output_buffer))
    {
        input_and_final_buffer.append(line_and_output_buffer);
        input_and_final_buffer.append(1, '\n');
    }

    line_and_output_buffer.clear();
    line_and_output_buffer.reserve(input_and_final_buffer.size() + 1024);

    majsdown::converter converter;

    constexpr majsdown::converter::config fst_pass_cfg{
        .skip_escaped_symbols = true,
        .skip_inline_expressions = false,
        .skip_inline_statements = false,
        .skip_block_statements = false,
        .skip_code_block_decorators = true //
    };

    if (!converter.convert(
            fst_pass_cfg, line_and_output_buffer, input_and_final_buffer))
    {
        std::cerr << "((MJSD ERROR))(?): Fatal error during majsdown "
                     "conversion process (first pass)\n"
                  << std::endl;

        return 1;
    }

    input_and_final_buffer.clear();

    constexpr majsdown::converter::config snd_pass_cfg{
        .skip_escaped_symbols = false,
        .skip_inline_expressions = false,
        .skip_inline_statements = false,
        .skip_block_statements = false,
        .skip_code_block_decorators = false //
    };

    if (!converter.convert(
            snd_pass_cfg, input_and_final_buffer, line_and_output_buffer))
    {
        std::cerr << "((MJSD ERROR))(?): Fatal error during majsdown "
                     "conversion process (second pass)\n"
                  << std::endl;

        return 2;
    }

    std::cout << input_and_final_buffer << std::endl;
    return 0;
}
