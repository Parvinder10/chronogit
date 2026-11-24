#include "chronogit/repo/repository.hpp"
#include "chronogit/core/atomic_file.hpp"
#include "chronogit/core/path_utils.hpp"
#include "chronogit/core/error.hpp"
#include "chronogit/storage/tree_builder.hpp"
#include <queue>
#include <set>
#include <fstream>
#include <algorithm>

namespace chronogit {

Repository::Repository(std::filesystem::path work_dir, std::filesystem::path git_dir)
    : work_dir_(std::move(work_dir)),
      git_dir_(std::move(git_dir)),
      db_(git_dir_ / "objects"),
      branch_manager_(git_dir_),
      scanner_(work_dir_),
      checkout_engine_(work_dir_, db_, index_, branch_manager_, scanner_) {
    index_.load(git_dir_ / "index");
}

Repository Repository::init(const std::filesystem::path& work_dir, const std::string& default_branch) {
    auto git_dir = work_dir / ".chronogit";
    std::error_code ec;
    std::filesystem::create_directories(git_dir / "objects", ec);
    std::filesystem::create_directories(git_dir / "refs" / "heads", ec);

    // Initial HEAD pointing to default branch
    std::string head_content = "ref: refs/heads/" + default_branch + "\n";
    AtomicFile::write_text(git_dir / "HEAD", head_content);

    // Config file
    std::string config_content = "[core]\n\trepositoryformatversion = 0\n\tfilemode = true\n\tbare = false\n";
    AtomicFile::write_text(git_dir / "config", config_content);

    // Description file
    std::string desc = "Unnamed ChronoGit repository; edit this file to name the repository.\n";
    AtomicFile::write_text(git_dir / "description", desc);

    return Repository(work_dir, git_dir);
}

Repository Repository::find_repo(const std::filesystem::path& start_path) {
    std::error_code ec;
    auto cur = std::filesystem::canonical(start_path, ec);
    if (ec) cur = std::filesystem::absolute(start_path);

    while (true) {
        auto candidate = cur / ".chronogit";
        if (std::filesystem::is_directory(candidate, ec)) {
            return Repository(cur, candidate);
        }
        if (!cur.has_parent_path() || cur.parent_path() == cur) {
            break;
        }
        cur = cur.parent_path();
    }

    throw RepositoryNotFoundException(start_path.string());
}

void Repository::add(const std::vector<std::string>& pathspecs) {
    auto work_files = scanner_.scan_workspace();

    for (const auto& spec : pathspecs) {
        std::string norm_spec = PathUtils::normalize(spec);
        bool matched = false;

        if (norm_spec.empty() || norm_spec == ".") {
            // Add all non-ignored workspace files
            for (const auto& [rel_path, wf] : work_files) {
                auto disk_path = work_dir_ / rel_path;
                auto content = AtomicFile::read_binary(disk_path);
                Blob blob(std::move(content));
                ObjectId blob_id = db_.write_object(blob);

                IndexEntry ie;
                ie.path = rel_path;
                ie.id = blob_id;
                ie.mode = wf.mode;
                ie.file_size = wf.file_size;
                ie.mtime_sec = wf.mtime_sec;
                ie.mtime_nsec = wf.mtime_nsec;
                index_.add_or_update(std::move(ie));
                matched = true;
            }
        } else {
            // Match specific path or prefix
            for (const auto& [rel_path, wf] : work_files) {
                if (rel_path == norm_spec || rel_path.rfind(norm_spec + "/", 0) == 0 ||
                    PathUtils::match_pattern(norm_spec, rel_path)) {
                    auto disk_path = work_dir_ / rel_path;
                    auto content = AtomicFile::read_binary(disk_path);
                    Blob blob(std::move(content));
                    ObjectId blob_id = db_.write_object(blob);

                    IndexEntry ie;
                    ie.path = rel_path;
                    ie.id = blob_id;
                    ie.mode = wf.mode;
                    ie.file_size = wf.file_size;
                    ie.mtime_sec = wf.mtime_sec;
                    ie.mtime_nsec = wf.mtime_nsec;
                    index_.add_or_update(std::move(ie));
                    matched = true;
                }
            }

            // Also check for removal of deleted files from index
            if (!matched) {
                // If the path was tracked in index but removed from workspace
                if (index_.find(norm_spec)) {
                    index_.remove(norm_spec);
                    matched = true;
                }
            }
        }

        if (!matched) {
            throw ChronoGitException("pathspec '" + spec + "' did not match any files");
        }
    }

    index_.save(git_dir_ / "index");
}

ObjectId Repository::commit(const std::string& message,
                            const std::string& author_name,
                            const std::string& author_email,
                            const std::vector<ObjectId>& extra_parents) {
    if (message.empty()) {
        throw ChronoGitException("Aborting commit due to empty commit message.");
    }

    // Build hierarchical trees from current index
    ObjectId tree_id = TreeBuilder::build_tree_from_index(db_, index_);

    // Gather parents
    std::vector<ObjectId> parents;
    auto head_commit_id = branch_manager_.get_head_commit_id();
    if (head_commit_id && !head_commit_id->empty()) {
        parents.push_back(*head_commit_id);
    }
    for (const auto& p : extra_parents) {
        if (std::find(parents.begin(), parents.end(), p) == parents.end()) {
            parents.push_back(p);
        }
    }

    // Check if anything changed compared to head commit (unless merge commit)
    if (head_commit_id && parents.size() == 1) {
        auto parent_commit = db_.read_commit(*head_commit_id);
        if (parent_commit->tree_id() == tree_id) {
            throw ChronoGitException("nothing to commit, working tree clean");
        }
    }

    Signature author = (author_name.empty()) ? Signature::now() : Signature::now(author_name, author_email);
    Signature committer = author;

    Commit commit_obj(tree_id, parents, author, committer, message);
    ObjectId commit_id = db_.write_object(commit_obj);

    // Advance branch ref or detached HEAD
    branch_manager_.update_head_ref(commit_id);

    return commit_id;
}

RepoStatus Repository::status() const {
    return scanner_.compute_status(db_, index_, branch_manager_.get_head_commit_id(), branch_manager_.get_current_branch());
}

void Repository::checkout(const std::string& target, bool force) {
    checkout_engine_.checkout(target, force);
}

void Repository::create_branch(const std::string& name) {
    auto head_id = branch_manager_.get_head_commit_id();
    if (!head_id) {
        throw ChronoGitException("fatal: not a valid object name: 'HEAD'");
    }
    branch_manager_.create_branch(name, *head_id);
}

void Repository::delete_branch(const std::string& name) {
    branch_manager_.delete_branch(name);
}

std::vector<BranchInfo> Repository::list_branches() const {
    return branch_manager_.list_branches();
}

std::vector<std::pair<ObjectId, Commit>> Repository::get_commit_history(const ObjectId& start_id, size_t limit) const {
    std::vector<std::pair<ObjectId, Commit>> history;
    if (start_id.empty() || !db_.has_object(start_id)) {
        return history;
    }

    std::queue<ObjectId> queue;
    std::set<ObjectId> visited;

    queue.push(start_id);
    visited.insert(start_id);

    while (!queue.empty()) {
        if (limit > 0 && history.size() >= limit) break;

        ObjectId current_id = queue.front();
        queue.pop();

        try {
            auto commit = db_.read_commit(current_id);
            for (const auto& parent : commit->parent_ids()) {
                if (visited.find(parent) == visited.end()) {
                    visited.insert(parent);
                    queue.push(parent);
                }
            }
            history.push_back({current_id, *commit});
        } catch (...) {
            // Handle dangling commit gracefully
        }
    }

    return history;
}

} // namespace chronogit
