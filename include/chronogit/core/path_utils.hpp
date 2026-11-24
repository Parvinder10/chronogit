#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace chronogit {

class PathUtils {
public:
    // Normalizes paths to use forward slashes '/', removes './' and trailing slashes
    static std::string normalize(const std::filesystem::path& path);

    // Checks if relative path is safe (does not escape repo root with '../' or drive letters)
    static bool is_safe_relative_path(std::string_view path);

    // Computes relative path from repo root, normalized with forward slashes
    static std::string to_repo_relative(const std::filesystem::path& repo_root, const std::filesystem::path& target);

    // Matches path against simple glob pattern (*, ?, directory suffixes)
    static bool match_pattern(std::string_view pattern, std::string_view path);

    // Checks if path should be ignored according to patterns (.chronogit/, .git/, build/, etc.)
    static bool is_ignored(std::string_view rel_path, const std::vector<std::string>& ignore_patterns);
};

} // namespace chronogit
