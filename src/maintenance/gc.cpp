#include "chronogit/maintenance/gc.hpp"
#include "chronogit/repo/repository.hpp"
#include "chronogit/storage/tree.hpp"
#include "chronogit/storage/commit.hpp"
#include <queue>
#include <set>

namespace chronogit {

namespace {

void mark_tree(const ObjectDb& db, const ObjectId& tree_id, std::set<ObjectId>& reachable) {
    if (tree_id.empty() || reachable.count(tree_id)) return;
    reachable.insert(tree_id);

    try {
        auto tree = db.read_tree(tree_id);
        for (const auto& entry : tree->entries()) {
            if (entry.type == ObjectType::Blob) {
                reachable.insert(entry.id);
            } else if (entry.type == ObjectType::Tree) {
                mark_tree(db, entry.id, reachable);
            }
        }
    } catch (...) {}
}

void mark_commit(const ObjectDb& db, const ObjectId& commit_id, std::set<ObjectId>& reachable) {
    std::queue<ObjectId> q;
    q.push(commit_id);

    while (!q.empty()) {
        ObjectId cur = q.front();
        q.pop();
        if (reachable.count(cur)) continue;
        reachable.insert(cur);

        try {
            auto commit = db.read_commit(cur);
            mark_tree(db, commit->tree_id(), reachable);
            for (const auto& parent : commit->parent_ids()) {
                if (!reachable.count(parent)) {
                    q.push(parent);
                }
            }
        } catch (...) {}
    }
}

} // namespace

GcReport GarbageCollector::run(Repository& repo, bool dry_run) {
    GcReport report;
    report.dry_run = dry_run;

    auto& db = repo.db();
    const auto& bm = repo.branch_manager();

    // Collect all reachable objects
    std::set<ObjectId> reachable;

    for (const auto& b : bm.list_branches()) {
        mark_commit(db, b.commit_id, reachable);
    }

    auto head_commit_id = bm.get_head_commit_id();
    if (head_commit_id) {
        mark_commit(db, *head_commit_id, reachable);
    }

    for (const auto& [_, ie] : repo.index().entries()) {
        reachable.insert(ie.id);
    }

    auto all_objects = db.list_all_objects();

    for (const auto& id : all_objects) {
        if (!reachable.count(id)) {
            report.unreachable_count++;
            report.unreachable_ids.push_back(id);

            auto path = db.object_path(id);
            std::error_code ec;
            auto fsize = std::filesystem::file_size(path, ec);
            if (!ec) {
                report.bytes_freed += fsize;
            }

            if (!dry_run) {
                db.remove_object(id);
            }
        } else {
            report.retained_count++;
        }
    }

    return report;
}

} // namespace chronogit
