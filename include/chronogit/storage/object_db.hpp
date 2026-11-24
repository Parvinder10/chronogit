#pragma once

#include "chronogit/core/types.hpp"
#include "chronogit/storage/object.hpp"
#include "chronogit/storage/blob.hpp"
#include "chronogit/storage/tree.hpp"
#include "chronogit/storage/commit.hpp"
#include <filesystem>
#include <memory>
#include <vector>
#include <span>

namespace chronogit {

class ObjectDb {
public:
    explicit ObjectDb(std::filesystem::path objects_dir);

    // Writes an Object to the database. Deduplicates if already present. Returns ObjectId.
    ObjectId write_object(const Object& obj);

    // Writes raw payload with specified type. Deduplicates if already present. Returns ObjectId.
    ObjectId write_raw(ObjectType type, std::span<const uint8_t> payload);

    // Reads raw decompressed payload and object type. Throws if not found or corrupted.
    std::pair<ObjectType, std::vector<uint8_t>> read_raw(const ObjectId& id) const;

    // Reads and parses a typed Object.
    std::unique_ptr<Object> read_object(const ObjectId& id) const;
    std::unique_ptr<Blob> read_blob(const ObjectId& id) const;
    std::unique_ptr<Tree> read_tree(const ObjectId& id) const;
    std::unique_ptr<Commit> read_commit(const ObjectId& id) const;

    // Checks if object exists in the database.
    bool has_object(const ObjectId& id) const;

    // Verifies object checksum against its stored contents.
    bool verify_object(const ObjectId& id) const;

    // Lists all object IDs currently in the database.
    std::vector<ObjectId> list_all_objects() const;

    // Returns filesystem path to object file.
    std::filesystem::path object_path(const ObjectId& id) const;

    // Deletes an object (used by GC). Returns true if deleted.
    bool remove_object(const ObjectId& id);

    const std::filesystem::path& objects_dir() const { return objects_dir_; }

private:
    std::filesystem::path objects_dir_;
};

} // namespace chronogit
