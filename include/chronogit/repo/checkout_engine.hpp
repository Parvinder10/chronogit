#pragma once

#include "chronogit/core/types.hpp"
#include "chronogit/storage/object_db.hpp"
#include "chronogit/storage/index.hpp"
#include "chronogit/repo/branch_manager.hpp"
#include "chronogit/repo/workspace_scanner.hpp"
#include <filesystem>
#include <string>

namespace chronogit {

class CheckoutEngine {
public:
    CheckoutEngine(std::filesystem::path work_dir,
                   ObjectDb& db,
                   Index& index,
                   BranchManager& branch_manager,
                   WorkspaceScanner& scanner);

    // Switches to a branch or detached commit, safely checking for uncommitted changes.
    void checkout(const std::string& target, bool force = false);

private:
    std::filesystem::path work_dir_;
    ObjectDb& db_;
    Index& index_;
    BranchManager& branch_manager_;
    WorkspaceScanner& scanner_;
};

} // namespace chronogit
