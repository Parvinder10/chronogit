#include "chronogit/diff/myers_diff.hpp"
#include <map>
#include <algorithm>
#include <sstream>

namespace chronogit {

std::string DiffHunk::format_header() const {
    std::ostringstream oss;
    oss << "@@ -" << old_start;
    if (old_count != 1) {
        oss << "," << old_count;
    }
    oss << " +" << new_start;
    if (new_count != 1) {
        oss << "," << new_count;
    }
    oss << " @@";
    return oss.str();
}

std::vector<std::string> MyersDiff::split_lines(std::string_view text) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.length()) {
        size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            std::string line(text.substr(start));
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
            break;
        } else {
            size_t len = end - start;
            std::string line(text.substr(start, len));
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
            start = end + 1;
        }
    }
    return lines;
}

std::vector<DiffLine> MyersDiff::compute_diff(const std::vector<std::string>& a,
                                              const std::vector<std::string>& b) {
    int n = static_cast<int>(a.size());
    int m = static_cast<int>(b.size());
    int max_d = n + m;

    std::vector<DiffLine> result;
    if (n == 0 && m == 0) {
        return result;
    }

    // Offset for negative indices in V array
    int offset = max_d;
    std::vector<int> v(2 * max_d + 1, 0);
    std::vector<std::vector<int>> trace;

    bool found = false;
    for (int d = 0; d <= max_d; ++d) {
        trace.push_back(v);
        for (int k = -d; k <= d; k += 2) {
            int x;
            if (k == -d || (k != d && v[k - 1 + offset] < v[k + 1 + offset])) {
                x = v[k + 1 + offset]; // Down (insertion)
            } else {
                x = v[k - 1 + offset] + 1; // Right (deletion)
            }
            int y = x - k;

            while (x < n && y < m && a[x] == b[y]) {
                x++;
                y++;
            }
            v[k + offset] = x;

            if (x >= n && y >= m) {
                found = true;
                break;
            }
        }
        if (found) break;
    }

    // Backtrack to find the edit path
    int x = n;
    int y = m;
    std::vector<DiffLine> reverse_diff;

    for (int d = static_cast<int>(trace.size()) - 1; d > 0; --d) {
        const auto& prev_v = trace[d];
        int k = x - y;

        int prev_k;
        if (k == -d || (k != d && prev_v[k - 1 + offset] < prev_v[k + 1 + offset])) {
            prev_k = k + 1;
        } else {
            prev_k = k - 1;
        }

        int prev_x = prev_v[prev_k + offset];
        int prev_y = prev_x - prev_k;

        // Diagonal steps (identical lines)
        while (x > prev_x && y > prev_y) {
            reverse_diff.push_back(DiffLine{
                .type = DiffOpType::Context,
                .text = a[x - 1],
                .old_lineno = x,
                .new_lineno = y
            });
            x--;
            y--;
        }

        if (d > 0) {
            if (x == prev_x) {
                // Insertion from B
                reverse_diff.push_back(DiffLine{
                    .type = DiffOpType::Insert,
                    .text = b[prev_y],
                    .old_lineno = 0,
                    .new_lineno = prev_y + 1
                });
                y = prev_y;
            } else {
                // Deletion from A
                reverse_diff.push_back(DiffLine{
                    .type = DiffOpType::Delete,
                    .text = a[prev_x],
                    .old_lineno = prev_x + 1,
                    .new_lineno = 0
                });
                x = prev_x;
            }
        }
    }

    // Remaining diagonal elements at root
    while (x > 0 && y > 0) {
        reverse_diff.push_back(DiffLine{
            .type = DiffOpType::Context,
            .text = a[x - 1],
            .old_lineno = x,
            .new_lineno = y
        });
        x--;
        y--;
    }

    std::reverse(reverse_diff.begin(), reverse_diff.end());
    return reverse_diff;
}

std::vector<DiffHunk> MyersDiff::make_hunks(const std::vector<DiffLine>& lines, int context) {
    std::vector<DiffHunk> hunks;
    if (lines.empty()) return hunks;

    // Find all indices of changes
    std::vector<size_t> change_indices;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].type != DiffOpType::Context) {
            change_indices.push_back(i);
        }
    }

    if (change_indices.empty()) {
        return hunks; // No changes
    }

    // Group changes into clusters separated by more than 2 * context
    struct Group {
        size_t start_idx = 0;
        size_t end_idx = 0;
    };
    std::vector<Group> groups;
    Group current_group{change_indices[0], change_indices[0]};

    for (size_t i = 1; i < change_indices.size(); ++i) {
        if (change_indices[i] <= current_group.end_idx + 2 * context + 1) {
            current_group.end_idx = change_indices[i];
        } else {
            groups.push_back(current_group);
            current_group = {change_indices[i], change_indices[i]};
        }
    }
    groups.push_back(current_group);

    // Expand each group with context lines
    for (const auto& g : groups) {
        size_t hunk_start = (g.start_idx > static_cast<size_t>(context)) ? g.start_idx - context : 0;
        size_t hunk_end = std::min(lines.size() - 1, g.end_idx + context);

        DiffHunk hunk;
        int old_first = -1, new_first = -1;
        int old_cnt = 0, new_cnt = 0;

        for (size_t i = hunk_start; i <= hunk_end; ++i) {
            const auto& line = lines[i];
            hunk.lines.push_back(line);

            if (line.type == DiffOpType::Context) {
                if (old_first == -1) old_first = line.old_lineno;
                if (new_first == -1) new_first = line.new_lineno;
                old_cnt++;
                new_cnt++;
            } else if (line.type == DiffOpType::Delete) {
                if (old_first == -1) old_first = line.old_lineno;
                old_cnt++;
            } else if (line.type == DiffOpType::Insert) {
                if (new_first == -1) new_first = line.new_lineno;
                new_cnt++;
            }
        }

        hunk.old_start = (old_first != -1) ? old_first : 1;
        hunk.old_count = old_cnt;
        hunk.new_start = (new_first != -1) ? new_first : 1;
        hunk.new_count = new_cnt;

        hunks.push_back(std::move(hunk));
    }

    return hunks;
}

} // namespace chronogit
