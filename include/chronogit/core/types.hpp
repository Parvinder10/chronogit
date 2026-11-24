#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <chrono>

namespace chronogit {

using ObjectId = std::string; // 64-character lowercase hex string

inline const ObjectId NULL_OBJECT_ID = "0000000000000000000000000000000000000000000000000000000000000000";

inline bool is_valid_object_id(std::string_view id) {
    if (id.length() != 64) return false;
    for (char c : id) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    return true;
}

enum class ObjectType : uint8_t {
    None = 0,
    Blob = 1,
    Tree = 2,
    Commit = 3
};

inline std::string_view object_type_to_string(ObjectType type) {
    switch (type) {
        case ObjectType::Blob: return "blob";
        case ObjectType::Tree: return "tree";
        case ObjectType::Commit: return "commit";
        default: return "none";
    }
}

inline ObjectType string_to_object_type(std::string_view str) {
    if (str == "blob") return ObjectType::Blob;
    if (str == "tree") return ObjectType::Tree;
    if (str == "commit") return ObjectType::Commit;
    return ObjectType::None;
}

enum class FileMode : uint32_t {
    Regular = 0100644,
    Executable = 0100755,
    Directory = 0040000
};

inline std::string file_mode_to_octal_string(FileMode mode) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%06o", static_cast<uint32_t>(mode));
    return std::string(buf);
}

inline FileMode octal_string_to_file_mode(std::string_view str) {
    uint32_t val = 0;
    for (char c : str) {
        if (c >= '0' && c <= '7') {
            val = (val << 3) + (c - '0');
        }
    }
    return static_cast<FileMode>(val);
}

struct Signature {
    std::string name;
    std::string email;
    int64_t timestamp = 0; // seconds since unix epoch
    std::string tz_offset = "+0000";

    static Signature now(std::string_view name = "ChronoGit User", std::string_view email = "user@chronogit.local");
    std::string to_string() const;
    static Signature parse(std::string_view line);
};

enum class MergeResultStatus {
    Clean,
    FastForward,
    AlreadyUpToDate,
    Conflict
};

inline std::ostream& operator<<(std::ostream& os, ObjectType type) {
    return os << object_type_to_string(type);
}

inline std::ostream& operator<<(std::ostream& os, FileMode mode) {
    return os << file_mode_to_octal_string(mode);
}

inline std::ostream& operator<<(std::ostream& os, MergeResultStatus status) {
    switch (status) {
        case MergeResultStatus::Clean: return os << "Clean";
        case MergeResultStatus::FastForward: return os << "FastForward";
        case MergeResultStatus::AlreadyUpToDate: return os << "AlreadyUpToDate";
        case MergeResultStatus::Conflict: return os << "Conflict";
        default: return os << "Unknown";
    }
}

} // namespace chronogit
