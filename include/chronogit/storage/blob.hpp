#pragma once

#include "chronogit/storage/object.hpp"
#include <vector>
#include <string>
#include <string_view>
#include <span>

namespace chronogit {

class Blob : public Object {
public:
    Blob() = default;
    explicit Blob(std::vector<uint8_t> data) : data_(std::move(data)) {}
    explicit Blob(std::string_view text)
        : data_(reinterpret_cast<const uint8_t*>(text.data()),
                reinterpret_cast<const uint8_t*>(text.data()) + text.size()) {}

    ObjectType type() const override { return ObjectType::Blob; }
    std::vector<uint8_t> serialize_payload() const override { return data_; }

    const std::vector<uint8_t>& data() const { return data_; }
    std::string_view as_string_view() const {
        return std::string_view(reinterpret_cast<const char*>(data_.data()), data_.size());
    }

    static std::unique_ptr<Blob> deserialize(std::span<const uint8_t> payload);

private:
    std::vector<uint8_t> data_;
};

} // namespace chronogit
