#pragma once

#include "chronogit/core/types.hpp"
#include <string>
#include <vector>
#include <set>

namespace chronogit {

class Repository;

struct FsckResult {
    size_t total_objects = 0;
    size_t valid_objects = 0;
    std::vector<std::pair<ObjectId, std::string>> corrupted_objects;
    std::vector<std::pair<ObjectId, std::string>> missing_objects;
    std::vector<ObjectId> dangling_objects;
    std::vector<std::string> ref_errors;

    bool is_healthy() const {
        return corrupted_objects.empty() && missing_objects.empty() && ref_errors.empty();
    }
};

class Fsck {
public:
    static FsckResult check_repository(const Repository& repo, bool find_dangling = true);
};

} // namespace chronogit
