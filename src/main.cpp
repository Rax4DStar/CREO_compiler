#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "IRGenerator.hpp"
#include "IRPrinter.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "SemanticAnalyser.hpp"
#include "Token.hpp"

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
            << "Error: could not open '"
            << argv[1]
            << "'\n";

        return 1;
    }

    std::ostringstream buffer;
    buffer << inputFile.rdbuf();

    if (inputFile.bad()) {
        std::cerr
            << "Error: failed while reading '"
            << argv[1]
            << "'\n";

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
        
        IRGenerator generator;
        IRProgram ir = generator.generate(*ast);

        dumpIR(std::cout, ir);
    }
    catch (const std::exception& error) {
        std::cerr
            << "Compilation error: "
            << error.what()
            << '\n';

        return 1;
    }
    return 0;
}