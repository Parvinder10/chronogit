#include "chronogit/merge/merge_engine.hpp"
#include "chronogit/repo/repository.hpp"
#include "chronogit/merge/lca.hpp"
#include "chronogit/merge/three_way_merge.hpp"
#include "chronogit/storage/tree_builder.hpp"
#include "chronogit/core/atomic_file.hpp"
#include "chronogit/core/error.hpp"
#include <set>
#include <sstream>

namespace chronogit {

MergeEngine::MergeEngine(Repository& repo) : repo_(repo) {}

bool MergeEngine::is_in_merge_state() const {
    return AtomicFile::exists(repo_.git_dir() / "MERGE_HEAD");
}

std::optional<ObjectId> MergeEngine::get_merge_head() const {
    auto path = repo_.git_dir() / "MERGE_HEAD";
    if (!AtomicFile::exists(path)) return std::nullopt;
    std::string text = AtomicFile::read_text(path);
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' ')) text.pop_back();
    if (is_valid_object_id(text)) return text;
    return std::nullopt;
}

void MergeEngine::clear_merge_state() {
    AtomicFile::remove(repo_.git_dir() / "MERGE_HEAD");
    AtomicFile::remove(repo_.git_dir() / "MERGE_MSG");
}

void MergeEngine::abort_merge() {
    if (!is_in_merge_state()) {
        throw ChronoGitException("fatal: There is no merge to abort (MERGE_HEAD missing).");
    }
    clear_merge_state();
    auto cur_head = repo_.branch_manager().get_head_commit_id();
    if (cur_head) {
        repo_.checkout(*cur_head, true);
    }
}

MergeResult MergeEngine::merge(const std::string& target_branch_or_commit) {
    if (is_in_merge_state()) {
        throw ChronoGitException("fatal: Merging is not possible because you have unmerged files.\n"
                                 "Please resolve conflicts and commit, or run 'chronogit merge --abort'.");
    }

    auto status = repo_.status();
    if (!status.unstaged_modified.empty() || !status.unstaged_deleted.empty() ||
        !status.staged_new.empty() || !status.staged_modified.empty() || !status.staged_deleted.empty()) {
        throw UncommittedChangesException("Your local changes would be overwritten by merge. Please commit or stash them.");
    }

    std::optional<std::string> target_branch;
    ObjectId target_commit_id;

    if (repo_.branch_manager().branch_exists(target_branch_or_commit)) {
        target_branch = target_branch_or_commit;
        auto cid = repo_.branch_manager().get_branch_commit_id(target_branch_or_commit);
        if (!cid) {
            throw ChronoGitException("fatal: target branch has no commits: " + target_branch_or_commit);
        }
        target_commit_id = *cid;
    } else if (repo_.db().has_object(target_branch_or_commit)) {
        target_commit_id = target_branch_or_commit;
    } else {
        throw BranchNotFoundException(target_branch_or_commit);
    }

    auto head_commit_id = repo_.branch_manager().get_head_commit_id();
    if (!head_commit_id) {
        throw ChronoGitException("fatal: current branch has no commits yet");
    }

    auto lca_opt = LowestCommonAncestor::find_lca(repo_.db(), *head_commit_id, target_commit_id);
    if (!lca_opt) {
        throw ChronoGitException("fatal: refusing to merge unrelated histories");
    }

    ObjectId lca_id = *lca_opt;

    // Already up to date?
    if (lca_id == target_commit_id) {
        return MergeResult{
            .status = MergeResultStatus::AlreadyUpToDate,
            .commit_id = *head_commit_id,
            .message = "Already up to date.",
            .conflict_files = {}
        };
    }

    std::string current_branch_name = repo_.branch_manager().get_current_branch().value_or("HEAD");

    // Fast-Forward?
    if (lca_id == *head_commit_id) {
        repo_.checkout(target_branch ? *target_branch : target_commit_id, false);
        return MergeResult{
            .status = MergeResultStatus::FastForward,
            .commit_id = target_commit_id,
            .message = "Fast-forward merge to " + target_commit_id,
            .conflict_files = {}
        };
    }

    // Three-Way Merge
    auto base_commit = repo_.db().read_commit(lca_id);
    auto ours_commit = repo_.db().read_commit(*head_commit_id);
    auto theirs_commit = repo_.db().read_commit(target_commit_id);

    auto base_files = TreeBuilder::flatten_tree(repo_.db(), base_commit->tree_id());
    auto ours_files = TreeBuilder::flatten_tree(repo_.db(), ours_commit->tree_id());
    auto theirs_files = TreeBuilder::flatten_tree(repo_.db(), theirs_commit->tree_id());

    std::set<std::string> all_paths;
    for (const auto& [p, _] : base_files) all_paths.insert(p);
    for (const auto& [p, _] : ours_files) all_paths.insert(p);
    for (const auto& [p, _] : theirs_files) all_paths.insert(p);

    std::vector<std::string> conflict_files;
    std::map<std::string, std::string> merged_file_contents;
    std::vector<std::string> files_to_delete;

    for (const auto& path : all_paths) {
        auto it_b = base_files.find(path);
        auto it_o = ours_files.find(path);
        auto it_t = theirs_files.find(path);

        bool in_b = (it_b != base_files.end());
        bool in_o = (it_o != ours_files.end());
        bool in_t = (it_t != theirs_files.end());

        if (in_b && in_o && in_t) {
            // In all three
            if (it_o->second.id == it_t->second.id) {
                // Both made identical change or no change
                auto blob = repo_.db().read_blob(it_o->second.id);
                merged_file_contents[path] = std::string(blob->as_string_view());
            } else if (it_b->second.id == it_o->second.id) {
                // Unchanged in Ours, changed in Theirs
                auto blob = repo_.db().read_blob(it_t->second.id);
                merged_file_contents[path] = std::string(blob->as_string_view());
            } else if (it_b->second.id == it_t->second.id) {
                // Changed in Ours, unchanged in Theirs
                auto blob = repo_.db().read_blob(it_o->second.id);
                merged_file_contents[path] = std::string(blob->as_string_view());
            } else {
                // Both changed differently: run line merge
                auto b_blob = repo_.db().read_blob(it_b->second.id);
                auto o_blob = repo_.db().read_blob(it_o->second.id);
                auto t_blob = repo_.db().read_blob(it_t->second.id);

                auto res = ThreeWayMerge::merge(b_blob->as_string_view(),
                                               o_blob->as_string_view(),
                                               t_blob->as_string_view(),
                                               "HEAD",
                                               target_branch.value_or(target_commit_id.substr(0, 7)));
                merged_file_contents[path] = res.merged_content;
                if (res.has_conflict) {
                    conflict_files.push_back(path);
                }
            }
        } else if (in_b && in_o && !in_t) {
            // Deleted in Theirs
            if (it_b->second.id == it_o->second.id) {
                // Clean delete
                files_to_delete.push_back(path);
            } else {
                // Modified in Ours, deleted in Theirs: conflict!
                conflict_files.push_back(path);
                auto o_blob = repo_.db().read_blob(it_o->second.id);
                merged_file_contents[path] = std::string(o_blob->as_string_view());
            }
        } else if (in_b && !in_o && in_t) {
            // Deleted in Ours
            if (it_b->second.id == it_t->second.id) {
                // Clean delete
                files_to_delete.push_back(path);
            } else {
                // Deleted in Ours, modified in Theirs: conflict!
                conflict_files.push_back(path);
                auto t_blob = repo_.db().read_blob(it_t->second.id);
                merged_file_contents[path] = std::string(t_blob->as_string_view());
            }
        } else if (!in_b && in_o && !in_t) {
            // Added in Ours only
            auto o_blob = repo_.db().read_blob(it_o->second.id);
            merged_file_contents[path] = std::string(o_blob->as_string_view());
        } else if (!in_b && !in_o && in_t) {
            // Added in Theirs only
            auto t_blob = repo_.db().read_blob(it_t->second.id);
            merged_file_contents[path] = std::string(t_blob->as_string_view());
        } else if (!in_b && in_o && in_t) {
            // Added in both
            if (it_o->second.id == it_t->second.id) {
                auto o_blob = repo_.db().read_blob(it_o->second.id);
                merged_file_contents[path] = std::string(o_blob->as_string_view());
            } else {
                auto o_blob = repo_.db().read_blob(it_o->second.id);
                auto t_blob = repo_.db().read_blob(it_t->second.id);
                auto res = ThreeWayMerge::merge("",
                                               o_blob->as_string_view(),
                                               t_blob->as_string_view(),
                                               "HEAD",
                                               target_branch.value_or(target_commit_id.substr(0, 7)));
                merged_file_contents[path] = res.merged_content;
                conflict_files.push_back(path);
            }
        }
    }

    // Apply file deletions to workspace and index
    for (const auto& path : files_to_delete) {
        auto disk_path = repo_.work_dir() / path;
        AtomicFile::remove(disk_path);
        repo_.index().remove(path);
    }

    // Apply merged contents to workspace
    for (const auto& [path, content] : merged_file_contents) {
        auto disk_path = repo_.work_dir() / path;
        AtomicFile::write_text(disk_path, content);
    }

    if (!conflict_files.empty()) {
        // Record merge head so repository knows it is in merge conflict state
        AtomicFile::write_text(repo_.git_dir() / "MERGE_HEAD", target_commit_id + "\n");
        std::string merge_msg = "Merge branch '" + target_branch.value_or(target_commit_id.substr(0, 7)) + "'\n";
        AtomicFile::write_text(repo_.git_dir() / "MERGE_MSG", merge_msg);

        // Stage non-conflicting files
        std::vector<std::string> clean_paths;
        for (const auto& [path, _] : merged_file_contents) {
            if (std::find(conflict_files.begin(), conflict_files.end(), path) == conflict_files.end()) {
                clean_paths.push_back(path);
            }
        }
        if (!clean_paths.empty()) {
            repo_.add(clean_paths);
        }

        return MergeResult{
            .status = MergeResultStatus::Conflict,
            .commit_id = "",
            .message = "Automatic merge failed; fix conflicts and then commit the result.",
            .conflict_files = conflict_files
        };
    }

    // Clean merge! Add all merged files to index and create merge commit
    std::vector<std::string> all_merged_paths;
    for (const auto& [p, _] : merged_file_contents) all_merged_paths.push_back(p);
    if (!all_merged_paths.empty()) {
        repo_.add(all_merged_paths);
    }

    std::string commit_msg = "Merge branch '" + target_branch.value_or(target_commit_id.substr(0, 7)) + "'";
    ObjectId merge_commit_id = repo_.commit(commit_msg, "", "", {target_commit_id});

    clear_merge_state();

    return MergeResult{
        .status = MergeResultStatus::Clean,
        .commit_id = merge_commit_id,
        .message = "Merge made by the 'three-way' strategy.",
        .conflict_files = {}
    };
}

} // namespace chronogit
