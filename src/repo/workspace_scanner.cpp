#include "chronogit/repo/workspace_scanner.hpp"
#include "chronogit/core/path_utils.hpp"
#include "chronogit/core/atomic_file.hpp"
#include "chronogit/storage/tree_builder.hpp"
#include "chronogit/core/sha256.hpp"
#include <fstream>
#include <algorithm>

namespace chronogit {

WorkspaceScanner::WorkspaceScanner(std::filesystem::path work_dir) : work_dir_(std::move(work_dir)) {}

std::vector<std::string> WorkspaceScanner::load_ignore_patterns() const {
    std::vector<std::string> patterns;
    auto ignore_file = work_dir_ / ".chronoignore";
    if (AtomicFile::exists(ignore_file)) {
        std::ifstream ifs(ignore_file);
        std::string line;
        while (std::getline(ifs, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            if (!line.empty() && line[0] != '#') {
                patterns.push_back(line);
            }
        }
    }
    return patterns;
}

std::map<std::string, WorkingFile> WorkspaceScanner::scan_workspace() const {
    std::map<std::string, WorkingFile> files;
    auto patterns = load_ignore_patterns();

    std::error_code ec;
    std::filesystem::recursive_directory_iterator iter(
        work_dir_,
        std::filesystem::directory_options::skip_permission_denied,
        ec
    );
    std::filesystem::recursive_directory_iterator end;

    while (iter != end) {
        const auto& entry = *iter;
        std::string rel_path = PathUtils::to_repo_relative(work_dir_, entry.path());

        // Skip ignored directories without descending into them!
        if (entry.is_directory()) {
            if (PathUtils::is_ignored(rel_path, patterns)) {
                iter.disable_recursion_pending();
            }
            iter.increment(ec);
            continue;
        }

        if (entry.is_regular_file()) {
            if (!PathUtils::is_ignored(rel_path, patterns)) {
                WorkingFile wf;
                wf.rel_path = rel_path;
                wf.file_size = entry.file_size(ec);

                auto lwt = entry.last_write_time(ec);
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    lwt - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
                );
                auto duration = sctp.time_since_epoch();
                wf.mtime_sec = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
                wf.mtime_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() % 1000000000;
                wf.mode = FileMode::Regular;

                files[rel_path] = std::move(wf);
            }
        }

        iter.increment(ec);
    }

    return files;
}

RepoStatus WorkspaceScanner::compute_status(const ObjectDb& db,
                                           const Index& index,
                                           const std::optional<ObjectId>& head_commit_id,
                                           const std::optional<std::string>& current_branch) const {
    RepoStatus status;
    status.branch_name = current_branch.value_or("detached HEAD");
    status.is_detached = !current_branch.has_value();
    status.head_commit = head_commit_id.value_or("");

    std::map<std::string, TreeEntry> head_files;
    if (head_commit_id && !head_commit_id->empty()) {
        try {
            auto commit = db.read_commit(*head_commit_id);
            head_files = TreeBuilder::flatten_tree(db, commit->tree_id());
        } catch (...) {
            // Unborn branch or missing commit
        }
    }

    // 1. Staged vs HEAD
    for (const auto& [path, entry] : index.entries()) {
        auto it = head_files.find(path);
        if (it == head_files.end()) {
            status.staged_new.push_back(path);
        } else if (it->second.id != entry.id || it->second.mode != entry.mode) {
            status.staged_modified.push_back(path);
        }
    }

    for (const auto& [path, head_entry] : head_files) {
        if (!index.find(path)) {
            status.staged_deleted.push_back(path);
        }
    }

    // 2. Working tree vs Index
    auto work_files = scan_workspace();

    for (const auto& [path, entry] : index.entries()) {
        auto it = work_files.find(path);
        if (it == work_files.end()) {
            status.unstaged_deleted.push_back(path);
        } else {
            const auto& wf = it->second;
            bool content_changed = false;

            // Stat check first
            if (wf.file_size != entry.file_size || wf.mtime_sec != entry.mtime_sec) {
                // Content check by reading and hashing blob
                try {
                    auto disk_path = work_dir_ / path;
                    auto content = AtomicFile::read_binary(disk_path);
                    Blob disk_blob(content);
                    ObjectId disk_id = disk_blob.compute_id();
                    if (disk_id != entry.id) {
                        content_changed = true;
                    }
                } catch (...) {
                    content_changed = true;
                }
            }

            if (content_changed) {
                status.unstaged_modified.push_back(path);
            }
        }
    }

    // 3. Untracked
    for (const auto& [path, wf] : work_files) {
        if (!index.find(path)) {
            status.untracked.push_back(path);
        }
    }

    std::sort(status.staged_new.begin(), status.staged_new.end());
    std::sort(status.staged_modified.begin(), status.staged_modified.end());
    std::sort(status.staged_deleted.begin(), status.staged_deleted.end());
    std::sort(status.unstaged_modified.begin(), status.unstaged_modified.end());
    std::sort(status.unstaged_deleted.begin(), status.unstaged_deleted.end());
    std::sort(status.untracked.begin(), status.untracked.end());

    return status;
}

} // namespace chronogit
