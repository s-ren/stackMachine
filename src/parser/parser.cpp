#include "mlir/IR/Builders.h"

#include "ir/StackIRDialect.h.inc"
#include "ir/StackIROps.h.inc"

#include <iostream>
#include <string>
#include <vector>
using string = std::string;

namespace parser {

string parse(std::vector<uint8_t> code) {
    // initialize MLIR context and load the StackIR dialect
    mlir::MLIRContext context;
    context.getOrLoadDialect(mlir::stackIR::StackIRDialect::getDialectNamespace());

    mlir::OpBuilder builder(&context);

    for (size_t i = 0; i < code.size(); i++) {
        uint8_t operation = code[i];
        uint8_t argument = 0;
        switch (operation) {
            case 0x00: // STOP
                std::cout << "Parsed STOP operation" << std::endl;
                break;
            case 0x01: // LOAD
                // read another argument
                if (i + 1 >= code.size()) {
                    throw std::runtime_error("Expected argument for LOAD operation at position " + std::to_string(i));
                }
                argument = code[++i];
                std::cout << "Parsed LOAD operation with argument: " << std::to_string(argument) << std::endl;
                break;
            case 0x02: // STORE
                // read another argument
                if (i + 1 >= code.size()) {
                    throw std::runtime_error("Expected argument for STORE operation at position " + std::to_string(i));
                }
                argument = code[++i];
                std::cout << "Parsed STORE operation with argument: " << std::to_string(argument) << std::endl;
                break;
            case 0x03: // POP
                std::cout << "Parsed POP operation" << std::endl;
                break;
            case 0x04: // ADD
                std::cout << "Parsed ADD operation" << std::endl;
                break;
            case 0x05: // SUB
                std::cout << "Parsed SUB operation" << std::endl;
                break;
            case 0x06: // DUP
                std::cout << "Parsed DUP operation" << std::endl;
                break;
            default:
                std::cout << "Parsed other operation: " << std::to_string(operation) << std::endl;
                //throw std::runtime_error("Unknown opcode: " + std::to_string(operation));
        }
       // parse the bytecode and generate IR
    }
    return "";
}
}