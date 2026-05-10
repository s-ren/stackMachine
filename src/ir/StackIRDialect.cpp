#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/Bytecode/BytecodeOpInterface.h"

#define GET_DIALECT_INFO
#include "ir/StackIRDialect.h.inc"
#include "ir/StackIRDialect.cpp.inc"

#define GET_OP_CLASSES
#include "ir/StackIROps.h.inc"
#define GET_OP_CLASSES
#include "ir/StackIROps.cpp.inc"

void mlir::stackIR::StackIRDialect::initialize() {
    addOperations<
        mlir::stackIR::StopOp,
        mlir::stackIR::LoadOp,
        mlir::stackIR::StoreOp,
        mlir::stackIR::PopOp,
        mlir::stackIR::AddOp,
        mlir::stackIR::SubOp,
        mlir::stackIR::DupOp
    >();
}
