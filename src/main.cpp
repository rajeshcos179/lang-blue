#include <fstream>

#include "./generator.hpp"

int main(int argc, char *argv[])
{
    // argument to the executable is .blu file
    if (argc != 2)
    {
        std::cerr << "Incorrect usage. Correct usage ... " << std::endl;
        std::cerr << "blue <input.blu>" << std::endl;
        exit(EXIT_FAILURE);
    }

    // transferring file content into stringstream then to string
    std::string contents;
    {
        std::stringstream contents_stream;
        std::fstream input(argv[1], std::ios::in);
        contents_stream << input.rdbuf();
        contents = contents_stream.str();
    }

    // tokenising each string or symbol
    Tokenizer tokenizer(std::move(contents));
    std::vector<Token> tokens = tokenizer.tokenize();

    // generating parse tree
    Parser parser(std::move(tokens));
    std::optional<NodeProg> prog = parser.parse_prog();

    if (!prog.has_value())
    {
        std::cerr << "No statement found" << std::endl;
        exit(EXIT_FAILURE);
    }

    // generating assembly code
    Generator generator(std::move(prog.value()));

    // transferring assembly code to file out.asm
    {
        std::fstream file("out.asm", std::ios::out);
        file << generator.gen_prog();
    }

    // generating object code by assember - nasm
    // system("nasm -felf64 out.asm")

    // generating object code by assembler - yasm (gives .lst file for examining text segment)
    system("yasm -felf64 -g dwarf2 -l out.lst out.asm");

    // linking object code gives executable
    system("ld out.o -o out");

    return EXIT_SUCCESS;
}