#pragma once

#include "chronogit/core/types.hpp"
#include <vector>
#include <cstdint>

namespace chronogit {

class Repository;

struct GcReport {
    size_t unreachable_count = 0;
    size_t retained_count = 0;
    uint64_t bytes_freed = 0;
    bool dry_run = false;
    std::vector<ObjectId> unreachable_ids;
};

class GarbageCollector {
public:
    static GcReport run(Repository& repo, bool dry_run = false);
};

} // namespace chronogit
