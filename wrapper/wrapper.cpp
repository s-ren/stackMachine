#include <cassert>
#include <cstring>
#include <fstream>
#include <ios>
#include <memory>

extern "C" void f(unsigned char* in, unsigned char* out);

int main(int const argc, char const *argv[])
{
    unsigned const out_size = 32 * 64; // 64 words

    std::unique_ptr<unsigned char[]> in;
    std::unique_ptr<unsigned char[]> out;

    {
        std::ifstream in_file{argv[1], std::ios::binary | std::ios::ate};
        auto const in_size = in_file.tellg();
        assert(in_size > 0);
        assert(in_size % 32 == 0);
        in_file.seekg(0, std::ios::beg);
        in.reset(new unsigned char[in_size]);
        in_file.read(reinterpret_cast<char*>(in.get()), in_size);
        assert(in_file.gcount() == in_size);
    }

    out.reset(new unsigned char[out_size]);
    std::memset(out.get(), 0, out_size);

    f(in.get(), out.get());

    {
        std::ofstream out_file(argv[2], std::ios::binary);
        out_file.write(reinterpret_cast<const char*>(out.get()), out_size);
    }

    return 0;
}