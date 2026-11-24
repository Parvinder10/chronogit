#pragma once

#include "chronogit/core/types.hpp"
#include "chronogit/storage/object_db.hpp"
#include "chronogit/diff/myers_diff.hpp"
#include <string>
#include <vector>
#include <memory>

namespace chronogit {

class Repository;

enum class ChangeType {
    Added,
    Deleted,
    Modified
};

struct FileDiff {
    std::string path;
    ChangeType change_type = ChangeType::Modified;
    ObjectId old_id;
    ObjectId new_id;
    std::vector<DiffHunk> hunks;
    bool is_binary = false;
};

class DiffEngine {
public:
    // Compares two strings / file contents
    static FileDiff diff_content(const std::string& path,
                                 std::string_view old_content,
                                 std::string_view new_content,
                                 ChangeType type = ChangeType::Modified);

    // Compares two trees
    static std::vector<FileDiff> diff_trees(const ObjectDb& db,
                                           const ObjectId& tree_a_id,
                                           const ObjectId& tree_b_id);

    // Compares two commits
    static std::vector<FileDiff> diff_commits(const ObjectDb& db,
                                             const ObjectId& commit_a_id,
                                             const ObjectId& commit_b_id);

    // Compares Index against working directory (unstaged changes)
    static std::vector<FileDiff> diff_index_to_workdir(const Repository& repo);

    // Compares HEAD against Index (staged changes)
    static std::vector<FileDiff> diff_head_to_index(const Repository& repo);

    // Formats a list of FileDiffs as unified diff string
    static std::string format_unified_diff(const std::vector<FileDiff>& diffs, bool use_color = false);

    // Formats a summary diffstat
    static std::string format_stat(const std::vector<FileDiff>& diffs);
};

} // namespace chronogit
