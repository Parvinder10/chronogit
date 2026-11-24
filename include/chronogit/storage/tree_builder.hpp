#pragma once

#include "chronogit/core/types.hpp"
#include "chronogit/storage/object_db.hpp"
#include "chronogit/storage/index.hpp"
#include "chronogit/storage/tree.hpp"
#include <map>
#include <string>

namespace chronogit {

class TreeBuilder {
public:
    // Recursively builds hierarchical trees from index and writes them to ObjectDb. Returns root tree ObjectId.
    static ObjectId build_tree_from_index(ObjectDb& db, const Index& index);

    // Recursively builds hierarchical trees from a map of relative paths to (ObjectId, FileMode).
    static ObjectId build_tree_from_map(ObjectDb& db, const std::map<std::string, std::pair<ObjectId, FileMode>>& files);

    // Flattens a tree into a map of full relative paths to their leaf TreeEntry (Blobs).
    static std::map<std::string, TreeEntry> flatten_tree(const ObjectDb& db, const ObjectId& root_tree_id, const std::string& prefix = "");
};

} // namespace chronogit
