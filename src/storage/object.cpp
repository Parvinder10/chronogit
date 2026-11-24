#include "chronogit/storage/object.hpp"
#include "chronogit/core/sha256.hpp"
#include <string>

namespace chronogit {

std::vector<uint8_t> Object::serialize_canonical() const {
    auto payload = serialize_payload();
    std::string header = std::string(object_type_to_string(type())) + " " + std::to_string(payload.size());
    
    std::vector<uint8_t> result;
    result.reserve(header.size() + 1 + payload.size());
    result.insert(result.end(), header.begin(), header.end());
    result.push_back('\0');
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}

ObjectId Object::compute_id() const {
    auto canonical = serialize_canonical();
    return SHA256::hash(canonical);
}

} // namespace chronogit
