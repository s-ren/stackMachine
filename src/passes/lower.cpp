#include "passes/lower.h"

#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMTypes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"

// StackIR dialect ops
#define GET_DIALECT_INFO
#include "ir/StackIRDialect.h.inc"
#define GET_OP_CLASSES
#include "ir/StackIROps.h.inc"

namespace lower {

mlir::ModuleOp lower(mlir::MLIRContext &context, mlir::ModuleOp stackIR) {
    context.getOrLoadDialect<mlir::LLVM::LLVMDialect>();

    // Read the stack size computed by the parser.
    auto stack_size_attr = stackIR->getAttr("stackIR.stack_size");
    int32_t stack_size =
        mlir::cast<mlir::IntegerAttr>(stack_size_attr).getInt();

    auto loc = mlir::UnknownLoc::get(&context);
    auto llvm_module = mlir::ModuleOp::create(loc);

    mlir::OpBuilder builder(&context);
    builder.setInsertionPointToEnd(llvm_module.getBody());

    // ── Types ────────────────────────────────────────────────────────────────
    auto i32   = mlir::IntegerType::get(&context, 32);
    auto i256  = mlir::IntegerType::get(&context, 256);
    auto ptr   = mlir::LLVM::LLVMPointerType::get(&context);
    auto voidT = mlir::LLVM::LLVMVoidType::get(&context);

    // ── Function: void @main(ptr %in, ptr %out) ──────────────────────────────
    auto funcType = mlir::LLVM::LLVMFunctionType::get(voidT, {ptr, ptr});
    auto func = builder.create<mlir::LLVM::LLVMFuncOp>(loc, "main", funcType);

    auto *entry = func.addEntryBlock();
    builder.setInsertionPointToStart(entry);

    mlir::Value in_ptr  = entry->getArgument(0); // %in
    mlir::Value out_ptr = entry->getArgument(1); // %out

    // ── Stack allocation: alloca i256, i32 stack_size ────────────────────────
    mlir::Value size_val = builder.create<mlir::LLVM::ConstantOp>(
        loc, i32, builder.getI32IntegerAttr(stack_size));
    mlir::Value stack = builder.create<mlir::LLVM::AllocaOp>(
        loc, ptr, i256, size_val);

    // ── Helpers ───────────────────────────────────────────────────────────────
    // Create a compile-time i32 constant.
    auto ci32 = [&](int32_t v) -> mlir::Value {
        return builder.create<mlir::LLVM::ConstantOp>(
            loc, i32, builder.getI32IntegerAttr(v));
    };
    // GEP into a flat i256 array.
    auto gep256 = [&](mlir::Value base, mlir::Value idx) -> mlir::Value {
        return builder.create<mlir::LLVM::GEPOp>(
            loc, ptr, i256, base, mlir::ValueRange{idx});
    };

    // ── Walk StackIR ops and emit LLVM dialect ops ───────────────────────────
    for (auto &op : stackIR.getBody()->getOperations()) {

        if (auto o = mlir::dyn_cast<mlir::stackIR::LoadOp>(op)) {
            // LOAD i, p  →  val = in[i];  stack[p] = val
            int32_t p = o.getStackPtr(), i = (int32_t)o.getIndex();
            auto val = builder.create<mlir::LLVM::LoadOp>(
                loc, i256, gep256(in_ptr, ci32(i)));
            builder.create<mlir::LLVM::StoreOp>(
                loc, val, gep256(stack, ci32(p)));

        } else if (auto o = mlir::dyn_cast<mlir::stackIR::StoreOp>(op)) {
            // STORE i, p  →  val = stack[p-1];  out[i] = val
            int32_t p = o.getStackPtr(), i = (int32_t)o.getIndex();
            auto val = builder.create<mlir::LLVM::LoadOp>(
                loc, i256, gep256(stack, ci32(p - 1)));
            builder.create<mlir::LLVM::StoreOp>(
                loc, val, gep256(out_ptr, ci32(i)));

        } else if (auto o = mlir::dyn_cast<mlir::stackIR::PopOp>(op)) {
            // POP  →  noop (stack pointer tracked statically)
            (void)o;

        } else if (auto o = mlir::dyn_cast<mlir::stackIR::AddOp>(op)) {
            // ADD, p  →  stack[p-2] = stack[p-2] + stack[p-1]
            int32_t p = o.getStackPtr();
            auto v1 = builder.create<mlir::LLVM::LoadOp>(
                loc, i256, gep256(stack, ci32(p - 2)));
            auto v2 = builder.create<mlir::LLVM::LoadOp>(
                loc, i256, gep256(stack, ci32(p - 1)));
            auto res = builder.create<mlir::LLVM::AddOp>(loc, i256, v1, v2);
            builder.create<mlir::LLVM::StoreOp>(
                loc, res, gep256(stack, ci32(p - 2)));

        } else if (auto o = mlir::dyn_cast<mlir::stackIR::SubOp>(op)) {
            // SUB, p  →  stack[p-2] = stack[p-1] - stack[p-2]
            int32_t p = o.getStackPtr();
            auto v1 = builder.create<mlir::LLVM::LoadOp>(
                loc, i256, gep256(stack, ci32(p - 1)));
            auto v2 = builder.create<mlir::LLVM::LoadOp>(
                loc, i256, gep256(stack, ci32(p - 2)));
            auto res = builder.create<mlir::LLVM::SubOp>(loc, i256, v1, v2);
            builder.create<mlir::LLVM::StoreOp>(
                loc, res, gep256(stack, ci32(p - 2)));

        } else if (auto o = mlir::dyn_cast<mlir::stackIR::DupOp>(op)) {
            // DUP, p  →  stack[p] = stack[p-1]
            int32_t p = o.getStackPtr();
            auto val = builder.create<mlir::LLVM::LoadOp>(
                loc, i256, gep256(stack, ci32(p - 1)));
            builder.create<mlir::LLVM::StoreOp>(
                loc, val, gep256(stack, ci32(p)));

        } else if (mlir::isa<mlir::stackIR::StopOp>(op)) {
            // STOP  →  ret void
            builder.create<mlir::LLVM::ReturnOp>(loc, mlir::ValueRange{});
            break;
        }
        // Other ops (e.g. implicit module terminator) are skipped.
    }

    return llvm_module;
}

} // namespace lower
