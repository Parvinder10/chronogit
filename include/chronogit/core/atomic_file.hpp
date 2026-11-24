#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <span>

namespace chronogit {

class AtomicFile {
public:
    static void write_binary(const std::filesystem::path& path, std::span<const uint8_t> data);
    static void write_text(const std::filesystem::path& path, std::string_view content);
    static std::vector<uint8_t> read_binary(const std::filesystem::path& path);
    static std::string read_text(const std::filesystem::path& path);
    static bool exists(const std::filesystem::path& path);
    static void remove(const std::filesystem::path& path);
};

} // namespace chronogit
