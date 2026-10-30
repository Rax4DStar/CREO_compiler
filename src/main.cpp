#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ASTPrinter.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "SemanticAnalyser.hpp"


int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr
            << "Usage: " << argv[0]
            << " <source_file.creo>\n";
        return 1;
    }

    std::ifstream inputFile(argv[1], std::ios::binary);

    if (!inputFile) {
        std::cerr
            << "Error: Could not open file '"
            << argv[1] << "'\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << inputFile.rdbuf();

    if (inputFile.bad()) {
        std::cerr
            << "Error: Failed while reading file '"
            << argv[1] << "'\n";
        return 1;
    }

    try {
        Lexer lexer(buffer.str());
        std::vector<Token> tokens = lexer.tokenise();

        Parser parser(std::move(tokens));
        std::unique_ptr<ProgramNode> ast =
            parser.parseProgram();

        SemanticAnalyser analyser;
        analyser.analyze(*ast);

        std::cout << "Semantic analysis passed\n\n";

        ASTPrinter printer(std::cout);
        printer.print(*ast);
    } catch (const std::exception& error) {
        std::cerr
            << "Compilation error: "
            << error.what() << '\n';
        return 1;
    }

    return 0;
}