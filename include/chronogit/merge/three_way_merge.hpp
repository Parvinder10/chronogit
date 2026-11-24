#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace chronogit {

struct MergeFileResult {
    bool has_conflict = false;
    std::string merged_content;
    int conflict_count = 0;
};

class ThreeWayMerge {
public:
    static MergeFileResult merge(std::string_view base_content,
                                 std::string_view ours_content,
                                 std::string_view theirs_content,
                                 std::string_view ours_label = "HEAD",
                                 std::string_view theirs_label = "theirs");
};

} // namespace chronogit
