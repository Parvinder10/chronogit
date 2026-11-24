#pragma once

#include "chronogit/core/types.hpp"
#include "chronogit/storage/object_db.hpp"
#include "chronogit/storage/index.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <set>
#include <map>

namespace chronogit {

struct WorkingFile {
    std::string rel_path;
    uint64_t file_size = 0;
    uint64_t mtime_sec = 0;
    uint64_t mtime_nsec = 0;
    FileMode mode = FileMode::Regular;
};

struct RepoStatus {
    std::string branch_name;
    bool is_detached = false;
    ObjectId head_commit;

    std::vector<std::string> staged_new;
    std::vector<std::string> staged_modified;
    std::vector<std::string> staged_deleted;

    std::vector<std::string> unstaged_modified;
    std::vector<std::string> unstaged_deleted;

    std::vector<std::string> untracked;

    bool is_clean() const {
        return staged_new.empty() && staged_modified.empty() && staged_deleted.empty() &&
               unstaged_modified.empty() && unstaged_deleted.empty() && untracked.empty();
    }
};

class WorkspaceScanner {
public:
    explicit WorkspaceScanner(std::filesystem::path work_dir);

    // Loads .chronoignore rules
    std::vector<std::string> load_ignore_patterns() const;

    // Scans workspace directory respecting ignore rules
    std::map<std::string, WorkingFile> scan_workspace() const;

    // Calculates repository status given the current Index, ObjectDb, and HEAD commit
    RepoStatus compute_status(const ObjectDb& db,
                              const Index& index,
                              const std::optional<ObjectId>& head_commit_id,
                              const std::optional<std::string>& current_branch) const;

    const std::filesystem::path& work_dir() const { return work_dir_; }

private:
    std::filesystem::path work_dir_;
};

} // namespace chronogit
