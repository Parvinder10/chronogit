#include "test_framework.hpp"
#include "chronogit/merge/three_way_merge.hpp"
#include "chronogit/merge/lca.hpp"
#include "chronogit/storage/object_db.hpp"
#include "chronogit/storage/commit.hpp"
#include "chronogit/storage/tree.hpp"

#include <filesystem>

using namespace chronogit;

TEST(MergeTest, ThreeWayCleanNonOverlapping) {
    std::string base = "line 1\nline 2\nline 3\nline 4\nline 5\n";
    // Ours modifies line 1
    std::string ours = "line 1 modified by ours\nline 2\nline 3\nline 4\nline 5\n";
    // Theirs modifies line 5
    std::string theirs = "line 1\nline 2\nline 3\nline 4\nline 5 modified by theirs\n";

    auto result = ThreeWayMerge::merge(base, ours, theirs, "HEAD", "feature");
    EXPECT_FALSE(result.has_conflict);
    EXPECT_EQ(result.conflict_count, 0);

    // Both modifications must be present in the merged output!
    EXPECT_TRUE(result.merged_content.find("line 1 modified by ours") != std::string::npos);
    EXPECT_TRUE(result.merged_content.find("line 5 modified by theirs") != std::string::npos);
    EXPECT_TRUE(result.merged_content.find("line 3") != std::string::npos);
}

TEST(MergeTest, ThreeWayConflictingEdits) {
    std::string base = "function compute() {\n    return 42;\n}\n";
    // Both modify the same return statement differently
    std::string ours = "function compute() {\n    return 100;\n}\n";
    std::string theirs = "function compute() {\n    return 999;\n}\n";

    auto result = ThreeWayMerge::merge(base, ours, theirs, "HEAD", "feature");
    EXPECT_TRUE(result.has_conflict);
    EXPECT_TRUE(result.conflict_count > 0);

    // Verify conflict markers
    EXPECT_TRUE(result.merged_content.find("<<<<<<< HEAD") != std::string::npos);
    EXPECT_TRUE(result.merged_content.find("return 100;") != std::string::npos);
    EXPECT_TRUE(result.merged_content.find("=======") != std::string::npos);
    EXPECT_TRUE(result.merged_content.find("return 999;") != std::string::npos);
    EXPECT_TRUE(result.merged_content.find(">>>>>>> feature") != std::string::npos);
}

TEST(MergeTest, LowestCommonAncestorDAG) {
    auto test_dir = std::filesystem::temp_directory_path() / "chronogit_test_lca";
    std::error_code ec;
    std::filesystem::remove_all(test_dir, ec);
    std::filesystem::create_directories(test_dir, ec);

    ObjectDb db(test_dir / "objects");
    Tree empty_tree;
    ObjectId tree_id = db.write_object(empty_tree);
    Signature sig = Signature::now("Dev", "dev@example.com");

    // Commit DAG:
    //      C0 (root)
    //     /  \
    //    C1   C2
    //    |    |
    //    C3   C4
    // LCA of C3 and C4 should be C0!
    Commit c0(tree_id, {}, sig, sig, "C0");
    ObjectId id_c0 = db.write_object(c0);

    Commit c1(tree_id, {id_c0}, sig, sig, "C1");
    ObjectId id_c1 = db.write_object(c1);

    Commit c2(tree_id, {id_c0}, sig, sig, "C2");
    ObjectId id_c2 = db.write_object(c2);

    Commit c3(tree_id, {id_c1}, sig, sig, "C3");
    ObjectId id_c3 = db.write_object(c3);

    Commit c4(tree_id, {id_c2}, sig, sig, "C4");
    ObjectId id_c4 = db.write_object(c4);

    auto lca = LowestCommonAncestor::find_lca(db, id_c3, id_c4);
    ASSERT_TRUE(lca.has_value());
    EXPECT_EQ(*lca, id_c0);

    // LCA of C1 and C3 should be C1 (fast-forward relation)
    auto lca_ff = LowestCommonAncestor::find_lca(db, id_c1, id_c3);
    ASSERT_TRUE(lca_ff.has_value());
    EXPECT_EQ(*lca_ff, id_c1);

    std::filesystem::remove_all(test_dir, ec);
}
