#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace normalize_uk_cpp::detail {

inline char32_t decode_one(std::string_view text, std::size_t pos, std::size_t& next)
{
    const auto b0 = static_cast<unsigned char>(text[pos]);
    if (b0 < 0x80) {
        next = pos + 1;
        return b0;
    }
    if ((b0 & 0xE0) == 0xC0 && pos + 1 < text.size()) {
        next = pos + 2;
        return ((b0 & 0x1F) << 6) | (static_cast<unsigned char>(text[pos + 1]) & 0x3F);
    }
    if ((b0 & 0xF0) == 0xE0 && pos + 2 < text.size()) {
        next = pos + 3;
        return ((b0 & 0x0F) << 12) | ((static_cast<unsigned char>(text[pos + 1]) & 0x3F) << 6) |
               (static_cast<unsigned char>(text[pos + 2]) & 0x3F);
    }
    if ((b0 & 0xF8) == 0xF0 && pos + 3 < text.size()) {
        next = pos + 4;
        return ((b0 & 0x07) << 18) | ((static_cast<unsigned char>(text[pos + 1]) & 0x3F) << 12) |
               ((static_cast<unsigned char>(text[pos + 2]) & 0x3F) << 6) |
               (static_cast<unsigned char>(text[pos + 3]) & 0x3F);
    }
    next = pos + 1;
    return b0;
}

inline void append_utf8(std::string& out, char32_t cp)
{
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

} // namespace normalize_uk_cpp::detail
