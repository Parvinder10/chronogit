#pragma once

#include "chronogit/core/types.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <memory>
#include <cstdint>

namespace chronogit {

class SHA256 {
public:
    SHA256();
    ~SHA256();

    SHA256(const SHA256&) = delete;
    SHA256& operator=(const SHA256&) = delete;
    SHA256(SHA256&&) noexcept;
    SHA256& operator=(SHA256&&) noexcept;

    void update(std::span<const uint8_t> data);
    void update(std::string_view data);
    ObjectId finalize();

    static ObjectId hash(std::span<const uint8_t> data);
    static ObjectId hash(std::string_view data);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace chronogit
