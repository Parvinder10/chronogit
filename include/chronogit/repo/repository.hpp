#pragma once

#include "chronogit/core/types.hpp"
#include "chronogit/storage/object_db.hpp"
#include "chronogit/storage/index.hpp"
#include "chronogit/repo/branch_manager.hpp"
#include "chronogit/repo/workspace_scanner.hpp"
#include "chronogit/repo/checkout_engine.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace chronogit {

class Repository {
public:
    Repository(std::filesystem::path work_dir, std::filesystem::path git_dir);

    static Repository init(const std::filesystem::path& work_dir, const std::string& default_branch = "main");
    static Repository find_repo(const std::filesystem::path& start_path);

    // Staging and Committing
    void add(const std::vector<std::string>& pathspecs);
    ObjectId commit(const std::string& message,
                    const std::string& author_name = "",
                    const std::string& author_email = "",
                    const std::vector<ObjectId>& extra_parents = {});

    // Status and Navigation
    RepoStatus status() const;
    void checkout(const std::string& target, bool force = false);
    void create_branch(const std::string& name);
    void delete_branch(const std::string& name);
    std::vector<BranchInfo> list_branches() const;

    // History traversal (DAG topological order)
    std::vector<std::pair<ObjectId, Commit>> get_commit_history(const ObjectId& start_id, size_t limit = 0) const;

    // Direct access to modules
    const std::filesystem::path& work_dir() const { return work_dir_; }
    const std::filesystem::path& git_dir() const { return git_dir_; }
    ObjectDb& db() { return db_; }
    const ObjectDb& db() const { return db_; }
    Index& index() { return index_; }
    const Index& index() const { return index_; }
    BranchManager& branch_manager() { return branch_manager_; }
    const BranchManager& branch_manager() const { return branch_manager_; }
    WorkspaceScanner& scanner() { return scanner_; }
    const WorkspaceScanner& scanner() const { return scanner_; }

private:
    std::filesystem::path work_dir_;
    std::filesystem::path git_dir_;
    ObjectDb db_;
    Index index_;
    BranchManager branch_manager_;
    WorkspaceScanner scanner_;
    CheckoutEngine checkout_engine_;
};

} // namespace chronogit
