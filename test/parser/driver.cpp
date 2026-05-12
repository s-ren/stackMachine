#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "llvm/Support/raw_ostream.h"

#include "parser/parser.h"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int decodeHexDigit(char ch) {
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (ch >= 'a' && ch <= 'f')
        return 10 + (ch - 'a');
    throw std::runtime_error("Invalid hex digit");
}

std::vector<uint8_t> readEncodedBinaryFile(const std::string &path) {
    std::ifstream input(path);
    if (!input.is_open())
        throw std::runtime_error("Could not open file " + path);

    const std::string encoded((std::istreambuf_iterator<char>(input)),
                              std::istreambuf_iterator<char>());

    std::vector<uint8_t> decoded;
    for (size_t index = 0; index < encoded.size();) {
        const unsigned char ch = static_cast<unsigned char>(encoded[index]);
        if (std::isspace(ch)) {
            ++index;
            continue;
        }

        if (index + 3 >= encoded.size() || encoded[index] != '0' ||
            (encoded[index + 1] != 'x' && encoded[index + 1] != 'X')) {
            throw std::runtime_error(
                "Expected byte sequence in the form 0xNN in " + path);
        }

        const int hi = decodeHexDigit(encoded[index + 2]);
        const int lo = decodeHexDigit(encoded[index + 3]);
        decoded.push_back(static_cast<uint8_t>((hi << 4) | lo));
        index += 4;
    }

    return decoded;
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        llvm::errs() << "usage: " << argv[0] << " <input-binary>\n";
        return 1;
    }

    try {
        std::vector<uint8_t> input = readEncodedBinaryFile(argv[1]);
        mlir::MLIRContext context;
        mlir::ModuleOp module = parser::parse(context, input);
        module.print(llvm::outs(), mlir::OpPrintingFlags().printGenericOpForm());
        llvm::outs() << '\n';
        return 0;
    } catch (const std::exception &ex) {
        llvm::errs() << ex.what() << '\n';
        return 1;
    }
}