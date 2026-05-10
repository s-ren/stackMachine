// include standard library headers
#include <iostream>
#include <fstream>
#include "include/CLI11.hpp"

// include project headers
#include "lexer/token.h"
#include "parser/parser.h"

using string = std::string;

/*
    kaleidoscope requires a source file as input and produces a binary file as output.
    It takes the following flags:
    -i <input_file>: specify the input file path (required)
    -o <output_file>: specify the output file path (required)
    -v: verbose mode.
    -h: display help message
*/
int main(int argc, char *argv[]) {
    // command line parsing using CLI11 library
    CLI::App app{"Kaleidoscope Compiler"};
    argv = app.ensure_utf8(argv);

    string input_file_name;
    string command_file_name;
    string output_file_name;
    bool verbose = false;

    app.add_option("-i,--input", input_file_name, "Input source file")->required();
    app.add_option("-o,--output", output_file_name, "Output binary file")->required();
    app.add_flag("-v,--verbose", verbose, "Verbose mode");
    // execute the command line parser and handle errors
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    }

    // read to input file
    if (verbose) {
        std::cout << "Opening input file " << input_file_name << "..." << std::endl;
    }
    std::ifstream input_file(input_file_name);
    if (!input_file.is_open()) {
        std::cerr << "Error: Could not open file " << input_file_name << std::endl;
        return 1;
    }

    std::ostringstream input_buffer;
    input_buffer << input_file.rdbuf();
    string input = input_buffer.str();

    // parse the input file to generate the IR
    if (verbose) {
        std::cout << "Parsing..." << std::endl;
    }
    string ir = parse(input);

    // compile IR into binary
    if (verbose) {
        std::cout << "Compiling..." << std::endl;
    }


    // write to output file
    if (verbose) {
        std::cout << "Writing to output file " << output_file_name << "..." << std::endl;
    }

    return 0;
}