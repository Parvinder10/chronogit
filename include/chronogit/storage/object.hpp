#pragma once

#include "chronogit/core/types.hpp"
#include <vector>
#include <span>
#include <memory>
#include <string_view>

namespace chronogit {

class Object {
public:
    virtual ~Object() = default;

    virtual ObjectType type() const = 0;
    virtual std::vector<uint8_t> serialize_payload() const = 0;

    // Constructs canonical header + payload: "<type> <size>\0<payload>"
    std::vector<uint8_t> serialize_canonical() const;

    // Computes deterministic SHA-256 hash over canonical representation
    ObjectId compute_id() const;
};

} // namespace chronogit
