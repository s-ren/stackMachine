#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Location.h"

#define GET_DIALECT_INFO
#include "ir/StackIRDialect.h.inc"
#define GET_OP_CLASSES
#include "ir/StackIROps.h.inc"

#include <string>
#include <vector>
using string = std::string;

namespace parser {

mlir::ModuleOp parse(mlir::MLIRContext &context, std::vector<uint8_t> code) {
    // Register the StackIR dialect so its ops are recognized in this context.
    context.getOrLoadDialect<mlir::stackIR::StackIRDialect>();

    // Convenience builder; tracks the current insertion point for new ops.
    mlir::OpBuilder builder(&context);

    // Create the top-level container op. Every MLIR program lives inside a ModuleOp.
    // It owns a single region with a single block where ops are inserted.
    auto module = mlir::ModuleOp::create(mlir::UnknownLoc::get(&context));

    // Point the builder at the end of the module's body block so new ops
    // are appended in program order.
    builder.setInsertionPointToEnd(module.getBody());

    // i8 integer type, used as the attribute type for LOAD/STORE indices.
    auto i8 = builder.getIntegerType(8);

    // stack pointer
    auto stack_ptr = 0;
    auto stack_size = 0;

    bool stop = false;
    for (size_t i = 0; i < code.size(); i++) {
        uint8_t operation = code[i];
        uint8_t argument = 0;
        // Use bytecode offset i as the location for each op.
        auto loc = mlir::FileLineColLoc::get(&context, "", 0, i);
        // create attribute for current stack pointer value
        mlir::IntegerAttr stack_ptr_attr = builder.getIntegerAttr(i8, stack_ptr);

        switch (operation) {
            case 0x00: // STOP
                // create a (STOP, stack_ptr)
                mlir::stackIR::StopOp::create(builder, loc, stack_ptr_attr);
                stop = true;
                break;

            case 0x01: { // LOAD
                // read argument byte for LOAD opcode
                if (i + 1 >= code.size())
                    throw std::runtime_error("Expected argument for LOAD at position " + std::to_string(i) + ", LOAD.");
                argument = code[++i];
                // create a (LOAD, index, stack_ptr)
                mlir::IntegerAttr index_attr = builder.getIntegerAttr(i8, argument);
                mlir::stackIR::LoadOp::create(builder, loc, index_attr, stack_ptr_attr);
                stack_ptr++;
                break;
            }

            case 0x02: { // STORE
                // fail if STORE is called when stack pointer is at 0 (stack underflow)
                if (stack_ptr <= 0)
                    throw std::runtime_error("Stack underflow at bytecode offset " + std::to_string(i) + ", STORE.");
                // read argument byte for STORE opcode
                if (i + 1 >= code.size())
                    throw std::runtime_error("Expected argument for STORE at position " + std::to_string(i) + ", STORE.");
                argument = code[++i];
                // fail if it stores beyond the 64-word output buffer
                if (argument >= 64)
                    throw std::runtime_error("Invalid store index " + std::to_string(argument) + " at bytecode offset " + std::to_string(i - 1) + ", STORE. Valid range is 0-63.");
                // create a (STORE, index, stack_ptr)
                mlir::IntegerAttr store_index_attr = builder.getIntegerAttr(i8, argument);
                mlir::stackIR::StoreOp::create(builder, loc, store_index_attr, stack_ptr_attr);
                break;
            }

            case 0x03: // POP
                // fail if POP is called when stack pointer is at 0 (stack underflow)
                if (stack_ptr <= 0)
                    throw std::runtime_error("Stack underflow at bytecode offset " + std::to_string(i) + ", POP.");
                // create a (POP, stack_ptr)
                mlir::stackIR::PopOp::create(builder, loc, stack_ptr_attr);
                stack_ptr--;
                break;

            case 0x04: // ADD
                // fail if ADD is called when stack pointer is below 1 (stack underflow)
                if (stack_ptr <= 1)
                    throw std::runtime_error("Stack underflow at bytecode offset " + std::to_string(i) + ", ADD.");
                // create a (ADD, stack_ptr)
                mlir::stackIR::AddOp::create(builder, loc, stack_ptr_attr);
                stack_ptr--;
                break;

            case 0x05: // SUB
                // fail if SUB is called when stack pointer is below 1 (stack underflow)
                if (stack_ptr <= 1)
                    throw std::runtime_error("Stack underflow at bytecode offset " + std::to_string(i) + ", SUB.");
                // create a (SUB, stack_ptr)
                mlir::stackIR::SubOp::create(builder, loc, stack_ptr_attr);
                stack_ptr--;
                break;

            case 0x06: // DUP
                // fail if DUP is called when stack pointer is at 0 (stack underflow)
                if (stack_ptr <= 0)
                    throw std::runtime_error("Stack underflow at bytecode offset " + std::to_string(i) + ", DUP.");
                // create a (DUP, stack_ptr)
                mlir::stackIR::DupOp::create(builder, loc, stack_ptr_attr);
                stack_ptr++;
                break;

            default:
                throw std::runtime_error("Unknown opcode: 0x" + std::to_string(operation) + " at bytecode offset " + std::to_string(i) + ".");
        }
        if (stack_ptr > stack_size)
            stack_size = stack_ptr;
        if (stop)
            break;
    }
    if (!stop)
        throw std::runtime_error("No STOP opcode found in bytecode.");
    // set the stack size attribute on the module
    module->setAttr("stackIR.stack_size", builder.getI32IntegerAttr(stack_size));

    // return the module
    return module;
}

}