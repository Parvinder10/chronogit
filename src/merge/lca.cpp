#include "chronogit/merge/lca.hpp"
#include <queue>
#include <map>
#include <set>

namespace chronogit {

std::optional<ObjectId> LowestCommonAncestor::find_lca(const ObjectDb& db,
                                                       const ObjectId& commit_a,
                                                       const ObjectId& commit_b) {
    if (commit_a.empty() || commit_b.empty()) return std::nullopt;
    if (commit_a == commit_b) return commit_a;

    if (!db.has_object(commit_a) || !db.has_object(commit_b)) return std::nullopt;

    // Traverse ancestors of commit_a
    std::map<ObjectId, int> dist_a;
    std::queue<ObjectId> q_a;
    dist_a[commit_a] = 0;
    q_a.push(commit_a);

    while (!q_a.empty()) {
        ObjectId cur = q_a.front();
        q_a.pop();
        int d = dist_a[cur];

        try {
            auto commit = db.read_commit(cur);
            for (const auto& parent : commit->parent_ids()) {
                if (dist_a.find(parent) == dist_a.end()) {
                    dist_a[parent] = d + 1;
                    q_a.push(parent);
                }
            }
        } catch (...) {
            // Dangling commit
        }
    }

    // Traverse ancestors of commit_b and find common ancestors
    std::map<ObjectId, int> dist_b;
    std::queue<ObjectId> q_b;
    dist_b[commit_b] = 0;
    q_b.push(commit_b);

    std::optional<ObjectId> best_ancestor;
    int min_total_dist = 1000000000;

    while (!q_b.empty()) {
        ObjectId cur = q_b.front();
        q_b.pop();
        int d = dist_b[cur];

        auto it = dist_a.find(cur);
        if (it != dist_a.end()) {
            int total_dist = it->second + d;
            if (total_dist < min_total_dist) {
                min_total_dist = total_dist;
                best_ancestor = cur;
            }
        }

        try {
            auto commit = db.read_commit(cur);
            for (const auto& parent : commit->parent_ids()) {
                if (dist_b.find(parent) == dist_b.end()) {
                    dist_b[parent] = d + 1;
                    q_b.push(parent);
                }
            }
        } catch (...) {
            // Dangling commit
        }
    }

    return best_ancestor;
}

} // namespace chronogit
