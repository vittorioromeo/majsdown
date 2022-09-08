#include <majsdown/converter.hpp>

#include <iostream>
#include <string>

int main()
{
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string input_buffer;
    input_buffer.reserve(128000);

    std::string line_and_output_buffer;
    line_and_output_buffer.reserve(512);

    while (std::getline(std::cin, line_and_output_buffer))
    {
        input_buffer.append(line_and_output_buffer);
        input_buffer.append(1, '\n');
    }

    line_and_output_buffer.clear();
    line_and_output_buffer.reserve(input_buffer.size() + 1024);

    if (majsdown::converter converter;
        !converter.convert(line_and_output_buffer, input_buffer))
    {
        std::cerr << "((MJSD ERROR))(?): Fatal error during majsdown "
                     "conversion process.\n"
                  << std::endl;

        return 1;
    }

    std::cout << line_and_output_buffer << std::endl;
    return 0;
}
