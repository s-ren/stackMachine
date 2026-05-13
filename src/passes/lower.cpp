#include "passes/lower.h"

#include "mlir/Dialect/LLVMIR/LLVMDialect.h"


#define GET_DIALECT_INFO
#include "ir/StackIRDialect.h.inc"
#define GET_OP_CLASSES
#include "ir/StackIROps.h.inc"

namespace lower {

// Loads the LLVM dialect and returns the context reference.
// Used in the member initializer list so the dialect is registered
// before LLVMPointerType/LLVMVoidType are constructed.
static mlir::MLIRContext& initLLVM(mlir::MLIRContext& ctx) {
    ctx.getOrLoadDialect<mlir::LLVM::LLVMDialect>();
    return ctx;
}

// ── Constructor ───────────────────────────────────────────────────────────────
Lowerer::Lowerer(mlir::MLIRContext &context)
    : context(initLLVM(context)),
      builder(&context),
      loc(mlir::UnknownLoc::get(&context)),
      i8(mlir::IntegerType::get(&context, 8)),
    i64(mlir::IntegerType::get(&context, 64)),
      i256(mlir::IntegerType::get(&context, 256)),
      ptr(mlir::LLVM::LLVMPointerType::get(&context)),
      voidT(mlir::LLVM::LLVMVoidType::get(&context))
{
}

// ── Private helpers ───────────────────────────────────────────────────────────
// Creates [load i256, ptr %pointer]
mlir::Value Lowerer::load(mlir::Value pointer) {
    return builder.create<mlir::LLVM::LoadOp>(loc, i256, pointer);
}

// Creates [store i256 %value, ptr %pointer]
void Lowerer::store(mlir::Value value, mlir::Value pointer) {
    builder.create<mlir::LLVM::StoreOp>(loc, value, pointer);
}

// Creates [ret void]
void Lowerer::ret() {
    builder.create<mlir::LLVM::ReturnOp>(loc, mlir::ValueRange{});
}

// Creates [i64 v]
mlir::Value Lowerer::ci64(uint64_t v) {
    return builder.create<mlir::LLVM::ConstantOp>(
        loc, i64, builder.getIntegerAttr(i64, v));
}

// Creates [getelementptr i256, ptr %base, i64 %idx]
mlir::Value Lowerer::gep256(mlir::Value base, mlir::Value idx) {
    return builder.create<mlir::LLVM::GEPOp>(
        loc, ptr, i256, base, mlir::ValueRange{idx});
}

// lowering pass
mlir::ModuleOp Lowerer::lower(mlir::ModuleOp stackIR_module) {
    // read stack size from module attribute
    uint64_t stack_size = mlir::cast<mlir::IntegerAttr>(
        stackIR_module->getAttr("stackIR.stack_size")).getValue().getZExtValue();

    // create llvm module
    auto llvm_module = mlir::ModuleOp::create(loc);
    builder.setInsertionPointToEnd(llvm_module.getBody());

    // create function definition: void f(i8* in, i8* out)
    auto funcType = mlir::LLVM::LLVMFunctionType::get(voidT, {ptr, ptr});
    auto func = builder.create<mlir::LLVM::LLVMFuncOp>(loc, "f", funcType);
    
    // create entry block for the function and set insertion point
    auto *entry = func.addEntryBlock();
    builder.setInsertionPointToStart(entry);
    mlir::Value in  = entry->getArgument(0);
    mlir::Value out = entry->getArgument(1);

    // create [alloca i256, i64 stack_size]
    mlir::Value stack = builder.create<mlir::LLVM::AllocaOp>(loc, ptr, i256, ci64(stack_size));
        
    // lower each StackIR op to LLVM dialect ops
    for (auto &op : stackIR_module.getBodyRegion().front().getOperations()) {
        std::string opName = op.getName().getStringRef().str();
        if (opName == "stackIR.stop") {
            // STOP, p ->
            // ret void
            ret();

        } else if (opName == "stackIR.load") {
            uint64_t i = mlir::cast<mlir::IntegerAttr>(op.getAttr("index")).getValue().getZExtValue();
            uint64_t p = mlir::cast<mlir::IntegerAttr>(op.getAttr("stack_ptr")).getValue().getZExtValue();
            // LOAD i, p ->
            // %in_ptr = getelementptr i256, ptr %in, i64 i
            auto in_ptr    = gep256(in, ci64(i));
            // %val = load i256, ptr %in_ptr
            auto val       = load(in_ptr);
            // %stack_ptr = getelementptr i256, ptr %stack, i64 p
            auto stack_ptr = gep256(stack, ci64(p));
            // store i256 %val, ptr %stack_ptr
            store(val, stack_ptr);

        } else if (opName == "stackIR.store") {
            uint64_t i = mlir::cast<mlir::IntegerAttr>(op.getAttr("index")).getValue().getZExtValue();
            uint64_t p = mlir::cast<mlir::IntegerAttr>(op.getAttr("stack_ptr")).getValue().getZExtValue();
            // STORE i, p ->
            // %stack_ptr = getelementptr i256, ptr %stack, i64 (p-1)
            auto stack_ptr = gep256(stack, ci64(p - 1));
            // %val = load i256, ptr %stack_ptr
            auto val       = load(stack_ptr);
            // %out_ptr = getelementptr i256, ptr %out, i64 i
            auto out_ptr   = gep256(out, ci64(i));
            // store i256 %val, ptr %out_ptr
            store(val, out_ptr);

        } else if (opName == "stackIR.pop") {
            // POP, p ->
            // noop

        } else if (opName == "stackIR.add") {
            uint64_t p = mlir::cast<mlir::IntegerAttr>(op.getAttr("stack_ptr")).getValue().getZExtValue();
            // ADD, p ->
            // %stack_ptr_1 = getelementptr i256, ptr %stack, i64 (p-2)
            auto stack_ptr_1 = gep256(stack, ci64(p - 2));
            // %val1 = load i256, ptr %stack_ptr_1
            auto val1        = load(stack_ptr_1);
            // %stack_ptr_2 = getelementptr i256, ptr %stack, i64 (p-1)
            auto stack_ptr_2 = gep256(stack, ci64(p - 1));
            // %val2 = load i256, ptr %stack_ptr_2
            auto val2        = load(stack_ptr_2);
            // %val = add i256 %val1, %val2
            auto val         = builder.create<mlir::LLVM::AddOp>(loc, i256, val1, val2);
            // %stack_ptr = getelementptr i256, ptr %stack, i64 (p-2)
            auto stack_ptr   = gep256(stack, ci64(p - 2));
            // store i256 %val, ptr %stack_ptr
            store(val, stack_ptr);

        } else if (opName == "stackIR.sub") {
            uint64_t p = mlir::cast<mlir::IntegerAttr>(op.getAttr("stack_ptr")).getValue().getZExtValue();
            // SUB, p ->
            // %stack_ptr_1 = getelementptr i256, ptr %stack, i64 (p-1)
            auto stack_ptr_1 = gep256(stack, ci64(p - 1));
            // %val1 = load i256, ptr %stack_ptr_1
            auto val1        = load(stack_ptr_1);
            // %stack_ptr_2 = getelementptr i256, ptr %stack, i64 (p-2)
            auto stack_ptr_2 = gep256(stack, ci64(p - 2));
            // %val2 = load i256, ptr %stack_ptr_2
            auto val2        = load(stack_ptr_2);
            // %val = sub i256 %val1, %val2
            auto val         = builder.create<mlir::LLVM::SubOp>(loc, i256, val1, val2);
            // %stack_ptr = getelementptr i256, ptr %stack, i64 (p-2)
            auto stack_ptr   = gep256(stack, ci64(p - 2));
            // store i256 %val, ptr %stack_ptr
            store(val, stack_ptr);

        } else if (opName == "stackIR.dup") {
            uint64_t p = mlir::cast<mlir::IntegerAttr>(op.getAttr("stack_ptr")).getValue().getZExtValue();
            // DUP, p ->
            // %stack_ptr = getelementptr i256, ptr %stack, i64 (p-1)
            auto stack_ptr_src = gep256(stack, ci64(p - 1));
            // %val = load i256, ptr %stack_ptr
            auto val           = load(stack_ptr_src);
            // %stack_ptr = getelementptr i256, ptr %stack, i64 p
            auto stack_ptr_dst = gep256(stack, ci64(p));
            // store i256 %val, ptr %stack_ptr
            store(val, stack_ptr_dst);
        }
        // module terminator and unknown ops are skipped
    }

    return llvm_module;
}

}
