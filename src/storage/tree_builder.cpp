#include "chronogit/storage/tree_builder.hpp"
#include <sstream>

namespace chronogit {

namespace {

struct TreeNode {
    std::map<std::string, TreeNode> subdirs;
    std::map<std::string, std::pair<ObjectId, FileMode>> files;
};

void insert_path(TreeNode& root, const std::string& path, const ObjectId& id, FileMode mode) {
    size_t start = 0;
    TreeNode* current = &root;

    while (true) {
        size_t slash = path.find('/', start);
        if (slash == std::string::npos) {
            // Leaf file
            std::string filename = path.substr(start);
            current->files[filename] = {id, mode};
            break;
        } else {
            std::string dir = path.substr(start, slash - start);
            current = &current->subdirs[dir];
            start = slash + 1;
        }
    }
}

ObjectId write_node(ObjectDb& db, const TreeNode& node) {
    Tree tree;

    // Subdirectories first or alongside files (Tree sorts by name)
    for (const auto& [dirname, subdir] : node.subdirs) {
        ObjectId sub_id = write_node(db, subdir);
        tree.add_entry(TreeEntry{
            .mode = FileMode::Directory,
            .type = ObjectType::Tree,
            .id = sub_id,
            .name = dirname
        });
    }

    for (const auto& [filename, file_info] : node.files) {
        tree.add_entry(TreeEntry{
            .mode = file_info.second,
            .type = ObjectType::Blob,
            .id = file_info.first,
            .name = filename
        });
    }

    return db.write_object(tree);
}

} // namespace

ObjectId TreeBuilder::build_tree_from_index(ObjectDb& db, const Index& index) {
    std::map<std::string, std::pair<ObjectId, FileMode>> files;
    for (const auto& [path, entry] : index.entries()) {
        files[path] = {entry.id, entry.mode};
    }
    return build_tree_from_map(db, files);
}

ObjectId TreeBuilder::build_tree_from_map(ObjectDb& db, const std::map<std::string, std::pair<ObjectId, FileMode>>& files) {
    if (files.empty()) {
        Tree empty_tree;
        return db.write_object(empty_tree);
    }

    TreeNode root;
    for (const auto& [path, info] : files) {
        insert_path(root, path, info.first, info.second);
    }

    return write_node(db, root);
}

std::map<std::string, TreeEntry> TreeBuilder::flatten_tree(const ObjectDb& db, const ObjectId& root_tree_id, const std::string& prefix) {
    std::map<std::string, TreeEntry> result;
    if (root_tree_id.empty() || root_tree_id == NULL_OBJECT_ID) {
        return result;
    }

    auto tree = db.read_tree(root_tree_id);
    for (const auto& entry : tree->entries()) {
        std::string full_path = prefix.empty() ? entry.name : (prefix + "/" + entry.name);
        if (entry.type == ObjectType::Blob) {
            result[full_path] = entry;
        } else if (entry.type == ObjectType::Tree) {
            auto sub_entries = flatten_tree(db, entry.id, full_path);
            result.insert(sub_entries.begin(), sub_entries.end());
        }
    }

    return result;
}

} // namespace chronogit
