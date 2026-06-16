#include "rozpodil/rozpodil.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void usage(std::ostream& out)
{
    out << "Usage: rozpodil [options] <file>\n"
        << "Options:\n"
        << "  --tokens       Tokenize instead of sentence-splitting.\n"
        << "  -h, --help     Show this message.\n";
}

std::string read_file(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open " + path);
    }
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

} // namespace

int main(int argc, char** argv)
{
    bool tokens = false;
    std::string path;

    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "-h" || arg == "--help") {
                usage(std::cout);
                return 0;
            }
            if (arg == "--tokens") {
                tokens = true;
            } else if (arg.starts_with('-')) {
                throw std::runtime_error("unknown option: " + arg);
            } else if (path.empty()) {
                path = arg;
            } else {
                throw std::runtime_error("unexpected argument: " + arg);
            }
        }
        if (path.empty()) {
            usage(std::cerr);
            return 1;
        }

        const auto text = read_file(path);
        const auto chunks = tokens ? rozpodil::tokenize(text) : rozpodil::split_sentences(text);
        for (const auto& chunk : chunks) {
            std::cout << chunk.text << '\n';
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n\n";
        usage(std::cerr);
        return 1;
    }
}
