#pragma once

#include "chronogit/storage/object.hpp"
#include "chronogit/core/types.hpp"
#include <string>
#include <vector>
#include <memory>
#include <span>

namespace chronogit {

class Commit : public Object {
public:
    Commit() = default;
    Commit(ObjectId tree_id,
           std::vector<ObjectId> parent_ids,
           Signature author,
           Signature committer,
           std::string message);

    ObjectType type() const override { return ObjectType::Commit; }
    std::vector<uint8_t> serialize_payload() const override;

    const ObjectId& tree_id() const { return tree_id_; }
    const std::vector<ObjectId>& parent_ids() const { return parent_ids_; }
    const Signature& author() const { return author_; }
    const Signature& committer() const { return committer_; }
    const std::string& message() const { return message_; }

    static std::unique_ptr<Commit> deserialize(std::span<const uint8_t> payload);

private:
    ObjectId tree_id_;
    std::vector<ObjectId> parent_ids_;
    Signature author_;
    Signature committer_;
    std::string message_;
};

} // namespace chronogit
