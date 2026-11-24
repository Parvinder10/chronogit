#include "chronogit/cli/formatter.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace chronogit {

namespace {
bool g_color_enabled = []() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return false;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return false;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
    return isatty(fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}();

std::string wrap(std::string_view code, std::string_view text) {
    if (!g_color_enabled) return std::string(text);
    return std::string("\033[") + std::string(code) + "m" + std::string(text) + "\033[0m";
}
} // namespace

bool Formatter::is_color_enabled() {
    return g_color_enabled;
}

void Formatter::set_color_enabled(bool enabled) {
    g_color_enabled = enabled;
}

std::string Formatter::red(std::string_view text) { return wrap("31", text); }
std::string Formatter::green(std::string_view text) { return wrap("32", text); }
std::string Formatter::yellow(std::string_view text) { return wrap("33", text); }
std::string Formatter::blue(std::string_view text) { return wrap("34", text); }
std::string Formatter::magenta(std::string_view text) { return wrap("35", text); }
std::string Formatter::cyan(std::string_view text) { return wrap("36", text); }
std::string Formatter::bold(std::string_view text) { return wrap("1", text); }
std::string Formatter::dim(std::string_view text) { return wrap("2", text); }

std::string Formatter::format_size(uint64_t bytes) {
    std::ostringstream oss;
    if (bytes < 1024) {
        oss << bytes << " B";
    } else if (bytes < 1024 * 1024) {
        oss << std::fixed << std::setprecision(1) << (bytes / 1024.0) << " KiB";
    } else {
        oss << std::fixed << std::setprecision(1) << (bytes / (1024.0 * 1024.0)) << " MiB";
    }
    return oss.str();
}

std::string Formatter::format_timestamp(int64_t timestamp, std::string_view tz_offset) {
    std::time_t t = static_cast<std::time_t>(timestamp);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y", &tm_buf);
    std::string s(buf);
    if (!tz_offset.empty()) {
        s += " " + std::string(tz_offset);
    }
    return s;
}

} // namespace chronogit
