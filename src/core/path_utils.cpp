#include "chronogit/core/path_utils.hpp"
#include <algorithm>

namespace chronogit {

std::string PathUtils::normalize(const std::filesystem::path& path) {
    std::string s = path.generic_string();
    // Remove leading "./"
    while (s.rfind("./", 0) == 0) {
        s = s.substr(2);
    }
    // Remove trailing '/'
    while (s.length() > 1 && s.back() == '/') {
        s.pop_back();
    }
    if (s == ".") return "";
    return s;
}

bool PathUtils::is_safe_relative_path(std::string_view path) {
    if (path.empty()) return true;
    if (path.front() == '/' || path.front() == '\\') return false;
    // Check for drive letter on Windows (e.g. "C:")
    if (path.length() >= 2 && path[1] == ':') return false;

    // Check components for ".."
    size_t start = 0;
    while (start < path.length()) {
        size_t next = path.find_first_of("/\\", start);
        if (next == std::string_view::npos) next = path.length();
        std::string_view comp = path.substr(start, next - start);
        if (comp == "..") return false;
        start = next + 1;
    }
    return true;
}

std::string PathUtils::to_repo_relative(const std::filesystem::path& repo_root, const std::filesystem::path& target) {
    std::error_code ec;
    auto canon_root = std::filesystem::canonical(repo_root, ec);
    if (ec) canon_root = std::filesystem::absolute(repo_root);

    auto canon_target = std::filesystem::canonical(target, ec);
    if (ec) canon_target = std::filesystem::absolute(target);

    auto rel = std::filesystem::relative(canon_target, canon_root, ec);
    if (ec || rel.empty()) return normalize(target);
    return normalize(rel);
}

bool PathUtils::match_pattern(std::string_view pattern, std::string_view path) {
    if (pattern.empty()) return path.empty();

    // If pattern ends with '/', it only matches directories or files inside that directory
    if (pattern.back() == '/') {
        std::string_view dir_prefix = pattern.substr(0, pattern.length() - 1);
        if (path == dir_prefix) return true;
        if (path.rfind(std::string(dir_prefix) + "/", 0) == 0) return true;
    }

    size_t p_idx = 0;
    size_t s_idx = 0;
    size_t star_p = std::string_view::npos;
    size_t match_s = 0;

    while (s_idx < path.length()) {
        if (p_idx < pattern.length() && (pattern[p_idx] == path[s_idx] || pattern[p_idx] == '?')) {
            p_idx++;
            s_idx++;
        } else if (p_idx < pattern.length() && pattern[p_idx] == '*') {
            star_p = p_idx++;
            match_s = s_idx;
        } else if (star_p != std::string_view::npos) {
            p_idx = star_p + 1;
            match_s++;
            s_idx = match_s;
        } else {
            return false;
        }
    }

    while (p_idx < pattern.length() && pattern[p_idx] == '*') {
        p_idx++;
    }

    return p_idx == pattern.length();
}

bool PathUtils::is_ignored(std::string_view rel_path, const std::vector<std::string>& ignore_patterns) {
    if (rel_path.empty()) return false;

    // Hardcoded internal ignores
    if (rel_path == ".chronogit" || rel_path.rfind(".chronogit/", 0) == 0) return true;
    if (rel_path == ".git" || rel_path.rfind(".git/", 0) == 0) return true;

    // Extract filename component
    size_t last_slash = rel_path.find_last_of('/');
    std::string_view filename = (last_slash == std::string_view::npos) ? rel_path : rel_path.substr(last_slash + 1);

    for (const auto& pat : ignore_patterns) {
        if (pat.empty() || pat[0] == '#') continue;
        if (match_pattern(pat, rel_path)) return true;
        if (match_pattern(pat, filename)) return true;
    }

    return false;
}

} // namespace chronogit
