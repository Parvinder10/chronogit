#include "chronogit/storage/index.hpp"
#include "chronogit/core/atomic_file.hpp"
#include "chronogit/core/error.hpp"
#include <sstream>

namespace chronogit {

void Index::add_or_update(IndexEntry entry) {
    entries_[entry.path] = std::move(entry);
}

bool Index::remove(const std::string& path) {
    return entries_.erase(path) > 0;
}

void Index::clear() {
    entries_.clear();
}

const IndexEntry* Index::find(const std::string& path) const {
    auto it = entries_.find(path);
    if (it != entries_.end()) {
        return &it->second;
    }
    return nullptr;
}

void Index::save(const std::filesystem::path& index_path) const {
    std::ostringstream oss;
    oss << "# CHRONOGIT INDEX v1\n";
    for (const auto& [path, entry] : entries_) {
        oss << file_mode_to_octal_string(entry.mode) << " "
            << entry.id << " "
            << entry.file_size << " "
            << entry.mtime_sec << " "
            << entry.mtime_nsec << " "
            << entry.path << "\n";
    }
    AtomicFile::write_text(index_path, oss.str());
}

void Index::load(const std::filesystem::path& index_path) {
    entries_.clear();
    if (!AtomicFile::exists(index_path)) {
        return;
    }

    std::string content = AtomicFile::read_text(index_path);
    std::istringstream iss(content);
    std::string line;

    if (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line != "# CHRONOGIT INDEX v1") {
            throw ChronoGitException("Unsupported index version or corrupted index file: " + index_path.string());
        }
    }

    while (std::getline(iss, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::istringstream line_iss(line);
        std::string mode_str, id_str;
        uint64_t size = 0, mtime_sec = 0, mtime_nsec = 0;
        if (!(line_iss >> mode_str >> id_str >> size >> mtime_sec >> mtime_nsec)) {
            continue;
        }

        line_iss.get(); // consume single space separator
        std::string path;
        std::getline(line_iss, path);
        if (path.empty()) continue;

        IndexEntry entry;
        entry.mode = octal_string_to_file_mode(mode_str);
        entry.id = id_str;
        entry.file_size = size;
        entry.mtime_sec = mtime_sec;
        entry.mtime_nsec = mtime_nsec;
        entry.path = path;

        entries_[entry.path] = std::move(entry);
    }
}

} // namespace chronogit
