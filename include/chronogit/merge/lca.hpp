#pragma once

#include "chronogit/core/types.hpp"
#include "chronogit/storage/object_db.hpp"
#include <optional>

namespace chronogit {

class LowestCommonAncestor {
public:
    // Finds the best (closest) common ancestor between two commits in the commit DAG
    static std::optional<ObjectId> find_lca(const ObjectDb& db,
                                            const ObjectId& commit_a,
                                            const ObjectId& commit_b);
};

} // namespace chronogit
