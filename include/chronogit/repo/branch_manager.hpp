#pragma once

#include "chronogit/core/types.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <optional>

namespace chronogit {

struct BranchInfo {
    std::string name;
    ObjectId commit_id;
    bool is_current = false;
};

class BranchManager {
public:
    explicit BranchManager(std::filesystem::path git_dir);

    // Creates new branch pointing to given commit. Throws if already exists.
    void create_branch(const std::string& name, const ObjectId& commit_id);

    // Deletes branch. Throws if not found or if deleting current active branch.
    void delete_branch(const std::string& name);

    // Lists all branches, indicating which one is active.
    std::vector<BranchInfo> list_branches() const;

    // Checks if branch exists.
    bool branch_exists(const std::string& name) const;

    // Gets commit ID pointed by branch.
    std::optional<ObjectId> get_branch_commit_id(const std::string& name) const;

    // Gets active branch name (nullopt if in detached HEAD state).
    std::optional<std::string> get_current_branch() const;

    // Resolves HEAD to a commit ID (nullopt if unborn branch).
    std::optional<ObjectId> get_head_commit_id() const;

    // Sets HEAD to symbolic branch reference ("ref: refs/heads/<name>").
    void set_head_symbolic(const std::string& branch_name);

    // Sets HEAD to detached commit ID.
    void set_head_detached(const ObjectId& commit_id);

    // Updates whatever HEAD points to (branch or detached commit).
    void update_head_ref(const ObjectId& commit_id);

    // Updates a specific branch ref.
    void set_branch_commit_id(const std::string& name, const ObjectId& commit_id);

    const std::filesystem::path& git_dir() const { return git_dir_; }
    std::filesystem::path heads_dir() const { return git_dir_ / "refs" / "heads"; }
    std::filesystem::path head_file() const { return git_dir_ / "HEAD"; }

private:
    std::filesystem::path git_dir_;
};

} // namespace chronogit
