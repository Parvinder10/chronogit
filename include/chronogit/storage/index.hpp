#pragma once

#include "chronogit/core/types.hpp"
#include <string>
#include <map>
#include <vector>
#include <filesystem>
#include <cstdint>

namespace chronogit {

struct IndexEntry {
    std::string path; // relative to repo root, normalized with '/'
    ObjectId id;      // Blob SHA-256
    FileMode mode = FileMode::Regular;
    uint64_t file_size = 0;
    uint64_t mtime_sec = 0;
    uint64_t mtime_nsec = 0;

    bool operator==(const IndexEntry& other) const = default;
};

class Index {
public:
    Index() = default;

    void add_or_update(IndexEntry entry);
    bool remove(const std::string& path);
    void clear();

    const IndexEntry* find(const std::string& path) const;
    const std::map<std::string, IndexEntry>& entries() const { return entries_; }
    size_t size() const { return entries_.size(); }
    bool empty() const { return entries_.empty(); }

    void save(const std::filesystem::path& index_path) const;
    void load(const std::filesystem::path& index_path);

private:
    std::map<std::string, IndexEntry> entries_;
};

} // namespace chronogit
