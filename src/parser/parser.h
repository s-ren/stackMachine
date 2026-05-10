#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"

#include <string>
#include <vector>
#include <cstdint>

using string = std::string;

namespace parser {
    mlir::ModuleOp parse(mlir::MLIRContext &context, std::vector<uint8_t> code);
}