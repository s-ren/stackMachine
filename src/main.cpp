// mlir includes
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "llvm/Support/raw_ostream.h"
#include "mlir/registerBuiltinDialectTranslation.h"

// project includes
#include "parser/parser.h"
#include "passes/lower.h"

// include standard library headers
#include <iostream>
#include <fstream>
#include <llvm-18/llvm/ADT/StringRef.h>
#include <stdexcept>
#include <vector>
#include "include/CLI11.hpp"

using string = std::string;

std::vector<uint8_t> read_file(const string &source_file_name) {
    std::ifstream input_file(source_file_name, std::ios::binary);
    if (!input_file.is_open()) {
        throw std::runtime_error("Could not open file " + source_file_name);
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(input_file),
                                std::istreambuf_iterator<char>());
}

// Parse a string of the form "0x010xcc0x00..." into bytes.
std::vector<uint8_t> parse_cmdline(const string &hex_str) {
    if (hex_str.size() % 2 != 0)
        throw std::runtime_error("Hex string length must be even");
    std::vector<uint8_t> result;
    for (size_t pos = 0; pos < hex_str.size(); pos += 2) {
        string byte_str = hex_str.substr(pos, 2);
        result.push_back(static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16)));
    }
    return result;
}

// Helper function to dump LLVM IR.
static int dumpLLVMIR(mlir::ModuleOp module) {
  // Register the translation to LLVM IR with the MLIR context.
  mlir::registerBuiltinDialectTranslation(*module->getContext());
  mlir::registerLLVMDialectTranslation(*module->getContext());

  // Convert the module to LLVM IR in a new LLVM IR context.
  llvm::LLVMContext llvmContext;
  auto llvmModule = mlir::translateModuleToLLVMIR(module, llvmContext);
  if (!llvmModule) {
    llvm::errs() << "Failed to emit LLVM IR\n";
    return -1;
  }

  // Initialize LLVM targets.
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();

  // Configure the LLVM Module
  auto tmBuilderOrError = llvm::orc::JITTargetMachineBuilder::detectHost();
  if (!tmBuilderOrError) {
    llvm::errs() << "Could not create JITTargetMachineBuilder\n";
    return -1;
  }

  auto tmOrError = tmBuilderOrError->createTargetMachine();
  if (!tmOrError) {
    llvm::errs() << "Could not create TargetMachine\n";
    return -1;
  }
  mlir::ExecutionEngine::setupTargetTripleAndDataLayout(llvmModule.get(),
                                                        tmOrError.get().get());

  /// Optionally run an optimization pipeline over the llvm module.
  auto optPipeline = mlir::makeOptimizingTransformer(/*optLevel=*/3, /*sizeLevel=*/0,/*targetMachine=*/nullptr);
  if (auto err = optPipeline(llvmModule.get())) {
    llvm::errs() << "Failed to optimize LLVM IR " << err << "\n";
    return -1;
  }
  llvm::errs() << *llvmModule << "\n";
  return 0;
}


int main(int argc, char *argv[]) {
    CLI::App app{"Stack Compiler"};
    argv = app.ensure_utf8(argv);

    string source_file_name;
    string cmdline_hex;
    string target_file_name;

    auto *opt_i = app.add_option("-i,--input", source_file_name, "Input source file");
    auto *opt_c = app.add_option("-c,--cmdline", cmdline_hex,
                                 "Inline bytecode as hex string, e.g. 0001cc00");
    opt_i->excludes(opt_c);
    opt_c->excludes(opt_i);
    app.add_option("-o,--output", target_file_name, "Output binary file (default: stdout)");
    app.require_option(1, 2); // at least one of -i/-c must be given

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    }

    try {
    std::vector<uint8_t> input;
    if (!source_file_name.empty()) {
        std::cout << "Reading source file..." << std::endl;
        input = read_file(source_file_name);
    } else {
        std::cout << "Parsing inline bytecode..." << std::endl;
        input = parse_cmdline(cmdline_hex);
    }

    // parse the input file to generate the IR
    std::cout << "Generating StackIR..."<< std::endl;
    mlir::MLIRContext context;
    mlir::ModuleOp stackIR_module = parser::parse(context, input);

    // print the MLIR module
    stackIR_module.print(llvm::outs());
    llvm::outs() << "\n";

    // compile IR into binary
    std::cout << "Generating LLVMIR..." << std::endl;
    lower::Lowerer lowerer(context);
    mlir::ModuleOp llvm_module = lowerer.lower(stackIR_module);

    // print the lowered LLVM dialect module
    llvm_module.print(llvm::outs());
    llvm::outs() << "\n";

    // lower to LLVM
    std::cout << "Generating actual LLVM..." << std::endl;
    dumpLLVMIR(llvm_module);

  // Convert the module to LLVM IR in a new LLVM IR context.
  llvm::LLVMContext llvmContext;
  auto llvmModule = mlir::translateModuleToLLVMIR(module, llvmContext);
  if (!llvmModule) {
    llvm::errs() << "Failed to emit LLVM IR\n";
    return -1;
  }



    if (!llvm_ir) {
        throw std::runtime_error("Failed to translate MLIR module to LLVM IR");
    }

    // optimize and emit object code

    // write to output file
    if (!target_file_name.empty()) {
        std::ofstream output_file(target_file_name, std::ios::binary);
        if (!output_file.is_open()) {
            std::cerr << "Error: Could not open file " << target_file_name << std::endl;
            return 1;
        }
        // TODO: write compiled output to output_file
        std::cout << "Writing output to file..." << std::endl;
    } else {
        // TODO: write compiled output to std::cout
        std::cout << "Writing output to stdout..." << std::endl;
    }

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}