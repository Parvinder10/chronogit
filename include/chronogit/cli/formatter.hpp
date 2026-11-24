#pragma once

#include <string>
#include <string_view>
#include <cstdint>

namespace chronogit {

class Formatter {
public:
    static bool is_color_enabled();
    static void set_color_enabled(bool enabled);

    static std::string red(std::string_view text);
    static std::string green(std::string_view text);
    static std::string yellow(std::string_view text);
    static std::string blue(std::string_view text);
    static std::string magenta(std::string_view text);
    static std::string cyan(std::string_view text);
    static std::string bold(std::string_view text);
    static std::string dim(std::string_view text);

    static std::string format_size(uint64_t bytes);
    static std::string format_timestamp(int64_t timestamp, std::string_view tz_offset = "+0000");
};

} // namespace chronogit
