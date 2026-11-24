#include "chronogit/repo/checkout_engine.hpp"
#include "chronogit/core/atomic_file.hpp"
#include "chronogit/core/error.hpp"
#include "chronogit/storage/tree_builder.hpp"
#include <sstream>

namespace chronogit {

CheckoutEngine::CheckoutEngine(std::filesystem::path work_dir,
                               ObjectDb& db,
                               Index& index,
                               BranchManager& branch_manager,
                               WorkspaceScanner& scanner)
    : work_dir_(std::move(work_dir)),
      db_(db),
      index_(index),
      branch_manager_(branch_manager),
      scanner_(scanner) {}

void CheckoutEngine::checkout(const std::string& target, bool force) {
    std::optional<std::string> target_branch;
    ObjectId target_commit_id;

    if (branch_manager_.branch_exists(target)) {
        target_branch = target;
        auto cid = branch_manager_.get_branch_commit_id(target);
        if (!cid) {
            throw ChronoGitException("fatal: branch '" + target + "' has no commits yet");
        }
        target_commit_id = *cid;
    } else if (db_.has_object(target)) {
        // Detached HEAD checkout
        target_commit_id = target;
    } else {
        throw BranchNotFoundException(target);
    }

    auto target_commit = db_.read_commit(target_commit_id);
    auto target_files = TreeBuilder::flatten_tree(db_, target_commit->tree_id());

    auto current_head_id = branch_manager_.get_head_commit_id();
    std::map<std::string, TreeEntry> current_head_files;
    if (current_head_id) {
        auto cur_commit = db_.read_commit(*current_head_id);
        current_head_files = TreeBuilder::flatten_tree(db_, cur_commit->tree_id());
    }

    if (!force) {
        auto status = scanner_.compute_status(db_, index_, current_head_id, branch_manager_.get_current_branch());
        std::vector<std::string> conflicting_files;

        // Check if unstaged modified or staged files would be overwritten
        for (const auto& f : status.unstaged_modified) {
            auto tgt_it = target_files.find(f);
            auto cur_it = current_head_files.find(f);
            ObjectId cur_id = (cur_it != current_head_files.end()) ? cur_it->second.id : "";
            ObjectId tgt_id = (tgt_it != target_files.end()) ? tgt_it->second.id : "";
            if (cur_id != tgt_id) {
                conflicting_files.push_back(f);
            }
        }

        // Check if untracked files would be overwritten
        for (const auto& f : status.untracked) {
            if (target_files.find(f) != target_files.end()) {
                conflicting_files.push_back(f);
            }
        }

        if (!conflicting_files.empty()) {
            std::ostringstream oss;
            oss << "The following files would be overwritten or removed by checkout:\n";
            for (const auto& cf : conflicting_files) {
                oss << "    " << cf << "\n";
            }
            oss << "Please commit your changes or discard them before switching branches.";
            throw UncommittedChangesException(oss.str());
        }
    }

    // Remove files in current index that are not in target_files
    std::vector<std::string> paths_to_remove;
    for (const auto& [path, entry] : index_.entries()) {
        if (target_files.find(path) == target_files.end()) {
            paths_to_remove.push_back(path);
        }
    }

    for (const auto& p : paths_to_remove) {
        auto full = work_dir_ / p;
        AtomicFile::remove(full);
        index_.remove(p);

        // Clean empty parent directories
        auto parent = full.parent_path();
        while (parent != work_dir_ && parent.string().length() > work_dir_.string().length()) {
            std::error_code ec;
            if (std::filesystem::is_empty(parent, ec)) {
                std::filesystem::remove(parent, ec);
                parent = parent.parent_path();
            } else {
                break;
            }
        }
    }

    // Write all files from target_files into workspace and update index
    for (const auto& [path, entry] : target_files) {
        auto blob = db_.read_blob(entry.id);
        auto dest = work_dir_ / path;
        AtomicFile::write_binary(dest, blob->data());

        std::error_code ec;
        auto fsize = std::filesystem::file_size(dest, ec);
        auto lwt = std::filesystem::last_write_time(dest, ec);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            lwt - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        auto duration = sctp.time_since_epoch();

        IndexEntry ie;
        ie.path = path;
        ie.id = entry.id;
        ie.mode = entry.mode;
        ie.file_size = fsize;
        ie.mtime_sec = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        ie.mtime_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() % 1000000000;

        index_.add_or_update(std::move(ie));
    }

    index_.save(branch_manager_.git_dir() / "index");

    if (target_branch) {
        branch_manager_.set_head_symbolic(*target_branch);
    } else {
        branch_manager_.set_head_detached(target_commit_id);
    }
}

} // namespace chronogit
