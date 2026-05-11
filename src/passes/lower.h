#pragma once
#include "mlir/Dialect/LLVMIR/LLVMTypes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/MLIRContext.h"

namespace lower {

class Lowerer {
public:
    explicit Lowerer(mlir::MLIRContext &context);
    mlir::ModuleOp lower(mlir::ModuleOp stackIR_module);

private:
    mlir::MLIRContext           &context;
    mlir::OpBuilder              builder;
    mlir::Location               loc;
    mlir::IntegerType            i8;
    mlir::IntegerType            i256;
    mlir::LLVM::LLVMPointerType  ptr;
    mlir::LLVM::LLVMVoidType     voidT;

    mlir::Value ci8(int8_t v);
    mlir::Value gep256(mlir::Value base, mlir::Value idx);
    mlir::Value load(mlir::Value pointer);
    void        store(mlir::Value value, mlir::Value pointer);
    void        ret();
};

} // namespace lower
