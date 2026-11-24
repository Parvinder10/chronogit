#include "chronogit/maintenance/fsck.hpp"
#include "chronogit/repo/repository.hpp"
#include "chronogit/storage/tree.hpp"
#include "chronogit/storage/commit.hpp"
#include <queue>
#include <set>

namespace chronogit {

namespace {

void mark_tree_reachable(const ObjectDb& db, const ObjectId& tree_id, std::set<ObjectId>& reachable) {
    if (tree_id.empty() || reachable.count(tree_id)) return;
    reachable.insert(tree_id);

    try {
        auto tree = db.read_tree(tree_id);
        for (const auto& entry : tree->entries()) {
            if (entry.type == ObjectType::Blob) {
                reachable.insert(entry.id);
            } else if (entry.type == ObjectType::Tree) {
                mark_tree_reachable(db, entry.id, reachable);
            }
        }
    } catch (...) {
        // Missing tree object handled in check
    }
}

void mark_reachable_from_commit(const ObjectDb& db, const ObjectId& root_commit, std::set<ObjectId>& reachable) {
    std::queue<ObjectId> queue;
    queue.push(root_commit);

    while (!queue.empty()) {
        ObjectId cur = queue.front();
        queue.pop();
        if (reachable.count(cur)) continue;
        reachable.insert(cur);

        try {
            auto commit = db.read_commit(cur);
            mark_tree_reachable(db, commit->tree_id(), reachable);
            for (const auto& parent : commit->parent_ids()) {
                if (!reachable.count(parent)) {
                    queue.push(parent);
                }
            }
        } catch (...) {
            // Missing commit object handled in check
        }
    }
}

} // namespace

FsckResult Fsck::check_repository(const Repository& repo, bool find_dangling) {
    FsckResult result;
    const auto& db = repo.db();
    const auto& bm = repo.branch_manager();

    auto all_objects = db.list_all_objects();
    result.total_objects = all_objects.size();

    // 1. Verify integrity of all objects on disk
    std::set<ObjectId> existing_objects;
    for (const auto& id : all_objects) {
        existing_objects.insert(id);
        if (!db.verify_object(id)) {
            result.corrupted_objects.push_back({id, "SHA-256 hash or compression checksum mismatch"});
        } else {
            result.valid_objects++;
            // Check internal structure
            try {
                auto [type, payload] = db.read_raw(id);
                if (type == ObjectType::Tree) {
                    auto tree = Tree::deserialize(payload);
                    for (const auto& entry : tree->entries()) {
                        if (!db.has_object(entry.id)) {
                            result.missing_objects.push_back({entry.id, "referenced by tree " + id});
                        }
                    }
                } else if (type == ObjectType::Commit) {
                    auto commit = Commit::deserialize(payload);
                    if (!db.has_object(commit->tree_id())) {
                        result.missing_objects.push_back({commit->tree_id(), "referenced by commit " + id});
                    }
                    for (const auto& parent : commit->parent_ids()) {
                        if (!db.has_object(parent)) {
                            result.missing_objects.push_back({parent, "parent referenced by commit " + id});
                        }
                    }
                }
            } catch (const std::exception& e) {
                result.corrupted_objects.push_back({id, e.what()});
            }
        }
    }

    // 2. Verify all references
    std::set<ObjectId> reachable_roots;
    auto branches = bm.list_branches();
    for (const auto& b : branches) {
        if (!db.has_object(b.commit_id)) {
            result.ref_errors.push_back("branch '" + b.name + "' points to missing commit " + b.commit_id);
        } else {
            reachable_roots.insert(b.commit_id);
        }
    }

    auto head_commit_id = bm.get_head_commit_id();
    if (head_commit_id && !db.has_object(*head_commit_id)) {
        result.ref_errors.push_back("HEAD points to missing commit " + *head_commit_id);
    } else if (head_commit_id) {
        reachable_roots.insert(*head_commit_id);
    }

    // 3. Reachability analysis for dangling objects
    if (find_dangling) {
        std::set<ObjectId> reachable_all;
        for (const auto& root : reachable_roots) {
            mark_reachable_from_commit(db, root, reachable_all);
        }

        // Also mark index objects as reachable
        for (const auto& [_, ie] : repo.index().entries()) {
            reachable_all.insert(ie.id);
        }

        for (const auto& id : all_objects) {
            if (!reachable_all.count(id)) {
                result.dangling_objects.push_back(id);
            }
        }
    }

    return result;
}

} // namespace chronogit
