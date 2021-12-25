#include <majsdown/converter.hpp>

#include <iostream>
#include <string>

int main()
{
    std::ios_base::sync_with_stdio(false);

    std::string input_buffer;
    input_buffer.reserve(128000);

    {
        std::string line_input;
        line_input.reserve(512);

        while (std::getline(std::cin, line_input))
        {
            input_buffer.append(line_input);
            input_buffer.append(1, '\n');
        }
    }

    majsdown::converter converter;

    std::string output_buffer;
    output_buffer.reserve(input_buffer.size() + 1024);

    if (!converter.convert(output_buffer, input_buffer))
    {
        return 1;
    }

    std::cout << output_buffer << std::endl;
    return 0;
}
