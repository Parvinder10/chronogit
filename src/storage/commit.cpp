#include "chronogit/storage/commit.hpp"
#include "chronogit/core/error.hpp"
#include <sstream>

namespace chronogit {

Commit::Commit(ObjectId tree_id,
               std::vector<ObjectId> parent_ids,
               Signature author,
               Signature committer,
               std::string message)
    : tree_id_(std::move(tree_id)),
      parent_ids_(std::move(parent_ids)),
      author_(std::move(author)),
      committer_(std::move(committer)),
      message_(std::move(message)) {}

std::vector<uint8_t> Commit::serialize_payload() const {
    std::ostringstream oss;
    oss << "tree " << tree_id_ << "\n";
    for (const auto& parent : parent_ids_) {
        oss << "parent " << parent << "\n";
    }
    oss << "author " << author_.to_string() << "\n";
    oss << "committer " << committer_.to_string() << "\n";
    oss << "\n";
    oss << message_ << "\n";

    std::string str = oss.str();
    return std::vector<uint8_t>(str.begin(), str.end());
}

std::unique_ptr<Commit> Commit::deserialize(std::span<const uint8_t> payload) {
    std::string text(reinterpret_cast<const char*>(payload.data()), payload.size());
    std::istringstream iss(text);

    ObjectId tree_id;
    std::vector<ObjectId> parents;
    Signature author;
    Signature committer;
    std::string message;

    std::string line;
    bool reading_headers = true;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (reading_headers) {
            if (line.empty()) {
                reading_headers = false;
                continue;
            }

            if (line.rfind("tree ", 0) == 0) {
                tree_id = line.substr(5);
            } else if (line.rfind("parent ", 0) == 0) {
                parents.push_back(line.substr(7));
            } else if (line.rfind("author ", 0) == 0) {
                author = Signature::parse(line.substr(7));
            } else if (line.rfind("committer ", 0) == 0) {
                committer = Signature::parse(line.substr(10));
            }
        } else {
            if (!message.empty()) {
                message += "\n";
            }
            message += line;
        }
    }

    if (tree_id.empty()) {
        throw ObjectCorruptedException("unknown", "commit missing tree header");
    }

    return std::make_unique<Commit>(std::move(tree_id), std::move(parents),
                                    std::move(author), std::move(committer),
                                    std::move(message));
}

} // namespace chronogit
