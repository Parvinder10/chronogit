#include "chronogit/core/atomic_file.hpp"
#include "chronogit/core/error.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <fstream>
#include <random>
#include <chrono>

namespace chronogit {

namespace {
std::filesystem::path make_temp_path(const std::filesystem::path& target) {
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    static thread_local std::mt19937_64 rng(static_cast<uint64_t>(now));
    uint64_t r = rng();
    return target.string() + ".tmp." + std::to_string(r);
}

void atomic_replace(const std::filesystem::path& temp_path, const std::filesystem::path& target_path) {
#ifdef _WIN32
    std::wstring wtemp = temp_path.wstring();
    std::wstring wtarget = target_path.wstring();
    if (!MoveFileExW(wtemp.c_str(), wtarget.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::error_code ec;
        std::filesystem::remove(temp_path, ec);
        throw ChronoGitException("Failed to atomically replace file: " + target_path.string() +
                                 " (Windows error " + std::to_string(GetLastError()) + ")");
    }
#else
    std::error_code ec;
    std::filesystem::rename(temp_path, target_path, ec);
    if (ec) {
        std::filesystem::remove(temp_path);
        throw ChronoGitException("Failed to atomically rename file to " + target_path.string() + ": " + ec.message());
    }
#endif
}
} // namespace

void AtomicFile::write_binary(const std::filesystem::path& path, std::span<const uint8_t> data) {
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    auto temp = make_temp_path(path);
    {
        std::ofstream ofs(temp, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            throw ChronoGitException("Failed to open temporary file for writing: " + temp.string());
        }
        if (!data.empty()) {
            ofs.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        }
        ofs.flush();
        if (!ofs.good()) {
            ofs.close();
            std::filesystem::remove(temp);
            throw ChronoGitException("I/O error while writing to temporary file: " + temp.string());
        }
    }

    atomic_replace(temp, path);
}

void AtomicFile::write_text(const std::filesystem::path& path, std::string_view content) {
    std::span<const uint8_t> data(reinterpret_cast<const uint8_t*>(content.data()), content.size());
    write_binary(path, data);
}

std::vector<uint8_t> AtomicFile::read_binary(const std::filesystem::path& path) {
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        throw ChronoGitException("Cannot open file for reading: " + path.string());
    }
    auto size = ifs.tellg();
    if (size < 0) {
        throw ChronoGitException("Failed to determine size of file: " + path.string());
    }
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    ifs.seekg(0, std::ios::beg);
    if (size > 0) {
        ifs.read(reinterpret_cast<char*>(buffer.data()), size);
        if (!ifs.good()) {
            throw ChronoGitException("I/O error reading file: " + path.string());
        }
    }
    return buffer;
}

std::string AtomicFile::read_text(const std::filesystem::path& path) {
    auto bin = read_binary(path);
    return std::string(reinterpret_cast<const char*>(bin.data()), bin.size());
}

bool AtomicFile::exists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

void AtomicFile::remove(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

} // namespace chronogit
