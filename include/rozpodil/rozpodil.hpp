#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace rozpodil {

struct Substring {
    std::size_t start = 0;
    std::size_t stop = 0;
    std::string_view text;

    friend bool operator==(const Substring&, const Substring&) = default;
};

std::vector<Substring> sentenize(std::string_view text);
std::vector<Substring> tokenize(std::string_view text);

} // namespace rozpodil
