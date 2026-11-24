#include "chronogit/merge/three_way_merge.hpp"
#include "chronogit/diff/myers_diff.hpp"
#include <algorithm>
#include <sstream>

namespace chronogit {

namespace {

struct SliceEdit {
    int base_start = 0; // 0-based
    int base_end = 0;   // 0-based exclusive
    std::vector<std::string> replacement;
};

std::vector<SliceEdit> extract_edits(const std::vector<std::string>& base_lines,
                                     const std::vector<std::string>& target_lines) {
    auto diff_lines = MyersDiff::compute_diff(base_lines, target_lines);
    std::vector<SliceEdit> edits;

    int cur_base = 0;
    size_t i = 0;

    while (i < diff_lines.size()) {
        if (diff_lines[i].type == DiffOpType::Context) {
            cur_base++;
            i++;
            continue;
        }

        // We encountered a change
        int start_base = cur_base;
        std::vector<std::string> repl;

        while (i < diff_lines.size() && diff_lines[i].type != DiffOpType::Context) {
            if (diff_lines[i].type == DiffOpType::Delete) {
                cur_base++;
            } else if (diff_lines[i].type == DiffOpType::Insert) {
                repl.push_back(diff_lines[i].text);
            }
            i++;
        }

        edits.push_back(SliceEdit{
            .base_start = start_base,
            .base_end = cur_base,
            .replacement = std::move(repl)
        });
    }

    return edits;
}

} // namespace

MergeFileResult ThreeWayMerge::merge(std::string_view base_content,
                                     std::string_view ours_content,
                                     std::string_view theirs_content,
                                     std::string_view ours_label,
                                     std::string_view theirs_label) {
    MergeFileResult result;

    if (ours_content == theirs_content) {
        result.merged_content = std::string(ours_content);
        return result;
    }
    if (base_content == ours_content) {
        result.merged_content = std::string(theirs_content);
        return result;
    }
    if (base_content == theirs_content) {
        result.merged_content = std::string(ours_content);
        return result;
    }

    auto b_lines = MyersDiff::split_lines(base_content);
    auto o_lines = MyersDiff::split_lines(ours_content);
    auto t_lines = MyersDiff::split_lines(theirs_content);

    // If base was empty and both added content
    if (b_lines.empty()) {
        // Both added different content
        result.has_conflict = true;
        result.conflict_count = 1;
        std::ostringstream oss;
        oss << "<<<<<<< " << ours_label << "\n";
        for (const auto& l : o_lines) oss << l << "\n";
        oss << "=======\n";
        for (const auto& l : t_lines) oss << l << "\n";
        oss << ">>>>>>> " << theirs_label << "\n";
        result.merged_content = oss.str();
        return result;
    }

    auto edits_o = extract_edits(b_lines, o_lines);
    auto edits_t = extract_edits(b_lines, t_lines);

    size_t idx_o = 0;
    size_t idx_t = 0;
    int base_idx = 0;
    int b_count = static_cast<int>(b_lines.size());

    std::ostringstream out;

    while (base_idx < b_count || idx_o < edits_o.size() || idx_t < edits_t.size()) {
        // Check next edit positions
        bool has_o = (idx_o < edits_o.size());
        bool has_t = (idx_t < edits_t.size());

        int next_o_start = has_o ? edits_o[idx_o].base_start : 1000000000;
        int next_t_start = has_t ? edits_t[idx_t].base_start : 1000000000;

        int next_edit_start = std::min(next_o_start, next_t_start);

        // Emit unmodified base lines up to next_edit_start
        while (base_idx < next_edit_start && base_idx < b_count) {
            out << b_lines[base_idx] << "\n";
            base_idx++;
        }

        if (!has_o && !has_t) {
            break;
        }

        // Determine which edit(s) to process
        if (has_o && has_t && edits_o[idx_o].base_start < edits_t[idx_t].base_end &&
            edits_t[idx_t].base_start < edits_o[idx_o].base_end) {
            // Overlapping edits between Ours and Theirs
            // Combine all overlapping edits on both sides
            int union_end = std::max(edits_o[idx_o].base_end, edits_t[idx_t].base_end);

            std::vector<std::string> combined_o = edits_o[idx_o].replacement;
            std::vector<std::string> combined_t = edits_t[idx_t].replacement;
            idx_o++;
            idx_t++;

            while ((idx_o < edits_o.size() && edits_o[idx_o].base_start < union_end) ||
                   (idx_t < edits_t.size() && edits_t[idx_t].base_start < union_end)) {
                if (idx_o < edits_o.size() && edits_o[idx_o].base_start < union_end) {
                    union_end = std::max(union_end, edits_o[idx_o].base_end);
                    combined_o.insert(combined_o.end(), edits_o[idx_o].replacement.begin(), edits_o[idx_o].replacement.end());
                    idx_o++;
                }
                if (idx_t < edits_t.size() && edits_t[idx_t].base_start < union_end) {
                    union_end = std::max(union_end, edits_t[idx_t].base_end);
                    combined_t.insert(combined_t.end(), edits_t[idx_t].replacement.begin(), edits_t[idx_t].replacement.end());
                    idx_t++;
                }
            }

            if (combined_o == combined_t) {
                // Identical edits! Clean merge.
                for (const auto& l : combined_o) {
                    out << l << "\n";
                }
            } else {
                // Real conflict!
                result.has_conflict = true;
                result.conflict_count++;
                out << "<<<<<<< " << ours_label << "\n";
                for (const auto& l : combined_o) {
                    out << l << "\n";
                }
                out << "=======\n";
                for (const auto& l : combined_t) {
                    out << l << "\n";
                }
                out << ">>>>>>> " << theirs_label << "\n";
            }

            base_idx = union_end;
        } else if (has_o && next_o_start <= next_t_start) {
            // Edit in Ours only
            for (const auto& l : edits_o[idx_o].replacement) {
                out << l << "\n";
            }
            base_idx = edits_o[idx_o].base_end;
            idx_o++;
        } else if (has_t) {
            // Edit in Theirs only
            for (const auto& l : edits_t[idx_t].replacement) {
                out << l << "\n";
            }
            base_idx = edits_t[idx_t].base_end;
            idx_t++;
        }
    }

    result.merged_content = out.str();
    return result;
}

} // namespace chronogit
