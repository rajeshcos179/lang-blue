#include<fstream>

#include "./generator.hpp"   

int main(int argc, char* argv[])
{
    if(argc != 2){
        std::cerr << "Incorrect usage. Correct usage ... " << std::endl;
        std::cerr << "blue <input.blu>" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string contents;
    {
        std::stringstream contents_stream;
        std::fstream input(argv[1], std::ios::in);
        contents_stream << input.rdbuf();
        contents = contents_stream.str();
    }

    Tokenizer tokenizer(std::move(contents));
    std::vector<Token> tokens = tokenizer.tokenize();

    Parser parser(std::move(tokens));
    std::optional<NodeProg> prog = parser.parse_prog();

    if(!prog.has_value()){
        std::cerr << "No statement found" << std::endl;
        exit(EXIT_FAILURE);
    }

    Generator generator(std::move(prog.value()));

    {
        std::fstream file("out.asm", std::ios::out);
        file << generator.gen_prog();
    }

    system("nasm -felf64 out.asm");
    system("ld out.o -o out");

    return EXIT_SUCCESS;
}