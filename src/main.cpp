// mlir includes
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Target/LLVMIR/Export.h"
#include "mlir/Target/LLVMIR/Dialect/LLVMIR/LLVMToLLVMIRTranslation.h"
#include "mlir/Target/LLVMIR/Dialect/Builtin/BuiltinToLLVMIRTranslation.h"

// llvm includes
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/LegacyPassManager.h"

// project includes
#include "parser/parser.h"
#include "passes/lower.h"

// standard library
#include <filesystem>
#include <iostream>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include "include/CLI11.hpp"

#ifndef STACK_HOST_CXX
#define STACK_HOST_CXX "c++"
#endif

#ifndef STACK_WRAPPER_SOURCE
#define STACK_WRAPPER_SOURCE "wrapper/wrapper.cpp"
#endif

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

static std::filesystem::path outputBaseFor(const std::filesystem::path &outputPath) {
    const std::string ext = outputPath.extension().string();
    if (ext == ".o" || ext == ".out" || ext == ".exe")
        return outputPath.parent_path() / outputPath.stem();
    return outputPath;
}

static std::filesystem::path objectPathFor(const std::filesystem::path &outputBase) {
    return std::filesystem::path(outputBase.string() + ".o");
}

static std::filesystem::path executablePathFor(const std::filesystem::path &outputBase) {
#ifdef _WIN32
    return std::filesystem::path(outputBase.string() + ".exe");
#else
    return std::filesystem::path(outputBase.string() + ".out");
#endif
}

static void linkExecutable(const std::filesystem::path &objectPath,
                           const std::filesystem::path &outputPath) {
    const std::filesystem::path wrapperPath{STACK_WRAPPER_SOURCE};
    if (!std::filesystem::exists(wrapperPath))
        throw std::runtime_error("Wrapper source not found: " + wrapperPath.string());

    auto compilerPathOrErr = llvm::sys::findProgramByName(STACK_HOST_CXX);
    if (!compilerPathOrErr)
        throw std::runtime_error("Failed to find host compiler: " + std::string(STACK_HOST_CXX));

    const std::string compilerPath = *compilerPathOrErr;
    std::vector<std::string> argStorage{
        compilerPath,
        "-std=c++17",
        wrapperPath.string(),
        objectPath.string(),
        "-o",
        outputPath.string(),
    };
    std::vector<llvm::StringRef> args;
    args.reserve(argStorage.size());
    for (const std::string &arg : argStorage)
        args.emplace_back(arg);

    std::string errMsg;
    bool executionFailed = false;
    int rc = llvm::sys::ExecuteAndWait(compilerPath, args, std::nullopt, {}, 0, 0,
                                       &errMsg, &executionFailed);
    if (executionFailed || rc != 0) {
        throw std::runtime_error(
            "Link step failed" + (errMsg.empty() ? std::string{} : ": " + errMsg));
    }
}

// Translate MLIR LLVM dialect → native object file (or just optimized IR if no output path).
static void emitObjectCode(mlir::ModuleOp mlirModule,
                           const std::filesystem::path &outputPath) {
    // Register dialect → LLVM IR translation hooks
    mlir::registerBuiltinDialectTranslation(*mlirModule->getContext());
    mlir::registerLLVMDialectTranslation(*mlirModule->getContext());

    // Translate MLIR LLVM dialect → llvm::Module
    llvm::LLVMContext llvmCtx;
    auto llvmModule = mlir::translateModuleToLLVMIR(mlirModule, llvmCtx);
    if (!llvmModule)
        throw std::runtime_error("Failed to translate MLIR module to LLVM IR");

    // Set up native target
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    llvm::Triple triple(llvm::sys::getDefaultTargetTriple());
    std::string error;
    const llvm::Target *target = llvm::TargetRegistry::lookupTarget(triple, error);
    if (!target)
        throw std::runtime_error("Target lookup failed: " + error);

    // target machine
    auto tm = std::unique_ptr<llvm::TargetMachine>(target->createTargetMachine(
        triple, llvm::sys::getHostCPUName(), "", {}, std::nullopt));
    if (!tm)
        throw std::runtime_error("Failed to create target machine");
    llvmModule->setTargetTriple(triple);
    llvmModule->setDataLayout(tm->createDataLayout());

    // Optimize with new pass manager (O2)
    llvm::PassBuilder pb(tm.get());
    llvm::LoopAnalysisManager lam;
    llvm::FunctionAnalysisManager fam;
    llvm::CGSCCAnalysisManager cgam;
    llvm::ModuleAnalysisManager mam;
    pb.registerModuleAnalyses(mam);
    pb.registerCGSCCAnalyses(cgam);
    pb.registerFunctionAnalyses(fam);
    pb.registerLoopAnalyses(lam);
    pb.crossRegisterProxies(lam, fam, cgam, mam);
    auto mpm = pb.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
    mpm.run(*llvmModule, mam);

    // Print optimized LLVM IR
    llvmModule->print(llvm::outs(), nullptr);
    llvm::outs() << "\n";

    // Emit object file (legacy pass manager is required for addPassesToEmitFile)
    if (!outputPath.empty()) {
        std::error_code ec;
        llvm::raw_fd_ostream dest(outputPath.string(), ec, llvm::sys::fs::OF_None);
        if (ec)
            throw std::runtime_error("Cannot open output file: " + ec.message());
        llvm::legacy::PassManager codegenPM;
        if (tm->addPassesToEmitFile(codegenPM, dest, nullptr,
                                    llvm::CodeGenFileType::ObjectFile))
            throw std::runtime_error("Target cannot emit object files");
        codegenPM.run(*llvmModule);
        dest.flush();
    }
}


int main(int argc, char *argv[]) {
    // parse arg
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
    app.add_option("-o,--output", target_file_name,
                   "Output basename; emits both <name>.o and linked executable");
    app.require_option(1, 2); // at least one of -i/-c must be given
    
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    }
    // determine output paths
    std::filesystem::path outputBase;
    std::filesystem::path objectPath;
    std::filesystem::path executablePath;
    if (!target_file_name.empty()) {
        outputBase = outputBaseFor(std::filesystem::path(target_file_name));
        objectPath = objectPathFor(outputBase);
        executablePath = executablePathFor(outputBase);
    }

    try {
        // read source file
        std::vector<uint8_t> input;
        if (!source_file_name.empty()) {
            std::cout << "Reading source file..." << std::endl;
            input = read_file(source_file_name);
        } else {
            std::cout << "Parsing inline bytecode..." << std::endl;
            input = parse_cmdline(cmdline_hex);
        }

        // parse the source to generate the IR
        std::cout << "Generating StackIR..." << std::endl;
        mlir::MLIRContext context;
        mlir::ModuleOp stackIR_module = parser::parse(context, input);

        // print the StackIR
        stackIR_module.print(llvm::outs());
        llvm::outs() << "\n";

        // lower StackIR to MLIR LLVM dialect
        std::cout << "Generating LLVM dialect..." << std::endl;
        lower::Lowerer lowerer(context);
        mlir::ModuleOp llvm_module = lowerer.lower(stackIR_module);

        // print the lowered LLVM dialect module
        llvm_module.print(llvm::outs());
        llvm::outs() << "\n";

        std::cout << "Optimizing LLVM IR and emitting object code..." << std::endl;
        emitObjectCode(llvm_module, objectPath);

        // emit linked executable if output path is given
        if (target_file_name.empty()) {
            std::cout << "No output file requested." << std::endl;
        } else {
            std::cout << "Wrote object file to " << objectPath << std::endl;
            std::cout << "Linking executable..." << std::endl;
            linkExecutable(objectPath, executablePath);
            std::cout << "Wrote executable to " << executablePath << std::endl;
        }
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}