#include <string>
#include <vector>
#include <cstdint>

using string = std::string;

namespace parser {
    string parse(std::vector<uint8_t> code);
}