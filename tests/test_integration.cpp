#include "test_framework.hpp"
#include "chronogit/repo/repository.hpp"
#include "chronogit/merge/merge_engine.hpp"
#include "chronogit/core/atomic_file.hpp"

#include <filesystem>

using namespace chronogit;

TEST(IntegrationTest, FullLifecycleAndBranchMerging) {
    auto test_dir = std::filesystem::temp_directory_path() / "chronogit_test_lifecycle";
    std::error_code ec;
    std::filesystem::remove_all(test_dir, ec);
    std::filesystem::create_directories(test_dir, ec);

    // 1. Init repository
    auto repo = Repository::init(test_dir, "main");
    EXPECT_EQ(repo.branch_manager().get_current_branch(), "main");

    // 2. Add files and initial commit
    AtomicFile::write_text(test_dir / "README.md", "# Project ChronoGit\nInitial description\n");
    AtomicFile::write_text(test_dir / "main.cpp", "int main() { return 0; }\n");

    repo.add({"README.md", "main.cpp"});
    auto status1 = repo.status();
    EXPECT_EQ(status1.staged_new.size(), 2u);

    ObjectId c1 = repo.commit("Initial commit");
    EXPECT_TRUE(is_valid_object_id(c1));
    EXPECT_TRUE(repo.status().is_clean());

    // 3. Create feature branch and switch to it
    repo.create_branch("feature");
    repo.checkout("feature");
    EXPECT_EQ(repo.branch_manager().get_current_branch(), "feature");

    // 4. Modify on feature branch
    AtomicFile::write_text(test_dir / "feature.cpp", "// Feature implementation\nvoid feature() {}\n");
    repo.add({"feature.cpp"});
    ObjectId c_feat = repo.commit("Add feature module");

    // 5. Switch back to main and commit independent change
    repo.checkout("main");
    EXPECT_EQ(repo.branch_manager().get_current_branch(), "main");
    EXPECT_FALSE(AtomicFile::exists(test_dir / "feature.cpp")); // Not on main!

    AtomicFile::write_text(test_dir / "README.md", "# Project ChronoGit\nUpdated description on main\n");
    repo.add({"README.md"});
    ObjectId c_main2 = repo.commit("Update README on main");

    // 6. Three-way merge feature into main (clean merge)
    MergeEngine engine(repo);
    auto merge_res = engine.merge("feature");
    EXPECT_EQ(merge_res.status, MergeResultStatus::Clean);
    EXPECT_TRUE(is_valid_object_id(merge_res.commit_id));

    // Both feature.cpp and updated README.md must exist in working tree
    EXPECT_TRUE(AtomicFile::exists(test_dir / "feature.cpp"));
    EXPECT_TRUE(AtomicFile::read_text(test_dir / "README.md").find("Updated description on main") != std::string::npos);

    // Commit history must contain both branch lines and merge commit
    auto history = repo.get_commit_history(merge_res.commit_id);
    EXPECT_TRUE(history.size() >= 4); // merge commit, c_main2, c_feat, c1

    // 7. Verify conflict workflow:
    // Create branch-a and branch-b from current merge commit
    repo.create_branch("branch-a");
    repo.create_branch("branch-b");

    repo.checkout("branch-a");
    AtomicFile::write_text(test_dir / "conflict.txt", "Version A by Branch A\n");
    repo.add({"conflict.txt"});
    repo.commit("Add conflict.txt on branch-a");

    repo.checkout("branch-b");
    AtomicFile::write_text(test_dir / "conflict.txt", "Version B by Branch B\n");
    repo.add({"conflict.txt"});
    repo.commit("Add conflict.txt on branch-b");

    // Merge branch-a into branch-b -> must detect conflict!
    auto conflict_merge_res = engine.merge("branch-a");
    EXPECT_EQ(conflict_merge_res.status, MergeResultStatus::Conflict);
    EXPECT_TRUE(engine.is_in_merge_state());

    // Verify conflict markers in conflict.txt
    std::string conflict_content = AtomicFile::read_text(test_dir / "conflict.txt");
    EXPECT_TRUE(conflict_content.find("<<<<<<< HEAD") != std::string::npos);
    EXPECT_TRUE(conflict_content.find("Version B by Branch B") != std::string::npos);
    EXPECT_TRUE(conflict_content.find("=======") != std::string::npos);
    EXPECT_TRUE(conflict_content.find("Version A by Branch A") != std::string::npos);
    EXPECT_TRUE(conflict_content.find(">>>>>>> branch-a") != std::string::npos);

    // Resolve conflict by writing resolved content and committing
    AtomicFile::write_text(test_dir / "conflict.txt", "Resolved Version combining A and B\n");
    repo.add({"conflict.txt"});

    ObjectId resolved_commit = repo.commit("Merge branch 'branch-a' into branch-b (resolved)");
    EXPECT_TRUE(is_valid_object_id(resolved_commit));
    engine.clear_merge_state();
    EXPECT_FALSE(engine.is_in_merge_state());

    std::filesystem::remove_all(test_dir, ec);
}
