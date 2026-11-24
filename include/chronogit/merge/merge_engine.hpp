#pragma once

#include "chronogit/core/types.hpp"
#include <string>
#include <vector>
#include <optional>

namespace chronogit {

class Repository;

struct MergeResult {
    MergeResultStatus status = MergeResultStatus::Clean;
    ObjectId commit_id;
    std::string message;
    std::vector<std::string> conflict_files;
};

class MergeEngine {
public:
    explicit MergeEngine(Repository& repo);

    // Merges target branch (or commit) into current branch
    MergeResult merge(const std::string& target_branch_or_commit);

    // Checks if repository is currently in a conflicted merge state (MERGE_HEAD exists)
    bool is_in_merge_state() const;

    // Gets the target commit ID from MERGE_HEAD
    std::optional<ObjectId> get_merge_head() const;

    // Aborts ongoing merge, reverting workspace to HEAD
    void abort_merge();

    // Cleans MERGE_HEAD state file
    void clear_merge_state();

private:
    Repository& repo_;
};

} // namespace chronogit
