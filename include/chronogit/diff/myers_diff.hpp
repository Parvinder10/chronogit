#pragma once

#include <string>
#include <vector>
#include <string_view>

namespace chronogit {

enum class DiffOpType {
    Context, // Unchanged
    Delete,  // '-'
    Insert   // '+'
};

struct DiffLine {
    DiffOpType type;
    std::string text;
    int old_lineno = 0; // 1-based, 0 if insert
    int new_lineno = 0; // 1-based, 0 if delete
};

struct DiffHunk {
    int old_start = 0;
    int old_count = 0;
    int new_start = 0;
    int new_count = 0;
    std::vector<DiffLine> lines;

    std::string format_header() const;
};

class MyersDiff {
public:
    static std::vector<std::string> split_lines(std::string_view text);

    // Computes edit script between line sequences A and B
    static std::vector<DiffLine> compute_diff(const std::vector<std::string>& a,
                                              const std::vector<std::string>& b);

    // Groups line diffs into unified diff hunks with given context lines (default 3)
    static std::vector<DiffHunk> make_hunks(const std::vector<DiffLine>& lines, int context = 3);
};

} // namespace chronogit
