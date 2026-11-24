#include "chronogit/repo/branch_manager.hpp"
#include "chronogit/core/atomic_file.hpp"
#include "chronogit/core/error.hpp"
#include <algorithm>
#include <sstream>

namespace chronogit {

namespace {
std::string trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) {
        s.remove_suffix(1);
    }
    return std::string(s);
}

bool is_valid_branch_name(std::string_view name) {
    if (name.empty()) return false;
    if (name == "." || name == "..") return false;
    if (name.find("..") != std::string_view::npos) return false;
    for (char c : name) {
        if (c == ' ' || c == '~' || c == '^' || c == ':' || c == '?' || c == '*' || c == '[' || c == '\\') {
            return false;
        }
    }
    return true;
}
} // namespace

BranchManager::BranchManager(std::filesystem::path git_dir) : git_dir_(std::move(git_dir)) {
    std::error_code ec;
    std::filesystem::create_directories(heads_dir(), ec);
}

void BranchManager::create_branch(const std::string& name, const ObjectId& commit_id) {
    if (!is_valid_branch_name(name)) {
        throw ChronoGitException("fatal: '" + name + "' is not a valid branch name");
    }
    if (branch_exists(name)) {
        throw ChronoGitException("fatal: a branch named '" + name + "' already exists");
    }
    set_branch_commit_id(name, commit_id);
}

void BranchManager::delete_branch(const std::string& name) {
    if (!branch_exists(name)) {
        throw BranchNotFoundException(name);
    }
    auto current = get_current_branch();
    if (current && *current == name) {
        throw ChronoGitException("error: cannot delete the branch '" + name + "' which you are currently on");
    }
    AtomicFile::remove(heads_dir() / name);
}

bool BranchManager::branch_exists(const std::string& name) const {
    return AtomicFile::exists(heads_dir() / name);
}

std::optional<ObjectId> BranchManager::get_branch_commit_id(const std::string& name) const {
    auto path = heads_dir() / name;
    if (!AtomicFile::exists(path)) return std::nullopt;
    std::string text = trim(AtomicFile::read_text(path));
    if (is_valid_object_id(text)) {
        return text;
    }
    return std::nullopt;
}

std::optional<std::string> BranchManager::get_current_branch() const {
    if (!AtomicFile::exists(head_file())) return std::nullopt;
    std::string content = trim(AtomicFile::read_text(head_file()));
    constexpr std::string_view prefix = "ref: refs/heads/";
    if (content.rfind(prefix, 0) == 0) {
        return content.substr(prefix.length());
    }
    return std::nullopt; // Detached HEAD
}

std::optional<ObjectId> BranchManager::get_head_commit_id() const {
    if (!AtomicFile::exists(head_file())) return std::nullopt;
    std::string content = trim(AtomicFile::read_text(head_file()));
    constexpr std::string_view prefix = "ref: refs/heads/";
    if (content.rfind(prefix, 0) == 0) {
        std::string branch = content.substr(prefix.length());
        return get_branch_commit_id(branch);
    }
    if (is_valid_object_id(content)) {
        return content;
    }
    return std::nullopt;
}

void BranchManager::set_head_symbolic(const std::string& branch_name) {
    if (!is_valid_branch_name(branch_name)) {
        throw ChronoGitException("fatal: '" + branch_name + "' is not a valid branch name");
    }
    std::string content = "ref: refs/heads/" + branch_name + "\n";
    AtomicFile::write_text(head_file(), content);
}

void BranchManager::set_head_detached(const ObjectId& commit_id) {
    AtomicFile::write_text(head_file(), commit_id + "\n");
}

void BranchManager::set_branch_commit_id(const std::string& name, const ObjectId& commit_id) {
    AtomicFile::write_text(heads_dir() / name, commit_id + "\n");
}

void BranchManager::update_head_ref(const ObjectId& commit_id) {
    auto current_branch = get_current_branch();
    if (current_branch) {
        set_branch_commit_id(*current_branch, commit_id);
    } else {
        set_head_detached(commit_id);
    }
}

std::vector<BranchInfo> BranchManager::list_branches() const {
    std::vector<BranchInfo> result;
    auto current = get_current_branch();
    std::error_code ec;

    if (!std::filesystem::exists(heads_dir(), ec)) return result;

    for (const auto& entry : std::filesystem::directory_iterator(heads_dir(), ec)) {
        if (!entry.is_regular_file()) continue;
        std::string name = entry.path().filename().string();
        if (name.find(".tmp.") != std::string::npos) continue;

        auto cid = get_branch_commit_id(name);
        if (cid) {
            result.push_back(BranchInfo{
                .name = name,
                .commit_id = *cid,
                .is_current = (current && *current == name)
            });
        }
    }

    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.name < b.name;
    });

    return result;
}

} // namespace chronogit
