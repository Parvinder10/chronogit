#include "chronogit/diff/diff_engine.hpp"
#include "chronogit/repo/repository.hpp"
#include "chronogit/storage/tree_builder.hpp"
#include "chronogit/core/atomic_file.hpp"
#include <sstream>
#include <iomanip>
#include <set>

namespace chronogit {

namespace {
bool check_is_binary(std::string_view content) {
    size_t check_len = std::min<size_t>(content.size(), 8000);
    for (size_t i = 0; i < check_len; ++i) {
        if (content[i] == '\0') return true;
    }
    return false;
}
} // namespace

FileDiff DiffEngine::diff_content(const std::string& path,
                                  std::string_view old_content,
                                  std::string_view new_content,
                                  ChangeType type) {
    FileDiff fd;
    fd.path = path;
    fd.change_type = type;

    if (check_is_binary(old_content) || check_is_binary(new_content)) {
        fd.is_binary = true;
        return fd;
    }

    auto lines_a = MyersDiff::split_lines(old_content);
    auto lines_b = MyersDiff::split_lines(new_content);

    auto diff_lines = MyersDiff::compute_diff(lines_a, lines_b);
    fd.hunks = MyersDiff::make_hunks(diff_lines, 3);
    return fd;
}

std::vector<FileDiff> DiffEngine::diff_trees(const ObjectDb& db,
                                            const ObjectId& tree_a_id,
                                            const ObjectId& tree_b_id) {
    std::vector<FileDiff> diffs;
    auto files_a = TreeBuilder::flatten_tree(db, tree_a_id);
    auto files_b = TreeBuilder::flatten_tree(db, tree_b_id);

    std::set<std::string> all_paths;
    for (const auto& [p, _] : files_a) all_paths.insert(p);
    for (const auto& [p, _] : files_b) all_paths.insert(p);

    for (const auto& path : all_paths) {
        auto it_a = files_a.find(path);
        auto it_b = files_b.find(path);

        if (it_a != files_a.end() && it_b == files_b.end()) {
            // Deleted
            auto blob_a = db.read_blob(it_a->second.id);
            auto fd = diff_content(path, blob_a->as_string_view(), "", ChangeType::Deleted);
            fd.old_id = it_a->second.id;
            diffs.push_back(std::move(fd));
        } else if (it_a == files_a.end() && it_b != files_b.end()) {
            // Added
            auto blob_b = db.read_blob(it_b->second.id);
            auto fd = diff_content(path, "", blob_b->as_string_view(), ChangeType::Added);
            fd.new_id = it_b->second.id;
            diffs.push_back(std::move(fd));
        } else if (it_a != files_a.end() && it_b != files_b.end()) {
            // Modified or unchanged
            if (it_a->second.id != it_b->second.id) {
                auto blob_a = db.read_blob(it_a->second.id);
                auto blob_b = db.read_blob(it_b->second.id);
                auto fd = diff_content(path, blob_a->as_string_view(), blob_b->as_string_view(), ChangeType::Modified);
                fd.old_id = it_a->second.id;
                fd.new_id = it_b->second.id;
                diffs.push_back(std::move(fd));
            }
        }
    }

    return diffs;
}

std::vector<FileDiff> DiffEngine::diff_commits(const ObjectDb& db,
                                              const ObjectId& commit_a_id,
                                              const ObjectId& commit_b_id) {
    auto commit_a = db.read_commit(commit_a_id);
    auto commit_b = db.read_commit(commit_b_id);
    return diff_trees(db, commit_a->tree_id(), commit_b->tree_id());
}

std::vector<FileDiff> DiffEngine::diff_index_to_workdir(const Repository& repo) {
    std::vector<FileDiff> diffs;
    auto work_files = repo.scanner().scan_workspace();

    for (const auto& [path, entry] : repo.index().entries()) {
        auto it = work_files.find(path);
        if (it == work_files.end()) {
            // Deleted in workdir
            auto blob = repo.db().read_blob(entry.id);
            auto fd = diff_content(path, blob->as_string_view(), "", ChangeType::Deleted);
            fd.old_id = entry.id;
            diffs.push_back(std::move(fd));
        } else {
            auto disk_path = repo.work_dir() / path;
            std::string content;
            try {
                content = AtomicFile::read_text(disk_path);
            } catch (...) {
                continue;
            }

            auto blob = repo.db().read_blob(entry.id);
            if (content != blob->as_string_view()) {
                auto fd = diff_content(path, blob->as_string_view(), content, ChangeType::Modified);
                fd.old_id = entry.id;
                diffs.push_back(std::move(fd));
            }
        }
    }

    return diffs;
}

std::vector<FileDiff> DiffEngine::diff_head_to_index(const Repository& repo) {
    std::vector<FileDiff> diffs;
    auto head_commit_id = repo.branch_manager().get_head_commit_id();
    std::map<std::string, TreeEntry> head_files;
    if (head_commit_id) {
        auto commit = repo.db().read_commit(*head_commit_id);
        head_files = TreeBuilder::flatten_tree(repo.db(), commit->tree_id());
    }

    std::set<std::string> all_paths;
    for (const auto& [p, _] : head_files) all_paths.insert(p);
    for (const auto& [p, _] : repo.index().entries()) all_paths.insert(p);

    for (const auto& path : all_paths) {
        auto head_it = head_files.find(path);
        auto idx_it = repo.index().find(path);

        if (head_it != head_files.end() && idx_it == nullptr) {
            // Deleted in index
            auto blob = repo.db().read_blob(head_it->second.id);
            auto fd = diff_content(path, blob->as_string_view(), "", ChangeType::Deleted);
            fd.old_id = head_it->second.id;
            diffs.push_back(std::move(fd));
        } else if (head_it == head_files.end() && idx_it != nullptr) {
            // Added in index
            auto blob = repo.db().read_blob(idx_it->id);
            auto fd = diff_content(path, "", blob->as_string_view(), ChangeType::Added);
            fd.new_id = idx_it->id;
            diffs.push_back(std::move(fd));
        } else if (head_it != head_files.end() && idx_it != nullptr) {
            if (head_it->second.id != idx_it->id) {
                auto blob_a = repo.db().read_blob(head_it->second.id);
                auto blob_b = repo.db().read_blob(idx_it->id);
                auto fd = diff_content(path, blob_a->as_string_view(), blob_b->as_string_view(), ChangeType::Modified);
                fd.old_id = head_it->second.id;
                fd.new_id = idx_it->id;
                diffs.push_back(std::move(fd));
            }
        }
    }

    return diffs;
}

std::string DiffEngine::format_unified_diff(const std::vector<FileDiff>& diffs, bool use_color) {
    const std::string color_bold = use_color ? "\033[1m" : "";
    const std::string color_cyan = use_color ? "\033[36m" : "";
    const std::string color_green = use_color ? "\033[32m" : "";
    const std::string color_red = use_color ? "\033[31m" : "";
    const std::string color_reset = use_color ? "\033[0m" : "";

    std::ostringstream oss;
    for (const auto& fd : diffs) {
        oss << color_bold << "diff --chronogit a/" << fd.path << " b/" << fd.path << color_reset << "\n";

        if (fd.is_binary) {
            oss << "Binary files differ\n";
            continue;
        }

        if (fd.change_type == ChangeType::Added) {
            oss << "new file mode 100644\n";
            oss << color_bold << "--- /dev/null\n+++ b/" << fd.path << color_reset << "\n";
        } else if (fd.change_type == ChangeType::Deleted) {
            oss << "deleted file mode 100644\n";
            oss << color_bold << "--- a/" << fd.path << "\n+++ /dev/null" << color_reset << "\n";
        } else {
            oss << color_bold << "--- a/" << fd.path << "\n+++ b/" << fd.path << color_reset << "\n";
        }

        for (const auto& hunk : fd.hunks) {
            oss << color_cyan << hunk.format_header() << color_reset << "\n";
            for (const auto& line : hunk.lines) {
                if (line.type == DiffOpType::Context) {
                    oss << " " << line.text << "\n";
                } else if (line.type == DiffOpType::Insert) {
                    oss << color_green << "+" << line.text << color_reset << "\n";
                } else if (line.type == DiffOpType::Delete) {
                    oss << color_red << "-" << line.text << color_reset << "\n";
                }
            }
        }
    }

    return oss.str();
}

std::string DiffEngine::format_stat(const std::vector<FileDiff>& diffs) {
    if (diffs.empty()) return "";

    std::ostringstream oss;
    int total_added = 0, total_deleted = 0;

    for (const auto& fd : diffs) {
        int added = 0, deleted = 0;
        for (const auto& hunk : fd.hunks) {
            for (const auto& line : hunk.lines) {
                if (line.type == DiffOpType::Insert) added++;
                else if (line.type == DiffOpType::Delete) deleted++;
            }
        }
        total_added += added;
        total_deleted += deleted;

        oss << " " << std::left << std::setw(30) << fd.path << " | "
            << std::right << std::setw(4) << (added + deleted) << " ";
        for (int i = 0; i < std::min(added, 20); ++i) oss << "+";
        for (int i = 0; i < std::min(deleted, 20); ++i) oss << "-";
        oss << "\n";
    }

    oss << " " << diffs.size() << " file(s) changed, "
        << total_added << " insertion(s)(+), "
        << total_deleted << " deletion(s)(-)\n";

    return oss.str();
}

} // namespace chronogit
