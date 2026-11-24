#include "chronogit/storage/object_db.hpp"
#include "chronogit/core/atomic_file.hpp"
#include "chronogit/core/sha256.hpp"
#include "chronogit/core/compress.hpp"
#include "chronogit/core/error.hpp"
#include <string>
#include <sstream>

namespace chronogit {

ObjectDb::ObjectDb(std::filesystem::path objects_dir) : objects_dir_(std::move(objects_dir)) {
    std::error_code ec;
    std::filesystem::create_directories(objects_dir_, ec);
}

std::filesystem::path ObjectDb::object_path(const ObjectId& id) const {
    if (id.length() < 2) return objects_dir_ / id;
    return objects_dir_ / id.substr(0, 2) / id.substr(2);
}

ObjectId ObjectDb::write_raw(ObjectType type, std::span<const uint8_t> payload) {
    std::string header = std::string(object_type_to_string(type)) + " " + std::to_string(payload.size());
    std::vector<uint8_t> canonical;
    canonical.reserve(header.size() + 1 + payload.size());
    canonical.insert(canonical.end(), header.begin(), header.end());
    canonical.push_back('\0');
    canonical.insert(canonical.end(), payload.begin(), payload.end());

    ObjectId id = SHA256::hash(canonical);
    auto path = object_path(id);

    // Deduplication: if object already exists, skip write
    if (AtomicFile::exists(path)) {
        return id;
    }

    auto compressed = Compressor::compress(canonical);
    AtomicFile::write_binary(path, compressed);
    return id;
}

ObjectId ObjectDb::write_object(const Object& obj) {
    auto canonical = obj.serialize_canonical();
    ObjectId id = SHA256::hash(canonical);
    auto path = object_path(id);

    // Deduplication
    if (AtomicFile::exists(path)) {
        return id;
    }

    auto compressed = Compressor::compress(canonical);
    AtomicFile::write_binary(path, compressed);
    return id;
}

std::pair<ObjectType, std::vector<uint8_t>> ObjectDb::read_raw(const ObjectId& id) const {
    auto path = object_path(id);
    if (!AtomicFile::exists(path)) {
        throw ObjectNotFoundException(id);
    }

    auto compressed = AtomicFile::read_binary(path);
    auto canonical = Compressor::decompress(compressed);

    // Find null terminator
    size_t null_pos = 0;
    while (null_pos < canonical.size() && canonical[null_pos] != '\0') {
        null_pos++;
    }

    if (null_pos >= canonical.size()) {
        throw ObjectCorruptedException(id, "missing header null terminator");
    }

    std::string header(reinterpret_cast<const char*>(canonical.data()), null_pos);
    std::istringstream iss(header);
    std::string type_str;
    size_t size = 0;
    if (!(iss >> type_str >> size)) {
        throw ObjectCorruptedException(id, "invalid object header: " + header);
    }

    ObjectType type = string_to_object_type(type_str);
    if (type == ObjectType::None) {
        throw ObjectCorruptedException(id, "unknown object type: " + type_str);
    }

    size_t payload_start = null_pos + 1;
    size_t actual_payload_size = canonical.size() - payload_start;
    if (actual_payload_size != size) {
        throw ObjectCorruptedException(id, "payload size mismatch");
    }

    std::vector<uint8_t> payload(canonical.begin() + payload_start, canonical.end());
    return {type, std::move(payload)};
}

std::unique_ptr<Object> ObjectDb::read_object(const ObjectId& id) const {
    auto [type, payload] = read_raw(id);
    switch (type) {
        case ObjectType::Blob:
            return Blob::deserialize(payload);
        case ObjectType::Tree:
            return Tree::deserialize(payload);
        case ObjectType::Commit:
            return Commit::deserialize(payload);
        default:
            throw ObjectCorruptedException(id, "unsupported object type");
    }
}

std::unique_ptr<Blob> ObjectDb::read_blob(const ObjectId& id) const {
    auto [type, payload] = read_raw(id);
    if (type != ObjectType::Blob) {
        throw ObjectCorruptedException(id, "expected blob object, found " + std::string(object_type_to_string(type)));
    }
    return Blob::deserialize(payload);
}

std::unique_ptr<Tree> ObjectDb::read_tree(const ObjectId& id) const {
    auto [type, payload] = read_raw(id);
    if (type != ObjectType::Tree) {
        throw ObjectCorruptedException(id, "expected tree object, found " + std::string(object_type_to_string(type)));
    }
    return Tree::deserialize(payload);
}

std::unique_ptr<Commit> ObjectDb::read_commit(const ObjectId& id) const {
    auto [type, payload] = read_raw(id);
    if (type != ObjectType::Commit) {
        throw ObjectCorruptedException(id, "expected commit object, found " + std::string(object_type_to_string(type)));
    }
    return Commit::deserialize(payload);
}

bool ObjectDb::has_object(const ObjectId& id) const {
    return AtomicFile::exists(object_path(id));
}

bool ObjectDb::verify_object(const ObjectId& id) const {
    auto path = object_path(id);
    if (!AtomicFile::exists(path)) return false;

    try {
        auto compressed = AtomicFile::read_binary(path);
        auto canonical = Compressor::decompress(compressed);
        ObjectId computed_id = SHA256::hash(canonical);
        return (computed_id == id);
    } catch (...) {
        return false;
    }
}

std::vector<ObjectId> ObjectDb::list_all_objects() const {
    std::vector<ObjectId> ids;
    std::error_code ec;
    if (!std::filesystem::exists(objects_dir_, ec)) return ids;

    for (const auto& prefix_dir : std::filesystem::directory_iterator(objects_dir_, ec)) {
        if (!prefix_dir.is_directory()) continue;
        std::string prefix = prefix_dir.path().filename().string();
        if (prefix.length() != 2) continue;

        for (const auto& obj_file : std::filesystem::directory_iterator(prefix_dir.path(), ec)) {
            if (!obj_file.is_regular_file()) continue;
            std::string rest = obj_file.path().filename().string();
            // Skip temporary files
            if (rest.find(".tmp.") != std::string::npos) continue;

            std::string full_id = prefix + rest;
            if (is_valid_object_id(full_id)) {
                ids.push_back(full_id);
            }
        }
    }
    return ids;
}

bool ObjectDb::remove_object(const ObjectId& id) {
    auto path = object_path(id);
    if (AtomicFile::exists(path)) {
        AtomicFile::remove(path);
        // Also remove parent prefix dir if empty
        std::error_code ec;
        if (std::filesystem::is_empty(path.parent_path(), ec)) {
            std::filesystem::remove(path.parent_path(), ec);
        }
        return true;
    }
    return false;
}

} // namespace chronogit
