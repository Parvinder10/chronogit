#include "chronogit/storage/blob.hpp"

namespace chronogit {

std::unique_ptr<Blob> Blob::deserialize(std::span<const uint8_t> payload) {
    std::vector<uint8_t> data(payload.begin(), payload.end());
    return std::make_unique<Blob>(std::move(data));
}

} // namespace chronogit
