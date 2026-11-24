#pragma once

#include "chronogit/storage/object.hpp"
#include "chronogit/core/types.hpp"
#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <memory>
#include <span>

namespace chronogit {

struct TreeEntry {
    FileMode mode = FileMode::Regular;
    ObjectType type = ObjectType::Blob;
    ObjectId id;
    std::string name;

    bool operator==(const TreeEntry& other) const = default;
    bool operator<(const TreeEntry& other) const {
        return name < other.name;
    }
};

class Tree : public Object {
public:
    Tree() = default;
    explicit Tree(std::vector<TreeEntry> entries);

    ObjectType type() const override { return ObjectType::Tree; }
    std::vector<uint8_t> serialize_payload() const override;

    void add_entry(TreeEntry entry);
    std::optional<TreeEntry> find_entry(std::string_view name) const;
    const std::vector<TreeEntry>& entries() const { return entries_; }

    static std::unique_ptr<Tree> deserialize(std::span<const uint8_t> payload);

private:
    std::vector<TreeEntry> entries_;
};

} // namespace chronogit
