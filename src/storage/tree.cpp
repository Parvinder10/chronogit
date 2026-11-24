#include "chronogit/storage/tree.hpp"
#include "chronogit/core/error.hpp"
#include <algorithm>
#include <sstream>

namespace chronogit {

Tree::Tree(std::vector<TreeEntry> entries) : entries_(std::move(entries)) {
    std::sort(entries_.begin(), entries_.end());
}

void Tree::add_entry(TreeEntry entry) {
    auto it = std::lower_bound(entries_.begin(), entries_.end(), entry);
    if (it != entries_.end() && it->name == entry.name) {
        *it = std::move(entry);
    } else {
        entries_.insert(it, std::move(entry));
    }
}

std::optional<TreeEntry> Tree::find_entry(std::string_view name) const {
    for (const auto& entry : entries_) {
        if (entry.name == name) {
            return entry;
        }
    }
    return std::nullopt;
}

std::vector<uint8_t> Tree::serialize_payload() const {
    std::ostringstream oss;
    for (const auto& entry : entries_) {
        oss << file_mode_to_octal_string(entry.mode) << " "
            << object_type_to_string(entry.type) << " "
            << entry.id << " "
            << entry.name << "\n";
    }
    std::string str = oss.str();
    return std::vector<uint8_t>(str.begin(), str.end());
}

std::unique_ptr<Tree> Tree::deserialize(std::span<const uint8_t> payload) {
    std::string text(reinterpret_cast<const char*>(payload.data()), payload.size());
    std::istringstream iss(text);
    std::vector<TreeEntry> entries;

    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        std::istringstream line_iss(line);
        std::string mode_str, type_str, id_str, name_str;
        if (!(line_iss >> mode_str >> type_str >> id_str)) {
            throw ObjectCorruptedException("unknown", "invalid tree entry format");
        }
        // Name may contain spaces, so read rest of line
        line_iss.get(); // consume delimiter space
        std::getline(line_iss, name_str);
        if (name_str.empty()) {
            throw ObjectCorruptedException("unknown", "missing tree entry name");
        }

        TreeEntry entry;
        entry.mode = octal_string_to_file_mode(mode_str);
        entry.type = string_to_object_type(type_str);
        entry.id = id_str;
        entry.name = name_str;
        entries.push_back(std::move(entry));
    }

    return std::make_unique<Tree>(std::move(entries));
}

} // namespace chronogit
