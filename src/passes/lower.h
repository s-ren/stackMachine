#pragma once
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"

namespace lower {

// Lowers a StackIR ModuleOp into an LLVM-dialect ModuleOp.
// Both modules share the same MLIRContext.
mlir::ModuleOp lower(mlir::MLIRContext &context, mlir::ModuleOp stackIR);

} // namespace lower
