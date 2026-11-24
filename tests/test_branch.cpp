#include "test_framework.hpp"
#include "chronogit/repo/branch_manager.hpp"
#include "chronogit/core/error.hpp"

#include <filesystem>

using namespace chronogit;

TEST(BranchManagerTest, CreateListDeleteSwitch) {
    auto test_dir = std::filesystem::temp_directory_path() / "chronogit_test_branch";
    std::error_code ec;
    std::filesystem::remove_all(test_dir, ec);
    std::filesystem::create_directories(test_dir, ec);

    BranchManager bm(test_dir);

    // Initial HEAD pointing to main
    bm.set_head_symbolic("main");
    EXPECT_EQ(bm.get_current_branch(), "main");
    EXPECT_FALSE(bm.get_head_commit_id().has_value()); // Unborn branch

    ObjectId c1 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    ObjectId c2 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

    // Commit on main
    bm.update_head_ref(c1);
    EXPECT_EQ(bm.get_branch_commit_id("main"), c1);
    EXPECT_EQ(bm.get_head_commit_id(), c1);

    // Create feature branch
    bm.create_branch("feature", c1);
    EXPECT_TRUE(bm.branch_exists("feature"));
    EXPECT_EQ(bm.get_branch_commit_id("feature"), c1);

    // List branches
    auto list = bm.list_branches();
    EXPECT_EQ(list.size(), 2u);

    // Switch to feature branch
    bm.set_head_symbolic("feature");
    EXPECT_EQ(bm.get_current_branch(), "feature");
    EXPECT_EQ(bm.get_head_commit_id(), c1);

    // Update feature branch to c2
    bm.update_head_ref(c2);
    EXPECT_EQ(bm.get_branch_commit_id("feature"), c2);
    EXPECT_EQ(bm.get_branch_commit_id("main"), c1);

    // Cannot delete active branch
    EXPECT_THROW(bm.delete_branch("feature"), ChronoGitException);

    // Switch to main and delete feature
    bm.set_head_symbolic("main");
    bm.delete_branch("feature");
    EXPECT_FALSE(bm.branch_exists("feature"));

    // Detached HEAD test
    bm.set_head_detached(c2);
    EXPECT_FALSE(bm.get_current_branch().has_value());
    EXPECT_EQ(bm.get_head_commit_id(), c2);

    std::filesystem::remove_all(test_dir, ec);
}
